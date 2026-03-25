#pragma once
#include <JuceHeader.h>
#include "EQBand.h"
#include "FIREngine.h"

class InstaLPEQProcessor : public juce::AudioProcessor
{
public:
    static constexpr int maxBands = 8;

    InstaLPEQProcessor();
    ~InstaLPEQProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Band management (called from GUI thread)
    std::vector<EQBand> getBands() const;
    void setBand (int index, const EQBand& band);
    void addBand (float freq, float gainDb);
    void removeBand (int index);
    int getNumBands() const;

    // Signal chain stages
    enum ChainStage { MasterGain = 0, Limiter, MakeupGain, NumStages };
    static constexpr int numChainStages = (int) NumStages;

    // Settings
    std::atomic<bool> bypassed { false };
    std::atomic<float> masterGainDb { 0.0f };
    std::atomic<bool> limiterEnabled { true };
    std::atomic<float> makeupGainDb { 0.0f };  // -24 to +24 dB

    // Chain order (read/write from GUI, read from audio thread)
    std::array<ChainStage, numChainStages> getChainOrder() const;
    void setChainOrder (const std::array<ChainStage, numChainStages>& order);

    void setQuality (int fftOrder);

    FIREngine& getFIREngine() { return firEngine; }
    double getCurrentSampleRate() const { return currentSampleRate; }

private:
    std::vector<EQBand> bands;
    juce::SpinLock bandLock;

    FIREngine firEngine;
    juce::dsp::Convolution convolution;
    juce::dsp::Limiter<float> limiter;

    // Spectrum analyzer
    static constexpr int spectrumFFTOrder = 11;  // 2048-point FFT
    static constexpr int spectrumFFTSize = 1 << spectrumFFTOrder;
    juce::dsp::FFT spectrumFFT { spectrumFFTOrder };
    juce::dsp::WindowingFunction<float> spectrumWindow { spectrumFFTSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, spectrumFFTSize> fifoBuffer {};
    int fifoIndex = 0;
    std::array<float, spectrumFFTSize * 2> fftData {};
    std::array<float, spectrumFFTSize / 2> spectrumMagnitude {};
    juce::SpinLock spectrumLock;
    std::atomic<bool> spectrumReady { false };

public:
    bool getSpectrum (float* dest, int maxBins) const;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    bool firLoaded = false;

    std::array<ChainStage, numChainStages> chainOrder { MasterGain, Limiter, MakeupGain };
    juce::SpinLock chainLock;

    void updateFIR();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstaLPEQProcessor)
};
