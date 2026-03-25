#include "NodeParameterPanel.h"
#include "LookAndFeel.h"

NodeParameterPanel::NodeParameterPanel()
{
    setupSlider (freqSlider, freqLabel, 20.0, 20000.0, 1.0, " Hz");
    freqSlider.setSkewFactorFromMidPoint (1000.0);
    freqSlider.setDoubleClickReturnValue (true, 1000.0);

    setupSlider (gainSlider, gainLabel, -24.0, 24.0, 0.1, " dB");
    gainSlider.setDoubleClickReturnValue (true, 0.0);

    setupSlider (qSlider, qLabel, 0.1, 18.0, 0.01, "");
    qSlider.setSkewFactorFromMidPoint (1.0);
    qSlider.setDoubleClickReturnValue (true, 1.0);
    qSlider.getProperties().set (InstaLPEQLookAndFeel::knobTypeProperty, "dark");

    typeSelector.addItem ("Peak", 1);
    typeSelector.addItem ("Low Shelf", 2);
    typeSelector.addItem ("High Shelf", 3);
    typeSelector.setSelectedId (1, juce::dontSendNotification);
    typeSelector.addListener (this);
    addAndMakeVisible (typeSelector);

    deleteButton.addListener (this);
    addAndMakeVisible (deleteButton);

    addAndMakeVisible (bandInfoLabel);
    bandInfoLabel.setJustificationType (juce::Justification::centredLeft);

    setSelectedBand (-1, nullptr);
}

void NodeParameterPanel::setupSlider (juce::Slider& s, juce::Label& l, double min, double max, double step, const char* suffix)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    s.setRange (min, max, step);
    s.setTextValueSuffix (suffix);
    s.addListener (this);
    addAndMakeVisible (s);

    l.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (l);
}

void NodeParameterPanel::setSelectedBand (int index, const EQBand* band)
{
    selectedIndex = index;
    updatingFromExternal = true;

    bool hasBand = (index >= 0 && band != nullptr);
    freqSlider.setEnabled (hasBand);
    gainSlider.setEnabled (hasBand);
    qSlider.setEnabled (hasBand);
    typeSelector.setEnabled (hasBand);
    deleteButton.setEnabled (hasBand);

    if (hasBand)
    {
        currentBand = *band;
        freqSlider.setValue (band->frequency, juce::dontSendNotification);
        gainSlider.setValue (band->gainDb, juce::dontSendNotification);
        qSlider.setValue (band->q, juce::dontSendNotification);
        typeSelector.setSelectedId ((int) band->type + 1, juce::dontSendNotification);
        bandInfoLabel.setText ("Band " + juce::String (index + 1), juce::dontSendNotification);
    }
    else
    {
        bandInfoLabel.setText ("No band selected", juce::dontSendNotification);
    }

    updatingFromExternal = false;
    repaint();
}

void NodeParameterPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (InstaLPEQLookAndFeel::bgMedium.darker (0.2f));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (InstaLPEQLookAndFeel::bgLight.withAlpha (0.3f));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
}

void NodeParameterPanel::resized()
{
    auto bounds = getLocalBounds().reduced (6);
    float scale = (float) getHeight() / 90.0f;

    auto left = bounds.removeFromLeft (100);
    bandInfoLabel.setBounds (left.removeFromTop ((int) (20 * scale)));

    auto typeBounds = left.removeFromTop ((int) (22 * scale));
    typeSelector.setBounds (typeBounds.reduced (2));

    auto delBounds = left.removeFromTop ((int) (22 * scale));
    deleteButton.setBounds (delBounds.reduced (2));

    // Knobs take the rest
    int knobW = bounds.getWidth() / 3;
    int labelH = (int) std::max (14.0f, 16.0f * scale);

    auto freqArea = bounds.removeFromLeft (knobW);
    freqLabel.setBounds (freqArea.removeFromTop (labelH));
    freqSlider.setBounds (freqArea);

    auto gainArea = bounds.removeFromLeft (knobW);
    gainLabel.setBounds (gainArea.removeFromTop (labelH));
    gainSlider.setBounds (gainArea);

    auto qArea = bounds;
    qLabel.setBounds (qArea.removeFromTop (labelH));
    qSlider.setBounds (qArea);
}

void NodeParameterPanel::sliderValueChanged (juce::Slider* slider)
{
    if (updatingFromExternal || selectedIndex < 0 || listener == nullptr)
        return;

    if (slider == &freqSlider)
        currentBand.frequency = (float) freqSlider.getValue();
    else if (slider == &gainSlider)
        currentBand.gainDb = (float) gainSlider.getValue();
    else if (slider == &qSlider)
        currentBand.q = (float) qSlider.getValue();

    listener->nodeParameterChanged (selectedIndex, currentBand);
}

void NodeParameterPanel::comboBoxChanged (juce::ComboBox* box)
{
    if (updatingFromExternal || selectedIndex < 0 || listener == nullptr)
        return;

    if (box == &typeSelector)
    {
        currentBand.type = static_cast<EQBand::Type> (typeSelector.getSelectedId() - 1);
        listener->nodeParameterChanged (selectedIndex, currentBand);
    }
}

void NodeParameterPanel::buttonClicked (juce::Button* button)
{
    if (button == &deleteButton && selectedIndex >= 0 && listener != nullptr)
        listener->nodeDeleteRequested (selectedIndex);
}
