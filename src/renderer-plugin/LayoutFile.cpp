#include "LayoutFile.h"

namespace ananas::WFS::LayoutFile
{
    namespace
    {
        const juce::String FormatName{"ananas-speaker-layout"};
        const juce::String Units{"metres"};
        const juce::String Coordinates{
            "array frame: x to the right, y towards the back of the array, as seen by a listener in front of it; "
            "the array is on y = 0"
        };

        // Positions are written to a tenth of a millimetre, so the file shows
        // 0.2 rather than the float's 0.200000003.
        double rounded(const float value)
        {
            return std::round(static_cast<double>(value) * 1e4) / 1e4;
        }

        bool isNumber(const juce::var &v)
        {
            return v.isInt() || v.isInt64() || v.isDouble();
        }
    }

    juce::String toJson(const SpeakerLayout &layout)
    {
        juce::Array<juce::var> speakers;

        for (const auto &s: layout.getSpeakers()) {
            auto *speaker = new juce::DynamicObject;
            speaker->setProperty("x", rounded(s.position.x));
            speaker->setProperty("y", rounded(s.position.y));
            if (s.moduleIp.isNotEmpty()) {
                speaker->setProperty("module", s.moduleIp);
                speaker->setProperty("output", s.output);
            }
            speakers.add(speaker);
        }

        auto *file = new juce::DynamicObject;
        file->setProperty("format", FormatName);
        file->setProperty("version", CurrentVersion);
        file->setProperty("units", Units);
        file->setProperty("coordinates", Coordinates);
        file->setProperty("speakers", speakers);

        return juce::JSON::toString(juce::var{file});
    }

    ReadResult fromJson(const juce::String &json)
    {
        ReadResult result;

        juce::var file;
        if (const auto parsed = juce::JSON::parse(json, file); parsed.failed()) {
            result.errors.add("Not a valid JSON file: " + parsed.getErrorMessage());
            return result;
        }
        if (!file.isObject()) {
            result.errors.add("Not a speaker layout file.");
            return result;
        }

        if (file["format"].toString() != FormatName) {
            result.errors.add("Not an Ananas speaker layout file (\"format\" isn't \"" + FormatName + "\").");
            return result;
        }

        if (!isNumber(file["version"])) {
            result.errors.add("The file has no version.");
            return result;
        }
        if (static_cast<int>(file["version"]) > CurrentVersion) {
            result.errors.add("The file is from a newer version of Ananas (format version "
                              + file["version"].toString() + ").");
            return result;
        }

        if (file.hasProperty("units") && file["units"].toString() != Units) {
            result.errors.add("Units \"" + file["units"].toString() + "\" aren't supported; only \"" + Units + "\".");
        }

        const auto *speakers = file["speakers"].getArray();
        if (speakers == nullptr) {
            result.errors.add("The file has no speaker list (\"speakers\").");
            return result;
        }

        std::vector<Speaker> loaded;
        auto ignoresZ = false;

        for (int i = 0; i < speakers->size(); ++i) {
            const auto &entry = speakers->getReference(i);
            const auto name = "Speaker " + juce::String{i + 1};

            if (!entry.isObject()) {
                result.errors.add(name + " isn't an object.");
                continue;
            }
            if (!isNumber(entry["x"]) || !isNumber(entry["y"])) {
                result.errors.add(name + " needs numbers for \"x\" and \"y\".");
                continue;
            }

            Speaker s;
            s.position = {static_cast<float>(entry["x"]), static_cast<float>(entry["y"])};

            if (entry.hasProperty("module")) {
                s.moduleIp = entry["module"].toString().trim();
                if (!isNumber(entry["output"]) || static_cast<int>(entry["output"]) < 0) {
                    result.errors.add(name + " has a module, but no output (a number from 0).");
                    continue;
                }
                s.output = entry["output"];
            }

            if (isNumber(entry["z"]) && !juce::exactlyEqual(static_cast<double>(entry["z"]), 0.0)) {
                ignoresZ = true;
            }

            loaded.push_back(std::move(s));
        }

        if (ignoresZ) {
            result.warnings.add("Heights (\"z\") are ignored for now; all speakers are placed at z = 0.");
        }

        if (result.errors.isEmpty()) {
            result.speakers = std::move(loaded);
        }

        return result;
    }
}
