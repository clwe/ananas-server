#ifndef RENDERLAYOUT_H
#define RENDERLAYOUT_H

#include <ClientInfo.h>
#include <juce_graphics/juce_graphics.h>
#include "SpeakerLayout.h"

namespace ananas::WFS
{
    /**
     * What the renderers and modules need, derived from a SpeakerLayout and the module
     * list: each module's speaker positions, the outermost speaker's x, and the sources
     * all WFS modules can render
     */
    struct RenderLayout
    {
        static RenderLayout compute(const SpeakerLayout& speakerLayout, const std::vector<ModuleList::Entry> &modules);

        [[nodiscard]] juce::var toVar() const;

        // x-coordinate of the rightmost speaker; virtual source x-coordinates
        // are scaled to this.
        float outerSpeakerX{0.f};
        // Sources rendered by every assigned WFS module.
        int numRenderedSources{0};
        // Assigned WFS modules that render fewer sources than the plugin provides.
        int numLimitingModules{0};
        // Speaker positions for every known module. Unassigned modules get
        // (0, 0) for all speakers.
        std::map<juce::String, std::vector<juce::Point<float>>> modulePositions;
        // Firmware type of each module referened by at least one speaker.
        std::map<juce::String, ::ananas::Utils::FirmwareType> assignedModuleTypes;

        /**
         * Positions of the speakers of all assigned modules of one type.
         */
        [[nodiscard]] std::vector<juce::Point<float>> getSpeakerPositions(::ananas::Utils::FirmwareType firmwareType) const;
    };
}

#endif //RENDERLAYOUT_H
