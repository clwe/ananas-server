#include "CustomLayout.h"
#include "WFSUtils.h"

namespace ananas::WFS
{
    namespace
    {
        // The rules every speaker of a custom layout follows.
        Speaker normalise(Speaker s)
        {
            if (!std::isfinite(s.position.x)) s.position.x = 0.f;
            if (!std::isfinite(s.position.y)) s.position.y = 0.f;
            if (s.moduleIp.isEmpty()) s.output = -1;
            return s;
        }
    }

    const SpeakerLayout &CustomLayout::getLayout() const
    {
        return layout;
    }

    void CustomLayout::setSpeakers(std::vector<Speaker> speakers)
    {
        apply(std::move(speakers));
    }

    int CustomLayout::addSpeaker(const Speaker &speaker)
    {
        auto speakers = layout.getSpeakers();
        speakers.push_back(speaker);
        apply(std::move(speakers));
        return static_cast<int>(layout.getSpeakers().size()) - 1;
    }

    void CustomLayout::removeSpeaker(const int index)
    {
        if (!isValidIndex(index)) return;

        auto speakers = layout.getSpeakers();
        speakers.erase(speakers.begin() + index);
        apply(std::move(speakers));
    }

    void CustomLayout::setPosition(const int index, const juce::Point<float> position)
    {
        if (!isValidIndex(index)) return;

        auto speakers = layout.getSpeakers();
        speakers[static_cast<size_t>(index)].position = position;
        apply(std::move(speakers));
    }

    void CustomLayout::setModule(const int index, const juce::String &moduleIp, const int output)
    {
        if (!isValidIndex(index)) return;

        auto speakers = layout.getSpeakers();
        auto &s = speakers[static_cast<size_t>(index)];
        s.moduleIp = moduleIp;
        s.output = output;
        apply(std::move(speakers));
    }

    juce::ValueTree CustomLayout::toValueTree() const
    {
        juce::ValueTree tree(Identifiers::CustomLayoutTreeType);
        const auto &speakers = layout.getSpeakers();

        for (size_t i = 0; i < speakers.size(); ++i) {
            const auto &s = speakers[i];
            juce::ValueTree speaker(Identifiers::SpeakerTreeType);
            speaker.setProperty(Identifiers::SpeakerIndexPropertyID, static_cast<int>(i), nullptr);
            speaker.setProperty(Identifiers::SpeakerXPropertyID, s.position.x, nullptr);
            speaker.setProperty(Identifiers::SpeakerYPropertyID, s.position.y, nullptr);
            speaker.setProperty(Identifiers::SpeakerModuleIpPropertyID, s.moduleIp, nullptr);
            speaker.setProperty(Identifiers::SpeakerOutputPropertyID, s.output, nullptr);
            tree.addChild(speaker, -1, nullptr);
        }

        return tree;
    }

    void CustomLayout::fromValueTree(const juce::ValueTree &tree)
    {
        if (!tree.hasType(Identifiers::CustomLayoutTreeType)) return;

        // Ordered by their index property, not by their order in the tree.
        std::vector<std::pair<int, Speaker>> indexed;

        for (const auto &child: tree) {
            if (!child.hasType(Identifiers::SpeakerTreeType)) continue;

            Speaker s;
            s.position = {child[Identifiers::SpeakerXPropertyID], child[Identifiers::SpeakerYPropertyID]};
            s.moduleIp = child[Identifiers::SpeakerModuleIpPropertyID].toString();
            s.output = child.getProperty(Identifiers::SpeakerOutputPropertyID, -1);
            indexed.emplace_back(child.getProperty(Identifiers::SpeakerIndexPropertyID, static_cast<int>(indexed.size())), s);
        }

        std::stable_sort(indexed.begin(), indexed.end(), [](const auto &a, const auto &b) { return a.first < b.first; });

        std::vector<Speaker> speakers;
        speakers.reserve(indexed.size());
        for (auto &[index, s]: indexed) {
            speakers.push_back(std::move(s));
        }

        apply(std::move(speakers));
    }

    void CustomLayout::apply(std::vector<Speaker> speakers)
    {
        for (auto &s: speakers) {
            s = normalise(std::move(s));
        }

        if (speakers == layout.getSpeakers()) return;

        layout.setSpeakers(std::move(speakers));
        sendChangeMessage();
    }

    bool CustomLayout::isValidIndex(const int index) const
    {
        return index >= 0 && index < static_cast<int>(layout.getSpeakers().size());
    }
}
