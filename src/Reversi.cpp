#include "Reversi.h"

#include <cctype>
#include <format>
#include <utility>

namespace reversi {
    namespace {
        constexpr std::uint64_t mix(std::uint64_t x)
        {
            x ^= x >> 33;
            x *= 0xFF51AFD7ED558CCDull;
            x ^= x >> 33;
            x *= 0xC4CEB9FE1A85EC53ull;
            x ^= x >> 33;
            return x;
        }

        bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
    }

    std::string SquareName(Square square)
    {
        if (square < 0 || square >= SquareCount)
            return "pass";
        return { static_cast<char>('a' + FileOf(square)), static_cast<char>('1' + RowOf(square)) };
    }

    std::optional<Square> ParseSquare(std::string_view text)
    {
        if (text.size() != 2)
            return std::nullopt;
        const int file = std::tolower(static_cast<unsigned char>(text[0])) - 'a', row = text[1] - '1';
        const Square square = SquareAt(file, row);
        return square >= 0 ? std::optional<Square>(square) : std::nullopt;
    }

    namespace {
        // The opponent's discs in runs from the player's, along a direction and its opposite, one to six long.
        // `inner` is the opponent's discs that a run may pass through: away from the side edges for the directions
        // with a sideways step, so that nothing wraps round from one side of the board to the other.
        template <int Step> std::uint64_t Runs(std::uint64_t from, std::uint64_t inner)
        {
            std::uint64_t forward = inner & (from << Step), backward = inner & (from >> Step);
            forward |= inner & (forward << Step);
            backward |= inner & (backward >> Step);
            const std::uint64_t pairsForward = inner & (inner << Step), pairsBackward = inner & (inner >> Step);
            forward |= pairsForward & (forward << (2 * Step));
            backward |= pairsBackward & (backward >> (2 * Step));
            forward |= pairsForward & (forward << (2 * Step));
            backward |= pairsBackward & (backward >> (2 * Step));
            return (forward << Step) | (backward >> Step);
        }

        // The opponent's discs between a new disc and the player's, along a direction and its opposite.
        template <int Step> std::uint64_t Line(std::uint64_t start, std::uint64_t player, std::uint64_t inner)
        {
            std::uint64_t flips = 0;
            std::uint64_t forward = inner & (start << Step), backward = inner & (start >> Step);

            for (int i = 0; i < 5; ++i) {
                forward |= inner & (forward << Step);
                backward |= inner & (backward >> Step);
            }

            if ((forward << Step) & player)
                flips |= forward;

            if ((backward >> Step) & player)
                flips |= backward;

            return flips;
        }

        constexpr std::uint64_t Inner = 0x7E7E7E7E7E7E7E7Eull;
    }

    std::uint64_t MovesOf(std::uint64_t player, std::uint64_t opponent)
    {
        const std::uint64_t inner = opponent & Inner;
        return (Runs<1>(player, inner) | Runs<8>(player, opponent) | Runs<7>(player, inner) | Runs<9>(player, inner)) & ~(player | opponent);
    }

    std::uint64_t FlipsOf(Square square, std::uint64_t player, std::uint64_t opponent)
    {
        const std::uint64_t start = Bit(square), inner = opponent & Inner;
        return Line<1>(start, player, inner) | Line<8>(start, player, opponent) | Line<7>(start, player, inner) | Line<9>(start, player, inner);
    }

    Position Position::Start()
    {
        Position position;
        position.m_white = Bit(SquareAt(3, 3)) | Bit(SquareAt(4, 4));
        position.m_black = Bit(SquareAt(4, 3)) | Bit(SquareAt(3, 4));
        position.m_side = Color::Black;
        return position;
    }

    std::expected<Position, std::string> Position::FromText(std::string_view text)
    {
        Position position;
        int square = 0;
        std::size_t i = 0;
        for (; i < text.size() && square < SquareCount; ++i) {
            const char c = text[i];
            if (is_space(c))
                continue;
            if (c == 'X' || c == 'x' || c == '*')
                position.m_black |= Bit(square);
            else if (c == 'O' || c == 'o')
                position.m_white |= Bit(square);
            else if (c != '-' && c != '.' && c != '_')
                return std::unexpected(std::format("unexpected '{}' on the board", c));
            ++square;
        }
        if (square < SquareCount)
            return std::unexpected("the board has fewer than 64 squares");
        while (i < text.size() && is_space(text[i]))
            ++i;
        if (i < text.size()) {
            const char c = text[i];
            if (c == 'O' || c == 'o' || c == 'W' || c == 'w')
                position.m_side = Color::White;
            else if (c != 'X' && c != 'x' && c != '*' && c != 'B' && c != 'b')
                return std::unexpected("the side to move is not X or O");
        }
        return position;
    }

    std::string Position::ToText() const
    {
        std::string text;
        text.reserve(SquareCount + 2);
        for (Square square = 0; square < SquareCount; ++square)
            text += (m_black & Bit(square)) ? 'X' : (m_white & Bit(square)) ? 'O' : '-';
        text += ' ';
        text += m_side == Color::Black ? 'X' : 'O';
        return text;
    }

    std::optional<Color> Position::At(Square square) const
    {
        if (m_black & Bit(square))
            return Color::Black;
        if (m_white & Bit(square))
            return Color::White;
        return std::nullopt;
    }

    Position Position::After(Square square) const
    {
        Position next = *this;
        if (square >= 0) {
            const std::uint64_t flips = Flips(square);
            std::uint64_t& own = m_side == Color::Black ? next.m_black : next.m_white;
            std::uint64_t& other = m_side == Color::Black ? next.m_white : next.m_black;
            own |= flips | Bit(square);
            other &= ~flips;
        }
        next.m_side = Opposite(m_side);
        return next;
    }

    std::uint64_t Position::Key() const
    {
        return mix(m_black) ^ mix(m_white ^ 0x9E3779B97F4A7C15ull) ^ (m_side == Color::White ? 0x2545F4914F6CDD1Dull : 0);
    }

    void Game::Reset(const Position& start)
    {
        m_positions.assign(1, start.MustPass() ? start.After(Pass) : start);
        m_moves.clear();
        m_redo.clear();
        UpdateStatus();
    }

    void Game::Push(Square square)
    {
        const Position& position = Current();
        MoveRecord record;
        record.square = square;
        record.side = position.SideToMove();
        record.flipped = square >= 0 ? position.Flips(square) : 0;
        m_moves.push_back(record);
        m_positions.push_back(position.After(square));
    }

    bool Game::Play(Square square)
    {
        if (m_status.Over() || !Current().IsLegal(square))
            return false;
        Push(square);
        if (Current().MustPass())
            Push(Pass);
        m_redo.clear();
        UpdateStatus();
        return true;
    }

    bool Game::Undo()
    {
        if (m_moves.empty())
            return false;
        do {
            m_redo.push_back(m_moves.back());
            m_moves.pop_back();
            m_positions.pop_back();
        } while (m_redo.back().IsPass() && !m_moves.empty());
        UpdateStatus();
        return true;
    }

    bool Game::Redo()
    {
        if (m_redo.empty())
            return false;
        // The disc, then the pass that came after it.
        do {
            Push(m_redo.back().square);
            m_redo.pop_back();
        } while (!m_redo.empty() && m_redo.back().IsPass());
        UpdateStatus();
        return true;
    }

    void Game::UpdateStatus()
    {
        const Position& position = Current();
        m_status.black = position.Count(Color::Black);
        m_status.white = position.Count(Color::White);
        if (!position.IsOver())
            m_status.result = GameResult::Ongoing;
        else
            m_status.result = m_status.black > m_status.white ? GameResult::BlackWins
            : m_status.white > m_status.black ? GameResult::WhiteWins
            : GameResult::Draw;
    }

    std::string Game::Transcript() const
    {
        std::string text;
        for (const MoveRecord& record : m_moves)
            if (!record.IsPass())
                text += SquareName(record.square);
        return text;
    }

    std::string Game::ToText() const
    {
        if (Start() == Position::Start())
            return Transcript();
        return Start().ToText() + "\r\n" + Transcript();
    }

    std::expected<Game, std::string> Game::Parse(std::string_view text)
    {
        Game game;
        // A board first, if the text begins with 64 squares.
        std::size_t begin = 0;
        while (begin < text.size() && is_space(text[begin]))
            ++begin;
        std::size_t squares = 0, end = begin;
        for (; end < text.size() && squares < SquareCount; ++end) {
            const char c = text[end];
            if (is_space(c))
                continue;
            if (c != 'X' && c != 'x' && c != '*' && c != 'O' && c != 'o' && c != '-' && c != '.' && c != '_')
                break;
            ++squares;
        }
        bool hasPosition = false;
        if (squares == SquareCount) {
            // The side to move may follow.
            while (end < text.size() && is_space(text[end]))
                ++end;
            std::size_t sideEnd = end;
            if (sideEnd < text.size() && std::string_view("XxOoBbWw*").find(text[sideEnd]) != std::string_view::npos &&
                (sideEnd + 1 >= text.size() || !std::isdigit(static_cast<unsigned char>(text[sideEnd + 1]))))
                ++sideEnd;
            auto position = Position::FromText(text.substr(begin, sideEnd - begin));
            if (!position)
                return std::unexpected(position.error());
            game.Reset(*position);
            text.remove_prefix(sideEnd);
            hasPosition = true;
        }

        // Then the moves: every letter a-h followed by a digit 1-8; passes are left out or skipped.
        int count = 0;
        for (std::size_t i = 0; i + 1 < text.size(); ++i) {
            const char letter = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
            const char digit = text[i + 1];
            if (letter < 'a' || letter > 'h' || digit < '1' || digit > '8')
                continue;
            // Part of a longer word, such as a name, is not a move.
            if (i > 0 && std::isalpha(static_cast<unsigned char>(text[i - 1])))
                continue;
            const Square square = SquareAt(letter - 'a', digit - '1');
            ++count;
            if (!game.Play(square))
                return std::unexpected(std::format("move {} ({}) is not legal", count, SquareName(square)));
            ++i;
        }

        if (count == 0 && !hasPosition)
            return std::unexpected("no moves or position");

        return game;
    }

    std::string Game::Serialize() const
    {
        return Start().ToText() + "|" + Transcript();
    }

    std::optional<Game> Game::Deserialize(std::string_view text)
    {
        const std::size_t bar = text.find('|');

        if (bar == std::string_view::npos)
            return std::nullopt;

        const auto start = Position::FromText(text.substr(0, bar));

        if (!start)
            return std::nullopt;

        auto game = Parse(start->ToText() + " " + std::string(text.substr(bar + 1)));

        if (!game)
            return std::nullopt;

        return std::move(*game);
    }
}
