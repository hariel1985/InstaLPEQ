#pragma once
#include <JuceHeader.h>
#include "EQBand.h"

class EQCurveDisplay : public juce::Component
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void bandAdded (int index, float freq, float gainDb) = 0;
        virtual void bandRemoved (int index) = 0;
        virtual void bandChanged (int index, const EQBand& band) = 0;
        virtual void selectedBandChanged (int index) = 0;
    };

    EQCurveDisplay();

    void setListener (Listener* l) { listener = l; }
    void setBands (const std::vector<EQBand>& bands);
    void setMagnitudeResponse (const std::vector<float>& magnitudesDb, double sampleRate, int fftSize);
    int getSelectedBandIndex() const { return selectedBand; }
    void setSelectedBand (int index);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

private:
    std::vector<EQBand> bands;
    std::vector<float> magnitudeResponseDb;
    double responseSampleRate = 44100.0;
    int responseFftSize = 8192;
    int selectedBand = -1;
    int draggedBand = -1;
    Listener* listener = nullptr;

    static constexpr float minFreq = 20.0f;
    static constexpr float maxFreq = 20000.0f;
    static constexpr float minDb = -24.0f;
    static constexpr float maxDb = 24.0f;

    // Node colours (8 distinct colours for up to 8 bands)
    static inline const juce::Colour nodeColours[8] = {
        juce::Colour (0xffff6644),  // orange-red
        juce::Colour (0xff44bbff),  // sky blue
        juce::Colour (0xffff44aa),  // pink
        juce::Colour (0xff44ff88),  // green
        juce::Colour (0xffffff44),  // yellow
        juce::Colour (0xffaa44ff),  // purple
        juce::Colour (0xff44ffff),  // cyan
        juce::Colour (0xffff8844),  // orange
    };

    juce::Rectangle<float> getPlotArea() const;
    float freqToX (float freq) const;
    float xToFreq (float x) const;
    float dbToY (float db) const;
    float yToDb (float y) const;

    void drawGrid (juce::Graphics& g);
    void drawResponseCurve (juce::Graphics& g);
    void drawPerBandCurves (juce::Graphics& g);
    void drawNodes (juce::Graphics& g);
    int findNodeAt (float x, float y, float radius = 14.0f) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQCurveDisplay)
};
