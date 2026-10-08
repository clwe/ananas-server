#include "SwitchInfo.h"
#include "ServerUtils.h"
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

    juce::var SwitchInfo::toVar() const
    {
        const auto object{new juce::DynamicObject()};

        object->setProperty(Utils::Identifiers::SwitchIpPropertyID, ip);
        object->setProperty(Utils::Identifiers::SwitchUsernamePropertyID, username);
        object->setProperty(Utils::Identifiers::SwitchPasswordPropertyID, password);
        object->setProperty(Utils::Identifiers::SwitchFreqDriftPropertyId, static_cast<int>(freqDrift));
        object->setProperty(Utils::Identifiers::SwitchOffsetPropertyId, static_cast<int>(offset));
        object->setProperty(Utils::Identifiers::SwitchShouldResetPtpPropertyID, shouldResetPtp);

        return object;
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

                // Don't keep retrying a failed PTP reset in the background.
                if (!iter->second.shouldResetPtp) return;
                iter->second.shouldResetPtp = false;
            } else if (response.getArray()->isEmpty()) {
                // TODO: maybe don't make this dreadful assumption.
                //  Thing is, the switch returns an empty JSON array on
                //  (successful) PTP disable/enable.
                iter->second.lastError = {};
                iter->second.shouldResetPtp = false;
            } else {
                iter->second.lastError = {};
                const auto switchInfo{response.getArray()->getFirst()};
                iter->second.update(&switchInfo);
            }
        }

        sendChangeMessage();
    }

    juce::var SwitchList::toVar() const
    {
        const juce::ScopedLock sl{lock};
        const auto object{new juce::DynamicObject()};

        for (const auto &[identifier, switchInfo]: switches) {
            object->setProperty(identifier, switchInfo.toVar());
        }

        return object;
    }

    juce::ValueTree SwitchList::toValueTree(const bool includeStatus) const
    {
        const juce::ScopedLock sl{lock};
        juce::ValueTree tree(Utils::Identifiers::SwitchesParamID);

        for (const auto &[identifier, switchInfo]: switches) {
            auto switchTree{switchInfo.toValueTree(includeStatus)};
            switchTree.setProperty("identifier", identifier.toString(), nullptr);
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
                const auto identifier{switchTree.getProperty("identifier").toString()};
                if (identifier.isEmpty()) continue;
                switches[identifier] = SwitchInfo::fromValueTree(switchTree);
            }
        }

        // Let the UI know about the restored switches.
        sendChangeMessage();
    }
}
