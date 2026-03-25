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

    // Process through convolution
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    convolution.process (context);

    // Apply master gain
    float gain = juce::Decibels::decibelsToGain (masterGainDb.load());
    if (std::abs (gain - 1.0f) > 0.001f)
        buffer.applyGain (gain);
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

void InstaLPEQProcessor::updateFIR()
{
    auto currentBands = getBands();
    firEngine.setBands (currentBands);
}

// ============================================================
// State save/restore
// ============================================================

void InstaLPEQProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml ("InstaLPEQ");
    xml.setAttribute ("bypass", bypassed.load());
    xml.setAttribute ("masterGain", (double) masterGainDb.load());

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
