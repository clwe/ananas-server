#ifndef RENDERLAYOUT_H
#define RENDERLAYOUT_H

#include <ClientInfo.h>
#include <juce_graphics/juce_graphics.h>
#include "SpeakerLayout.h"
#include "WFSUtils.h"

namespace ananas::WFS
{
    /**
     * What one module output gets: exactly what /ss/<j> sends (position, z,
     * facing, and gain 1 or 0 for whether a speaker uses it).
     */
    struct OutputGeometry
    {
        juce::Point<float> position;
        float z = 0.f;
        Direction facing;
        // Whether a speaker in the layout uses this output; unused outputs
        // are muted (on modules that support it).
        bool isUsed = false;
    };

    inline bool operator==(const OutputGeometry &a, const OutputGeometry &b)
    {
        return a.position == b.position && juce::exactlyEqual(a.z, b.z) && a.facing == b.facing && a.isUsed == b.isUsed;
    }

    inline bool operator!=(const OutputGeometry &a, const OutputGeometry &b)
    {
        return !(a == b);
    }

    // The speaker set id after `last` for /ss/id: never 0, which means
    // "nothing received" to a module.
    inline juce::uint32 nextSpeakerSetId(const juce::uint32 last)
    {
        return last == std::numeric_limits<juce::uint32>::max() ? 1 : last + 1;
    }

    // The largest |x| or |y| a module accepts for its speakers (WFS modules
    // also clamp source x to it), by the speaker protocol it announces.
    float getMaxPositionMetres(const ModuleInfo &info);

    /**
     * What the renderers and modules need, derived from a SpeakerLayout and the module
     * list: each module's speaker positions, the outermost speaker's x, and the sources
     * all WFS modules can render
     */
    struct RenderLayout
    {
        static RenderLayout compute(const SpeakerLayout& speakerLayout, const std::vector<ModuleList::Entry> &modules);

        [[nodiscard]] juce::var toVar() const;

        // Largest |x| of the WFS array's speakers (and of speakers without a
        // module); source x-coordinates are scaled to this, for all renderers.
        float outerSpeakerX{0.f};
        // Sources rendered by every assigned WFS module.
        int numRenderedSources{0};
        // Assigned WFS modules that render fewer sources than the plugin provides.
        int numLimitingModules{0};
        // One entry per output of every known module. Outputs no speaker
        // uses are at the origin, facing the listener, and not used.
        std::map<juce::String, std::vector<OutputGeometry>> moduleOutputs;
        // The smallest position limit of the assigned WFS modules
        // (getMaxPositionMetres); speakers and sources beyond it are clipped.
        float maxWfsPositionMetres{Constants::MaxPositionMetres};
        // Firmware type of each module referened by at least one speaker.
        std::map<juce::String, ::ananas::Utils::FirmwareType> assignedModuleTypes;

        /**
         * The used outputs of all assigned modules of one type.
         */
        [[nodiscard]] std::vector<OutputGeometry> getSpeakers(::ananas::Utils::FirmwareType firmwareType) const;

        // The positions of one module's outputs, e.g. for the legacy protocol.
        [[nodiscard]] static std::vector<juce::Point<float>> getPositions(const std::vector<OutputGeometry> &outputs);
    };
}

#endif //RENDERLAYOUT_H
