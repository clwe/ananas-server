#include "PluginProcessor.h"
#include "LinearArray.h"
#include "LayoutCheck.h"
#include "PluginEditor.h"
#include "WFSUtils.h"
#include "Renderers/WfsRenderer.h"
#include "Renderers/AmbisonicsRenderer.h"
#include <AnanasUtils.h>

PluginProcessor::PluginProcessor()
    : AudioProcessor(getBusesProperties(ananas::WFS::Constants::NumSources)),
      apvts(*this, nullptr, ananas::WFS::Identifiers::StaticTreeType, createParameterLayout()),
      dynamicTree(ananas::Utils::Identifiers::DynamicTreeType),
      persistentTree(ananas::Utils::Identifiers::PersistentTreeType)
{
    renderers.push_back(std::make_unique<ananas::WFS::WfsRenderer>(apvts));
    // The pass-through bus follows the source inputs.
    renderers.push_back(std::make_unique<ananas::Ambisonics::AmbisonicsRenderer>(
        apvts, static_cast<int>(ananas::WFS::Constants::NumSources)));

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

    linearArrayTree = persistentTree.getOrCreateChildWithName(ananas::WFS::Identifiers::LinearArrayTreeType, nullptr);

    updateLinearArrayTree();

    linearArraySetup.addChangeListener(this);

    layoutTree = persistentTree.getOrCreateChildWithName(ananas::WFS::Identifiers::LayoutTreeType, nullptr);
    layoutTree.setProperty(ananas::WFS::Identifiers::LayoutModePropertyID, ananas::WFS::toString(layoutMode), nullptr);
    customLayoutTree = persistentTree.getOrCreateChildWithName(ananas::WFS::Identifiers::CustomLayoutTreeType, nullptr);
    updateCustomLayoutTree();
    customLayout.addChangeListener(this);

    for (uint n{0}; n < ananas::WFS::Constants::NumSources; ++n) {
        // Set up source amplitudes for visualisation.
        virtualSourceAmplitudes.set(static_cast<int>(n), new std::atomic{0.f});
    }

    modulesTree = persistentTree.getOrCreateChildWithName(ananas::Utils::Identifiers::ModulesParamID, nullptr);

    switchesTree = persistentTree.getOrCreateChildWithName(ananas::Utils::Identifiers::SwitchesParamID, nullptr);

    speakerLayoutTree = dynamicTree.getOrCreateChildWithName(ananas::WFS::Identifiers::SpeakerLayoutTreeType, nullptr);

    updateRenderLayout();
    startTimer(ananas::WFS::Constants::SpeakerPositionResendIntervalMs / 2);
}

PluginProcessor::~PluginProcessor()
{
    server->getClientList()->removeChangeListener(this);
    server->getModuleList()->removeChangeListener(this);
    server->getAuthority()->removeChangeListener(this);
    server->getSwitches()->removeChangeListener(this);
    linearArraySetup.removeChangeListener(this);
    customLayout.removeChangeListener(this);
    stopTimer();
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

    // Store the max dB level for each source for the current buffer.
    for (auto ch{0}; ch < std::min(buffer.getNumChannels(), static_cast<int>(ananas::WFS::Constants::NumSources)); ++ch) {
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
    removeChildrenWithType(state, ananas::WFS::Identifiers::LinearArrayTreeType);
    removeChildrenWithType(state, ananas::WFS::Identifiers::CustomLayoutTreeType);
    removeChildrenWithType(state, ananas::WFS::Identifiers::LayoutTreeType);

    state.addChild(getServer().getSwitches()->toValueTree(), -1, nullptr);
    state.addChild(getServer().getModuleList()->toValueTree(), -1, nullptr);
    state.addChild(linearArraySetup.toValueTree(), -1, nullptr);
    state.addChild(customLayout.toValueTree(), -1, nullptr);

    juce::ValueTree layout(ananas::WFS::Identifiers::LayoutTreeType);
    layout.setProperty(ananas::WFS::Identifiers::LayoutModePropertyID, ananas::WFS::toString(layoutMode), nullptr);
    state.addChild(layout, -1, nullptr);

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

            const auto savedLinearArray{getLastChildWithType(tree, ananas::WFS::Identifiers::LinearArrayTreeType)};
            if (savedLinearArray.isValid()) {
                linearArraySetup.fromValueTree(savedLinearArray);
            }

            removeChildrenWithType(tree, ananas::Utils::Identifiers::SwitchesParamID);
            removeChildrenWithType(tree, ananas::Utils::Identifiers::ModulesParamID);
            const auto savedCustomLayout{getLastChildWithType(tree, ananas::WFS::Identifiers::CustomLayoutTreeType)};
            if (savedCustomLayout.isValid()) {
                customLayout.fromValueTree(savedCustomLayout);
            }

            // Projects without a layout mode use the linear array.
            const auto savedLayout{getLastChildWithType(tree, ananas::WFS::Identifiers::LayoutTreeType)};
            setLayoutMode(ananas::WFS::layoutModeFromString(savedLayout[ananas::WFS::Identifiers::LayoutModePropertyID].toString()));

            removeChildrenWithType(tree, ananas::WFS::Identifiers::LinearArrayTreeType);
            removeChildrenWithType(tree, ananas::WFS::Identifiers::CustomLayoutTreeType);
            removeChildrenWithType(tree, ananas::WFS::Identifiers::LayoutTreeType);

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
        ananas::Utils::syncChildren(modulesTree, modules->toValueTree(true), ananas::Utils::Identifiers::ModuleIpPropertyID);

        updateRenderLayout();

        for (const auto &renderer: renderers) {
            renderer->modulesChanged();
        }
    } else if (const auto *authority = dynamic_cast<ananas::AuthorityInfo *>(source)) {
        dynamicTree.setProperty(ananas::Utils::Identifiers::TimeAuthorityParamID, authority->toVar(), nullptr);
    } else if (const auto *switches = dynamic_cast<ananas::SwitchList *>(source)) {
        ananas::Utils::syncChildren(switchesTree, switches->toValueTree(true), ananas::Utils::Identifiers::SwitchIdentifierPropertyID);
    } else if (source == &linearArraySetup) {
        updateLinearArrayTree();
        updateRenderLayout();
    } else if (source == &customLayout) {
        updateCustomLayoutTree();
        updateRenderLayout();
    }
}

void PluginProcessor::updateCustomLayoutTree()
{
    ananas::Utils::syncChildren(customLayoutTree, customLayout.toValueTree(), ananas::WFS::Identifiers::SpeakerIndexPropertyID);
}

ananas::WFS::CustomLayout &PluginProcessor::getCustomLayout()
{
    return customLayout;
}

ananas::WFS::LayoutMode PluginProcessor::getLayoutMode() const
{
    return layoutMode;
}

void PluginProcessor::setLayoutMode(const ananas::WFS::LayoutMode mode)
{
    if (mode == layoutMode) return;

    layoutMode = mode;
    layoutTree.setProperty(ananas::WFS::Identifiers::LayoutModePropertyID, ananas::WFS::toString(layoutMode), nullptr);
    updateRenderLayout();
}

void PluginProcessor::convertToCustomLayout()
{
    loadCustomLayout(generateLinearArray().layout.getSpeakers());
}

void PluginProcessor::loadCustomLayout(std::vector<ananas::WFS::Speaker> speakers)
{
    customLayout.setSpeakers(std::move(speakers));
    setLayoutMode(ananas::WFS::LayoutMode::custom);
    // setLayoutMode only updates if the mode changes.
    updateRenderLayout();
}

const ananas::WFS::SpeakerLayout &PluginProcessor::getSpeakerLayout() const
{
    return speakerLayout;
}

ananas::WFS::LinearArray PluginProcessor::generateLinearArray() const
{
    return ananas::WFS::generateLinearArray(linearArraySetup.getSettings(), server->getModuleList()->getEntries());
}

void PluginProcessor::updateLinearArrayTree()
{
    ananas::Utils::syncChildren(linearArrayTree, linearArraySetup.toValueTree(), ananas::WFS::Identifiers::LinearArraySlotIndexPropertyID);
    linearArrayTree.setProperty(ananas::WFS::Identifiers::LinearArrayNumSlotsPropertyID, linearArraySetup.getSettings().numSlots, nullptr);
    linearArrayTree.setProperty(ananas::WFS::Identifiers::LinearArraySpeakerSpacingPropertyID, linearArraySetup.getSettings().speakerSpacing, nullptr);
}

ananas::WFS::LinearArraySetup& PluginProcessor::getLinearArraySetup()
{
    return linearArraySetup;
}


void PluginProcessor::timerCallback()
{
    sendSpeakerPositions(true);
}

void PluginProcessor::assignModuleToSlot(const int slot, const juce::String &moduleIP)
{
    linearArraySetup.assignModule(slot, moduleIP);
}

void PluginProcessor::updateRenderLayout()
{
    const auto modules{server->getModuleList()->getEntries()};
    const auto isCustom = layoutMode == ananas::WFS::LayoutMode::custom;

    const auto linArr = generateLinearArray();
    speakerLayout = isCustom ? customLayout.getLayout() : linArr.layout;
    const auto &speakers = speakerLayout;

    renderLayout = ananas::WFS::RenderLayout::compute(speakers, modules);

    // The speakers and their problems, for the speaker table.
    const auto speakerLayoutMirror = ananas::WFS::toValueTree(speakers, ananas::WFS::checkLayout(speakers, modules));
    ananas::Utils::syncChildren(speakerLayoutTree, speakerLayoutMirror, ananas::WFS::Identifiers::SpeakerIndexPropertyID);
    speakerLayoutTree.setProperty(ananas::WFS::Identifiers::SpeakerLayoutModuleProblemsPropertyID,
                                  speakerLayoutMirror[ananas::WFS::Identifiers::SpeakerLayoutModuleProblemsPropertyID],
                                  nullptr);

    // Slots only exist in a linear array: a custom layout gets no slot
    // selectors, and the XY view spans its outermost speakers.
    auto layoutVar = renderLayout.toVar();
    juce::Array<juce::var> slots;
    if (!isCustom) {
        for (const auto n: linArr.slotNumSpeakers)
            slots.add(n);
    }

    layoutVar.getDynamicObject()->setProperty(ananas::WFS::Identifiers::ArraySlotNumSpeakersPropertyID, slots);
    layoutVar.getDynamicObject()->setProperty(ananas::WFS::Identifiers::ArrayWidthPropertyID,
                                              isCustom ? 2.f * renderLayout.outerSpeakerX : linArr.arrayWidth);

    dynamicTree.setProperty(ananas::WFS::Identifiers::ArrayLayoutParamID, layoutVar, nullptr);

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
        renderers[i]->layoutChanged(renderLayout);
    }

    sendSpeakerPositions(false);
}

void PluginProcessor::sendSpeakerPositions(const bool resendUnconfirmed)
{
    const auto now{juce::Time::getMillisecondCounter()};

    for (const auto &[ip, info, isConnected]: server->getModuleList()->getEntries()) {
        if (!isConnected) {
            // Send again when it reconnects, e.g. after a reboot.
            sentSpeakerGeometry.erase(ip);
            continue;
        }

        const auto desired{renderLayout.moduleOutputs.find(ip)};
        if (desired == renderLayout.moduleOutputs.end()) continue;

        const auto &outputs{desired->second};
        const auto protocol{info.speakerProtocol >= 1 ? 1 : 0};

        const auto sent{sentSpeakerGeometry.find(ip)};
        auto shouldSend{sent == sentSpeakerGeometry.end()
                        || sent->second.outputs != outputs
                        || sent->second.protocol != protocol};

        // Modules don't store positions. If one doesn't confirm what was
        // sent, it has probably rebooted (or missed the message), so send
        // again. On protocol 1 it echoes the set id; on the legacy protocol
        // only the first two positions, which can't tell a reboot from
        // speakers that really are at the origin.
        if (!shouldSend && resendUnconfirmed &&
            now - sent->second.timeMs >= ananas::WFS::Constants::SpeakerPositionResendIntervalMs) {
            shouldSend = protocol == 1
                             ? info.speakerSetId != sent->second.setId
                             : !isEchoedByModule(info, ananas::WFS::RenderLayout::getPositions(outputs));
        }

        if (shouldSend) {
            // Record failed attempts too, so they're retried at the resend
            // interval rather than on every call.
            juce::uint32 setId{0};
            if (protocol == 1) {
                setId = takeSpeakerSetId();
                secondarySourceMessenger.sendGeometry(ip, setId, outputs);
            } else {
                secondarySourceMessenger.sendPositions(ip, ananas::WFS::RenderLayout::getPositions(outputs));
            }
            sentSpeakerGeometry[ip] = {outputs, protocol, setId, now};
        }
    }
}

juce::uint32 PluginProcessor::takeSpeakerSetId()
{
    lastSpeakerSetId = ananas::WFS::nextSpeakerSetId(lastSpeakerSetId);
    return lastSpeakerSetId;
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

    // Already-encoded Ambisonics, for the pass-through mode.
    buses.addBus(true, "Ambisonics in (ACN/SN3D)",
                 juce::AudioChannelSet::discreteChannels(static_cast<int>(ananas::WFS::Constants::NumAmbisonicChannels)));

    return buses;
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout params{};

    params.add(std::make_unique<juce::AudioParameterFloat>(
        ananas::WFS::Params::ListenerX.id,
        ananas::WFS::Params::ListenerX.name,
        ananas::WFS::Params::ListenerX.range,
        ananas::WFS::Params::ListenerX.defaultValue
    ));

    params.add(std::make_unique<juce::AudioParameterFloat>(
        ananas::WFS::Params::ListenerY.id,
        ananas::WFS::Params::ListenerY.name,
        ananas::WFS::Params::ListenerY.range,
        ananas::WFS::Params::ListenerY.defaultValue
    ));

    params.add(std::make_unique<juce::AudioParameterChoice>(
        ananas::WFS::Params::AmbisonicsInput.id,
        ananas::WFS::Params::AmbisonicsInput.name,
        ananas::WFS::Params::AmbisonicsInput.choices,
        ananas::WFS::Params::AmbisonicsInput.defaultIndex
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
