#include "UiPainter.h"

#include <cmath>
#include <utility>

namespace ui {

    using namespace gl;
    using math::Vec4;

    Color Fade(Color color, float opacity)
    {
        color.a = static_cast<std::uint8_t>(std::clamp(static_cast<float>(color.a) * opacity, 0.0f, 255.0f));
        return color;
    }

    Color Mix(Color a, Color b, float t)
    {
        auto mix = [t](std::uint8_t x, std::uint8_t y) { return static_cast<std::uint8_t>(std::lround(math::Lerp(x, y, std::clamp(t, 0.0f, 1.0f)))); };
        return { mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a) };
    }

    Color FromLinear(math::Vec3 color, float alpha)
    {
        auto encode = [](float c) { return static_cast<std::uint8_t>(std::lround(std::pow(std::clamp(c, 0.0f, 1.0f), 1 / 2.2f) * 255)); };
        return { .r = encode(color.x), .g = encode(color.y), .b = encode(color.z), .a = static_cast<std::uint8_t>(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255)) };
    }

    Painter::Painter()
    {
        using Stage = render::Program::Stage;
        m_program = render::Program("interface", { Stage{ GL_VERTEX_SHADER,{ L"UI_VERT" } }, Stage{ GL_FRAGMENT_SHADER,{ L"COMMON_GLSL", L"UI_FRAG" } } });
        GLuint buffer = 0;
        glCreateBuffers(1, &buffer);
        m_buffer = Buffer(buffer);
        GLuint array = 0;
        glCreateVertexArrays(1, &array);
        m_vertexArray = VertexArray(array);
        glVertexArrayVertexBuffer(array, 0, buffer, 0, sizeof(Vertex));

        constexpr GLenum types[4] = { GL_FLOAT, GL_FLOAT, GL_UNSIGNED_BYTE, GL_FLOAT };

        for (GLuint attribute = 0; attribute < 4; ++attribute) {
	        constexpr GLint sizes[4] = { 2, 2, 4, 4 };
	        constexpr GLuint offsets[4] = { offsetof(Vertex, x), offsetof(Vertex, u), offsetof(Vertex, color), offsetof(Vertex, kind) };

	        glEnableVertexArrayAttrib(array, attribute);
            glVertexArrayAttribFormat(array, attribute, sizes[attribute], types[attribute], types[attribute] == GL_UNSIGNED_BYTE, offsets[attribute]);
            glVertexArrayAttribBinding(array, attribute, 0);
        }
    }

    void Painter::Begin(int width, int height)
    {
        m_width_ = width;
        m_height_ = height;
        m_vertices.clear();
    }

    void Painter::quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, Color color, Vec4 shape)
    {
        const Vertex a{ .x = x0, .y = y0, .u = u0, .v = v0, .color = color, .kind = shape.x, .a = shape.y, .b = shape.z, .c = shape.w };
        const Vertex b{ .x = x1, .y = y0, .u = u1, .v = v0, .color = color, .kind = shape.x, .a = shape.y, .b = shape.z, .c = shape.w };
        const Vertex c{ .x = x1, .y = y1, .u = u1, .v = v1, .color = color, .kind = shape.x, .a = shape.y, .b = shape.z, .c = shape.w };
        const Vertex d{ .x = x0, .y = y1, .u = u0, .v = v1, .color = color, .kind = shape.x, .a = shape.y, .b = shape.z, .c = shape.w };
        m_vertices.insert(m_vertices.end(), { a, b, c, a, c, d });
    }

    // A quad round a rectangle, its uv the position from the rectangle's center, for the distance shapes.
    void Painter::shape(Rect rect, float margin, Color color, Vec4 shape)
    {
        const float halfWidth = rect.width / 2, halfHeight = rect.height / 2;
        quad(rect.x - margin, rect.y - margin, rect.Right() + margin, rect.Bottom() + margin, -halfWidth - margin, -halfHeight - margin, halfWidth + margin, halfHeight + margin, color, shape);
    }

    void Painter::fill(Rect rect, float radius, Color color)
    {
        radius = std::min({ radius, rect.width / 2, rect.height / 2 });
        shape(rect, 1, color, { .x = 3, .y = rect.width / 2, .z = rect.height / 2, .w = radius });
    }

    void Painter::outline(Rect rect, float radius, Color color)
    {
        radius = std::min({ radius, rect.width / 2, rect.height / 2 });
        shape(rect, 1, color, { .x = 5, .y = rect.width / 2, .z = rect.height / 2, .w = radius });
    }

    void Painter::shadow(Rect rect, float blur, Color color)
    {
        shape(rect, blur * 1.5f, color, { 4, rect.width / 2, rect.height / 2, blur });
    }

    void Painter::disc(float x, float y, float radius, Color color)
    {
        shape({ .x = x - radius, .y = y - radius, .width = radius * 2, .height = radius * 2 }, 1, color, { 7, radius, 0, 0 });
    }

    void Painter::spinner(float x, float y, float radius, float thickness, float angle, Color color)
    {
        shape({ .x = x - radius, .y = y - radius, .width = radius * 2, .height = radius * 2 }, thickness, color, { 6, radius, thickness, angle });
    }

    void Painter::arrow(float x, float y, float halfWidth, float halfHeight, float direction, Color color)
    {
        shape({ .x = x - halfWidth, .y = y - halfHeight, .width = halfWidth * 2, .height = halfHeight * 2 }, 1, color, { 8, halfWidth, halfHeight, direction });
    }

    float Painter::text(FontId id, float x, float y, std::string_view utf8, Color color)
    {
        const text::Font& font = Font(id);
        const float kind = static_cast<float>(std::to_underlying(id));
        float pen = std::round(x);
        const float top = std::round(y);

        for (std::size_t i = 0; i < utf8.size();) {
            const text::Glyph& glyph = font.find(text::NextCodepoint(utf8, i));
            const float left = std::round(pen + glyph.left), glyphTop = top + glyph.top;

            if (glyph.width > 0)
                quad(left, glyphTop, left + glyph.width, glyphTop + glyph.height, glyph.u0, glyph.v0, glyph.u1, glyph.v1, color, { kind, 0, 0, 0 });

            pen += glyph.advance;
        }

        return pen - std::round(x);
    }

    void Painter::text_centered(FontId font, Rect rect, std::string_view utf8, Color color)
    {
        const float width = Measure(font, utf8);
        const float height = Font(font).ascent();
        text(font, rect.x + (rect.width - width) / 2, rect.y + (rect.height - height) / 2 - height * 0.04f, utf8, color);
    }

    void Painter::flush()
    {
        if (m_vertices.empty())
            return;

        glNamedBufferData(m_buffer.Id(), static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)), m_vertices.data(), GL_STREAM_DRAW);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, m_width_, m_height_);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        m_program.Set("uScreen", math::Vec2{ static_cast<float>(m_width_), static_cast<float>(m_height_) });
        m_program.Use();

        for (int i = 0; i < 3; ++i)
        {
            glBindTextureUnit(static_cast<GLuint>(8 + i), m_fonts[i] ? m_fonts[i]->texture() : 0);
        }

        glBindVertexArray(m_vertexArray.Id());
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));
        glDisable(GL_BLEND);
        m_vertices.clear();
    }
}
