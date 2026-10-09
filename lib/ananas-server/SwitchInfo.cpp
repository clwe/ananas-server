#include "SwitchInfo.h"
#include <AnanasUtils.h>

namespace ananas
{
    void SwitchInfo::update(const juce::var *response)
    {
        info = *response;
        const auto *obj{response->getDynamicObject()};
        if (obj->hasProperty(Utils::Identifiers::SwitchClockIdPropertyId)) {
            clockID = obj->getProperty(Utils::Identifiers::SwitchClockIdPropertyId);
        }
        if (obj->hasProperty(Utils::Identifiers::SwitchFreqDriftPropertyId)) {
            freqDrift = obj->getProperty(Utils::Identifiers::SwitchFreqDriftPropertyId);
        }
        if (obj->hasProperty(Utils::Identifiers::SwitchOffsetPropertyId)) {
            offset = obj->getProperty(Utils::Identifiers::SwitchOffsetPropertyId);
        }
    }

    juce::ValueTree SwitchInfo::toValueTree(const bool includeStatus) const
    {
        juce::ValueTree tree(Utils::Identifiers::SwitchTreeType);
        tree.setProperty(Utils::Identifiers::SwitchIpPropertyID, ip, nullptr);
        tree.setProperty(Utils::Identifiers::SwitchUsernamePropertyID, username, nullptr);
        tree.setProperty(Utils::Identifiers::SwitchPasswordPropertyID, password, nullptr);
        if (includeStatus) {
            tree.setProperty(Utils::Identifiers::SwitchFreqDriftPropertyId, freqDrift, nullptr);
            tree.setProperty(Utils::Identifiers::SwitchOffsetPropertyId, offset, nullptr);
        }
        return tree;
    }

    SwitchInfo SwitchInfo::fromValueTree(const juce::ValueTree &tree)
    {
        SwitchInfo info;
        info.ip = tree.getProperty(Utils::Identifiers::SwitchIpPropertyID);
        info.username = tree.getProperty(Utils::Identifiers::SwitchUsernamePropertyID);
        info.password = tree.getProperty(Utils::Identifiers::SwitchPasswordPropertyID);
        return info;
    }

    //==========================================================================
    void SwitchList::setSwitch(const juce::String& id, const juce::String& ip, const juce::String& username, const juce::String& password)
    {
        if (id.isEmpty()) return;

        bool changed{false};
        {
            const juce::ScopedLock sl{lock};

            // Find the switch, or create it.
            auto iter{switches.find(id)};
            if (iter == switches.end()) {
                iter = switches.insert(std::make_pair(juce::Identifier{id}, SwitchInfo{})).first;
                std::cout << "Adding " << id << std::endl;
                changed = true;
            }

            auto &s{iter->second};
            if (ip != s.ip || username != s.username || password != s.password) {
                s.ip = ip;
                s.username = username;
                s.password = password;
                // An error from the old settings no longer applies.
                s.lastError = {};
                changed = true;
            }
        }

        if (changed) sendChangeMessage();
    }

    void SwitchList::removeSwitch(const juce::String& id)
    {
        if (id.isEmpty()) return;

        bool changed{false};
        {
            const juce::ScopedLock sl{lock};

            // Find the switch and remove it.
            auto iter{switches.find(id)};
            if (iter != switches.end()) {
                switches.erase(iter);
                std::cout << "Removing " << id << std::endl;
                changed = true;
            }
        }

        if (changed) sendChangeMessage();
    }

    void SwitchList::requestPtpReset(const juce::String& id)
    {
        if (id.isEmpty()) return;

        {
            const juce::ScopedLock sl{lock};
            // Find the switch and set ptp reset request
            auto iter{switches.find(id)};
            if (iter != switches.end()) {
                iter->second.shouldResetPtp = true;
                std::cout << "Resetting PTP for " << id << std::endl;
            }
        }
    }

    bool SwitchList::takePtpResetRequest(const juce::Identifier &id)
    {
        const juce::ScopedLock sl{lock};
        const auto iter{switches.find(id)};
        if (iter == switches.end()) return false;
        return std::exchange(iter->second.shouldResetPtp, false);
    }

    void SwitchList::handleResponse(const juce::Identifier &switchID, const juce::var &response)
    {
        {
            const juce::ScopedLock sl{lock};

            // The switch may have been removed while the request was in flight.
            const auto iter{switches.find(switchID)};
            if (iter == switches.end()) return;

            if (!response.isArray()) {
                // Probably an error response, e.g. incorrect ip/username/password.
                // Report it once rather than on every poll.
                // TODO: Indicate this in the UI.
                const auto error{response.isVoid() ? juce::String{"no response"} : juce::JSON::toString(response, true)};
                if (error != iter->second.lastError) {
                    std::cerr << "Switch " << iter->second.ip << ": " << error << std::endl;
                    iter->second.lastError = error;
                }
                // Nothing the UI shows has changed.
                return;
            }

            if (response.getArray()->isEmpty()) {
                // TODO: maybe don't make this dreadful assumption.
                //  Thing is, the switch returns an empty JSON array on
                //  (successful) PTP disable/enable.
                iter->second.lastError = {};
            } else {
                iter->second.lastError = {};
                const auto switchInfo{response.getArray()->getFirst()};
                iter->second.update(&switchInfo);
            }
        }

        sendChangeMessage();
    }

    std::vector<SwitchList::Entry> SwitchList::getEntries() const
    {
        const juce::ScopedLock sl{lock};
        std::vector<Entry> entries;
        entries.reserve(switches.size());

        for (const auto &[id, s]: switches) {
            entries.push_back({id, s.ip, s.username, s.password});
        }

        return entries;
    }

    juce::ValueTree SwitchList::toValueTree(const bool includeStatus) const
    {
        const juce::ScopedLock sl{lock};
        juce::ValueTree tree(Utils::Identifiers::SwitchesParamID);

        for (const auto &[identifier, switchInfo]: switches) {
            auto switchTree{switchInfo.toValueTree(includeStatus)};
            switchTree.setProperty(Utils::Identifiers::SwitchIdentifierPropertyID, identifier.toString(), nullptr);
            tree.addChild(switchTree, -1, nullptr);
        }

        return tree;
    }

    void SwitchList::fromValueTree(const juce::ValueTree &tree)
    {
        {
            const juce::ScopedLock sl{lock};

            switches.clear();

            for (int i{0}; i < tree.getNumChildren(); ++i) {
                auto switchTree{tree.getChild(i)};
                if (!switchTree.hasType(Utils::Identifiers::SwitchTreeType)) continue;
                const auto identifier{switchTree.getProperty(Utils::Identifiers::SwitchIdentifierPropertyID).toString()};
                if (identifier.isEmpty()) continue;
                switches[identifier] = SwitchInfo::fromValueTree(switchTree);
            }
        }

        // Let the UI know about the restored switches.
        sendChangeMessage();
    }
}
