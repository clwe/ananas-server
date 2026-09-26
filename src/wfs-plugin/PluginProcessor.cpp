#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "WFSUtils.h"
#include <AnanasUtils.h>

PluginProcessor::PluginProcessor()
    : AudioProcessor(getBusesProperties(ananas::WFS::Constants::NumSources)),
      server(std::make_unique<ananas::Server::Server>(ananas::WFS::Constants::NumSources)),
      apvts(*this, nullptr, ananas::WFS::Identifiers::StaticTreeType, createParameterLayout()),
      dynamicTree(ananas::Utils::Identifiers::DynamicTreeType),
      persistentTree(ananas::Utils::Identifiers::PersistentTreeType),
      virtualSourceMessenger(ananas::WFS::Sockets::VirtualSourceMessengerSocketParams, apvts)
{
    server->getClientList()->addChangeListener(this);
    server->getModuleList()->addChangeListener(this);
    server->getAuthority()->addChangeListener(this);
    server->getSwitches()->addChangeListener(this);
    persistentTree.addListener(&secondarySourceMessenger);
    for (uint n{0}; n < ananas::WFS::Constants::NumSources; ++n) {
        // Set up source amplitudes for visualisation.
        virtualSourceAmplitudes.set(static_cast<int>(n), new std::atomic{0.f});
    }
}

PluginProcessor::~PluginProcessor()
{
    server->getClientList()->removeChangeListener(this);
    server->getModuleList()->removeChangeListener(this);
    server->getAuthority()->removeChangeListener(this);
    server->getSwitches()->removeChangeListener(this);
    persistentTree.removeListener(&secondarySourceMessenger);
    for (uint n{0}; n < ananas::WFS::Constants::NumSources; ++n) {
        apvts.removeParameterListener(ananas::WFS::Params::getVirtualSourcePositionParamID(n, ananas::WFS::SourcePositionAxis::X), &virtualSourceMessenger);
        apvts.removeParameterListener(ananas::WFS::Params::getVirtualSourcePositionParamID(n, ananas::WFS::SourcePositionAxis::Y), &virtualSourceMessenger);
    }
    if (virtualSourceMessenger.isThreadRunning()) {
        virtualSourceMessenger.stopThread(ananas::WFS::Constants::WFSMessengerThreadTimeout);
    }
}

void PluginProcessor::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
    server->prepareToPlay(samplesPerBlock, sampleRate);
    virtualSourceMessenger.startThread();
}

void PluginProcessor::releaseResources()
{
    server->releaseResources();
}

void PluginProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    ignoreUnused(midiMessages);

    juce::ScopedNoDenormals noDenormals;

    const juce::AudioSourceChannelInfo block{buffer};

    server->getNextAudioBlock(block);

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

        virtualSourceMessenger.parameterChanged(
            ananas::WFS::Params::SpeakerSpacing.id,
            apvts.getRawParameterValue(ananas::WFS::Params::SpeakerSpacing.id)->load()
        );

        for (size_t n{0}; n < ananas::WFS::Constants::NumSources; ++n) {
            auto idX{ananas::WFS::Params::getVirtualSourcePositionParamID(n, ananas::WFS::SourcePositionAxis::X)},
                    idY{ananas::WFS::Params::getVirtualSourcePositionParamID(n, ananas::WFS::SourcePositionAxis::Y)};
            virtualSourceMessenger.parameterChanged(idX, apvts.getRawParameterValue(idX)->load());
            virtualSourceMessenger.parameterChanged(idY, apvts.getRawParameterValue(idY)->load());
        }
    } else if (const auto *authority = dynamic_cast<ananas::AuthorityInfo *>(source)) {
        dynamicTree.setProperty(ananas::Utils::Identifiers::TimeAuthorityParamID, authority->toVar(), nullptr);
    } else if (const auto *switches = dynamic_cast<ananas::SwitchList *>(source)) {
        persistentTree.setProperty(ananas::Utils::Identifiers::SwitchesParamID, switches->toVar(), nullptr);
        dynamicTree.setProperty(ananas::Utils::Identifiers::SwitchesParamID, switches->toVar(), nullptr);
    }
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
