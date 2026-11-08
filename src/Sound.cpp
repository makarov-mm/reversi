#include "Sound.h"

#include "Win32.h"

#include <mmsystem.h>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <utility>

namespace {
    constexpr int Rate = 44100;
    constexpr double TwoPi = 2 * std::numbers::pi;

    using Samples = std::vector<float>;

    Samples silence(double seconds)
    {
	    return Samples(static_cast<std::size_t>(seconds * Rate), 0.0f);
    }

    void Add(Samples& target, const Samples& source, double at, float gain = 1)
    {
        const auto offset = static_cast<std::size_t>(at * Rate);
        if (target.size() < offset + source.size())
            target.resize(offset + source.size(), 0.0f);
        for (std::size_t i = 0; i < source.size(); ++i)
            target[offset + i] += source[i] * gain;
    }

    // A knock of a disc on wood: a short burst of filtered noise and quickly damped resonances; `length` shortens
    // or lengthens how it rings.
    Samples knock(double pitch, std::uint32_t seed, double length = 1)
    {
        Samples samples = silence(0.16 * length);
        std::minstd_rand random(seed);
        std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
        float filtered = 0;
        for (std::size_t i = 0; i < samples.size(); ++i) {
            const double t = static_cast<double>(i) / Rate;
            filtered += (noise(random) - filtered) * 0.35f;
            const double burst = filtered * std::exp(-t / (0.006 * length));
            const double body = 0.55 * std::sin(TwoPi * 620 * pitch * t) * std::exp(-t / (0.028 * length)) +
                0.3 * std::sin(TwoPi * 1480 * pitch * t) * std::exp(-t / (0.012 * length)) +
                0.45 * std::sin(TwoPi * 170 * pitch * t) * std::exp(-t / (0.045 * length));
            const double attack = std::min(1.0, t / 0.0008);
            samples[i] = static_cast<float>((burst * 0.9 + body) * attack);
        }
        return samples;
    }

    // A small bell: inharmonic partials dying away at their own rates.
    Samples bell(double frequency, double length)
    {
        Samples samples = silence(length);
        const double partials[4][3] = { { 1.0, 1.0, 1.0 },{ 2.0, 0.45, 0.6 },{ 2.76, 0.3, 0.4 },{ 5.4, 0.12, 0.25 } };
        for (std::size_t i = 0; i < samples.size(); ++i) {
            const double t = static_cast<double>(i) / Rate;
            double value = 0;
            for (const auto& [ratio, gain, decay] : partials)
                value += gain * std::sin(TwoPi * frequency * ratio * t) * std::exp(-t / (length * 0.35 * decay));
            samples[i] = static_cast<float>(value * std::min(1.0, t / 0.004));
        }
        return samples;
    }

    std::vector<std::uint8_t> Wave(const Samples& samples, float volume)
    {
        float peak = 1e-6f;

        for (float sample : samples)
            peak = std::max(peak, std::abs(sample));

        const auto dataSize = static_cast<std::uint32_t>(samples.size() * 2);
        std::vector<std::uint8_t> wave(44 + dataSize);
        auto write32 = [&](std::size_t at, std::uint32_t value) { std::memcpy(&wave[at], &value, 4); };
        auto write16 = [&](std::size_t at, std::uint16_t value) { std::memcpy(&wave[at], &value, 2); };
        std::memcpy(&wave[0], "RIFF", 4);
        write32(4, 36 + dataSize);
        std::memcpy(&wave[8], "WAVEfmt ", 8);
        write32(16, 16);
        write16(20, 1); // PCM
        write16(22, 1); // mono
        write32(24, Rate);
        write32(28, Rate * 2);
        write16(32, 2);
        write16(34, 16);
        std::memcpy(&wave[36], "data", 4);
        write32(40, dataSize);

        for (std::size_t i = 0; i < samples.size(); ++i) {
            const auto value = static_cast<std::int16_t>(std::clamp(samples[i] / peak * volume, -1.0f, 1.0f) * 32000);
            std::memcpy(&wave[44 + i * 2], &value, 2);
        }

        return wave;
    }
}

SoundBank::SoundBank()
{
    auto index = [](Effect effect) { return std::to_underlying(effect); };

    m_waves[index(Effect::Place)] = Wave(knock(1.0, 1), 0.6f);
    m_waves[index(Effect::Flip)] = Wave(knock(2.2, 4, 0.45), 0.3f);
    m_waves[index(Effect::Stack)] = Wave(knock(1.6, 6), 0.3f);
    m_waves[index(Effect::Pass)] = Wave(bell(659.25, 0.8), 0.22f);

    Samples chord = silence(1.8);
    const double notes[4] = {523.25, 659.25, 783.99, 1046.5};
    for (int i = 0; i < 4; ++i)
        Add(chord, bell(notes[i], 1.5), i * 0.11, 0.8f);
    m_waves[index(Effect::GameOver)] = Wave(chord, 0.35f);

    Samples click = silence(0.03);

    for (std::size_t i = 0; i < click.size(); ++i) {
        const double t = static_cast<double>(i) / Rate;
        click[i] = static_cast<float>(std::sin(TwoPi * 2600 * t) * std::exp(-t / 0.004));
    }

    m_waves[index(Effect::Click)] = Wave(click, 0.18f);
}

void SoundBank::play(Effect effect) const
{
    if (!enabled)
        return;

    const auto& wave = m_waves[std::to_underlying(effect)];
    PlaySoundW(reinterpret_cast<LPCWSTR>(wave.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}
