#ifndef IEMLAYOUT_H
#define IEMLAYOUT_H

#include "LayoutFile.h"

namespace ananas::WFS
{
    /**
     * Import of IEM Plug-in Suite loudspeaker layouts (also used by SPARTA),
     * JSON like
     *
     *   { "Name": "...", "LoudspeakerLayout": { "Loudspeakers": [
     *       { "Azimuth": -18.0, "Elevation": 0.0, "Radius": 1.0,
     *         "IsImaginary": false, "Channel": 1, "Gain": 1.0 }, ... ] } }
     *
     * Directions are seen from the listening position: azimuth in degrees
     * from the front, counter-clockwise (positive is left); elevation in
     * degrees up from the horizontal plane. In the array frame, front is +y
     * and left is -x (as for the Ambisonics encoder).
     */
    namespace IemLayout
    {
        // Whether parsed JSON looks like an IEM layout rather than another format.
        bool isIemLayout(const juce::var &json);

        // Whether every non-imaginary speaker has radius 1, as in layouts
        // that only describe directions.
        bool hasUnitRadii(const juce::var &json);

        /**
         * The speakers, placed around `centre` (metres, array frame) with
         * their radii multiplied by `radiusScale`, each facing the centre,
         * sorted by channel, without a module. Imaginary speakers are left
         * out (with a warning), as are gains other than 1 (noted: there's
         * no per-speaker gain in Ananas).
         */
        LayoutFile::ReadResult read(const juce::var &json, juce::Point<float> centre, float radiusScale);

        // A speaker at a direction and distance from `centre`, facing it.
        Speaker fromSpherical(juce::Point<float> centre, float azimuthDegrees, float elevationDegrees, float radius);
    }
}

#endif //IEMLAYOUT_H
