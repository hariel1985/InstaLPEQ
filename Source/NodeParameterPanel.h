#pragma once
#include <JuceHeader.h>
#include "EQBand.h"

class NodeParameterPanel : public juce::Component,
                           private juce::Slider::Listener,
                           private juce::ComboBox::Listener,
                           private juce::Button::Listener
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void nodeParameterChanged (int bandIndex, const EQBand& band) = 0;
        virtual void nodeDeleteRequested (int bandIndex) = 0;
    };

    NodeParameterPanel();

    void setListener (Listener* l) { listener = l; }
    void setSelectedBand (int index, const EQBand* band);
    int getSelectedIndex() const { return selectedIndex; }

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    void sliderValueChanged (juce::Slider* slider) override;
    void comboBoxChanged (juce::ComboBox* box) override;
    void buttonClicked (juce::Button* button) override;

    int selectedIndex = -1;
    EQBand currentBand;
    bool updatingFromExternal = false;

    juce::Slider freqSlider, gainSlider, qSlider;
    juce::Label freqLabel { {}, "FREQ" };
    juce::Label gainLabel { {}, "GAIN" };
    juce::Label qLabel    { {}, "Q" };
    juce::Label bandInfoLabel { {}, "No band selected" };
    juce::ComboBox typeSelector;
    juce::TextButton deleteButton { "DELETE" };

    Listener* listener = nullptr;

    void setupSlider (juce::Slider& s, juce::Label& l, double min, double max, double step, const char* suffix);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NodeParameterPanel)
};
