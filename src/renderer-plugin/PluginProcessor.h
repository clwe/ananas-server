#ifndef PLUGINPROCESSOR_H
#define PLUGINPROCESSOR_H

#ifndef PRESENTATION_OFFSET
#define PRESENTATION_OFFSET 80
#endif

#include <Server.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ArrayLayout.h"
#include "SecondarySourceMessenger.h"
#include "Renderers/Renderer.h"

class PluginProcessor final : public juce::AudioProcessor,
                              public juce::ChangeListener,
                              public juce::AudioProcessorValueTreeState::Listener,
                              juce::AsyncUpdater,
                              juce::Timer
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

    void parameterChanged(const juce::String &parameterID, float newValue) override;

    /**
     * Put a module in an array slot; an empty IP clears the slot.
     */
    void assignModuleToSlot(int slot, const juce::String &moduleIP) const;

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

    static juce::ValueTree getLastChildWithType(const juce::ValueTree &tree, const juce::Identifier &type);

    static void removeChildrenWithType(juce::ValueTree &tree, const juce::Identifier &type);

    BusesProperties getBusesProperties(size_t numChannels);

    void handleAsyncUpdate() override;

    void timerCallback() override;

    /**
     * Recompute speaker positions from the module slots, number of modules
     * and speaker spacing, and send any that changed. Also activates each
     * renderer's stream if modules it drives are connected.
     */
    void updateArrayLayout();

    /**
     * Send speaker positions to connected modules that haven't got them yet.
     * @param resendUnconfirmed Also resend to modules whose announcements
     * don't echo the positions sent.
     */
    void sendSpeakerPositions(bool resendUnconfirmed);

    static bool isEchoedByModule(const ananas::ModuleInfo &info, const std::vector<juce::Point<float>> &positions);

    std::unique_ptr<ananas::Server::Server> server;

    // For handling (audio) parameters that are known at compile time.
    juce::AudioProcessorValueTreeState apvts;
    // For handling data that is not known until runtime.
    juce::ValueTree dynamicTree;
    // For handling user-entered data that should be storable/retrievable.
    juce::ValueTree persistentTree;

    ananas::WFS::SecondarySourceMessenger secondarySourceMessenger;

    // One per audio stream; renderers[i] feeds the server's stream i.
    std::vector<std::unique_ptr<ananas::Rendering::Renderer>> renderers;
    std::vector<juce::AudioBuffer<float>> streamBuffers;

    juce::HashMap<int, std::atomic<float> *> virtualSourceAmplitudes;

    ananas::WFS::ArrayLayout arrayLayout;

    struct SentPositions
    {
        std::vector<juce::Point<float>> positions;
        juce::uint32 timeMs{0};
    };

    std::map<juce::String, SentPositions> sentSpeakerPositions;
};


#endif //PLUGINPROCESSOR_H
