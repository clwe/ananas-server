#include "RenderLayout.h"
#include "WFSUtils.h"

namespace ananas::WFS
{
    float getMaxPositionMetres(const ModuleInfo &info)
    {
        return info.speakerProtocol >= 1 ? Constants::MaxPositionMetres : Constants::LegacyMaxPositionMetres;
    }

    RenderLayout RenderLayout::compute(const SpeakerLayout& speakerLayout, const std::vector<ModuleList::Entry> &modules)
    {
        RenderLayout layout;

        std::map<juce::String, const ModuleList::Entry*> modulesByIp;
        // Unassigned modules sit at the origin.
        for (const auto &m: modules) {
            layout.moduleOutputs[m.ip] = std::vector<OutputGeometry>(static_cast<size_t>(m.info.numSpeakers));
            modulesByIp[m.ip] = &m;
        }

        layout.numRenderedSources = static_cast<int>(Constants::NumSources);

        for (const auto &s: speakerLayout.getSpeakers()) {
            if (s.moduleIp.isEmpty()) continue; // no module drives it

            const auto it = layout.moduleOutputs.find(s.moduleIp);
            if (it == layout.moduleOutputs.end()) continue; // module not in the list

            auto &outputs = it->second; // one entry per module output
            const auto numOutputs = static_cast<int>(outputs.size());
            if (s.output < 0 || s.output >= numOutputs) continue;

            outputs[static_cast<size_t>(s.output)] = {s.position, s.z, s.facing, true};
            layout.assignedModuleTypes[s.moduleIp] = modulesByIp[s.moduleIp]->info.firmwareType;
        }

        // check the maximum possible number of sources that can be rendered for WFS
        for (const auto &m: modules) {
            // don't account for modules that are not assigned to any speaker
            if (layout.assignedModuleTypes.count(m.ip) == 0) continue;
            // Only WFS modules render the sources individually; for an
            // Ambisonics module, numSources counts Ambisonic channels.
            if (m.info.firmwareType == ananas::Utils::FirmwareType::wfsModule) {
                if (m.info.numSources < static_cast<int>(Constants::NumSources)) {
                    ++layout.numLimitingModules;
                }
                layout.numRenderedSources = juce::jmin(layout.numRenderedSources, m.info.numSources);
                layout.maxWfsPositionMetres = juce::jmin(layout.maxWfsPositionMetres, getMaxPositionMetres(m.info));
            }
        }
        return layout;
    }

    std::vector<OutputGeometry> RenderLayout::getSpeakers(const ::ananas::Utils::FirmwareType firmwareType) const
    {
        std::vector<OutputGeometry> speakers;

        for (const auto &[ip, type]: assignedModuleTypes) {
            if (type != firmwareType) continue;
            for (const auto &output: moduleOutputs.at(ip)) {
                if (output.isUsed) speakers.push_back(output);
            }
        }

        return speakers;
    }

    std::vector<juce::Point<float>> RenderLayout::getPositions(const std::vector<OutputGeometry> &outputs)
    {
        std::vector<juce::Point<float>> positions;
        positions.reserve(outputs.size());
        for (const auto &output: outputs) {
            positions.push_back(output.position);
        }
        return positions;
    }

    juce::var RenderLayout::toVar() const
    {
        const auto object{new juce::DynamicObject()};

        object->setProperty(Identifiers::SourceAreaXFromPropertyID, sourceArea.xFrom);
        object->setProperty(Identifiers::SourceAreaXToPropertyID, sourceArea.xTo);
        object->setProperty(Identifiers::SourceAreaYFromPropertyID, sourceArea.yFrom);
        object->setProperty(Identifiers::SourceAreaYToPropertyID, sourceArea.yTo);
        object->setProperty(Identifiers::ArrayNumRenderedSourcesPropertyID, numRenderedSources);
        object->setProperty(Identifiers::ArrayNumLimitingModulesPropertyID, numLimitingModules);
        object->setProperty(Identifiers::ArrayMaxWfsPositionPropertyID, maxWfsPositionMetres);

        return object;
    }
}
