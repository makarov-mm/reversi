#pragma once

#include "MathTypes.h"
#include "Reversi.h"

#include <cmath>
#include <cstdint>
#include <vector>

// The board, its frame and the table. A square is one unit; the playing surface is the plane y = 0. Row 1
// is at the far side seen from the default view, from +z, as at the top of a printed board: a1 is the far left corner.
namespace board {

constexpr float FrameInner = 4.0f;
constexpr float FrameOuter = 4.62f;
constexpr float FrameTop = 0.03f;
constexpr float TableHeight = -0.32f;

struct Vertex {
    math::Vec3 position;
    math::Vec3 normal;
    math::Vec2 uv;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

inline math::Vec3 SquareCenter(reversi::Square square)
{
    return {static_cast<float>(reversi::FileOf(square)) - 3.5f, 0, static_cast<float>(reversi::RowOf(square)) - 3.5f};
}

// The square under a point of the playing surface, or -1.
inline reversi::Square SquareAt(math::Vec3 point)
{
    return reversi::SquareAt(static_cast<int>(std::floor(point.x + 4)), static_cast<int>(std::floor(point.z + 4)));
}

Mesh BuildSquares();
Mesh BuildFrame();
Mesh BuildTable(float radius);

} // namespace board
