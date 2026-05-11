#ifndef PLUGINPROCESSOR_H
#define PLUGINPROCESSOR_H

#ifndef PRESENTATION_OFFSET
#define PRESENTATION_OFFSET 80
#endif

#include <Server.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "VirtualSourceMessenger.h"
#include "SecondarySourceMessenger.h"

class PluginProcessor final : public juce::AudioProcessor,
                              public juce::ChangeListener
{
public:
    PluginProcessor();

    ~PluginProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;

    void releaseResources() override;

    void processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override;

    void processBlock(juce::AudioBuffer<double> &buffer, juce::MidiBuffer &midiMessages) override;

    juce::AudioProcessorEditor *createEditor() override;

    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;

    bool producesMidi() const override;

    bool isMidiEffect() const override;

    double getTailLengthSeconds() const override;

    int getNumPrograms() override;

    int getCurrentProgram() override;

    void setCurrentProgram(int index) override;

    const juce::String getProgramName(int index) override;

    void changeProgramName(int index, const juce::String &newName) override;

    void getStateInformation(juce::MemoryBlock &destData) override;

    void setStateInformation(const void *data, int size) override;

    void changeListenerCallback(juce::ChangeBroadcaster *source) override;

    juce::AudioProcessorValueTreeState &getParamState();

    const juce::AudioProcessorValueTreeState &getParamState() const;

    juce::ValueTree &getDynamicTree();

    const juce::ValueTree &getDynamicTree() const;

    juce::ValueTree &getPersistentTree();

    const juce::ValueTree &getPersistentTree() const;

    ananas::Server::Server &getServer() const;

    juce::HashMap<int, std::atomic<float> *> &getSourceAmplitudes();

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    BusesProperties getBusesProperties(size_t numChannels);

    std::unique_ptr<ananas::Server::Server> server;

    // For handling (audio) parameters that are known at compile time.
    juce::AudioProcessorValueTreeState apvts;
    // For handling data that is not known until runtime.
    juce::ValueTree dynamicTree;
    // For handling user-entered data that should be storable/retrievable.
    juce::ValueTree persistentTree;

    ananas::WFS::SecondarySourceMessenger secondarySourceMessenger;

    ananas::WFS::VirtualSourceMessenger virtualSourceMessenger;

    juce::HashMap<int, std::atomic<float> *> virtualSourceAmplitudes;
};


#endif //PLUGINPROCESSOR_H
