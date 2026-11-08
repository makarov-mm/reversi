#pragma once

#include "Strings.h"
#include "Win32.h"

#include <string>

// The choices and the game in progress, kept between sessions in %APPDATA%\Reversi3D\Reversi3D.ini.
struct Settings {
    int theme = 0;
    int level = 3;
    int mode = 0;
    bool showMoves = true; // the squares where the side to move can play
    bool sound = true;
    bool panel = true;
    Language language = Language::English;
    bool hasWindow = false;
    RECT window{};
    bool maximized = false;
    std::string game; // see reversi::Game::Serialize

    static Settings Load();
    void Save() const;
};
