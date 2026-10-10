#include "SpeakerLayout.h"

namespace ananas::WFS { 

const std::vector<Speaker> &SpeakerLayout::getSpeakers() const 
{
    return speakers;
}
void SpeakerLayout::setSpeakers(std::vector<Speaker> newSpeakers) 
{
    speakers = std::move(newSpeakers);
}
}