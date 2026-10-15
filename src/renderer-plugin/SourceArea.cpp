#include "SourceArea.h"
#include "WFSUtils.h"

namespace ananas::WFS
{
    namespace
    {
        // -1 ... 1 to from ... to, and back.
        float toRange(const float normalised, const float from, const float to)
        {
            return from + (normalised + 1.f) * .5f * (to - from);
        }

        float fromRange(const float metres, const float from, const float to)
        {
            if (to <= from) return 0.f;
            return juce::jlimit(-1.f, 1.f, 2.f * (metres - from) / (to - from) - 1.f);
        }

        // Within ±MaxPositionMetres, `from` below `to`, at least the minimum
        // size apart; keeps `from` if it can.
        void limitAxis(float &from, float &to)
        {
            constexpr auto max = Constants::MaxPositionMetres;
            constexpr auto minSize = Constants::MinSourceAreaSizeMetres;

            if (to < from) std::swap(from, to);
            from = juce::jlimit(-max, max - minSize, from);
            to = juce::jlimit(from + minSize, max, to);
        }
    }

    juce::Point<float> SourceArea::toMetres(const juce::Point<float> normalised) const
    {
        return {toRange(normalised.x, xFrom, xTo), toRange(normalised.y, yFrom, yTo)};
    }

    juce::Point<float> SourceArea::toNormalised(const juce::Point<float> metres) const
    {
        return {fromRange(metres.x, xFrom, xTo), fromRange(metres.y, yFrom, yTo)};
    }

    juce::Rectangle<float> SourceArea::getBounds() const
    {
        return {xFrom, yFrom, xTo - xFrom, yTo - yFrom};
    }

    bool operator==(const SourceArea &a, const SourceArea &b)
    {
        return juce::exactlyEqual(a.xFrom, b.xFrom) && juce::exactlyEqual(a.xTo, b.xTo)
               && juce::exactlyEqual(a.yFrom, b.yFrom) && juce::exactlyEqual(a.yTo, b.yTo);
    }

    bool operator!=(const SourceArea &a, const SourceArea &b)
    {
        return !(a == b);
    }

    void AtomicSourceArea::store(const SourceArea &area)
    {
        xFrom.store(area.xFrom);
        xTo.store(area.xTo);
        yFrom.store(area.yFrom);
        yTo.store(area.yTo);
    }

    SourceArea AtomicSourceArea::load() const
    {
        return {xFrom.load(), xTo.load(), yFrom.load(), yTo.load()};
    }

    const SourceArea &SourceAreaSetup::getArea() const
    {
        return area;
    }

    void SourceAreaSetup::setX(const float from, const float to)
    {
        auto newArea = area;
        newArea.xFrom = from;
        newArea.xTo = to;
        apply(newArea);
    }

    void SourceAreaSetup::setY(const float from, const float to)
    {
        auto newArea = area;
        newArea.yFrom = from;
        newArea.yTo = to;
        apply(newArea);
    }

    juce::ValueTree SourceAreaSetup::toValueTree() const
    {
        juce::ValueTree tree(Identifiers::SourceAreaTreeType);
        tree.setProperty(Identifiers::SourceAreaXFromPropertyID, area.xFrom, nullptr);
        tree.setProperty(Identifiers::SourceAreaXToPropertyID, area.xTo, nullptr);
        tree.setProperty(Identifiers::SourceAreaYFromPropertyID, area.yFrom, nullptr);
        tree.setProperty(Identifiers::SourceAreaYToPropertyID, area.yTo, nullptr);
        return tree;
    }

    void SourceAreaSetup::fromValueTree(const juce::ValueTree &tree)
    {
        if (!tree.hasType(Identifiers::SourceAreaTreeType)) return;

        const SourceArea defaults;
        SourceArea newArea;
        newArea.xFrom = tree.getProperty(Identifiers::SourceAreaXFromPropertyID, defaults.xFrom);
        newArea.xTo = tree.getProperty(Identifiers::SourceAreaXToPropertyID, defaults.xTo);
        newArea.yFrom = tree.getProperty(Identifiers::SourceAreaYFromPropertyID, defaults.yFrom);
        newArea.yTo = tree.getProperty(Identifiers::SourceAreaYToPropertyID, defaults.yTo);
        apply(newArea);
    }

    void SourceAreaSetup::apply(SourceArea newArea)
    {
        limitAxis(newArea.xFrom, newArea.xTo);
        limitAxis(newArea.yFrom, newArea.yTo);

        if (newArea == area) return;

        area = newArea;
        sendChangeMessage();
    }
}
