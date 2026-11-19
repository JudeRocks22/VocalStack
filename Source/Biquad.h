#pragma once
#include <cmath>

// Small RBJ-cookbook biquad (transposed direct form II), 2 channels, no allocation.
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] { 0, 0 }, z2[2] { 0, 0 };

    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }

    inline float process (int ch, float x) noexcept
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }

    void makeHighPass (double sr, double freq, double q)
    {
        const double w0 = 2.0 * M_PI * freq / sr, c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + alpha, -2 * c, 1 - alpha);
    }

    void makePeak (double sr, double freq, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w0 = 2.0 * M_PI * freq / sr;
        const double c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        set (1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A);
    }

    void makeHighShelf (double sr, double freq, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w0 = 2.0 * M_PI * freq / sr;
        const double c = std::cos (w0), s = std::sin (w0), alpha = s / 2.0 * std::sqrt (2.0);
        const double t = 2.0 * std::sqrt (A) * alpha;
        set (A * ((A + 1) + (A - 1) * c + t),
             -2 * A * ((A - 1) + (A + 1) * c),
             A * ((A + 1) + (A - 1) * c - t),
             (A + 1) - (A - 1) * c + t,
             2 * ((A - 1) - (A + 1) * c),
             (A + 1) - (A - 1) * c - t);
    }

private:
    void set (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
};
