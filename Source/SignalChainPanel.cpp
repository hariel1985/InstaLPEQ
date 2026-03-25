#include "SignalChainPanel.h"
#include "LookAndFeel.h"

SignalChainPanel::SignalChainPanel()
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void SignalChainPanel::setOrder (const std::array<InstaLPEQProcessor::ChainStage, numBlocks>& order)
{
    if (currentOrder != order)
    {
        currentOrder = order;
        repaint();
    }
}

juce::String SignalChainPanel::getStageName (InstaLPEQProcessor::ChainStage stage) const
{
    switch (stage)
    {
        case InstaLPEQProcessor::MasterGain: return "MASTER GAIN";
        case InstaLPEQProcessor::Limiter:    return "LIMITER";
        case InstaLPEQProcessor::MakeupGain: return "AUTO GAIN";
        default: return "?";
    }
}

juce::Colour SignalChainPanel::getStageColour (InstaLPEQProcessor::ChainStage stage) const
{
    switch (stage)
    {
        case InstaLPEQProcessor::MasterGain: return juce::Colour (0xffff8833);
        case InstaLPEQProcessor::Limiter:    return juce::Colour (0xffff4455);
        case InstaLPEQProcessor::MakeupGain: return juce::Colour (0xff44bbff);
        default: return InstaLPEQLookAndFeel::textSecondary;
    }
}

juce::Rectangle<float> SignalChainPanel::getBlockRect (int index) const
{
    auto bounds = getLocalBounds().toFloat().reduced (2);
    float gap = 6.0f;
    float blockW = (bounds.getWidth() - gap * (numBlocks - 1)) / numBlocks;
    float x = bounds.getX() + index * (blockW + gap);
    return { x, bounds.getY(), blockW, bounds.getHeight() };
}

int SignalChainPanel::getBlockAtX (float x) const
{
    for (int i = 0; i < numBlocks; ++i)
    {
        if (getBlockRect (i).contains (x, getHeight() * 0.5f))
            return i;
    }
    return -1;
}

void SignalChainPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background
    g.setColour (InstaLPEQLookAndFeel::bgDark.darker (0.3f));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (InstaLPEQLookAndFeel::bgLight.withAlpha (0.2f));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    auto* lf = dynamic_cast<InstaLPEQLookAndFeel*> (&getLookAndFeel());

    // Draw arrows between blocks (scale with height)
    float arrowScale = bounds.getHeight() / 30.0f;
    for (int i = 0; i < numBlocks - 1; ++i)
    {
        auto r1 = getBlockRect (i);
        auto r2 = getBlockRect (i + 1);
        float arrowX = (r1.getRight() + r2.getX()) * 0.5f;
        float arrowY = bounds.getCentreY();
        float aw = 5.0f * arrowScale;
        float ah = 6.0f * arrowScale;
        g.setColour (InstaLPEQLookAndFeel::textSecondary.withAlpha (0.5f));

        juce::Path arrow;
        arrow.addTriangle (arrowX - aw, arrowY - ah, arrowX - aw, arrowY + ah, arrowX + aw, arrowY);
        g.fillPath (arrow);
    }

    // Draw blocks
    for (int i = 0; i < numBlocks; ++i)
    {
        bool isDragged = (i == draggedIndex);
        auto rect = getBlockRect (i);

        // If this block is being dragged, offset it
        if (isDragged)
        {
            float offset = dragCurrentX - dragOffsetX;
            rect = rect.withX (rect.getX() + offset);
        }

        auto colour = getStageColour (currentOrder[i]);

        // Block background
        g.setColour (isDragged ? colour.withAlpha (0.25f) : colour.withAlpha (0.12f));
        g.fillRoundedRectangle (rect, 4.0f);

        // Block border
        g.setColour (isDragged ? colour.withAlpha (0.8f) : colour.withAlpha (0.4f));
        g.drawRoundedRectangle (rect, 4.0f, isDragged ? 2.0f : 1.0f);

        // Label — scale with block height
        juce::Font font = lf ? lf->getBoldFont (std::max (12.0f, rect.getHeight() * 0.45f))
                              : juce::Font (juce::FontOptions (14.0f));
        g.setFont (font);
        g.setColour (isDragged ? colour : colour.withAlpha (0.8f));
        g.drawText (getStageName (currentOrder[i]), rect.reduced (4), juce::Justification::centred, false);
    }

    // "SIGNAL CHAIN" label on the left
    if (lf)
    {
        g.setFont (lf->getRegularFont (10.0f));
        g.setColour (InstaLPEQLookAndFeel::textSecondary.withAlpha (0.5f));
    }
}

void SignalChainPanel::resized() {}

void SignalChainPanel::mouseDown (const juce::MouseEvent& e)
{
    draggedIndex = getBlockAtX (e.position.x);
    if (draggedIndex >= 0)
    {
        dragOffsetX = e.position.x;
        dragCurrentX = e.position.x;
    }
}

void SignalChainPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (draggedIndex < 0)
        return;

    dragCurrentX = e.position.x;

    // Check if we should swap with a neighbor
    int targetIndex = getBlockAtX (e.position.x);
    if (targetIndex >= 0 && targetIndex != draggedIndex)
    {
        std::swap (currentOrder[draggedIndex], currentOrder[targetIndex]);
        draggedIndex = targetIndex;
        dragOffsetX = e.position.x;

        if (listener)
            listener->chainOrderChanged (currentOrder);
    }

    repaint();
}

void SignalChainPanel::mouseUp (const juce::MouseEvent&)
{
    draggedIndex = -1;
    repaint();
}
