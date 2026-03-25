#pragma once
#include <JuceHeader.h>
#include "EQBand.h"

class FIREngine : private juce::Thread
{
public:
    static constexpr int defaultFFTOrder = 13;  // 8192 taps
    static constexpr int maxBands = 8;

    FIREngine();
    ~FIREngine() override;

    void start (double sampleRate);
    void stop();

    // Called from GUI thread
    void setBands (const std::vector<EQBand>& newBands);
    void setFFTOrder (int order);

    // Called from audio thread — returns new FIR if available, nullptr otherwise
    std::unique_ptr<juce::AudioBuffer<float>> getNewFIR();

    // Get magnitude response in dB for display (thread-safe copy)
    std::vector<float> getMagnitudeResponseDb() const;

    int getFIRLength() const { return 1 << fftOrder.load(); }
    int getLatencySamples() const { return getFIRLength() / 2; }

private:
    void run() override;
    juce::AudioBuffer<float> generateFIR (const std::vector<EQBand>& bands, double sr, int order);

    std::atomic<double> sampleRate { 44100.0 };
    std::atomic<int> fftOrder { defaultFFTOrder };
    std::atomic<bool> needsUpdate { true };

    std::vector<EQBand> currentBands;
    mutable juce::SpinLock bandLock;

    std::unique_ptr<juce::AudioBuffer<float>> pendingFIR;
    juce::SpinLock firLock;

    std::vector<float> magnitudeDb;
    mutable juce::SpinLock magLock;
};
