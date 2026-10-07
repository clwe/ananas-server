#ifndef SWITCHINFO_H
#define SWITCHINFO_H

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>

namespace ananas
{
    class SwitchInfo
    {
    public:
        void update(const juce::var *response);

        [[nodiscard]] juce::var toVar() const;

        [[nodiscard]] juce::ValueTree toValueTree(bool includeStatus = false) const;

        static SwitchInfo fromValueTree(const juce::ValueTree& tree);

        juce::String ip;
        juce::String username;
        juce::String password;
        bool shouldResetPtp{false};
        juce::String lastError;
    private:
        juce::var info;

        juce::String clockID;
        juce::int64 freqDrift{0};
        juce::String gmClockID;
        uint8_t gmPriority1{0};
        uint8_t gmPriority2{0};
        bool iAmGm{false};
        juce::String masterClockID;
        juce::String name;
        juce::int64 offset{0};
        uint8_t priority1{0};
        uint8_t priority2{0};
        juce::String slavePort;
        uint64_t slavePortDelay{0};
    };

    class SwitchList final : public juce::ChangeBroadcaster
    {
    public:
        void handleEdit(const juce::var &data);

        void handleResponse(const juce::Identifier &switchID, const juce::var &response);

        [[nodiscard]] juce::var toVar() const;

        [[nodiscard]] juce::ValueTree toValueTree(bool includeStatus = false) const;

        void fromValueTree(const juce::ValueTree& tree);

    private:
        // Accessed from the message thread (UI edits, state save/restore) and
        // the switch inspector thread.
        mutable juce::CriticalSection lock;
        std::map<juce::Identifier, SwitchInfo> switches;
    };
}


#endif //SWITCHINFO_H
