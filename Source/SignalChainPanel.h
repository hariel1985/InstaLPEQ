#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class SignalChainPanel : public juce::Component
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void chainOrderChanged (const std::array<InstaLPEQProcessor::ChainStage, InstaLPEQProcessor::numChainStages>& order) = 0;
    };

    SignalChainPanel();

    void setListener (Listener* l) { listener = l; }
    void setOrder (const std::array<InstaLPEQProcessor::ChainStage, InstaLPEQProcessor::numChainStages>& order);
    std::array<InstaLPEQProcessor::ChainStage, InstaLPEQProcessor::numChainStages> getOrder() const { return currentOrder; }

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    static constexpr int numBlocks = InstaLPEQProcessor::numChainStages;
    std::array<InstaLPEQProcessor::ChainStage, numBlocks> currentOrder {
        InstaLPEQProcessor::MasterGain,
        InstaLPEQProcessor::Limiter,
        InstaLPEQProcessor::MakeupGain
    };

    int draggedIndex = -1;
    float dragOffsetX = 0.0f;
    float dragCurrentX = 0.0f;

    Listener* listener = nullptr;

    juce::Rectangle<float> getBlockRect (int index) const;
    int getBlockAtX (float x) const;
    juce::String getStageName (InstaLPEQProcessor::ChainStage stage) const;
    juce::Colour getStageColour (InstaLPEQProcessor::ChainStage stage) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SignalChainPanel)
};
