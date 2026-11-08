#include "Settings.h"

#include "Themes.h"

#include <algorithm>
#include <vector>

namespace {
    constexpr const wchar_t* Section = L"Reversi3D";

    std::wstring file_path()
    {
        wchar_t buffer[MAX_PATH] = {};
        const DWORD length = GetEnvironmentVariableW(L"APPDATA", buffer, MAX_PATH);
        std::wstring folder = length > 0 && length < MAX_PATH ? std::wstring(buffer, length) : std::wstring(L".");
        folder += L"\\Reversi3D";
        CreateDirectoryW(folder.c_str(), nullptr);
        return folder + L"\\Reversi3D.ini";
    }

    int read_int(const std::wstring& file, const wchar_t* key, int fallback)
    {
        return static_cast<int>(GetPrivateProfileIntW(Section, key, fallback, file.c_str()));
    }

    void write_int(const std::wstring& file, const wchar_t* key, int value)
    {
        WritePrivateProfileStringW(Section, key, std::to_wstring(value).c_str(), file.c_str());
    }

    Language system_language()
    {
        const LANGID language = PRIMARYLANGID(GetUserDefaultUILanguage());
        return language == LANG_RUSSIAN || language == LANG_UKRAINIAN || language == LANG_BELARUSIAN ? Language::Russian : Language::English;
    }
}

Settings Settings::load()
{
    const std::wstring file = file_path();
    Settings settings;
    settings.theme = std::clamp(read_int(file, L"Theme", 0), 0, ThemeCount - 1);
    settings.level = std::clamp(read_int(file, L"Level", 3), 1, 5);
    settings.mode = std::clamp(read_int(file, L"Mode", 0), 0, 3);
    settings.showMoves = read_int(file, L"ShowMoves", 1) != 0;
    settings.sound = read_int(file, L"Sound", 1) != 0;
    settings.panel = read_int(file, L"Panel", 1) != 0;
    const int language = read_int(file, L"Language", -1);
    settings.language = language < 0 ? system_language() : language == 1 ? Language::Russian : Language::English;

    const int width = read_int(file, L"WindowWidth", 0), height = read_int(file, L"WindowHeight", 0);
    if (width > 0 && height > 0) {
        settings.hasWindow = true;
        settings.window.left = read_int(file, L"WindowLeft", 0);
        settings.window.top = read_int(file, L"WindowTop", 0);
        settings.window.right = settings.window.left + width;
        settings.window.bottom = settings.window.top + height;
        settings.maximized = read_int(file, L"Maximized", 0) != 0;
    }

    std::vector<wchar_t> game(65536);
    const DWORD length = GetPrivateProfileStringW(Section, L"Game", L"", game.data(), static_cast<DWORD>(game.size()), file.c_str());
    settings.game = win32::Narrow(std::wstring_view(game.data(), length));
    return settings;
}

void Settings::save() const
{
    const std::wstring file = file_path();
    write_int(file, L"Theme", theme);
    write_int(file, L"Level", level);
    write_int(file, L"Mode", mode);
    write_int(file, L"ShowMoves", showMoves ? 1 : 0);
    write_int(file, L"Sound", sound ? 1 : 0);
    write_int(file, L"Panel", panel ? 1 : 0);
    write_int(file, L"Language", language == Language::Russian ? 1 : 0);

    if (hasWindow) {
        write_int(file, L"WindowLeft", window.left);
        write_int(file, L"WindowTop", window.top);
        write_int(file, L"WindowWidth", window.right - window.left);
        write_int(file, L"WindowHeight", window.bottom - window.top);
        write_int(file, L"Maximized", maximized ? 1 : 0);
    }

    WritePrivateProfileStringW(Section, L"Game", win32::Widen(game).c_str(), file.c_str());
}
