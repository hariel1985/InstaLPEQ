#include "EQCurveDisplay.h"
#include "LookAndFeel.h"

EQCurveDisplay::EQCurveDisplay() {}

void EQCurveDisplay::setBands (const std::vector<EQBand>& newBands)
{
    bands = newBands;
    repaint();
}

void EQCurveDisplay::setMagnitudeResponse (const std::vector<float>& magnitudesDb, double sampleRate, int fftSize)
{
    magnitudeResponseDb = magnitudesDb;
    responseSampleRate = sampleRate;
    responseFftSize = fftSize;
    repaint();
}

void EQCurveDisplay::setSelectedBand (int index)
{
    if (selectedBand != index)
    {
        selectedBand = index;
        repaint();
        if (listener != nullptr)
            listener->selectedBandChanged (index);
    }
}

// ============================================================
// Coordinate mapping
// ============================================================

juce::Rectangle<float> EQCurveDisplay::getPlotArea() const
{
    float marginL = 38.0f;
    float marginR = 12.0f;
    float marginT = 10.0f;
    float marginB = 22.0f;
    return getLocalBounds().toFloat().withTrimmedLeft (marginL)
                                      .withTrimmedRight (marginR)
                                      .withTrimmedTop (marginT)
                                      .withTrimmedBottom (marginB);
}

float EQCurveDisplay::freqToX (float freq) const
{
    auto area = getPlotArea();
    float normLog = std::log10 (freq / minFreq) / std::log10 (maxFreq / minFreq);
    return area.getX() + normLog * area.getWidth();
}

float EQCurveDisplay::xToFreq (float x) const
{
    auto area = getPlotArea();
    float normLog = (x - area.getX()) / area.getWidth();
    return minFreq * std::pow (maxFreq / minFreq, normLog);
}

float EQCurveDisplay::dbToY (float db) const
{
    auto area = getPlotArea();
    float norm = (maxDb - db) / (maxDb - minDb);
    return area.getY() + norm * area.getHeight();
}

float EQCurveDisplay::yToDb (float y) const
{
    auto area = getPlotArea();
    float norm = (y - area.getY()) / area.getHeight();
    return maxDb - norm * (maxDb - minDb);
}

// ============================================================
// Paint
// ============================================================

void EQCurveDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background gradient
    {
        juce::ColourGradient bgGrad (InstaLPEQLookAndFeel::bgDark.darker (0.4f), 0, bounds.getY(),
                                      InstaLPEQLookAndFeel::bgDark.darker (0.2f), 0, bounds.getBottom(), false);
        g.setGradientFill (bgGrad);
        g.fillRoundedRectangle (bounds, 4.0f);
    }

    // Border
    g.setColour (InstaLPEQLookAndFeel::bgLight.withAlpha (0.3f));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    drawGrid (g);
    drawPerBandCurves (g);
    drawResponseCurve (g);
    drawNodes (g);
}

void EQCurveDisplay::drawGrid (juce::Graphics& g)
{
    auto area = getPlotArea();
    auto* lf = dynamic_cast<InstaLPEQLookAndFeel*> (&getLookAndFeel());
    juce::Font labelFont = lf ? lf->getRegularFont (11.0f) : juce::Font (juce::FontOptions (11.0f));
    g.setFont (labelFont);

    // Vertical lines — frequency markers
    const float freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    const char* freqLabels[] = { "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };

    for (int i = 0; i < 10; ++i)
    {
        float xPos = freqToX (freqs[i]);
        bool isMajor = (freqs[i] == 100 || freqs[i] == 1000 || freqs[i] == 10000);
        g.setColour (InstaLPEQLookAndFeel::bgLight.withAlpha (isMajor ? 0.2f : 0.08f));
        g.drawVerticalLine ((int) xPos, area.getY(), area.getBottom());

        g.setColour (InstaLPEQLookAndFeel::textSecondary.withAlpha (0.7f));
        g.drawText (freqLabels[i], (int) xPos - 16, (int) area.getBottom() + 2, 32, 16,
                     juce::Justification::centredTop, false);
    }

    // Horizontal lines — dB markers
    for (float db = minDb; db <= maxDb; db += 6.0f)
    {
        float yPos = dbToY (db);
        bool isZero = (std::abs (db) < 0.1f);
        g.setColour (isZero ? InstaLPEQLookAndFeel::accent.withAlpha (0.15f)
                            : InstaLPEQLookAndFeel::bgLight.withAlpha (0.1f));
        g.drawHorizontalLine ((int) yPos, area.getX(), area.getRight());

        if (std::fmod (std::abs (db), 12.0f) < 0.1f || isZero)
        {
            g.setColour (InstaLPEQLookAndFeel::textSecondary.withAlpha (0.7f));
            juce::String label = (db > 0 ? "+" : "") + juce::String ((int) db);
            g.drawText (label, (int) area.getX() - 36, (int) yPos - 8, 32, 16,
                         juce::Justification::centredRight, false);
        }
    }
}

void EQCurveDisplay::drawResponseCurve (juce::Graphics& g)
{
    if (magnitudeResponseDb.empty())
        return;

    auto area = getPlotArea();
    int numBins = (int) magnitudeResponseDb.size();

    juce::Path curvePath;
    juce::Path fillPath;
    float zeroY = dbToY (0.0f);
    bool started = false;

    for (float px = area.getX(); px <= area.getRight(); px += 1.0f)
    {
        float freq = xToFreq (px);
        if (freq < 1.0f || freq > responseSampleRate * 0.5)
            continue;

        // Convert frequency to bin index
        float binFloat = freq * (float) responseFftSize / (float) responseSampleRate;
        int bin = (int) binFloat;
        float frac = binFloat - (float) bin;

        if (bin < 0 || bin >= numBins - 1)
            continue;

        // Linear interpolation between bins
        float dbVal = magnitudeResponseDb[bin] * (1.0f - frac) + magnitudeResponseDb[bin + 1] * frac;
        dbVal = juce::jlimit (minDb - 6.0f, maxDb + 6.0f, dbVal);
        float yPos = dbToY (dbVal);

        if (! started)
        {
            curvePath.startNewSubPath (px, yPos);
            fillPath.startNewSubPath (px, zeroY);
            fillPath.lineTo (px, yPos);
            started = true;
        }
        else
        {
            curvePath.lineTo (px, yPos);
            fillPath.lineTo (px, yPos);
        }
    }

    if (! started)
        return;

    // Close fill path
    fillPath.lineTo (area.getRight(), zeroY);
    fillPath.closeSubPath();

    // Fill under curve
    g.setColour (InstaLPEQLookAndFeel::accent.withAlpha (0.1f));
    g.fillPath (fillPath);

    // Glow
    g.setColour (InstaLPEQLookAndFeel::accent.withAlpha (0.2f));
    g.strokePath (curvePath, juce::PathStrokeType (4.0f));

    // Core curve
    g.setColour (InstaLPEQLookAndFeel::accent);
    g.strokePath (curvePath, juce::PathStrokeType (2.0f));
}

void EQCurveDisplay::drawPerBandCurves (juce::Graphics& g)
{
    if (bands.empty())
        return;

    auto area = getPlotArea();

    for (int bandIdx = 0; bandIdx < (int) bands.size(); ++bandIdx)
    {
        const auto& band = bands[bandIdx];
        if (! band.enabled || std::abs (band.gainDb) < 0.01f)
            continue;

        float gainLinear = juce::Decibels::decibelsToGain (band.gainDb);
        juce::dsp::IIR::Coefficients<float>::Ptr coeffs;

        switch (band.type)
        {
            case EQBand::Peak:
                coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (responseSampleRate, band.frequency, band.q, gainLinear);
                break;
            case EQBand::LowShelf:
                coeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (responseSampleRate, band.frequency, band.q, gainLinear);
                break;
            case EQBand::HighShelf:
                coeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (responseSampleRate, band.frequency, band.q, gainLinear);
                break;
        }

        if (coeffs == nullptr)
            continue;

        juce::Path bandPath;
        bool started = false;
        auto colour = nodeColours[bandIdx % 8].withAlpha (bandIdx == selectedBand ? 0.4f : 0.15f);

        for (float px = area.getX(); px <= area.getRight(); px += 2.0f)
        {
            float freq = xToFreq (px);
            if (freq < 1.0f)
                continue;

            double mag = coeffs->getMagnitudeForFrequency (freq, responseSampleRate);
            float dbVal = (float) juce::Decibels::gainToDecibels (mag, -60.0);
            dbVal = juce::jlimit (minDb - 6.0f, maxDb + 6.0f, dbVal);
            float yPos = dbToY (dbVal);

            if (! started) { bandPath.startNewSubPath (px, yPos); started = true; }
            else           bandPath.lineTo (px, yPos);
        }

        g.setColour (colour);
        g.strokePath (bandPath, juce::PathStrokeType (1.5f));
    }
}

void EQCurveDisplay::drawNodes (juce::Graphics& g)
{
    for (int i = 0; i < (int) bands.size(); ++i)
    {
        const auto& band = bands[i];
        float nx = freqToX (band.frequency);
        float ny = dbToY (band.gainDb);
        auto colour = nodeColours[i % 8];
        bool isSel = (i == selectedBand);

        float r = isSel ? 10.0f : 8.0f;

        // Glow for selected
        if (isSel)
        {
            for (int gl = 0; gl < 3; ++gl)
            {
                float t = (float) gl / 2.0f;
                float gr = r * (2.0f - t * 0.6f);
                float alpha = 0.05f + t * t * 0.15f;
                g.setColour (colour.withAlpha (alpha));
                g.fillEllipse (nx - gr, ny - gr, gr * 2, gr * 2);
            }
        }

        // Fill
        g.setColour (band.enabled ? colour : colour.withAlpha (0.4f));
        g.fillEllipse (nx - r, ny - r, r * 2, r * 2);

        // Border
        g.setColour (isSel ? juce::Colours::white : colour.brighter (0.3f));
        g.drawEllipse (nx - r, ny - r, r * 2, r * 2, isSel ? 2.0f : 1.0f);

        // Band number
        auto* lf = dynamic_cast<InstaLPEQLookAndFeel*> (&getLookAndFeel());
        juce::Font numFont = lf ? lf->getBoldFont (11.0f) : juce::Font (juce::FontOptions (11.0f));
        g.setFont (numFont);
        g.setColour (juce::Colours::white);
        g.drawText (juce::String (i + 1), (int) (nx - r), (int) (ny - r), (int) (r * 2), (int) (r * 2),
                     juce::Justification::centred, false);
    }
}

// ============================================================
// Mouse interaction
// ============================================================

int EQCurveDisplay::findNodeAt (float x, float y, float radius) const
{
    for (int i = 0; i < (int) bands.size(); ++i)
    {
        float nx = freqToX (bands[i].frequency);
        float ny = dbToY (bands[i].gainDb);
        float dist = std::sqrt ((x - nx) * (x - nx) + (y - ny) * (y - ny));
        if (dist <= radius)
            return i;
    }
    return -1;
}

void EQCurveDisplay::mouseDown (const juce::MouseEvent& e)
{
    auto pos = e.position;
    int hit = findNodeAt (pos.x, pos.y);

    if (e.mods.isRightButtonDown() && hit >= 0)
    {
        // Right-click context menu
        juce::PopupMenu menu;
        menu.addItem (1, "Delete Band");
        menu.addItem (2, "Reset to 0 dB");
        menu.addSeparator();
        menu.addItem (3, "Peak", true, bands[hit].type == EQBand::Peak);
        menu.addItem (4, "Low Shelf", true, bands[hit].type == EQBand::LowShelf);
        menu.addItem (5, "High Shelf", true, bands[hit].type == EQBand::HighShelf);

        menu.showMenuAsync (juce::PopupMenu::Options(), [this, hit] (int result)
        {
            if (result == 1)
            {
                if (listener) listener->bandRemoved (hit);
            }
            else if (result == 2)
            {
                auto band = bands[hit];
                band.gainDb = 0.0f;
                if (listener) listener->bandChanged (hit, band);
            }
            else if (result >= 3 && result <= 5)
            {
                auto band = bands[hit];
                band.type = (result == 3) ? EQBand::Peak : (result == 4) ? EQBand::LowShelf : EQBand::HighShelf;
                if (listener) listener->bandChanged (hit, band);
            }
        });
        return;
    }

    if (hit >= 0)
    {
        draggedBand = hit;
        setSelectedBand (hit);
    }
    else if (e.mods.isLeftButtonDown() && (int) bands.size() < 8)
    {
        // Add new band
        float freq = juce::jlimit (minFreq, maxFreq, xToFreq (pos.x));
        float gain = juce::jlimit (minDb, maxDb, yToDb (pos.y));
        if (listener)
            listener->bandAdded ((int) bands.size(), freq, gain);
    }
}

void EQCurveDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (draggedBand < 0 || draggedBand >= (int) bands.size())
        return;

    auto pos = e.position;
    auto band = bands[draggedBand];
    band.frequency = juce::jlimit (minFreq, maxFreq, xToFreq (pos.x));
    band.gainDb = juce::jlimit (minDb, maxDb, yToDb (pos.y));

    if (listener)
        listener->bandChanged (draggedBand, band);
}

void EQCurveDisplay::mouseUp (const juce::MouseEvent&)
{
    draggedBand = -1;
}

void EQCurveDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    int hit = findNodeAt (e.position.x, e.position.y);
    if (hit >= 0)
    {
        auto band = bands[hit];
        band.gainDb = 0.0f;
        if (listener)
            listener->bandChanged (hit, band);
    }
}

void EQCurveDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    int hit = findNodeAt (e.position.x, e.position.y, 20.0f);
    if (hit >= 0)
    {
        auto band = bands[hit];
        float delta = wheel.deltaY * 2.0f;
        band.q = juce::jlimit (0.1f, 18.0f, band.q + delta);
        if (listener)
            listener->bandChanged (hit, band);
    }
}
