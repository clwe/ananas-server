#include "LinearArray.h"
#include "WFSUtils.h"
#include <numeric>

namespace ananas::WFS {

LinearArray generateLinearArray(const LinearArraySettings &settings, const std::vector<ModuleList::Entry> &modules)
{
    LinearArray array;

    const auto numSlots = static_cast<size_t>(juce::jmax(settings.numSlots, 0));
    std::vector<juce::String> slotIps(numSlots);

    for (size_t slot = 0; slot < numSlots; ++slot) {
        if (slot < settings.slotModuleIps.size())
            slotIps[slot] = settings.slotModuleIps[slot];

        // The module's output count if it's known; legacy stereo otherwise
        // (empty slot, or a module that isn't in the list).
        auto numSpeakers = ananas::Utils::Constants::LegacyNumSpeakers;
        for (const auto &m: modules) {
            if (!slotIps[slot].isEmpty() && m.ip == slotIps[slot])
                numSpeakers = m.info.numSpeakers;
        }
        array.slotNumSpeakers.push_back(numSpeakers);
    }

    int totalNumSpeakers = std::accumulate(array.slotNumSpeakers.begin(), array.slotNumSpeakers.end(), 0);
    array.arrayWidth = static_cast<float>(totalNumSpeakers) * settings.speakerSpacing;
    float outerSpeakerX = static_cast<float>(juce::jmax(totalNumSpeakers - 1, 0)) * settings.speakerSpacing / 2.f;

    std::vector<Speaker> speakers;
    int first = 0;
    for (size_t slot = 0; slot < slotIps.size(); ++slot) {
        for(int j = 0; j < array.slotNumSpeakers[slot]; ++j) {
            Speaker spk;
            spk.position = {-outerSpeakerX + float(first +j) * settings.speakerSpacing, 0};
            if (!slotIps[slot].isEmpty()) {
                spk.moduleIp = slotIps[slot];
                spk.output = j;
            }
            speakers.push_back(spk);
        }
        first += array.slotNumSpeakers[slot];
    }
    array.layout.setSpeakers(std::move(speakers));

    return array;
}

namespace
{
    bool operator==(const LinearArraySettings &a, const LinearArraySettings &b)
    {
        return a.numSlots == b.numSlots
               && juce::exactlyEqual(a.speakerSpacing, b.speakerSpacing)
               && a.slotModuleIps == b.slotModuleIps;
    }
}

LinearArraySetup::LinearArraySetup()
{
    settings.numSlots = Constants::DefaultNumSlots;
    settings.speakerSpacing = Constants::DefaultSpeakerSpacingMetres;
    settings.slotModuleIps.resize(static_cast<size_t>(settings.numSlots));
}

const LinearArraySettings &LinearArraySetup::getSettings() const
{
    return settings;
}

void LinearArraySetup::setNumSlots(const int numSlots)
{
    auto newSettings = settings;
    newSettings.numSlots = numSlots;
    apply(std::move(newSettings));
}

void LinearArraySetup::setSpeakerSpacing(const float spacing)
{
    auto newSettings = settings;
    newSettings.speakerSpacing = spacing;
    apply(std::move(newSettings));
}

void LinearArraySetup::assignModule(const int slot, const juce::String &moduleIp)
{
    if (slot < 0 || slot >= settings.numSlots) return;

    auto newSettings = settings;

    // A module is in one slot at most: take it out of any other slot first.
    if (moduleIp.isNotEmpty()) {
        for (auto &ip: newSettings.slotModuleIps) {
            if (ip == moduleIp) ip = {};
        }
    }

    newSettings.slotModuleIps[static_cast<size_t>(slot)] = moduleIp;
    apply(std::move(newSettings));
}

juce::ValueTree LinearArraySetup::toValueTree() const
{
    juce::ValueTree tree(Identifiers::LinearArrayTreeType);
    tree.setProperty(Identifiers::LinearArrayNumSlotsPropertyID, settings.numSlots, nullptr);
    tree.setProperty(Identifiers::LinearArraySpeakerSpacingPropertyID, settings.speakerSpacing, nullptr);

    for (size_t slot = 0; slot < settings.slotModuleIps.size(); ++slot) {
        juce::ValueTree slotTree(Identifiers::LinearArraySlotTreeType);
        slotTree.setProperty(Identifiers::LinearArraySlotIndexPropertyID, static_cast<int>(slot), nullptr);
        slotTree.setProperty(Identifiers::LinearArraySlotModuleIpPropertyID, settings.slotModuleIps[slot], nullptr);
        tree.addChild(slotTree, -1, nullptr);
    }

    return tree;
}

void LinearArraySetup::fromValueTree(const juce::ValueTree &tree)
{
    if (!tree.hasType(Identifiers::LinearArrayTreeType)) return;

    LinearArraySettings newSettings;
    newSettings.numSlots = tree.getProperty(Identifiers::LinearArrayNumSlotsPropertyID, Constants::DefaultNumSlots);
    newSettings.speakerSpacing = tree.getProperty(Identifiers::LinearArraySpeakerSpacingPropertyID, Constants::DefaultSpeakerSpacingMetres);
    newSettings.slotModuleIps.resize(static_cast<size_t>(juce::jmax(newSettings.numSlots, 0)));

    for (const auto &slotTree: tree) {
        if (!slotTree.hasType(Identifiers::LinearArraySlotTreeType)) continue;

        const int slot = slotTree.getProperty(Identifiers::LinearArraySlotIndexPropertyID, -1);
        if (slot < 0 || slot >= newSettings.numSlots) continue;

        newSettings.slotModuleIps[static_cast<size_t>(slot)] =
            slotTree.getProperty(Identifiers::LinearArraySlotModuleIpPropertyID).toString();
    }

    apply(std::move(newSettings));
}

void LinearArraySetup::apply(LinearArraySettings newSettings)
{
    newSettings.numSlots = juce::jlimit(Constants::MinNumSlots,
                                        static_cast<int>(Constants::MaxNumModules),
                                        newSettings.numSlots);
    newSettings.speakerSpacing = juce::jlimit(Constants::MinSpeakerSpacingMetres,
                                              Constants::MaxSpeakerSpacingMetres,
                                              newSettings.speakerSpacing);
    newSettings.slotModuleIps.resize(static_cast<size_t>(newSettings.numSlots));

    // A module is in one slot at most; the leftmost one wins.
    for (size_t slot = 0; slot < newSettings.slotModuleIps.size(); ++slot) {
        const auto &ip = newSettings.slotModuleIps[slot];
        if (ip.isEmpty()) continue;
        for (size_t other = slot + 1; other < newSettings.slotModuleIps.size(); ++other) {
            if (newSettings.slotModuleIps[other] == ip) newSettings.slotModuleIps[other] = {};
        }
    }

    if (newSettings == settings) return;

    settings = std::move(newSettings);
    sendChangeMessage();
}
}
