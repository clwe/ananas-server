#ifndef SPEAKERLAYOUT_H
#define SPEAKERLAYOUT_H

#include <juce_graphics/juce_graphics.h>

namespace ananas::WFS { 

struct Speaker
{
    juce::Point<float> position;   // metres, array frame
    juce::String moduleIp;         // empty: no module drives this speaker
    int output = -1;               // output index on that module
};

inline bool operator==(const Speaker &a, const Speaker &b)
{
    return a.position == b.position && a.moduleIp == b.moduleIp && a.output == b.output;
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
