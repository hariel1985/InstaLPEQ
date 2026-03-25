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

    // Settings
    std::atomic<bool> bypassed { false };
    std::atomic<float> masterGainDb { 0.0f };

    FIREngine& getFIREngine() { return firEngine; }
    double getCurrentSampleRate() const { return currentSampleRate; }

private:
    std::vector<EQBand> bands;
    juce::SpinLock bandLock;

    FIREngine firEngine;
    juce::dsp::Convolution convolution;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    bool firLoaded = false;

    void updateFIR();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstaLPEQProcessor)
};
