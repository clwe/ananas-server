#include "IemLayout.h"

namespace ananas::WFS::IemLayout
{
    namespace
    {
        bool isNumber(const juce::var &v)
        {
            return v.isInt() || v.isInt64() || v.isDouble();
        }

        const juce::Array<juce::var> *getLoudspeakers(const juce::var &json)
        {
            return json["LoudspeakerLayout"]["Loudspeakers"].getArray();
        }
    }

    bool isIemLayout(const juce::var &json)
    {
        return getLoudspeakers(json) != nullptr;
    }

    bool hasUnitRadii(const juce::var &json)
    {
        const auto *speakers = getLoudspeakers(json);
        if (speakers == nullptr) return false;

        for (const auto &s: *speakers) {
            if (static_cast<bool>(s["IsImaginary"])) continue;
            if (!isNumber(s["Radius"]) || std::abs(static_cast<double>(s["Radius"]) - 1.0) > 1e-6) return false;
        }
        return true;
    }

    Speaker fromSpherical(const juce::Point<float> centre, const float azimuthDegrees, const float elevationDegrees,
                          const float radius)
    {
        const auto azimuth = juce::degreesToRadians(azimuthDegrees);
        const auto elevation = juce::degreesToRadians(elevationDegrees);

        // Front is +y, left is -x.
        const Direction outwards{-std::sin(azimuth) * std::cos(elevation),
                                 std::cos(azimuth) * std::cos(elevation),
                                 std::sin(elevation)};

        Speaker s;
        s.position = {centre.x + radius * outwards.x, centre.y + radius * outwards.y};
        s.z = radius * outwards.z;
        s.facing = Direction{-outwards.x, -outwards.y, -outwards.z}.normalised();
        return s;
    }

    LayoutFile::ReadResult read(const juce::var &json, const juce::Point<float> centre, const float radiusScale)
    {
        LayoutFile::ReadResult result;

        const auto *speakers = getLoudspeakers(json);
        if (speakers == nullptr) {
            result.errors.add("Not an IEM loudspeaker layout (no \"LoudspeakerLayout\" with \"Loudspeakers\").");
            return result;
        }
        if (!(radiusScale > 0.f) || !std::isfinite(radiusScale)) {
            result.errors.add("The radius must be a positive number.");
            return result;
        }

        // By channel; speakers without a channel keep their order, after the others.
        std::vector<std::pair<int, Speaker>> loaded;
        std::set<int> channels;
        auto numImaginary = 0;
        juce::StringArray gains, duplicates;

        for (int i = 0; i < speakers->size(); ++i) {
            const auto &entry = speakers->getReference(i);
            const auto name = "Loudspeaker " + juce::String{i + 1};

            if (static_cast<bool>(entry["IsImaginary"])) {
                ++numImaginary;
                continue;
            }
            if (!isNumber(entry["Azimuth"]) || !isNumber(entry["Elevation"]) || !isNumber(entry["Radius"])) {
                result.errors.add(name + " needs numbers for \"Azimuth\", \"Elevation\" and \"Radius\".");
                continue;
            }

            const auto radius = static_cast<float>(entry["Radius"]) * radiusScale;
            if (!(radius > 0.f)) {
                result.errors.add(name + " has no positive \"Radius\".");
                continue;
            }

            const auto channel = isNumber(entry["Channel"]) ? static_cast<int>(entry["Channel"])
                                                             : std::numeric_limits<int>::max();
            if (channel != std::numeric_limits<int>::max() && !channels.insert(channel).second) {
                duplicates.addIfNotAlreadyThere(juce::String{channel});
            }
            if (isNumber(entry["Gain"]) && std::abs(static_cast<double>(entry["Gain"]) - 1.0) > 1e-6) {
                gains.add(name);
            }

            loaded.emplace_back(channel, fromSpherical(centre,
                                                       static_cast<float>(entry["Azimuth"]),
                                                       static_cast<float>(entry["Elevation"]),
                                                       radius));
        }

        if (numImaginary > 0) {
            result.warnings.add(juce::String{numImaginary} + " imaginary loudspeaker"
                                + (numImaginary == 1 ? " was" : "s were") + " left out.");
        }
        if (!gains.isEmpty()) {
            result.warnings.add("Gains other than 1 are ignored (" + gains.joinIntoString(", ") + ").");
        }
        if (!duplicates.isEmpty()) {
            result.warnings.add("Channel" + juce::String{duplicates.size() == 1 ? " " : "s "}
                                + duplicates.joinIntoString(", ") + " used more than once.");
        }

        if (result.errors.isEmpty()) {
            std::stable_sort(loaded.begin(), loaded.end(),
                             [](const auto &a, const auto &b) { return a.first < b.first; });
            for (auto &[channel, s]: loaded) {
                result.speakers.push_back(std::move(s));
            }
        }

        return result;
    }
}
