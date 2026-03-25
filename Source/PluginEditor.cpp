#include "PluginEditor.h"

InstaLPEQEditor::InstaLPEQEditor (InstaLPEQProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&customLookAndFeel);
    juce::LookAndFeel::setDefaultLookAndFeel (&customLookAndFeel);

    // Title
    titleLabel.setFont (customLookAndFeel.getBoldFont (26.0f));
    titleLabel.setColour (juce::Label::textColourId, InstaLPEQLookAndFeel::accent);
    addAndMakeVisible (titleLabel);

    versionLabel.setFont (customLookAndFeel.getRegularFont (13.0f));
    versionLabel.setColour (juce::Label::textColourId, InstaLPEQLookAndFeel::textSecondary);
    versionLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (versionLabel);

    // Bypass
    bypassToggle.setToggleState (processor.bypassed.load(), juce::dontSendNotification);
    addAndMakeVisible (bypassToggle);
    bypassLabel.setFont (customLookAndFeel.getMediumFont (13.0f));
    bypassLabel.setColour (juce::Label::textColourId, InstaLPEQLookAndFeel::textSecondary);
    addAndMakeVisible (bypassLabel);

    // New Band button
    newBandButton.onClick = [this]
    {
        if (processor.getNumBands() < InstaLPEQProcessor::maxBands)
        {
            processor.addBand (1000.0f, 0.0f);
            syncDisplayFromProcessor();
            curveDisplay.setSelectedBand (processor.getNumBands() - 1);
        }
    };
    addAndMakeVisible (newBandButton);

    // Quality selector (FIR latency)
    qualitySelector.addItem ("512 (~6ms)", 1);
    qualitySelector.addItem ("1024 (~12ms)", 2);
    qualitySelector.addItem ("2048 (~23ms)", 3);
    qualitySelector.addItem ("4096 (~46ms)", 4);
    qualitySelector.addItem ("8192 (~93ms)", 5);
    qualitySelector.addItem ("16384 (~186ms)", 6);
    qualitySelector.setSelectedId (3, juce::dontSendNotification);  // default 2048
    qualitySelector.onChange = [this]
    {
        int sel = qualitySelector.getSelectedId();
        int order = sel + 8;  // 1->9, 2->10, 3->11, 4->12, 5->13, 6->14
        processor.setQuality (order);

        if (sel <= 2)  // 512 or 1024
            qualityWarning.setText ("Low freq accuracy reduced", juce::dontSendNotification);
        else
            qualityWarning.setText ("", juce::dontSendNotification);
    };
    addAndMakeVisible (qualitySelector);
    qualityLabel.setFont (customLookAndFeel.getMediumFont (13.0f));
    qualityLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (qualityLabel);

    qualityWarning.setFont (customLookAndFeel.getRegularFont (11.0f));
    qualityWarning.setColour (juce::Label::textColourId, juce::Colour (0xffff6644));
    qualityWarning.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (qualityWarning);

    // EQ curve
    curveDisplay.setListener (this);
    addAndMakeVisible (curveDisplay);

    // Node panel
    nodePanel.setListener (this);
    addAndMakeVisible (nodePanel);

    // Master gain
    masterGainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    masterGainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    masterGainSlider.setRange (-24.0, 24.0, 0.1);
    masterGainSlider.setValue (0.0);
    masterGainSlider.setTextValueSuffix (" dB");
    masterGainSlider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (masterGainSlider);
    masterGainLabel.setFont (customLookAndFeel.getMediumFont (13.0f));
    masterGainLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (masterGainLabel);

    // Limiter toggle
    limiterToggle.setToggleState (processor.limiterEnabled.load(), juce::dontSendNotification);
    addAndMakeVisible (limiterToggle);
    limiterLabel.setFont (customLookAndFeel.getMediumFont (13.0f));
    limiterLabel.setColour (juce::Label::textColourId, InstaLPEQLookAndFeel::textSecondary);
    limiterLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (limiterLabel);

    // Makeup gain
    makeupGainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    makeupGainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    makeupGainSlider.setRange (-24.0, 24.0, 0.1);
    makeupGainSlider.setValue (0.0);
    makeupGainSlider.setTextValueSuffix (" dB");
    makeupGainSlider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (makeupGainSlider);
    makeupGainLabel.setFont (customLookAndFeel.getMediumFont (13.0f));
    makeupGainLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (makeupGainLabel);

    // Signal chain panel
    chainPanel.setListener (this);
    chainPanel.setOrder (processor.getChainOrder());
    addAndMakeVisible (chainPanel);

    // Sizing
    constrainer.setMinimumSize (700, 450);
    constrainer.setMaximumSize (1920, 1080);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (900, 650);

    startTimerHz (30);
    syncDisplayFromProcessor();
}

InstaLPEQEditor::~InstaLPEQEditor()
{
    setLookAndFeel (nullptr);
}

void InstaLPEQEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background gradient
    juce::ColourGradient bgGrad (InstaLPEQLookAndFeel::bgDark, 0, 0,
                                  InstaLPEQLookAndFeel::bgDark.darker (0.3f), 0, bounds.getBottom(), false);
    g.setGradientFill (bgGrad);
    g.fillAll();

    // Noise texture
    customLookAndFeel.drawBackgroundTexture (g, getLocalBounds());

    // Top header bar
    float scale = (float) getHeight() / 650.0f;
    int headerH = (int) std::max (28.0f, 36.0f * scale);
    g.setColour (InstaLPEQLookAndFeel::bgDark.darker (0.4f));
    g.fillRect (0, 0, getWidth(), headerH);

    g.setColour (InstaLPEQLookAndFeel::bgLight.withAlpha (0.3f));
    g.drawHorizontalLine (headerH, 0.0f, (float) getWidth());
}

void InstaLPEQEditor::resized()
{
    auto bounds = getLocalBounds();
    float scale = (float) getHeight() / 650.0f;

    // Top bar
    int headerH = (int) std::max (28.0f, 36.0f * scale);
    auto header = bounds.removeFromTop (headerH);
    int pad = (int) (8 * scale);
    header.reduce (pad, 2);

    titleLabel.setFont (customLookAndFeel.getBoldFont (std::max (18.0f, 26.0f * scale)));
    titleLabel.setBounds (header.removeFromLeft (200));

    versionLabel.setFont (customLookAndFeel.getRegularFont (std::max (10.0f, 13.0f * scale)));
    versionLabel.setBounds (header.removeFromRight (60));

    auto bypassArea = header.removeFromRight (80);
    bypassLabel.setBounds (bypassArea.removeFromLeft (50));
    bypassToggle.setBounds (bypassArea);

    newBandButton.setBounds (header.removeFromRight ((int) (90 * scale)).reduced (2));

    // Signal chain panel (bottom-most)
    int chainH = (int) std::max (28.0f, 36.0f * scale);
    chainPanel.setBounds (bounds.removeFromBottom (chainH).reduced (pad, 2));

    // Master controls row (above chain)
    int masterH = (int) std::max (50.0f, 65.0f * scale);
    auto masterArea = bounds.removeFromBottom (masterH).reduced (pad, 2);

    masterGainLabel.setFont (customLookAndFeel.getMediumFont (std::max (11.0f, 14.0f * scale)));
    auto labelArea = masterArea.removeFromLeft (60);
    masterGainLabel.setBounds (labelArea);
    masterGainSlider.setBounds (masterArea.removeFromLeft (masterH));

    // Limiter toggle next to master gain
    limiterLabel.setFont (customLookAndFeel.getMediumFont (std::max (11.0f, 14.0f * scale)));
    limiterLabel.setBounds (masterArea.removeFromLeft (55));
    limiterToggle.setBounds (masterArea.removeFromLeft (40));

    // Makeup gain knob
    makeupGainLabel.setFont (customLookAndFeel.getMediumFont (std::max (11.0f, 14.0f * scale)));
    makeupGainLabel.setBounds (masterArea.removeFromLeft (55));
    makeupGainSlider.setBounds (masterArea.removeFromLeft (masterH));

    // Quality selector on the right side of master row
    qualityLabel.setFont (customLookAndFeel.getMediumFont (std::max (11.0f, 14.0f * scale)));
    auto qLabelArea = masterArea.removeFromRight (30);
    qualityLabel.setBounds (qLabelArea);
    qualitySelector.setBounds (masterArea.removeFromRight ((int) (130 * scale)).reduced (2, (masterH - 24) / 2));
    qualityWarning.setFont (customLookAndFeel.getRegularFont (std::max (9.0f, 11.0f * scale)));
    qualityWarning.setBounds (masterArea.removeFromRight ((int) (170 * scale)));

    // Node parameter panel (15% of remaining height)
    int nodePanelH = (int) (bounds.getHeight() * 0.18f);
    auto nodePanelArea = bounds.removeFromBottom (nodePanelH).reduced (pad, 2);
    nodePanel.setBounds (nodePanelArea);

    // EQ curve display (rest)
    auto curveArea = bounds.reduced (pad, 2);
    curveDisplay.setBounds (curveArea);
}

void InstaLPEQEditor::timerCallback()
{
    // Sync bypass & limiter
    processor.bypassed.store (bypassToggle.getToggleState());
    processor.masterGainDb.store ((float) masterGainSlider.getValue());
    processor.limiterEnabled.store (limiterToggle.getToggleState());
    processor.makeupGainDb.store ((float) makeupGainSlider.getValue());

    // Update spectrum analyzer
    {
        std::array<float, 1024> specData {};
        if (processor.getSpectrum (specData.data(), (int) specData.size()))
            curveDisplay.setSpectrum (specData.data(), (int) specData.size(),
                                      processor.getCurrentSampleRate(), 2048);
    }

    // Update display with latest magnitude response
    auto magDb = processor.getFIREngine().getMagnitudeResponseDb();
    if (! magDb.empty())
    {
        curveDisplay.setMagnitudeResponse (magDb, processor.getCurrentSampleRate(),
                                           processor.getFIREngine().getFIRLength());
    }

    // Sync bands from processor (in case of state restore)
    auto currentBands = processor.getBands();
    curveDisplay.setBands (currentBands);

    // Update node panel if selected
    int sel = curveDisplay.getSelectedBandIndex();
    if (sel >= 0 && sel < (int) currentBands.size())
        nodePanel.setSelectedBand (sel, &currentBands[sel]);
}

// ============================================================
// EQCurveDisplay::Listener
// ============================================================

void InstaLPEQEditor::bandAdded (int /*index*/, float freq, float gainDb)
{
    processor.addBand (freq, gainDb);
    syncDisplayFromProcessor();
    curveDisplay.setSelectedBand (processor.getNumBands() - 1);
}

void InstaLPEQEditor::bandRemoved (int index)
{
    processor.removeBand (index);
    curveDisplay.setSelectedBand (-1);
    syncDisplayFromProcessor();
}

void InstaLPEQEditor::bandChanged (int index, const EQBand& band)
{
    processor.setBand (index, band);
    // Don't call syncDisplayFromProcessor here to avoid flicker during drag
    auto currentBands = processor.getBands();
    curveDisplay.setBands (currentBands);

    if (index == nodePanel.getSelectedIndex() && index < (int) currentBands.size())
        nodePanel.setSelectedBand (index, &currentBands[index]);
}

void InstaLPEQEditor::selectedBandChanged (int index)
{
    auto currentBands = processor.getBands();
    if (index >= 0 && index < (int) currentBands.size())
        nodePanel.setSelectedBand (index, &currentBands[index]);
    else
        nodePanel.setSelectedBand (-1, nullptr);
}

// ============================================================
// NodeParameterPanel::Listener
// ============================================================

void InstaLPEQEditor::nodeParameterChanged (int bandIndex, const EQBand& band)
{
    processor.setBand (bandIndex, band);
    syncDisplayFromProcessor();
}

void InstaLPEQEditor::nodeDeleteRequested (int bandIndex)
{
    processor.removeBand (bandIndex);
    curveDisplay.setSelectedBand (-1);
    nodePanel.setSelectedBand (-1, nullptr);
    syncDisplayFromProcessor();
}

void InstaLPEQEditor::chainOrderChanged (const std::array<InstaLPEQProcessor::ChainStage, InstaLPEQProcessor::numChainStages>& order)
{
    processor.setChainOrder (order);
}

void InstaLPEQEditor::syncDisplayFromProcessor()
{
    auto currentBands = processor.getBands();
    curveDisplay.setBands (currentBands);
}
