#ifndef LAYOUTFILE_H
#define LAYOUTFILE_H

#include "SpeakerLayout.h"

namespace ananas::WFS
{
    /**
     * The Ananas speaker layout file: JSON with file-wide settings and one
     * entry per speaker, e.g.
     *
     *   {
     *     "format": "ananas-speaker-layout",
     *     "version": 1,
     *     "units": "metres",
     *     "coordinates": "array frame: ...",
     *     "speakers": [
     *       { "x": -1.5, "y": 0.0, "z": 0.0, "facing": [0.0, -1.0, 0.0],
     *         "module": "192.168.10.21", "output": 0 },
     *       { "x": -1.3, "y": 0.0 }
     *     ]
     *   }
     *
     * "z" (height, default 0) and "facing" (the direction the speaker points,
     * a vector [x, y, z]; default [0, -1, 0], towards the listener) are
     * optional. A speaker without "module" isn't driven by any module.
     * Unknown fields are ignored, so newer files with extra fields still
     * load.
     */
    namespace LayoutFile
    {
        constexpr int CurrentVersion = 1;

        // The file's content for these speakers.
        juce::String toJson(const SpeakerLayout &layout);

        struct ReadResult
        {
            // Empty if anything in the file is wrong; a layout is loaded
            // completely or not at all.
            std::vector<Speaker> speakers;
            // Why the file can't be loaded; empty if it can.
            juce::StringArray errors;
            // Loaded, but worth knowing, e.g. fields that are ignored.
            juce::StringArray warnings;

            [[nodiscard]] bool wasLoaded() const { return errors.isEmpty(); }
        };

        ReadResult fromJson(const juce::String &json);
    }
}

#endif //LAYOUTFILE_H
