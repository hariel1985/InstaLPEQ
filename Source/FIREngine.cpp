#include "FIREngine.h"

FIREngine::FIREngine() : Thread ("FIREngine") {}

FIREngine::~FIREngine()
{
    stop();
}

void FIREngine::start (double sr)
{
    sampleRate.store (sr);
    needsUpdate.store (true);
    startThread (juce::Thread::Priority::normal);
}

void FIREngine::stop()
{
    signalThreadShouldExit();
    notify();
    stopThread (2000);
}

void FIREngine::setBands (const std::vector<EQBand>& newBands)
{
    {
        const juce::SpinLock::ScopedLockType lock (bandLock);
        currentBands = newBands;
    }
    needsUpdate.store (true);
    notify();
}

void FIREngine::setFFTOrder (int order)
{
    fftOrder.store (juce::jlimit (9, 14, order));
    needsUpdate.store (true);
    notify();
}

std::unique_ptr<juce::AudioBuffer<float>> FIREngine::getNewFIR()
{
    const juce::SpinLock::ScopedTryLockType lock (firLock);
    if (lock.isLocked() && pendingFIR != nullptr)
        return std::move (pendingFIR);
    return nullptr;
}

std::vector<float> FIREngine::getMagnitudeResponseDb() const
{
    const juce::SpinLock::ScopedLockType lock (magLock);
    return magnitudeDb;
}

void FIREngine::run()
{
    while (! threadShouldExit())
    {
        if (needsUpdate.exchange (false))
        {
            std::vector<EQBand> bands;
            {
                const juce::SpinLock::ScopedLockType lock (bandLock);
                bands = currentBands;
            }

            auto fir = generateFIR (bands, sampleRate.load(), fftOrder.load());

            {
                const juce::SpinLock::ScopedLockType lock (firLock);
                pendingFIR = std::make_unique<juce::AudioBuffer<float>> (std::move (fir));
            }
        }

        wait (50);  // Check every 50ms
    }
}

juce::AudioBuffer<float> FIREngine::generateFIR (const std::vector<EQBand>& bands, double sr, int order)
{
    const int fftSize = 1 << order;
    const int numBins = fftSize / 2 + 1;

    // Compute frequency for each FFT bin
    std::vector<double> frequencies (numBins);
    for (int i = 0; i < numBins; ++i)
        frequencies[i] = (double) i * sr / (double) fftSize;

    // Start with flat magnitude response (1.0 = 0dB)
    std::vector<double> magnitudes (numBins, 1.0);

    // For each active band, compute its magnitude contribution and multiply in
    for (const auto& band : bands)
    {
        if (! band.enabled || std::abs (band.gainDb) < 0.01f)
            continue;

        float gainLinear = juce::Decibels::decibelsToGain (band.gainDb);

        // Create IIR coefficients just for magnitude response analysis
        juce::dsp::IIR::Coefficients<float>::Ptr coeffs;

        switch (band.type)
        {
            case EQBand::Peak:
                coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, band.frequency, band.q, gainLinear);
                break;
            case EQBand::LowShelf:
                coeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, band.frequency, band.q, gainLinear);
                break;
            case EQBand::HighShelf:
                coeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, band.frequency, band.q, gainLinear);
                break;
        }

        if (coeffs == nullptr)
            continue;

        // Get magnitude for each bin
        std::vector<double> bandMag (numBins);
        coeffs->getMagnitudeForFrequencyArray (frequencies.data(), bandMag.data(), numBins, sr);

        for (int i = 0; i < numBins; ++i)
            magnitudes[i] *= bandMag[i];
    }

    // Store magnitude in dB for display
    {
        std::vector<float> magDb (numBins);
        for (int i = 0; i < numBins; ++i)
            magDb[i] = (float) juce::Decibels::gainToDecibels (magnitudes[i], -60.0);

        const juce::SpinLock::ScopedLockType lock (magLock);
        magnitudeDb = std::move (magDb);
    }

    // Build complex spectrum: magnitude only, zero phase (linear phase)
    // JUCE FFT expects interleaved [real, imag, real, imag, ...] for complex
    // For performRealOnlyInverseTransform, input is fftSize*2 floats
    std::vector<float> fftData (fftSize * 2, 0.0f);

    // Pack magnitude into real parts (positive frequencies)
    // performRealOnlyInverseTransform expects the format from performRealOnlyForwardTransform:
    // data[0] = DC real, data[1] = Nyquist real, then interleaved complex pairs
    fftData[0] = (float) magnitudes[0];           // DC
    fftData[1] = (float) magnitudes[numBins - 1]; // Nyquist

    for (int i = 1; i < numBins - 1; ++i)
    {
        fftData[i * 2]     = (float) magnitudes[i]; // real
        fftData[i * 2 + 1] = 0.0f;                  // imag (zero = linear phase)
    }

    // Inverse FFT to get time-domain impulse response
    juce::dsp::FFT fft (order);
    fft.performRealOnlyInverseTransform (fftData.data());

    // The result is in fftData[0..fftSize-1]
    // Circular shift by fftSize/2 to center the impulse (make it causal)
    juce::AudioBuffer<float> firBuffer (1, fftSize);
    float* firData = firBuffer.getWritePointer (0);
    int halfSize = fftSize / 2;

    for (int i = 0; i < fftSize; ++i)
        firData[i] = fftData[(i + halfSize) % fftSize];

    // Apply window to reduce truncation artifacts
    juce::dsp::WindowingFunction<float> window (fftSize, juce::dsp::WindowingFunction<float>::blackmanHarris);
    window.multiplyWithWindowingTable (firData, fftSize);

    return firBuffer;
}
