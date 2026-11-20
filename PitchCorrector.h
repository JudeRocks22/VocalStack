#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>
#include <algorithm>

// Real-time pitch corrector: YIN pitch detection + scale snapping + dual-tap
// crossfading delay-line pitch shifter. Latency is a constant ~15 ms (windowSamples / 2).
class PitchCorrector
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        inRing.assign ((size_t) analysisSize, 0.0f);
        tmp.assign ((size_t) analysisSize, 0.0f);
        yin.assign ((size_t) maxTau + 2, 0.0f);
        for (auto& d : delay) d.assign ((size_t) delaySize, 0.0f);
        windowSamples = (float) (sr * 0.030);
        ringPos = hopCounter = writePos = 0;
        phase = 0.0f;
        ratio = targetRatio = 1.0f;
    }

    int getLatencySamples() const { return (int) (windowSamples * 0.5f); }

    // scaleType: 0 = chromatic, 1 = major, 2 = natural minor
    void process (juce::AudioBuffer<float>& buf, bool enabled, int key, int scaleType, float speedMs, float amount)
    {
        const int n = buf.getNumSamples();
        const int nch = juce::jmin (2, buf.getNumChannels());
        const float coef = speedMs <= 0.5f ? 1.0f
                         : 1.0f - std::exp (-1.0f / (speedMs * 0.001f * (float) sampleRate));
        float* io[2] { buf.getWritePointer (0), nch > 1 ? buf.getWritePointer (1) : nullptr };

        for (int i = 0; i < n; ++i)
        {
            float mono = io[0][i];
            if (nch > 1) mono = 0.5f * (mono + io[1][i]);
            inRing[(size_t) ringPos] = mono;
            ringPos = (ringPos + 1) % analysisSize;

            if (enabled)
            {
                if (++hopCounter >= hop) { hopCounter = 0; analyse (key, scaleType, amount); }
            }
            else
                targetRatio = 1.0f;

            ratio += coef * (targetRatio - ratio);

            for (int c = 0; c < nch; ++c)
                delay[c][(size_t) writePos] = io[c][i];

            float dp = (1.0f - ratio) / windowSamples;
            if (std::abs (ratio - 1.0f) < 0.0005f)
            {
                // In tune: drift slowly to a phase where only one tap is audible (no comb filtering).
                const float target = std::round (phase * 2.0f) * 0.5f;
                dp += (target - phase) * 0.00002f;
            }
            phase += dp;
            phase -= std::floor (phase);

            const float gA = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase);
            const float gB = 1.0f - gA;
            float pB = phase + 0.5f; pB -= std::floor (pB);
            const float dA = phase * windowSamples + 2.0f;
            const float dB = pB * windowSamples + 2.0f;

            for (int c = 0; c < nch; ++c)
                io[c][i] = gA * readTap (c, dA) + gB * readTap (c, dB);

            writePos = (writePos + 1) % delaySize;
        }
    }

private:
    static constexpr int analysisSize = 2048, yinWindow = 1024, maxTau = 1000, hop = 512, delaySize = 16384;

    float readTap (int ch, float d) const
    {
        float pos = (float) writePos - d;
        while (pos < 0.0f) pos += (float) delaySize;
        const int i0 = (int) pos, i1 = (i0 + 1) % delaySize;
        const float f = pos - (float) i0;
        return delay[ch][(size_t) i0] * (1.0f - f) + delay[ch][(size_t) i1] * f;
    }

    void analyse (int key, int scaleType, float amount)
    {
        for (int i = 0; i < analysisSize; ++i)
            tmp[(size_t) i] = inRing[(size_t) ((ringPos + i) % analysisSize)];

        const float f0 = detect();
        if (f0 <= 0.0f) { targetRatio = 1.0f; return; }

        const float midi = 69.0f + 12.0f * std::log2 (f0 / 440.0f);
        static const bool major[12] { 1,0,1,0,1,1,0,1,0,1,0,1 };
        static const bool minor[12] { 1,0,1,1,0,1,0,1,1,0,1,0 };

        float best = std::round (midi), bestDist = 1e9f;
        const int base = (int) std::floor (midi);
        for (int cand = base - 2; cand <= base + 3; ++cand)
        {
            const int deg = ((cand - key) % 12 + 12) % 12;
            const bool ok = scaleType == 0 || (scaleType == 1 ? major[deg] : minor[deg]);
            const float dist = std::abs ((float) cand - midi);
            if (ok && dist < bestDist) { bestDist = dist; best = (float) cand; }
        }

        const float full = std::pow (2.0f, (best - midi) / 12.0f);
        targetRatio = juce::jlimit (0.7f, 1.4f, std::pow (full, amount));
    }

    // YIN pitch detector on the last yinWindow samples. Returns 0 when unvoiced.
    float detect()
    {
        double energy = 0.0;
        for (int j = 0; j < yinWindow; ++j) { const float s = tmp[(size_t) j]; energy += (double) s * s; }
        if (std::sqrt (energy / yinWindow) < 0.003) return 0.0f;   // ~ -50 dBFS gate

        const int tauMin = juce::jmax (2, (int) (sampleRate / 1000.0));
        const int tauMax = juce::jmin (maxTau, (int) (sampleRate / 70.0));

        yin[0] = 1.0f;
        float running = 0.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float d = 0.0f;
            for (int j = 0; j < yinWindow; ++j)
            {
                const float diff = tmp[(size_t) j] - tmp[(size_t) (j + tau)];
                d += diff * diff;
            }
            running += d;
            yin[(size_t) tau] = running > 0.0f ? d * (float) tau / running : 1.0f;
        }

        int tau = -1;
        for (int t = tauMin; t < tauMax; ++t)
        {
            if (yin[(size_t) t] < 0.15f)
            {
                while (t + 1 < tauMax && yin[(size_t) t + 1] < yin[(size_t) t]) ++t;
                tau = t;
                break;
            }
        }
        if (tau < 0) return 0.0f;

        float refined = (float) tau;
        if (tau > 1 && tau < tauMax)
        {
            const float a = yin[(size_t) tau - 1], b = yin[(size_t) tau], c = yin[(size_t) tau + 1];
            const float den = a - 2.0f * b + c;
            if (std::abs (den) > 1e-9f) refined += 0.5f * (a - c) / den;
        }
        return (float) sampleRate / refined;
    }

    double sampleRate = 44100.0;
    std::vector<float> inRing, tmp, yin, delay[2];
    int ringPos = 0, hopCounter = 0, writePos = 0;
    float windowSamples = 1323.0f, phase = 0.0f, ratio = 1.0f, targetRatio = 1.0f;
};
