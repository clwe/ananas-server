#ifndef LAYOUTCHECK_H
#define LAYOUTCHECK_H

#include "SpeakerLayout.h"
#include <ClientInfo.h>

namespace ananas::WFS
{
    /**
     * Something about a speaker layout that probably isn't what the user
     * intended, found by checkLayout().
     */
    struct LayoutProblem
    {
        enum class Type
        {
            // A speaker no module drives, e.g. in an empty slot.
            noModule,
            // The speaker's module isn't in the module list.
            unknownModule,
            // The speaker's module is known, but not connected.
            moduleNotConnected,
            // The module doesn't have the speaker's output.
            outputOutOfRange,
            // Another speaker uses the same module output.
            outputUsedTwice,
            // Another speaker is at (almost) the same position.
            samePosition,
            // A WFS module's speaker that isn't on y = 0. WFS modules assume a
            // straight array on y = 0, facing the listener, until speaker
            // orientation is sent too.
            wfsSpeakerOffArrayLine,
            // A WFS module's speaker further from the centre than the module
            // accepts (getMaxPositionMetres: ±10 m, or ±2.25 m on firmware
            // from before speaker protocol 1, for x and y); the module clips
            // its position.
            wfsSpeakerOutOfRange,
            // A WFS module's speaker pointing (almost) straight up or down:
            // WFS needs a horizontal direction.
            wfsSpeakerNotHorizontal,
            // A WFS module's speaker above or below the listening plane; WFS
            // is horizontal and ignores the height.
            wfsHeightIgnored,
            // A module that drives speakers has an output no speaker uses. On
            // speaker protocol 1 the server mutes it (info); on older firmware
            // it still plays, at the origin (warning). Not tied to one speaker.
            outputUnused,
        };

        enum class Severity
        {
            // Expected in some layouts, but worth knowing.
            info,
            // Probably wrong; the layout won't sound as intended.
            warning,
        };

        Type type;
        Severity severity;
        // Index into SpeakerLayout::getSpeakers(); -1 for problems that
        // concern a module rather than a speaker (outputUnused).
        int speakerIndex = -1;
        juce::String moduleIp;
        // Human-readable, for the UI.
        juce::String description;
    };

    /**
     * Checks a speaker layout against the module list, without changing
     * either. Problems are ordered by speaker, then module-level ones.
     */
    std::vector<LayoutProblem> checkLayout(const SpeakerLayout &layout,
                                           const std::vector<ModuleList::Entry> &modules);

    /**
     * The speakers and their problems as a tree for the UI: one Speaker child
     * per speaker (index, position, module, output, worst severity, problem
     * descriptions), and the module-level problems as a property.
     */
    juce::ValueTree toValueTree(const SpeakerLayout &layout, const std::vector<LayoutProblem> &problems);
}

#endif //LAYOUTCHECK_H
