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

    // Store theoretical magnitude in dB for display (from IIR target curve)
    {
        std::vector<float> magDb (numBins);
        for (int i = 0; i < numBins; ++i)
            magDb[i] = (float) juce::Decibels::gainToDecibels ((float) magnitudes[i], -60.0f);

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

    // Normalize: ensure flat spectrum → unity DC gain
    // Without this, IFFT scaling + windowing cause incorrect base level
    float dcGain = 0.0f;
    for (int i = 0; i < fftSize; ++i)
        dcGain += firData[i];

    if (std::abs (dcGain) > 1e-6f)
    {
        float normFactor = 1.0f / dcGain;
        for (int i = 0; i < fftSize; ++i)
            firData[i] *= normFactor;
    }

    // Compute auto makeup from the ACTUAL final FIR frequency response
    // (includes windowing + normalization effects)
    {
        std::vector<float> analysisBuf (fftSize * 2, 0.0f);
        std::copy (firData, firData + fftSize, analysisBuf.data());

        juce::dsp::FFT analysisFft (order);
        analysisFft.performRealOnlyForwardTransform (analysisBuf.data());

        // Extract actual magnitude from FFT result
        // Format: [DC_real, Nyquist_real, bin1_real, bin1_imag, bin2_real, bin2_imag, ...]
        // Evaluate at log-spaced frequencies (equal weight per octave)
        // This matches musical content much better than linear spacing
        // (linear gives 77% weight to 5k-22k where music has little energy)
        const int numPoints = 512;
        double powerSum = 0.0;
        double binRes = sr / (double) fftSize;  // Hz per bin

        for (int p = 0; p < numPoints; ++p)
        {
            // Log-spaced 20 Hz — 20 kHz
            double freq = 20.0 * std::pow (1000.0, (double) p / (double) (numPoints - 1));

            // Interpolate magnitude from FFT bins
            double binFloat = freq / binRes;
            int bin = (int) binFloat;
            double frac = binFloat - (double) bin;

            if (bin < 1 || bin >= fftSize / 2 - 1)
                continue;

            float re0 = analysisBuf[bin * 2];
            float im0 = analysisBuf[bin * 2 + 1];
            float re1 = analysisBuf[(bin + 1) * 2];
            float im1 = analysisBuf[(bin + 1) * 2 + 1];

            double mag0 = std::sqrt ((double) (re0 * re0 + im0 * im0));
            double mag1 = std::sqrt ((double) (re1 * re1 + im1 * im1));
            double mag = mag0 * (1.0 - frac) + mag1 * frac;

            powerSum += mag * mag;
        }

        if (numPoints > 0)
        {
            double avgPower = powerSum / (double) numPoints;
            float rmsGain = (float) std::sqrt (avgPower);
            float makeupDb = -20.0f * std::log10 (std::max (rmsGain, 1e-10f));
            autoMakeupDb.store (makeupDb);
        }

        // (magnitudeDb stays as theoretical IIR curve for display)
    }

    return firBuffer;
}

// A-weighting curve (IEC 61672:2003)
// Returns linear amplitude weighting factor for given frequency
float FIREngine::aWeighting (float f)
{
    if (f < 10.0f) return 0.0f;

    double f2 = (double) f * (double) f;
    double f4 = f2 * f2;

    double num = 12194.0 * 12194.0 * f4;
    double den = (f2 + 20.6 * 20.6)
               * std::sqrt ((f2 + 107.7 * 107.7) * (f2 + 737.9 * 737.9))
               * (f2 + 12194.0 * 12194.0);

    double ra = num / den;

    // Normalize so A(1000 Hz) = 1.0
    // A(1000) unnormalized ≈ 0.7943
    static const double norm = 1.0 / 0.7943282347;
    return (float) (ra * norm);
}
