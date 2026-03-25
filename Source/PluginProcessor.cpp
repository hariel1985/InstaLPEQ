#include "PluginProcessor.h"
#include "PluginEditor.h"

InstaLPEQProcessor::InstaLPEQProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

InstaLPEQProcessor::~InstaLPEQProcessor()
{
    firEngine.stop();
}

bool InstaLPEQProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::stereo() ||
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void InstaLPEQProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    convolution.prepare (spec);
    limiter.prepare (spec);
    limiter.setThreshold (0.0f);
    limiter.setRelease (50.0f);

    firEngine.start (sampleRate);
    updateFIR();

    setLatencySamples (firEngine.getLatencySamples());
}

void InstaLPEQProcessor::releaseResources()
{
    firEngine.stop();
    convolution.reset();
}

void InstaLPEQProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Check for new FIR from background thread
    if (auto newFIR = firEngine.getNewFIR())
    {
        convolution.loadImpulseResponse (std::move (*newFIR), currentSampleRate,
                                          juce::dsp::Convolution::Stereo::no,
                                          juce::dsp::Convolution::Trim::no,
                                          juce::dsp::Convolution::Normalise::no);
        firLoaded = true;
    }

    if (bypassed.load() || ! firLoaded)
        return;

    // Process through convolution (EQ)
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    convolution.process (context);

    // Apply chain in configured order
    std::array<ChainStage, numChainStages> order;
    {
        const juce::SpinLock::ScopedTryLockType lock (chainLock);
        if (lock.isLocked())
            order = chainOrder;
        else
            order = { MasterGain, Limiter, MakeupGain };
    }

    for (auto stage : order)
    {
        switch (stage)
        {
            case MasterGain:
            {
                float gain = juce::Decibels::decibelsToGain (masterGainDb.load());
                if (std::abs (gain - 1.0f) > 0.001f)
                    buffer.applyGain (gain);
                break;
            }
            case Limiter:
            {
                if (limiterEnabled.load())
                {
                    juce::dsp::AudioBlock<float> limBlock (buffer);
                    juce::dsp::ProcessContextReplacing<float> limContext (limBlock);
                    limiter.process (limContext);
                }
                break;
            }
            case MakeupGain:
            {
                if (autoMakeupEnabled.load())
                {
                    float mkGain = juce::Decibels::decibelsToGain (firEngine.getAutoMakeupGainDb());
                    if (std::abs (mkGain - 1.0f) > 0.001f)
                        buffer.applyGain (mkGain);
                }
                break;
            }
            default: break;
        }
    }

    // Feed spectrum analyzer (mono mix of output)
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    for (int i = 0; i < numSamples; ++i)
    {
        float sample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            sample += buffer.getSample (ch, i);
        sample /= (float) numChannels;

        fifoBuffer[fifoIndex++] = sample;

        if (fifoIndex >= spectrumFFTSize)
        {
            fifoIndex = 0;
            std::copy (fifoBuffer.begin(), fifoBuffer.end(), fftData.begin());
            std::fill (fftData.begin() + spectrumFFTSize, fftData.end(), 0.0f);
            spectrumWindow.multiplyWithWindowingTable (fftData.data(), spectrumFFTSize);
            spectrumFFT.performFrequencyOnlyForwardTransform (fftData.data());

            {
                const juce::SpinLock::ScopedLockType lock (spectrumLock);
                for (int b = 0; b < spectrumFFTSize / 2; ++b)
                {
                    float mag = fftData[b] / (float) spectrumFFTSize;
                    float dbVal = juce::Decibels::gainToDecibels (mag, -100.0f);
                    // Smooth: 70% old + 30% new
                    spectrumMagnitude[b] = spectrumMagnitude[b] * 0.7f + dbVal * 0.3f;
                }
            }
            spectrumReady.store (true);
        }
    }
}

// ============================================================
// Band management
// ============================================================

std::vector<EQBand> InstaLPEQProcessor::getBands() const
{
    const juce::SpinLock::ScopedLockType lock (bandLock);
    return bands;
}

void InstaLPEQProcessor::setBand (int index, const EQBand& band)
{
    {
        const juce::SpinLock::ScopedLockType lock (bandLock);
        if (index >= 0 && index < (int) bands.size())
            bands[index] = band;
    }
    updateFIR();
}

void InstaLPEQProcessor::addBand (float freq, float gainDb)
{
    {
        const juce::SpinLock::ScopedLockType lock (bandLock);
        if ((int) bands.size() >= maxBands)
            return;
        EQBand b;
        b.frequency = freq;
        b.gainDb = gainDb;
        bands.push_back (b);
    }
    updateFIR();
}

void InstaLPEQProcessor::removeBand (int index)
{
    {
        const juce::SpinLock::ScopedLockType lock (bandLock);
        if (index >= 0 && index < (int) bands.size())
            bands.erase (bands.begin() + index);
    }
    updateFIR();
}

int InstaLPEQProcessor::getNumBands() const
{
    const juce::SpinLock::ScopedLockType lock (bandLock);
    return (int) bands.size();
}

bool InstaLPEQProcessor::getSpectrum (float* dest, int maxBins) const
{
    if (! spectrumReady.load())
        return false;

    const juce::SpinLock::ScopedTryLockType lock (spectrumLock);
    if (! lock.isLocked())
        return false;

    int bins = std::min (maxBins, spectrumFFTSize / 2);
    std::copy (spectrumMagnitude.begin(), spectrumMagnitude.begin() + bins, dest);
    return true;
}

float InstaLPEQProcessor::getActiveAutoMakeupDb() const
{
    return autoMakeupEnabled.load() ? firEngine.getAutoMakeupGainDb() : 0.0f;
}

std::array<InstaLPEQProcessor::ChainStage, InstaLPEQProcessor::numChainStages> InstaLPEQProcessor::getChainOrder() const
{
    const juce::SpinLock::ScopedLockType lock (chainLock);
    return chainOrder;
}

void InstaLPEQProcessor::setChainOrder (const std::array<ChainStage, numChainStages>& order)
{
    const juce::SpinLock::ScopedLockType lock (chainLock);
    chainOrder = order;
}

void InstaLPEQProcessor::updateFIR()
{
    auto currentBands = getBands();
    firEngine.setBands (currentBands);
}

void InstaLPEQProcessor::setQuality (int fftOrder)
{
    firEngine.setFFTOrder (fftOrder);
    setLatencySamples (firEngine.getLatencySamples());
    updateFIR();
}

// ============================================================
// State save/restore
// ============================================================

void InstaLPEQProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml ("InstaLPEQ");
    xml.setAttribute ("bypass", bypassed.load());
    xml.setAttribute ("masterGain", (double) masterGainDb.load());
    xml.setAttribute ("limiter", limiterEnabled.load());
    xml.setAttribute ("autoMakeup", autoMakeupEnabled.load());

    auto order = getChainOrder();
    juce::String chainStr;
    for (int i = 0; i < numChainStages; ++i)
    {
        if (i > 0) chainStr += ",";
        chainStr += juce::String ((int) order[i]);
    }
    xml.setAttribute ("chainOrder", chainStr);

    auto currentBands = getBands();
    for (int i = 0; i < (int) currentBands.size(); ++i)
    {
        auto* bandXml = xml.createNewChildElement ("Band");
        bandXml->setAttribute ("freq", (double) currentBands[i].frequency);
        bandXml->setAttribute ("gain", (double) currentBands[i].gainDb);
        bandXml->setAttribute ("q", (double) currentBands[i].q);
        bandXml->setAttribute ("type", (int) currentBands[i].type);
        bandXml->setAttribute ("enabled", currentBands[i].enabled);
    }

    copyXmlToBinary (xml, destData);
}

void InstaLPEQProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("InstaLPEQ"))
        return;

    bypassed.store (xml->getBoolAttribute ("bypass", false));
    masterGainDb.store ((float) xml->getDoubleAttribute ("masterGain", 0.0));
    limiterEnabled.store (xml->getBoolAttribute ("limiter", true));
    autoMakeupEnabled.store (xml->getBoolAttribute ("autoMakeup", true));

    auto chainStr = xml->getStringAttribute ("chainOrder", "0,1,2");
    auto tokens = juce::StringArray::fromTokens (chainStr, ",", "");
    if (tokens.size() == numChainStages)
    {
        std::array<ChainStage, numChainStages> order;
        for (int i = 0; i < numChainStages; ++i)
            order[i] = static_cast<ChainStage> (tokens[i].getIntValue());
        setChainOrder (order);
    }

    std::vector<EQBand> loadedBands;
    for (auto* bandXml : xml->getChildIterator())
    {
        if (! bandXml->hasTagName ("Band"))
            continue;

        EQBand b;
        b.frequency = (float) bandXml->getDoubleAttribute ("freq", 1000.0);
        b.gainDb = (float) bandXml->getDoubleAttribute ("gain", 0.0);
        b.q = (float) bandXml->getDoubleAttribute ("q", 1.0);
        b.type = static_cast<EQBand::Type> (bandXml->getIntAttribute ("type", 0));
        b.enabled = bandXml->getBoolAttribute ("enabled", true);
        loadedBands.push_back (b);
    }

    {
        const juce::SpinLock::ScopedLockType lock (bandLock);
        bands = loadedBands;
    }
    updateFIR();
}

// ============================================================
// Editor
// ============================================================

juce::AudioProcessorEditor* InstaLPEQProcessor::createEditor()
{
    return new InstaLPEQEditor (*this);
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new InstaLPEQProcessor();
}
