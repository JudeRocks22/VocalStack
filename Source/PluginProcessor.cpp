#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;
using Attr = juce::AudioParameterFloatAttributes;

static juce::ParameterID pid (const char* id) { return { id, 1 }; }

juce::AudioProcessorValueTreeState::ParameterLayout VocalStackProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto flt = [&] (const char* id, const char* name, float lo, float hi, float def, const char* unit, float skewCentre = -1.0f)
    {
        juce::NormalisableRange<float> r (lo, hi, 0.01f);
        if (skewCentre > 0.0f) r.setSkewForCentre (skewCentre);
        p.push_back (std::make_unique<APF> (pid (id), name, r, def, Attr().withLabel (unit)));
    };
    auto tog = [&] (const char* id, const char* name, bool def)
    {
        p.push_back (std::make_unique<APB> (pid (id), name, def));
    };

    // Pitch correction
    tog ("pitch_on", "Pitch On", false);
    p.push_back (std::make_unique<APC> (pid ("pitch_key"), "Pitch Key",
        juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    p.push_back (std::make_unique<APC> (pid ("pitch_scale"), "Pitch Scale",
        juce::StringArray { "Chromatic", "Major", "Minor" }, 0));
    flt ("pitch_speed", "Pitch Speed", 1.0f, 200.0f, 25.0f, "ms", 30.0f);
    flt ("pitch_amount", "Pitch Amount", 0.0f, 100.0f, 100.0f, "%");

    // EQ
    tog ("eq_on", "EQ On", true);
    flt ("eq_hpf", "EQ Low Cut", 20.0f, 300.0f, 80.0f, "Hz", 90.0f);
    flt ("eq_low", "EQ Body", -12.0f, 12.0f, 0.0f, "dB");
    flt ("eq_mid", "EQ Presence", -12.0f, 12.0f, 0.0f, "dB");
    flt ("eq_air", "EQ Air", -12.0f, 12.0f, 0.0f, "dB");

    // De-esser
    tog ("ds_on", "De-Ess On", true);
    flt ("ds_freq", "De-Ess Freq", 4000.0f, 10000.0f, 6500.0f, "Hz");
    flt ("ds_thresh", "De-Ess Thresh", -40.0f, 0.0f, -22.0f, "dB");

    // Compressor
    tog ("cp_on", "Comp On", true);
    flt ("cp_thresh", "Comp Thresh", -40.0f, 0.0f, -18.0f, "dB");
    flt ("cp_ratio", "Comp Ratio", 1.0f, 20.0f, 3.0f, ":1", 4.0f);
    flt ("cp_attack", "Comp Attack", 0.1f, 100.0f, 10.0f, "ms", 10.0f);
    flt ("cp_release", "Comp Release", 10.0f, 500.0f, 120.0f, "ms", 120.0f);
    flt ("cp_makeup", "Comp Makeup", 0.0f, 18.0f, 3.0f, "dB");

    // Saturation
    tog ("sat_on", "Sat On", false);
    flt ("sat_drive", "Sat Drive", 0.0f, 24.0f, 6.0f, "dB");
    flt ("sat_mix", "Sat Mix", 0.0f, 100.0f, 50.0f, "%");

    // Doubler / chorus
    tog ("dbl_on", "Doubler On", false);
    flt ("dbl_mix", "Doubler Mix", 0.0f, 100.0f, 35.0f, "%");
    flt ("dbl_rate", "Doubler Rate", 0.1f, 5.0f, 0.6f, "Hz", 0.8f);
    flt ("dbl_depth", "Doubler Depth", 0.0f, 100.0f, 30.0f, "%");

    // Delay
    tog ("dly_on", "Delay On", false);
    flt ("dly_time", "Delay Time", 20.0f, 1000.0f, 280.0f, "ms", 250.0f);
    flt ("dly_fb", "Delay Feedback", 0.0f, 90.0f, 30.0f, "%");
    flt ("dly_tone", "Delay Tone", 1000.0f, 12000.0f, 5000.0f, "Hz", 4000.0f);
    flt ("dly_mix", "Delay Mix", 0.0f, 100.0f, 20.0f, "%");

    // Reverb
    tog ("rev_on", "Reverb On", false);
    flt ("rev_size", "Reverb Size", 0.0f, 100.0f, 50.0f, "%");
    flt ("rev_damp", "Reverb Damping", 0.0f, 100.0f, 50.0f, "%");
    flt ("rev_mix", "Reverb Mix", 0.0f, 100.0f, 18.0f, "%");

    // Output
    flt ("out_gain", "Output", -24.0f, 12.0f, 0.0f, "dB");

    return { p.begin(), p.end() };
}

VocalStackProcessor::VocalStackProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool VocalStackProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

void VocalStackProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };

    pitch.prepare (sampleRate);
    setLatencySamples (pitch.getLatencySamples());

    hpf.reset(); lowBell.reset(); midBell.reset(); airShelf.reset(); dsHp.reset();
    dsEnv = 0.0f;

    comp.prepare (spec);
    chorus.prepare (spec);
    chorus.setCentreDelay (18.0f);
    chorus.setFeedback (0.0f);

    dly.setMaximumDelayInSamples ((int) (sampleRate * 1.1));
    dly.prepare (spec);
    dly.reset();
    dlyLp[0] = dlyLp[1] = 0.0f;
    dlyTime.reset (sampleRate, 0.05);
    dlyTime.setCurrentAndTargetValue ((float) (get ("dly_time") * 0.001 * sampleRate));

    reverb.setSampleRate (sampleRate);
    reverb.reset();
}

void VocalStackProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buf.getNumSamples();
    const int nch = juce::jmin (2, buf.getNumChannels());
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buf.clear (c, 0, n);
    if (nch == 0 || n == 0) return;

    float* d[2] { buf.getWritePointer (0), nch > 1 ? buf.getWritePointer (1) : nullptr };

    // 1. Pitch correction (always runs so latency stays constant)
    pitch.process (buf, on ("pitch_on"), (int) get ("pitch_key"), (int) get ("pitch_scale"),
                   get ("pitch_speed"), get ("pitch_amount") * 0.01f);

    // 2. EQ
    if (on ("eq_on"))
    {
        hpf.makeHighPass (sr, get ("eq_hpf"), 0.707);
        lowBell.makePeak (sr, 250.0, 0.9, get ("eq_low"));
        midBell.makePeak (sr, 3500.0, 0.8, get ("eq_mid"));
        airShelf.makeHighShelf (sr, 10000.0, get ("eq_air"));
        for (int c = 0; c < nch; ++c)
            for (int i = 0; i < n; ++i)
            {
                float x = hpf.process (c, d[c][i]);
                x = lowBell.process (c, x);
                x = midBell.process (c, x);
                d[c][i] = airShelf.process (c, x);
            }
    }

    // 3. De-esser (split-band, linked detector, 4:1 on the high band)
    if (on ("ds_on"))
    {
        dsHp.makeHighPass (sr, get ("ds_freq"), 0.707);
        const float thresh = get ("ds_thresh");
        const float att = std::exp (-1.0f / (0.0005f * (float) sr));
        const float rel = std::exp (-1.0f / (0.03f * (float) sr));
        for (int i = 0; i < n; ++i)
        {
            float dry[2] { 0, 0 }, hi[2] { 0, 0 }, peak = 0.0f;
            for (int c = 0; c < nch; ++c)
            {
                dry[c] = d[c][i];
                hi[c] = dsHp.process (c, dry[c]);
                peak = juce::jmax (peak, std::abs (hi[c]));
            }
            dsEnv = peak > dsEnv ? att * dsEnv + (1.0f - att) * peak
                                 : rel * dsEnv + (1.0f - rel) * peak;
            const float envDb = 20.0f * std::log10 (juce::jmax (dsEnv, 1.0e-6f));
            const float over = envDb - thresh;
            const float g = over > 0.0f ? juce::Decibels::decibelsToGain (-over * 0.75f) : 1.0f;
            for (int c = 0; c < nch; ++c)
                d[c][i] = (dry[c] - hi[c]) + hi[c] * g;
        }
    }

    // 4. Compressor
    if (on ("cp_on"))
    {
        comp.setThreshold (get ("cp_thresh"));
        comp.setRatio (get ("cp_ratio"));
        comp.setAttack (get ("cp_attack"));
        comp.setRelease (get ("cp_release"));
        juce::dsp::AudioBlock<float> block (buf);
        auto sub = block.getSubsetChannelBlock (0, (size_t) nch);
        comp.process (juce::dsp::ProcessContextReplacing<float> (sub));
        buf.applyGain (juce::Decibels::decibelsToGain (get ("cp_makeup")));
    }

    // 5. Saturation (tanh waveshaper, parallel mix)
    if (on ("sat_on"))
    {
        const float drive = juce::Decibels::decibelsToGain (get ("sat_drive"));
        const float comp_ = 1.0f / std::sqrt (drive);
        const float mix = get ("sat_mix") * 0.01f;
        for (int c = 0; c < nch; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float x = d[c][i];
                const float wet = std::tanh (x * drive) * comp_;
                d[c][i] = x + mix * (wet - x);
            }
    }

    // 6. Doubler / chorus
    if (on ("dbl_on"))
    {
        chorus.setMix (get ("dbl_mix") * 0.01f);
        chorus.setRate (get ("dbl_rate"));
        chorus.setDepth (get ("dbl_depth") * 0.01f);
        juce::dsp::AudioBlock<float> block (buf);
        auto sub = block.getSubsetChannelBlock (0, (size_t) nch);
        chorus.process (juce::dsp::ProcessContextReplacing<float> (sub));
    }

    // 7. Delay (filtered feedback)
    if (on ("dly_on"))
    {
        dlyTime.setTargetValue (get ("dly_time") * 0.001f * (float) sr);
        const float fb = get ("dly_fb") * 0.01f;
        const float mix = get ("dly_mix") * 0.01f;
        const float a = std::exp (-juce::MathConstants<float>::twoPi * get ("dly_tone") / (float) sr);
        for (int i = 0; i < n; ++i)
        {
            const float t = dlyTime.getNextValue();
            for (int c = 0; c < nch; ++c)
            {
                const float delayed = dly.popSample (c, t);
                dlyLp[c] = a * dlyLp[c] + (1.0f - a) * delayed;
                dly.pushSample (c, d[c][i] + fb * dlyLp[c]);
                d[c][i] += mix * delayed;
            }
        }
    }

    // 8. Reverb
    if (on ("rev_on"))
    {
        const float mix = get ("rev_mix") * 0.01f;
        juce::Reverb::Parameters rp;
        rp.roomSize = get ("rev_size") * 0.01f;
        rp.damping = get ("rev_damp") * 0.01f;
        rp.wetLevel = mix * 0.5f;
        rp.dryLevel = 1.0f - mix * 0.25f;
        rp.width = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
        if (nch == 2) reverb.processStereo (d[0], d[1], n);
        else          reverb.processMono (d[0], n);
    }

    // 9. Output gain
    buf.applyGain (juce::Decibels::decibelsToGain (get ("out_gain")));
}

juce::AudioProcessorEditor* VocalStackProcessor::createEditor() { return new VocalStackEditor (*this); }

void VocalStackProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void VocalStackProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VocalStackProcessor(); }
