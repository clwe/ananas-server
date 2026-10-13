#ifndef PLUGINPROCESSOR_H
#define PLUGINPROCESSOR_H

#ifndef PRESENTATION_OFFSET
#define PRESENTATION_OFFSET 80
#endif

#include <Server.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "RenderLayout.h"
#include "SecondarySourceMessenger.h"
#include "Renderers/Renderer.h"
#include "LinearArray.h"
#include "CustomLayout.h"

class PluginProcessor final : public juce::AudioProcessor,
                              public juce::ChangeListener,
                              public juce::Timer
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

    /**
     * Put a module in an array slot; an empty IP clears the slot.
     */
    void assignModuleToSlot(int slot, const juce::String &moduleIP);

    juce::AudioProcessorValueTreeState &getParamState();

    const juce::AudioProcessorValueTreeState &getParamState() const;

    juce::ValueTree &getDynamicTree();

    const juce::ValueTree &getDynamicTree() const;

    juce::ValueTree &getPersistentTree();

    const juce::ValueTree &getPersistentTree() const;

    ananas::Server::Server &getServer() const;

    juce::HashMap<int, std::atomic<float> *> &getSourceAmplitudes();

    // get the linear array for the UI
    [[nodiscard]] ananas::WFS::LinearArraySetup& getLinearArraySetup();

    // The custom layout, for the UI to edit.
    [[nodiscard]] ananas::WFS::CustomLayout &getCustomLayout();

    [[nodiscard]] ananas::WFS::LayoutMode getLayoutMode() const;

    // Which layout the renderers use. The other one is kept, so switching
    // back and forth loses nothing.
    void setLayoutMode(ananas::WFS::LayoutMode mode);

    // Copies the speakers of the current linear array into the custom
    // layout, replacing what it had, and switches to it.
    void convertToCustomLayout();

    // Replaces the custom layout's speakers, e.g. with a loaded file, and
    // switches to it.
    void loadCustomLayout(std::vector<ananas::WFS::Speaker> speakers);

    // The speakers the renderers use now, from either layout.
    [[nodiscard]] const ananas::WFS::SpeakerLayout &getSpeakerLayout() const;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    static juce::ValueTree getLastChildWithType(const juce::ValueTree &tree, const juce::Identifier &type);

    static void removeChildrenWithType(juce::ValueTree &tree, const juce::Identifier &type);

    BusesProperties getBusesProperties(size_t numChannels);

    void timerCallback() override;

    /**
     * Recompute speaker positions from the module slots, number of modules
     * and speaker spacing, and send any that changed. Also activates each
     * renderer's stream if modules it drives are connected.
     */
    void updateRenderLayout();

    /**
     * Send speaker positions to connected modules that haven't got them yet.
     * @param resendUnconfirmed Also resend to modules whose announcements
     * don't echo the positions sent.
     */
    void sendSpeakerPositions(bool resendUnconfirmed);

    void updateLinearArrayTree();

    void updateCustomLayoutTree();

    // The linear array from the current setup and module list.
    [[nodiscard]] ananas::WFS::LinearArray generateLinearArray() const;

    static bool isEchoedByModule(const ananas::ModuleInfo &info, const std::vector<juce::Point<float>> &positions);

    std::unique_ptr<ananas::Server::Server> server;

    // For handling (audio) parameters that are known at compile time.
    juce::AudioProcessorValueTreeState apvts;
    // For handling data that is not known until runtime.
    juce::ValueTree dynamicTree;
    // For handling user-entered data that should be storable/retrievable.
    juce::ValueTree persistentTree;
    // The module list, mirrored as child nodes of persistentTree for the UI.
    juce::ValueTree modulesTree;
    // The switch list, mirrored as child nodes of persistentTree for the UI.
    juce::ValueTree switchesTree;
    // The linear array tree, mirrored for the UI.
    juce::ValueTree linearArrayTree;
    // The speakers in use and their problems, mirrored into dynamicTree for
    // the UI.
    juce::ValueTree speakerLayoutTree;
    // The layout mode and the custom layout, mirrored for the UI.
    juce::ValueTree layoutTree;
    juce::ValueTree customLayoutTree;

    ananas::WFS::SecondarySourceMessenger secondarySourceMessenger;

    // One per audio stream; renderers[i] feeds the server's stream i.
    std::vector<std::unique_ptr<ananas::Rendering::Renderer>> renderers;
    std::vector<juce::AudioBuffer<float>> streamBuffers;

    juce::HashMap<int, std::atomic<float> *> virtualSourceAmplitudes;

    ananas::WFS::RenderLayout renderLayout;

    ananas::WFS::LinearArraySetup linearArraySetup;
    ananas::WFS::CustomLayout customLayout;
    ananas::WFS::LayoutMode layoutMode = ananas::WFS::LayoutMode::linearArray;
    // The speakers in use, as of the last updateRenderLayout().
    ananas::WFS::SpeakerLayout speakerLayout;

    // The next speaker set id for /ss/id: never 0, which means "nothing
    // received" to a module.
    juce::uint32 takeSpeakerSetId();

    struct SentGeometry
    {
        std::vector<ananas::WFS::OutputGeometry> outputs;
        // The speaker protocol it was sent with, and the set id on protocol 1.
        int protocol{0};
        juce::uint32 setId{0};
        juce::uint32 timeMs{0};
    };

    std::map<juce::String, SentGeometry> sentSpeakerGeometry;
    juce::uint32 lastSpeakerSetId{0};
};


#endif //PLUGINPROCESSOR_H
