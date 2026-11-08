#pragma once

#include "Gl.h"

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

// Fonts drawn by GDI into textures, and text measured and laid out from them.
namespace text {
    struct Glyph {
        float u0 = 0, v0 = 0, u1 = 0, v1 = 0; // in the atlas
        float width = 0, height = 0;          // of the quad, in pixels
        float left = 0, top = 0;              // from the pen and the line's top to the quad
        float advance = 0;
    };

    class Font {
    public:
        // A font of the given pixel height, with the Latin and Cyrillic letters and a few symbols.
        Font(const wchar_t* face, float pixelHeight, int weight);

        const Glyph& find(char32_t codepoint) const;
        float line_height() const { return m_line_height_; }
        float ascent() const { return m_ascent_; }
        float measure(std::string_view utf8) const;
        gl::GLuint texture() const { return m_texture_.Id(); }

    private:
        std::unordered_map<char32_t, Glyph> m_glyphs_;
        Glyph m_fallback_;
        float m_line_height_ = 0;
        float m_ascent_ = 0;
        gl::Texture m_texture_;
    };

    // Decodes UTF-8 one code point at a time.
    char32_t NextCodepoint(std::string_view text, std::size_t& index);

    // The coordinates inlaid in the board's frame, as coverage over the square from -extent to extent in x and z.
    std::vector<std::uint8_t> render_board_labels(int size, float extent, float inner);
}
