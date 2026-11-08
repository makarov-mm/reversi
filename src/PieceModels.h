#pragma once

#include "MathTypes.h"

#include <cstdint>
#include <vector>

// The discs: two halves turned on a lathe, white on one face and black on the other, meeting in a fine
// groove round the edge. The disc's middle is the origin; the white face looks up the y axis.
namespace pieces {

struct Vertex {
    math::Vec3 position;
    math::Vec3 normal;
    float occlusion = 1; // light reaching into the groove
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

// A square of the board is 1 wide.
constexpr float Radius = 0.42f;
constexpr float HalfHeight = 0.06f;

struct DiscShape {
    Mesh white, black;             // the halves, for the scene
    Mesh simpleWhite, simpleBlack; // for the reflections
    Mesh simple;                   // the whole disc, for the shadows
};

DiscShape BuildDisc();

} // namespace pieces
