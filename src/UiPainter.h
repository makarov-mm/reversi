#pragma once

#include "Gl.h"
#include "MathTypes.h"
#include "Renderer.h"
#include "Text.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

// Draws the interface: rounded panels, shadows and text, in pixels from the window's top left corner.
namespace ui {
    struct Color {
        std::uint8_t r = 255, g = 255, b = 255, a = 255;
    };

    constexpr Color Rgba(int r, int g, int b, int a = 255)
    {
        return { .r = static_cast<std::uint8_t>(r), .g = static_cast<std::uint8_t>(g), .b = static_cast<std::uint8_t>(b), .a = static_cast<std::uint8_t>(a) };
    }

    Color Fade(Color color, float opacity);
    Color Mix(Color a, Color b, float t);
    // A linear color of the scene, as it shows on the screen.
    Color FromLinear(math::Vec3 color, float alpha = 1);

    struct Rect {
        float x = 0, y = 0, width = 0, height = 0;

        float Right() const { return x + width; }
        float Bottom() const { return y + height; }
        bool Contains(float px, float py) const { return px >= x && px < x + width && py >= y && py < y + height; }
        Rect Inset(float d) const { return { x + d, y + d, width - 2 * d, height - 2 * d }; }
    };

    enum class FontId { Body, Heading, Display };

    class Painter {
    public:
        Painter();

        void SetFonts(const text::Font* body, const text::Font* heading, const text::Font* display) { m_fonts = { body, heading, display }; }
        const text::Font& Font(FontId id) const { return *m_fonts[std::to_underlying(id)]; }

        void Begin(int width, int height);
        void fill(Rect rect, float radius, Color color);
        void outline(Rect rect, float radius, Color color);
        void shadow(Rect rect, float blur, Color color);
        void disc(float x, float y, float radius, Color color);
        void spinner(float x, float y, float radius, float thickness, float angle, Color color);
        // A small triangle pointing left (direction -1) or right (1).
        void arrow(float x, float y, float halfWidth, float halfHeight, float direction, Color color);
        // Text whose line starts at the top left point; returns its width.
        float text(FontId font, float x, float y, std::string_view utf8, Color color);
        void text_centered(FontId font, Rect rect, std::string_view utf8, Color color);
        float Measure(FontId font, std::string_view utf8) const { return Font(font).measure(utf8); }
        float LineHeight(FontId font) const { return Font(font).line_height(); }
        // Draws what has been queued; anything queued later is drawn on top of what was drawn meanwhile.
        void flush();

    private:
        struct Vertex {
            float x, y, u, v;
            Color color;
            float kind, a, b, c;
        };

        void quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, Color color, math::Vec4 shape);
        void shape(Rect rect, float margin, Color color, math::Vec4 shape);

        render::Program m_program;
        gl::Buffer m_buffer;
        gl::VertexArray m_vertexArray;
        std::vector<Vertex> m_vertices;
        std::array<const text::Font*, 3> m_fonts{};
        int m_width_ = 0, m_height_ = 0;
    };
}
