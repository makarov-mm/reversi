#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The rules of reversi (Othello) on the 8×8 board: positions, moves, notation, and a game with its history.
namespace reversi {

enum class Color : std::uint8_t { Black, White };

constexpr Color Opposite(Color color) { return color == Color::Black ? Color::White : Color::Black; }

// The squares: 0 is a1, 1 b1 ... 7 h1, 8 a2 ... 63 h8. Row 1 is at the top of a diagram, as on a printed board.
using Square = int;
constexpr int SquareCount = 64;
constexpr Square Pass = -1; // the move of a side that cannot place a disc

constexpr int FileOf(Square square) { return square & 7; }
constexpr int RowOf(Square square) { return square >> 3; }
constexpr Square SquareAt(int file, int row) { return file >= 0 && file < 8 && row >= 0 && row < 8 ? row * 8 + file : -1; }
constexpr std::uint64_t Bit(Square square) { return std::uint64_t{1} << square; }

// "f5"; a pass is "pass".
std::string SquareName(Square square);
std::optional<Square> ParseSquare(std::string_view text);

// The empty squares where `player` can place a disc.
std::uint64_t MovesOf(std::uint64_t player, std::uint64_t opponent);
// The opponent's discs that a disc placed on `square` turns over.
std::uint64_t FlipsOf(Square square, std::uint64_t player, std::uint64_t opponent);

class Position {
public:
    // d4 and e5 white, d5 and e4 black; Black moves first.
    static Position Start();
    // 64 squares from a1 to h8, X (or *) for Black, O for White, - (or .) for empty, then X or O for the side to move.
    static std::expected<Position, std::string> FromText(std::string_view text);
    std::string ToText() const;

    Color SideToMove() const { return m_side; }
    std::uint64_t Discs(Color color) const { return color == Color::Black ? m_black : m_white; }
    std::uint64_t Player() const { return Discs(m_side); }
    std::uint64_t Opponent() const { return Discs(Opposite(m_side)); }
    std::uint64_t Empty() const { return ~(m_black | m_white); }
    int Count(Color color) const { return std::popcount(Discs(color)); }
    int EmptyCount() const { return std::popcount(Empty()); }
    std::optional<Color> At(Square square) const;

    // Where the side to move can place a disc, and what a disc there turns over.
    std::uint64_t Moves() const { return MovesOf(Player(), Opponent()); }
    bool IsLegal(Square square) const { return square >= 0 && square < SquareCount && (Moves() & Bit(square)) != 0; }
    std::uint64_t Flips(Square square) const { return FlipsOf(square, Player(), Opponent()); }
    // The side to move has nowhere to go, but the other side has.
    bool MustPass() const { return Moves() == 0 && MovesOf(Opponent(), Player()) != 0; }
    // Neither side can move: the game is over.
    bool IsOver() const { return Moves() == 0 && MovesOf(Opponent(), Player()) == 0; }

    Position After(Square square) const; // a legal move, or a pass
    std::uint64_t Key() const;

    bool operator==(const Position&) const = default;

private:
    std::uint64_t m_black = 0;
    std::uint64_t m_white = 0;
    Color m_side = Color::Black;
};

enum class GameResult : std::uint8_t { Ongoing, BlackWins, WhiteWins, Draw };

struct GameStatus {
    GameResult result = GameResult::Ongoing;
    int black = 0, white = 0; // the discs on the board

    constexpr bool Over() const { return result != GameResult::Ongoing; }
};

struct MoveRecord {
    Square square = Pass;
    Color side = Color::Black;
    std::uint64_t flipped = 0; // the discs turned over

    bool IsPass() const { return square == Pass; }
};

// A game: the starting position, the moves played, and the moves taken back that can be replayed.
// A side left without a move passes at once; the pass is kept as a move of its own.
class Game {
public:
    Game() { Reset(Position::Start()); }

    void Reset(const Position& start);
    // A transcript ("f5d6c3d3"), a position, or a position and then a transcript.
    static std::expected<Game, std::string> Parse(std::string_view text);
    // The starting position and the moves, for saving.
    std::string Serialize() const;
    static std::optional<Game> Deserialize(std::string_view text);
    // The moves as Othello transcripts are written, without the passes: "f5d6c3d3".
    std::string Transcript() const;
    // For the clipboard: the transcript, after the starting position when it is not the usual one.
    std::string ToText() const;

    const Position& Current() const { return m_positions.back(); }
    const Position& Start() const { return m_positions.front(); }
    const Position& PositionAt(int ply) const { return m_positions[static_cast<std::size_t>(ply)]; }
    std::span<const MoveRecord> Moves() const { return m_moves; }
    int Ply() const { return static_cast<int>(m_moves.size()); }
    GameStatus Status() const { return m_status; }

    // Places a disc on a legal square; a pass that it forces on the other side follows.
    bool Play(Square square);
    bool CanUndo() const { return !m_moves.empty(); }
    bool CanRedo() const { return !m_redo.empty(); }
    // Takes back the last disc placed, with the pass after it.
    bool Undo();
    bool Redo();

private:
    void Push(Square square);
    void UpdateStatus();

    std::vector<Position> m_positions;
    std::vector<MoveRecord> m_moves;
    std::vector<MoveRecord> m_redo; // the next to replay at the end
    GameStatus m_status;
};

} // namespace reversi
