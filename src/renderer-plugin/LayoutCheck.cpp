#include "LayoutCheck.h"
#include "WFSUtils.h"

namespace ananas::WFS
{
    namespace
    {
        // Closer than this, two speakers count as being at the same position.
        constexpr float SamePositionToleranceMetres = .01f;
        // Further than this from y = 0, a WFS speaker is off the array line.
        constexpr float ArrayLineToleranceMetres = .01f;
        // Above or below the listening plane by more than this, a speaker's
        // height counts.
        constexpr float HeightToleranceMetres = .01f;
        // A facing direction with less of a horizontal part than this
        // (about 84 degrees of tilt) gives WFS no usable direction.
        constexpr float MinHorizontalFacing = .1f;

        juce::String outputName(const Speaker &s)
        {
            return s.moduleIp + " output " + juce::String{s.output};
        }
    }

    std::vector<LayoutProblem> checkLayout(const SpeakerLayout &layout,
                                           const std::vector<ModuleList::Entry> &modules)
    {
        using Type = LayoutProblem::Type;
        using Severity = LayoutProblem::Severity;

        std::vector<LayoutProblem> problems;
        const auto &speakers = layout.getSpeakers();

        std::map<juce::String, const ModuleList::Entry *> modulesByIp;
        for (const auto &m: modules) {
            modulesByIp[m.ip] = &m;
        }

        // Which speakers use each output of each module, to find outputs
        // used twice or not at all.
        std::map<juce::String, std::map<int, std::vector<int>>> speakersByOutput;

        for (size_t i = 0; i < speakers.size(); ++i) {
            const auto &s = speakers[i];
            const auto index = static_cast<int>(i);

            if (s.moduleIp.isEmpty()) {
                problems.push_back({Type::noModule, Severity::info, index, {},
                                    "No module drives this speaker."});
            } else {
                const auto it = modulesByIp.find(s.moduleIp);
                if (it == modulesByIp.end()) {
                    problems.push_back({Type::unknownModule, Severity::warning, index, s.moduleIp,
                                        "Module " + s.moduleIp + " is unknown."});
                } else {
                    const auto &module = *it->second;
                    // Counts the module as part of the layout, even if this
                    // speaker's output is out of range.
                    auto &outputs = speakersByOutput[s.moduleIp];
                    if (!module.isConnected) {
                        problems.push_back({Type::moduleNotConnected, Severity::warning, index, s.moduleIp,
                                            "Module " + s.moduleIp + " isn't connected."});
                    }
                    if (module.info.firmwareType == ::ananas::Utils::FirmwareType::wfsModule) {
                        if (std::abs(s.position.y) > ArrayLineToleranceMetres) {
                            problems.push_back({Type::wfsSpeakerOffArrayLine, Severity::warning, index, s.moduleIp,
                                                "WFS speakers must be on y = 0 for now."});
                        }
                        if (std::abs(s.position.x) > Constants::MaxXMetres
                            || std::abs(s.position.y) > Constants::MaxXMetres) {
                            problems.push_back({Type::wfsSpeakerOutOfRange, Severity::warning, index, s.moduleIp,
                                                "WFS modules clip positions beyond "
                                                + juce::String{Constants::MaxXMetres, 2} + " m from the centre."});
                        }
                        if (s.facing.horizontalLength() < MinHorizontalFacing) {
                            problems.push_back({Type::wfsSpeakerNotHorizontal, Severity::warning, index, s.moduleIp,
                                                "WFS speakers need to face sideways, not up or down."});
                        }
                        if (std::abs(s.z) > HeightToleranceMetres) {
                            problems.push_back({Type::wfsHeightIgnored, Severity::info, index, s.moduleIp,
                                                "WFS ignores the height."});
                        }
                    }
                    if (s.output < 0 || s.output >= module.info.numSpeakers) {
                        problems.push_back({Type::outputOutOfRange, Severity::warning, index, s.moduleIp,
                                            "Module " + s.moduleIp + " has no output " + juce::String{s.output}
                                            + " (it has " + juce::String{module.info.numSpeakers} + ")."});
                    } else {
                        outputs[s.output].push_back(index);
                    }
                }
            }

            for (size_t j = 0; j < speakers.size(); ++j) {
                if (j == i) continue;
                if (s.position.getDistanceFrom(speakers[j].position) < SamePositionToleranceMetres) {
                    problems.push_back({Type::samePosition, Severity::warning, index, s.moduleIp,
                                        "Speaker " + juce::String{j + 1} + " is at the same position."});
                    break;
                }
            }
        }

        for (const auto &[ip, outputs]: speakersByOutput) {
            for (const auto &[output, indices]: outputs) {
                if (indices.size() < 2) continue;
                for (const auto index: indices) {
                    problems.push_back({Type::outputUsedTwice, Severity::warning, index, ip,
                                        outputName(speakers[static_cast<size_t>(index)])
                                        + " drives more than one speaker."});
                }
            }
        }

        // Outputs of modules in the layout that no speaker uses. Modules
        // without any speaker are simply unassigned, not a problem.
        for (const auto &[ip, outputs]: speakersByOutput) {
            const auto &module = *modulesByIp.at(ip);
            for (int output = 0; output < module.info.numSpeakers; ++output) {
                if (outputs.count(output) == 0) {
                    problems.push_back({Type::outputUnused, Severity::warning, -1, ip,
                                        "Module " + ip + " output " + juce::String{output}
                                        + " drives no speaker, but still plays."});
                }
            }
        }

        // By speaker (module-level problems last), then in the order found.
        std::stable_sort(problems.begin(), problems.end(), [](const LayoutProblem &a, const LayoutProblem &b)
        {
            const auto key = [](const LayoutProblem &p) { return p.speakerIndex < 0 ? std::numeric_limits<int>::max() : p.speakerIndex; };
            return key(a) < key(b);
        });

        return problems;
    }

    juce::ValueTree toValueTree(const SpeakerLayout &layout, const std::vector<LayoutProblem> &problems)
    {
        const auto &speakers = layout.getSpeakers();

        // Problems per speaker, and those about modules.
        std::vector<juce::StringArray> descriptions(speakers.size());
        std::vector<int> severities(speakers.size(), -1);
        juce::StringArray moduleProblems;

        for (const auto &p: problems) {
            if (p.speakerIndex >= 0 && p.speakerIndex < static_cast<int>(speakers.size())) {
                const auto i = static_cast<size_t>(p.speakerIndex);
                descriptions[i].add(p.description);
                severities[i] = juce::jmax(severities[i], static_cast<int>(p.severity));
            } else {
                moduleProblems.add(p.description);
            }
        }

        juce::ValueTree tree(Identifiers::SpeakerLayoutTreeType);
        tree.setProperty(Identifiers::SpeakerLayoutModuleProblemsPropertyID, moduleProblems.joinIntoString("\n"), nullptr);

        for (size_t i = 0; i < speakers.size(); ++i) {
            const auto &s = speakers[i];
            juce::ValueTree speaker(Identifiers::SpeakerTreeType);
            speaker.setProperty(Identifiers::SpeakerIndexPropertyID, static_cast<int>(i), nullptr);
            speaker.setProperty(Identifiers::SpeakerXPropertyID, s.position.x, nullptr);
            speaker.setProperty(Identifiers::SpeakerYPropertyID, s.position.y, nullptr);
            speaker.setProperty(Identifiers::SpeakerZPropertyID, s.z, nullptr);
            speaker.setProperty(Identifiers::SpeakerFacingXPropertyID, s.facing.x, nullptr);
            speaker.setProperty(Identifiers::SpeakerFacingYPropertyID, s.facing.y, nullptr);
            speaker.setProperty(Identifiers::SpeakerFacingZPropertyID, s.facing.z, nullptr);
            speaker.setProperty(Identifiers::SpeakerModuleIpPropertyID, s.moduleIp, nullptr);
            speaker.setProperty(Identifiers::SpeakerOutputPropertyID, s.output, nullptr);
            speaker.setProperty(Identifiers::SpeakerSeverityPropertyID, severities[i], nullptr);
            speaker.setProperty(Identifiers::SpeakerProblemsPropertyID, descriptions[i].joinIntoString("\n"), nullptr);
            tree.addChild(speaker, -1, nullptr);
        }

        return tree;
    }
}
