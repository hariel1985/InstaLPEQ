#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "LookAndFeel.h"
#include "EQCurveDisplay.h"
#include "NodeParameterPanel.h"

class InstaLPEQEditor : public juce::AudioProcessorEditor,
                         private juce::Timer,
                         private EQCurveDisplay::Listener,
                         private NodeParameterPanel::Listener
{
public:
    explicit InstaLPEQEditor (InstaLPEQProcessor& p);
    ~InstaLPEQEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    // EQCurveDisplay::Listener
    void bandAdded (int index, float freq, float gainDb) override;
    void bandRemoved (int index) override;
    void bandChanged (int index, const EQBand& band) override;
    void selectedBandChanged (int index) override;

    // NodeParameterPanel::Listener
    void nodeParameterChanged (int bandIndex, const EQBand& band) override;
    void nodeDeleteRequested (int bandIndex) override;

    void syncDisplayFromProcessor();

    InstaLPEQProcessor& processor;
    InstaLPEQLookAndFeel customLookAndFeel;

    EQCurveDisplay curveDisplay;
    NodeParameterPanel nodePanel;

    juce::Label titleLabel { {}, "INSTALPEQ" };
    juce::Label versionLabel { {}, "v1.1.3" };
    juce::ToggleButton bypassToggle;
    juce::Label bypassLabel { {}, "BYPASS" };

    juce::TextButton newBandButton { "NEW BAND" };
    juce::ComboBox qualitySelector;
    juce::Label qualityLabel { {}, "FIR" };
    juce::Label qualityWarning { {}, "" };

    juce::Slider masterGainSlider;
    juce::Label masterGainLabel { {}, "MASTER" };
    juce::ToggleButton limiterToggle;
    juce::Label limiterLabel { {}, "LIMITER" };

    juce::ComponentBoundsConstrainer constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstaLPEQEditor)
};
