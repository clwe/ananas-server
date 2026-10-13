#ifndef SPEAKERLAYOUT_H
#define SPEAKERLAYOUT_H

#include <juce_graphics/juce_graphics.h>

namespace ananas::WFS { 

/**
 * The direction a speaker faces (its main axis), as a unit vector in the
 * array frame: x to the right, y towards the back of the array, z up. The
 * default faces the listener, who is in front of the array (y < 0).
 */
struct Direction
{
    float x = 0.f;
    float y = -1.f;
    float z = 0.f;

    /**
     * From angles in degrees: `facing` is the horizontal angle, 0 towards the
     * listener (-y), counter-clockwise seen from above (90 faces +x, to the
     * right); `tilt` is the angle above the horizontal plane (positive up).
     */
    static Direction fromAngles(float facingDegrees, float tiltDegrees);

    [[nodiscard]] float facingDegrees() const;

    [[nodiscard]] float tiltDegrees() const;

    // Length 1; the default direction if this one has no length or isn't finite.
    [[nodiscard]] Direction normalised() const;

    // Length of the part in the horizontal plane; 0 for straight up or down.
    [[nodiscard]] float horizontalLength() const;
};

inline bool operator==(const Direction &a, const Direction &b)
{
    return juce::exactlyEqual(a.x, b.x) && juce::exactlyEqual(a.y, b.y) && juce::exactlyEqual(a.z, b.z);
}

struct Speaker
{
    juce::Point<float> position;   // metres, array frame
    juce::String moduleIp;         // empty: no module drives this speaker
    int output = -1;               // output index on that module
    float z = 0.f;                 // metres above the listening plane
    Direction facing;              // where the speaker points
};

inline bool operator==(const Speaker &a, const Speaker &b)
{
    return a.position == b.position && a.moduleIp == b.moduleIp && a.output == b.output
           && juce::exactlyEqual(a.z, b.z) && a.facing == b.facing;
}

inline bool operator!=(const Speaker &a, const Speaker &b)
{
    return !(a == b);
}

class SpeakerLayout
{
public:
    [[nodiscard]] const std::vector<Speaker> &getSpeakers() const;
    void setSpeakers(std::vector<Speaker> newSpeakers);

private:
    std::vector<Speaker> speakers;
};

}
#endif //SPEAKERLAYOUT_H
