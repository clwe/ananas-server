#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <AnanasUtils.h>

PluginProcessor::PluginProcessor()
    : AudioProcessor(getBusesProperties(NumChannelsToSend)),
      server(std::make_unique<ananas::Server::Server>(NumChannelsToSend)),
      dynamicTree(ananas::Utils::Identifiers::DynamicTreeType),
      persistentTree(ananas::Utils::Identifiers::PersistentTreeType)
{
    switchesTree = persistentTree.getOrCreateChildWithName(ananas::Utils::Identifiers::SwitchesParamID, nullptr);

    server->getClientList()->addChangeListener(this);
    server->getAuthority()->addChangeListener(this);
    server->getSwitches()->addChangeListener(this);
}

PluginProcessor::~PluginProcessor()
{
    server->getClientList()->removeChangeListener(this);
    server->getAuthority()->removeChangeListener(this);
    server->getSwitches()->removeChangeListener(this);
}

void PluginProcessor::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
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

    const juce::AudioSourceChannelInfo block{buffer};

    server->getNextAudioBlock(block);
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
    ignoreUnused(destData);
}

void PluginProcessor::setStateInformation(const void *data, int size)
{
    juce::ignoreUnused(data, size);
}

void PluginProcessor::changeListenerCallback(juce::ChangeBroadcaster *source)
{
    if (const auto *clients = dynamic_cast<ananas::ClientList *>(source)) {
        dynamicTree.setProperty(ananas::Utils::Identifiers::ConnectedClientsParamID, clients->toVar(), nullptr);
    } else if (const auto *authority = dynamic_cast<ananas::AuthorityInfo *>(source)) {
        dynamicTree.setProperty(ananas::Utils::Identifiers::TimeAuthorityParamID, authority->toVar(), nullptr);
    } else if (const auto *switches = dynamic_cast<ananas::SwitchList *>(source)) {
        switchesTree.copyPropertiesAndChildrenFrom(switches->toValueTree(true), nullptr);
    }
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

juce::AudioProcessor::BusesProperties PluginProcessor::getBusesProperties(const size_t numChannels)
{
    BusesProperties buses;

    for (size_t i{1}; i <= numChannels; ++i) {
        buses.addBus(true, ananas::Utils::Strings::getInputLabel(i), juce::AudioChannelSet::mono());
        buses.addBus(false, ananas::Utils::Strings::getOutputLabel(i), juce::AudioChannelSet::mono());
    }

    return buses;
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
