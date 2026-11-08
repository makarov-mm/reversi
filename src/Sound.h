#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Short sounds made up at start: a knock for a disc set down, clicks for discs turning over, a bell for a
// pass, a chord at the end.
class SoundBank {
public:
    enum class Effect { Place, Flip, Stack, Pass, GameOver, Click, Count };

    SoundBank();
    void play(Effect effect) const;

    bool enabled = true;

private:
    std::array<std::vector<std::uint8_t>, static_cast<std::size_t>(Effect::Count)> m_waves;
};
