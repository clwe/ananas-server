#ifndef PLUGINPROCESSOR_H
#define PLUGINPROCESSOR_H

#include <Server.h>
#include <juce_audio_processors/juce_audio_processors.h>


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

    juce::ValueTree &getDynamicTree();

    const juce::ValueTree &getDynamicTree() const;

    juce::ValueTree &getPersistentTree();

    const juce::ValueTree &getPersistentTree() const;

    ananas::Server::Server &getServer() const;

    juce::HashMap<int, std::atomic<float> *> &getSourceAmplitudes();

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)

    constexpr static size_t NumChannelsToSend{NUM_CHANNELS_TO_SEND};

    BusesProperties getBusesProperties(size_t numChannels);

    std::unique_ptr<ananas::Server::Server> server;

    // For handling data that is not known until runtime.
    juce::ValueTree dynamicTree;
    // For handling user-entered data that should be storable/retrievable.
    juce::ValueTree persistentTree;
    // The switch list, mirrored as child nodes of persistentTree for the UI.
    juce::ValueTree switchesTree;
};


#endif //PLUGINPROCESSOR_H
