#include "PieceModels.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace pieces {

using namespace math;

namespace {

struct ProfilePoint {
    float r = 0, y = 0;
    float occlusion = 1;
};

// A smooth dip of width 2 * halfWidth: 1 in the middle, 0 at the edges.
float Bump(float x, float center, float halfWidth)
{
    const float t = (x - center) / halfWidth;
    return std::abs(t) < 1 ? (1 - t * t) * (1 - t * t) : 0.0f;
}

constexpr float SeamRadius = 0.012f; // the rounded edge at the seam, where the halves meet
constexpr float EdgeRadius = 0.022f; // the rounded edge of the face
constexpr float FaceEdge = Radius - EdgeRadius;
constexpr float Ring = 0.335f;       // a fine groove round the face

// The face: very slightly domed, with the groove near its edge.
float FaceHeight(float r)
{
    return HalfHeight + 0.003f * (1 - (r / FaceEdge) * (r / FaceEdge)) - 0.0022f * Bump(r, Ring, 0.006f);
}

float FaceOcclusion(float r) { return 1 - 0.35f * Bump(r, Ring, 0.008f); }

// The outline of the white half, from the seam round the edge and over the face to its middle. The face is
// nearly flat: its points crowd round the groove and thin out towards the middle.
std::vector<ProfilePoint> Profile(bool detailed)
{
    std::vector<ProfilePoint> points;

    // The rounded edge at the seam, darker deep in the groove.
    const int seam = detailed ? 5 : 2;
    for (int i = 0; i < seam; ++i) {
        const float angle = -Pi / 2 + Pi / 2 * static_cast<float>(i) / static_cast<float>(seam);
        const float t = static_cast<float>(i) / static_cast<float>(seam);
        points.push_back({Radius - SeamRadius + SeamRadius * std::cos(angle), SeamRadius + SeamRadius * std::sin(angle), 0.45f + 0.55f * t});
    }
    // The straight side.
    points.push_back({Radius, SeamRadius});
    // The edge of the face.
    const int edge = detailed ? 8 : 3;
    for (int i = 0; i < edge; ++i) {
        const float angle = Pi / 2 * static_cast<float>(i) / static_cast<float>(edge);
        points.push_back({FaceEdge + EdgeRadius * std::cos(angle), HalfHeight - EdgeRadius + EdgeRadius * std::sin(angle)});
    }
    // The face, inwards.
    std::vector<float> radii;
    if (detailed) {
        radii = {FaceEdge, 0.385f, 0.37f, 0.355f};
        for (int i = -4; i <= 4; ++i)
            radii.push_back(Ring + 0.003f * static_cast<float>(-i));
        radii.insert(radii.end(), {0.31f, 0.28f, 0.24f, 0.19f, 0.13f, 0.07f, 0.0f});
    }
    else {
        radii = {FaceEdge, 0.25f, 0.0f};
    }
    for (const float r : radii)
        points.push_back({r, FaceHeight(r), detailed ? FaceOcclusion(r) : 1.0f});
    return points;
}

// The profile turned round the y axis; `mirrored` turns it upside down, for the black half. Normals come from
// the neighbouring points of the outline.
Mesh Lathe(const std::vector<ProfilePoint>& profile, int segments, bool mirrored)
{
    Mesh mesh;
    const int count = static_cast<int>(profile.size());
    std::vector<Vec2> normals(profile.size());
    for (int i = 0; i < count; ++i) {
        const ProfilePoint& before = profile[static_cast<std::size_t>(std::max(i - 1, 0))];
        const ProfilePoint& after = profile[static_cast<std::size_t>(std::min(i + 1, count - 1))];
        const Vec2 tangent{after.r - before.r, after.y - before.y};
        const float length = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y);
        normals[static_cast<std::size_t>(i)] = length > 0 ? Vec2{tangent.y / length, -tangent.x / length} : Vec2{0, 1};
    }
    normals.front() = {0, -1}; // the seam faces the other half
    normals.back() = {0, 1};   // the middle of the face

    const float sign = mirrored ? -1.0f : 1.0f;
    for (int i = 0; i < count; ++i) {
        const ProfilePoint& point = profile[static_cast<std::size_t>(i)];
        const Vec2 n = normals[static_cast<std::size_t>(i)];
        for (int j = 0; j <= segments; ++j) {
            const float angle = 2 * Pi * static_cast<float>(j) / static_cast<float>(segments);
            const float c = std::cos(angle), s = std::sin(angle);
            mesh.vertices.push_back({{point.r * c, sign * point.y, point.r * s}, Normalize(Vec3{n.x * c, sign * n.y, n.x * s}), point.occlusion});
        }
    }
    const auto index = [&](int i, int j) { return static_cast<std::uint32_t>(i * (segments + 1) + j); };
    auto triangle = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
        // Mirroring turns the winding round; it is turned back so that the outside stays the front.
        if (mirrored)
            mesh.indices.insert(mesh.indices.end(), {a, c, b});
        else
            mesh.indices.insert(mesh.indices.end(), {a, b, c});
    };
    for (int i = 0; i + 1 < count; ++i) {
        for (int j = 0; j < segments; ++j) {
            // Next to the axis a quad narrows to a triangle.
            if (profile[static_cast<std::size_t>(i)].r > 0)
                triangle(index(i, j), index(i + 1, j + 1), index(i, j + 1));
            if (profile[static_cast<std::size_t>(i + 1)].r > 0)
                triangle(index(i, j), index(i + 1, j), index(i + 1, j + 1));
        }
    }
    return mesh;
}

Mesh Merge(const Mesh& a, const Mesh& b)
{
    Mesh mesh = a;
    const auto offset = static_cast<std::uint32_t>(a.vertices.size());
    mesh.vertices.insert(mesh.vertices.end(), b.vertices.begin(), b.vertices.end());
    for (const std::uint32_t index : b.indices)
        mesh.indices.push_back(index + offset);
    return mesh;
}

} // namespace

DiscShape BuildDisc()
{
    const auto detailed = Profile(true), simple = Profile(false);
    DiscShape disc;
    disc.white = Lathe(detailed, 96, false);
    disc.black = Lathe(detailed, 96, true);
    disc.simpleWhite = Lathe(simple, 32, false);
    disc.simpleBlack = Lathe(simple, 32, true);
    disc.simple = Merge(disc.simpleWhite, disc.simpleBlack);
    return disc;
}

} // namespace pieces
