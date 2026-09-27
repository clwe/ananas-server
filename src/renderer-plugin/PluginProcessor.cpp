#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "WFSUtils.h"
#include "Renderers/WfsRenderer.h"
#include <AnanasUtils.h>

PluginProcessor::PluginProcessor()
    : AudioProcessor(getBusesProperties(ananas::WFS::Constants::NumSources)),
      apvts(*this, nullptr, ananas::WFS::Identifiers::StaticTreeType, createParameterLayout()),
      dynamicTree(ananas::Utils::Identifiers::DynamicTreeType),
      persistentTree(ananas::Utils::Identifiers::PersistentTreeType)
{
    renderers.push_back(std::make_unique<ananas::WFS::WfsRenderer>(apvts));

    // One audio stream per renderer.
    std::vector<ananas::Server::StreamConfig> streamConfigs;
    for (const auto &renderer: renderers) {
        streamConfigs.push_back(renderer->getStreamConfig());
        streamBuffers.emplace_back(static_cast<int>(renderer->getStreamConfig().numChannels), 0);
    }
    server = std::make_unique<ananas::Server::Server>(streamConfigs);

    server->getClientList()->addChangeListener(this);
    server->getModuleList()->addChangeListener(this);
    server->getAuthority()->addChangeListener(this);
    server->getSwitches()->addChangeListener(this);
    apvts.addParameterListener(ananas::WFS::Params::NumModules.id, this);
    apvts.addParameterListener(ananas::WFS::Params::SpeakerSpacing.id, this);
    for (uint n{0}; n < ananas::WFS::Constants::NumSources; ++n) {
        // Set up source amplitudes for visualisation.
        virtualSourceAmplitudes.set(static_cast<int>(n), new std::atomic{0.f});
    }

    updateArrayLayout();
    startTimer(ananas::WFS::Constants::SpeakerPositionResendIntervalMs / 2);
}

PluginProcessor::~PluginProcessor()
{
    server->getClientList()->removeChangeListener(this);
    server->getModuleList()->removeChangeListener(this);
    server->getAuthority()->removeChangeListener(this);
    server->getSwitches()->removeChangeListener(this);
    apvts.removeParameterListener(ananas::WFS::Params::NumModules.id, this);
    apvts.removeParameterListener(ananas::WFS::Params::SpeakerSpacing.id, this);
    stopTimer();
    cancelPendingUpdate();
}

void PluginProcessor::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
    for (size_t i{0}; i < renderers.size(); ++i) {
        streamBuffers[i].setSize(streamBuffers[i].getNumChannels(), samplesPerBlock);
        renderers[i]->prepare(sampleRate, samplesPerBlock);
    }

    server->prepareToPlay(samplesPerBlock, sampleRate);
}

void PluginProcessor::releaseResources()
{
    server->releaseResources();
}

void PluginProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    ignoreUnused(midiMessages);

    juce::ScopedNoDenormals noDenormals;

    server->beginAudioBlock(buffer.getNumSamples());

    for (size_t i{0}; i < renderers.size(); ++i) {
        if (server->isStreamActive(i)) {
            auto &stream{streamBuffers[i]};
            // Doesn't reallocate unless the host sends a larger block than
            // announced in prepareToPlay.
            stream.setSize(stream.getNumChannels(), buffer.getNumSamples(), false, false, true);
            renderers[i]->process(buffer, stream);
            server->writeStream(i, stream);
        }
    }

    // Store the max dB level for each channel for the current buffer.
    for (auto ch{0}; ch < buffer.getNumChannels(); ++ch) {
        virtualSourceAmplitudes[ch]->store(juce::Decibels::gainToDecibels(buffer.getMagnitude(ch, 0, buffer.getNumSamples())));
    }
}

void PluginProcessor::processBlock(juce::AudioBuffer<double> &buffer, juce::MidiBuffer &midiMessages)
{
    juce::AudioBuffer<float> floatBuffer;

    floatBuffer.makeCopyOf(buffer);

    processBlock(floatBuffer, midiMessages);
}

juce::AudioProcessorEditor *PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

bool PluginProcessor::hasEditor() const
{
    return true;
}

const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool PluginProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool PluginProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String PluginProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void PluginProcessor::changeProgramName(int index, const juce::String &newName)
{
    ignoreUnused(index, newName);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state{apvts.copyState()};

    // The switch and module lists are managed by the server, not apvts; make
    // sure the saved state holds exactly one (current) copy of each.
    removeChildrenWithType(state, ananas::Utils::Identifiers::SwitchesParamID);
    removeChildrenWithType(state, ananas::Utils::Identifiers::ModulesParamID);

    state.addChild(getServer().getSwitches()->toValueTree(), -1, nullptr);

    state.addChild(getServer().getModuleList()->toValueTree(), -1, nullptr);

    const auto xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void *data, int size)
{
    const auto xmlState{getXmlFromBinary(data, size)};

    if (xmlState != nullptr) {
        auto tree{juce::ValueTree::fromXml(*xmlState)};

        if (tree.isValid()) {
            // Older versions saved an extra copy of these lists on every save;
            // the last one is the most recent.
            const auto switchListTree{getLastChildWithType(tree, ananas::Utils::Identifiers::SwitchesParamID)};
            if (switchListTree.isValid()) {
                getServer().getSwitches()->fromValueTree(switchListTree);
            }

            const auto moduleListTree{getLastChildWithType(tree, ananas::Utils::Identifiers::ModulesParamID)};
            if (moduleListTree.isValid()) {
                getServer().getModuleList()->fromValueTree(moduleListTree);
            }

            removeChildrenWithType(tree, ananas::Utils::Identifiers::SwitchesParamID);
            removeChildrenWithType(tree, ananas::Utils::Identifiers::ModulesParamID);

            apvts.replaceState(tree);
        }
    }
}

juce::ValueTree PluginProcessor::getLastChildWithType(const juce::ValueTree &tree, const juce::Identifier &type)
{
    for (auto i{tree.getNumChildren() - 1}; i >= 0; --i) {
        if (tree.getChild(i).hasType(type)) {
            return tree.getChild(i);
        }
    }
    return {};
}

void PluginProcessor::removeChildrenWithType(juce::ValueTree &tree, const juce::Identifier &type)
{
    for (auto i{tree.getNumChildren() - 1}; i >= 0; --i) {
        if (tree.getChild(i).hasType(type)) {
            tree.removeChild(i, nullptr);
        }
    }
}

void PluginProcessor::changeListenerCallback(juce::ChangeBroadcaster *source)
{
    if (const auto *clients = dynamic_cast<ananas::ClientList *>(source)) {
        dynamicTree.setProperty(ananas::Utils::Identifiers::ConnectedClientsParamID, clients->toVar(), nullptr);
    } else if (const auto *modules = dynamic_cast<ananas::ModuleList *>(source)) {
        persistentTree.setProperty(ananas::Utils::Identifiers::ModulesParamID, modules->toVar(), nullptr);
        persistentTree.sendPropertyChangeMessage(ananas::Utils::Identifiers::ModulesParamID);

        updateArrayLayout();

        for (const auto &renderer: renderers) {
            renderer->modulesChanged();
        }
    } else if (const auto *authority = dynamic_cast<ananas::AuthorityInfo *>(source)) {
        dynamicTree.setProperty(ananas::Utils::Identifiers::TimeAuthorityParamID, authority->toVar(), nullptr);
    } else if (const auto *switches = dynamic_cast<ananas::SwitchList *>(source)) {
        persistentTree.setProperty(ananas::Utils::Identifiers::SwitchesParamID, switches->toVar(), nullptr);
        dynamicTree.setProperty(ananas::Utils::Identifiers::SwitchesParamID, switches->toVar(), nullptr);
    }
}

void PluginProcessor::parameterChanged(const juce::String &parameterID, const float newValue)
{
    juce::ignoreUnused(parameterID, newValue);

    // May be called from the audio thread; update the layout on the message thread.
    triggerAsyncUpdate();
}

void PluginProcessor::handleAsyncUpdate()
{
    updateArrayLayout();
}

void PluginProcessor::timerCallback()
{
    sendSpeakerPositions(true);
}

void PluginProcessor::assignModuleToSlot(const int slot, const juce::String &moduleIP) const
{
    server->getModuleList()->assignSlot(slot, moduleIP);
}

void PluginProcessor::updateArrayLayout()
{
    const auto numSlots{static_cast<int>(apvts.getRawParameterValue(ananas::WFS::Params::NumModules.id)->load())};
    const auto speakerSpacing{apvts.getRawParameterValue(ananas::WFS::Params::SpeakerSpacing.id)->load()};
    const auto modules{server->getModuleList()->getEntries()};

    arrayLayout = ananas::WFS::ArrayLayout::compute(modules, numSlots, speakerSpacing);

    dynamicTree.setProperty(ananas::WFS::Identifiers::ArrayLayoutParamID, arrayLayout.toVar(), nullptr);

    // A renderer's stream is only sent while modules that play it are
    // connected.
    for (size_t i{0}; i < renderers.size(); ++i) {
        const auto isNeeded{
            std::any_of(modules.begin(), modules.end(), [&](const ananas::ModuleList::Entry &m)
            {
                return m.isConnected && renderers[i]->drivesModule(m.info.firmwareType);
            })
        };
        server->setStreamActive(i, isNeeded);
        renderers[i]->layoutChanged(arrayLayout);
    }

    sendSpeakerPositions(false);
}

void PluginProcessor::sendSpeakerPositions(const bool resendUnconfirmed)
{
    const auto now{juce::Time::getMillisecondCounter()};

    for (const auto &[ip, info, isConnected]: server->getModuleList()->getEntries()) {
        if (!isConnected) {
            // Send again when it reconnects, e.g. after a reboot.
            sentSpeakerPositions.erase(ip);
            continue;
        }

        const auto desired{arrayLayout.modulePositions.find(ip)};
        if (desired == arrayLayout.modulePositions.end()) continue;

        const auto sent{sentSpeakerPositions.find(ip)};
        auto shouldSend{sent == sentSpeakerPositions.end() || sent->second.positions != desired->second};

        // Modules don't store positions, and only echo the first two in their
        // announcements; if those don't match, the module has probably
        // rebooted (or missed the message), so send them again.
        if (!shouldSend && resendUnconfirmed &&
            now - sent->second.timeMs >= ananas::WFS::Constants::SpeakerPositionResendIntervalMs) {
            shouldSend = !isEchoedByModule(info, desired->second);
        }

        if (shouldSend) {
            // Record failed attempts too, so they're retried at the resend
            // interval rather than on every call.
            secondarySourceMessenger.sendPositions(ip, desired->second);
            sentSpeakerPositions[ip] = {desired->second, now};
        }
    }
}

bool PluginProcessor::isEchoedByModule(const ananas::ModuleInfo &info, const std::vector<juce::Point<float>> &positions)
{
    const auto matches{
        [](const std::pair<float, float> &reported, const juce::Point<float> &expected)
        {
            return std::abs(reported.first - expected.x) <= ananas::WFS::Constants::SpeakerPositionToleranceMetres &&
                   std::abs(reported.second - expected.y) <= ananas::WFS::Constants::SpeakerPositionToleranceMetres;
        }
    };

    return (positions.empty() || matches(info.reportedSecondarySource0, positions[0])) &&
           (positions.size() < 2 || matches(info.reportedSecondarySource1, positions[1]));
}

juce::AudioProcessorValueTreeState &PluginProcessor::getParamState()
{
    return apvts;
}

const juce::AudioProcessorValueTreeState &PluginProcessor::getParamState() const
{
    return apvts;
}

juce::ValueTree &PluginProcessor::getDynamicTree()
{
    return dynamicTree;
}

const juce::ValueTree &PluginProcessor::getDynamicTree() const
{
    return dynamicTree;
}

juce::ValueTree &PluginProcessor::getPersistentTree()
{
    return persistentTree;
}

const juce::ValueTree &PluginProcessor::getPersistentTree() const
{
    return persistentTree;
}

ananas::Server::Server &PluginProcessor::getServer() const
{
    return *server;
}

juce::HashMap<int, std::atomic<float> *> &PluginProcessor::getSourceAmplitudes()
{
    return virtualSourceAmplitudes;
}

juce::AudioProcessor::BusesProperties PluginProcessor::getBusesProperties(const size_t numChannels)
{
    BusesProperties buses;

    for (size_t i{1}; i <= numChannels; ++i) {
        buses.addBus(true, ananas::Utils::Strings::getInputLabel(i), juce::AudioChannelSet::mono());
        buses.addBus(false, ananas::Utils::Strings::getOutputLabel(i), juce::AudioChannelSet::mono());
    }

    return buses;
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout params{};

    params.add(std::make_unique<juce::AudioParameterInt>(
        ananas::WFS::Params::NumModules.id,
        ananas::WFS::Params::NumModules.name,
        ananas::WFS::Params::NumModules.minValue,
        ananas::WFS::Params::NumModules.maxValue,
        ananas::WFS::Params::NumModules.defaultValue
    ));

    params.add(std::make_unique<juce::AudioParameterFloat>(
        ananas::WFS::Params::SpeakerSpacing.id,
        ananas::WFS::Params::SpeakerSpacing.name,
        ananas::WFS::Params::SpeakerSpacing.range,
        ananas::WFS::Params::SpeakerSpacing.defaultValue
    ));

    params.add(std::make_unique<juce::AudioParameterBool>(
        ananas::WFS::Params::ShowModuleSelectors.id,
        ananas::WFS::Params::ShowModuleSelectors.name,
        ananas::WFS::Params::ShowModuleSelectors.defaultValue
    ));

    for (uint n{0}; n < ananas::WFS::Constants::NumSources; ++n) {
        params.add(std::make_unique<juce::AudioParameterFloat>(
            ananas::WFS::Params::getVirtualSourcePositionParamID(n, ananas::WFS::SourcePositionAxis::X),
            ananas::WFS::Params::getVirtualSourcePositionParamName(n, ananas::WFS::SourcePositionAxis::X),
            ananas::WFS::Params::VirtualSourcePositionRange,
            ananas::WFS::Params::getVirtualSourceDefaultX(n)
        ));
        params.add(std::make_unique<juce::AudioParameterFloat>(
            ananas::WFS::Params::getVirtualSourcePositionParamID(n, ananas::WFS::SourcePositionAxis::Y),
            ananas::WFS::Params::getVirtualSourcePositionParamName(n, ananas::WFS::SourcePositionAxis::Y),
            ananas::WFS::Params::VirtualSourcePositionRange,
            ananas::WFS::Params::VirtualSourceDefaultY
        ));
    }

    return params;
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
