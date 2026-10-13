#include "SpeakerLayout.h"

namespace ananas::WFS { 

Direction Direction::fromAngles(const float facingDegrees, const float tiltDegrees)
{
    const auto facing = juce::degreesToRadians(facingDegrees);
    const auto tilt = juce::degreesToRadians(tiltDegrees);
    // Facing 0 is -y; turning counter-clockwise (seen from above) goes to +x.
    return Direction{std::sin(facing) * std::cos(tilt), -std::cos(facing) * std::cos(tilt), std::sin(tilt)}.normalised();
}

float Direction::facingDegrees() const
{
    if (horizontalLength() < 1e-6f) return 0.f;
    return juce::radiansToDegrees(std::atan2(x, -y));
}

float Direction::tiltDegrees() const
{
    return juce::radiansToDegrees(std::atan2(z, horizontalLength()));
}

Direction Direction::normalised() const
{
    const auto length = std::sqrt(x * x + y * y + z * z);
    if (!std::isfinite(length) || length < 1e-6f) return {};
    return {x / length, y / length, z / length};
}

float Direction::horizontalLength() const
{
    return std::sqrt(x * x + y * y);
}

const std::vector<Speaker> &SpeakerLayout::getSpeakers() const 
{
    return speakers;
}
void SpeakerLayout::setSpeakers(std::vector<Speaker> newSpeakers) 
{
    speakers = std::move(newSpeakers);
}
}