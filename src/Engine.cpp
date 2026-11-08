#include "Engine.h"

#include <algorithm>
#include <array>
#include <bit>
#include <initializer_list>
#include <random>
#include <utility>

namespace reversi {

    namespace {

        constexpr int Infinity = 1 << 20;

        enum Bound : std::uint8_t { NoBound, UpperBound, LowerBound, ExactBound };

        constexpr std::uint64_t NotFileA = 0xFEFEFEFEFEFEFEFEull;
        constexpr std::uint64_t NotFileH = 0x7F7F7F7F7F7F7F7Full;
        constexpr std::uint64_t FileA = 0x0101010101010101ull;
        constexpr std::uint64_t FileH = 0x8080808080808080ull;
        constexpr std::uint64_t Row1 = 0x00000000000000FFull;
        constexpr std::uint64_t Row8 = 0xFF00000000000000ull;
        constexpr std::uint64_t Edges = FileA | FileH | Row1 | Row8;
        constexpr std::uint64_t Corners = 0x8100000000000081ull;

        // A corner, the square diagonally next to it (the X-square) and the two beside it on the edges (the C-squares):
        // taking those while the corner is empty usually hands the corner over.
        struct CornerArea {
            std::uint64_t corner, x, c;
        };

        constexpr std::array<CornerArea, 4> CornerAreas{ {
            { Bit(0), Bit(9), Bit(1) | Bit(8) },
            { Bit(7), Bit(14), Bit(6) | Bit(15) },
            { Bit(56), Bit(49), Bit(57) | Bit(48) },
            { Bit(63), Bit(54), Bit(62) | Bit(55) },
        } };

        // The worth of holding each square, for the evaluation and for ordering the moves.
        constexpr std::array<int, 64> SquareValue{
            20, -3, 11, 8,  8,  11, -3, 20, //
            -3, -7, -4, 1,  1,  -4, -7, -3, //
            11, -4, 2,  2,  2,  2,  -4, 11, //
            8,  1,  2,  -3, -3, 2,  1,  8,  //
            8,  1,  2,  -3, -3, 2,  1,  8,  //
            11, -4, 2,  2,  2,  2,  -4, 11, //
            -3, -7, -4, 1,  1,  -4, -7, -3, //
            20, -3, 11, 8,  8,  11, -3, 20, //
        };

        // The squares of each value, so that the position can be scored a few masks at a time.
        struct ValueMask {
            int value;
            std::uint64_t mask;
        };

        constexpr std::array<ValueMask, 8> ValueMasks = [] {
            std::array<ValueMask, 8> masks{};
            int count = 0;
            for (int square = 0; square < 64; ++square) {
                int i = 0;
                while (i < count && masks[static_cast<std::size_t>(i)].value != SquareValue[static_cast<std::size_t>(square)])
                    ++i;
                if (i == count)
                    masks[static_cast<std::size_t>(count++)].value = SquareValue[static_cast<std::size_t>(square)];
                masks[static_cast<std::size_t>(i)].mask |= Bit(square);
            }
            return masks;
            }();

        // The diagonals in both directions, for finding the full ones.
        constexpr std::array<std::uint64_t, 15> Diagonals = [] {
            std::array<std::uint64_t, 15> lines{};
            for (int square = 0; square < 64; ++square)
                lines[static_cast<std::size_t>(FileOf(square) - RowOf(square) + 7)] |= Bit(square);
            return lines;
            }();

        constexpr std::array<std::uint64_t, 15> AntiDiagonals = [] {
            std::array<std::uint64_t, 15> lines{};
            for (int square = 0; square < 64; ++square)
                lines[static_cast<std::size_t>(FileOf(square) + RowOf(square))] |= Bit(square);
            return lines;
            }();

        // The squares next to each square.
        constexpr std::array<std::uint64_t, 64> NeighbourMasks = [] {
            std::array<std::uint64_t, 64> masks{};
            for (int square = 0; square < 64; ++square)
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if ((dx != 0 || dy != 0) && SquareAt(FileOf(square) + dx, RowOf(square) + dy) >= 0)
                            masks[static_cast<std::size_t>(square)] |= Bit(SquareAt(FileOf(square) + dx, RowOf(square) + dy));
            return masks;
            }();

        // The quarters of the board: at the very end, a move into a quarter with an odd number of empty squares
        // tends to get the last move there.
        constexpr std::array<std::uint64_t, 4> Quarters{ 0x000000000F0F0F0Full, 0x00000000F0F0F0F0ull, 0x0F0F0F0F00000000ull, 0xF0F0F0F000000000ull };

        constexpr std::uint64_t Mix(std::uint64_t x)
        {
            x ^= x >> 33;
            x *= 0xFF51AFD7ED558CCDull;
            x ^= x >> 33;
            x *= 0xC4CEB9FE1A85EC53ull;
            x ^= x >> 33;
            return x;
        }

        std::uint64_t Hash(std::uint64_t player, std::uint64_t opponent) { return Mix(player) ^ Mix(opponent ^ 0x9E3779B97F4A7C15ull); }

        // The squares next to any of the given ones.
        std::uint64_t Neighbours(std::uint64_t bits)
        {
            const std::uint64_t sideways = ((bits << 1) & NotFileA) | ((bits >> 1) & NotFileH);
            const std::uint64_t row = bits | sideways;
            return sideways | (row << 8) | (row >> 8);
        }

        // The discs that can never be turned over: along each of the four lines through such a disc, the line is
        // full, or the disc has the edge or another such disc of its color next to it.
        std::uint64_t StableDiscs(std::uint64_t own, std::uint64_t occupied)
        {
            if ((own & Corners) == 0)
                return 0; // there are none without a corner, but in full lines along the edges, which are rare
            std::uint64_t fullRows = 0;
            for (int row = 0; row < 8; ++row)
                if (((occupied >> (8 * row)) & 0xFF) == 0xFF)
                    fullRows |= std::uint64_t{ 0xFF } << (8 * row);
            std::uint64_t columns = occupied & (occupied >> 8);
            columns &= columns >> 16;
            columns &= columns >> 32;
            const std::uint64_t fullColumns = (columns & 0xFF) * FileA;
            std::uint64_t fullDiagonals = 0, fullAntiDiagonals = 0;
            for (std::size_t i = 0; i < Diagonals.size(); ++i) {
                if ((occupied & Diagonals[i]) == Diagonals[i])
                    fullDiagonals |= Diagonals[i];
                if ((occupied & AntiDiagonals[i]) == AntiDiagonals[i])
                    fullAntiDiagonals |= AntiDiagonals[i];
            }

            std::uint64_t stable = 0;
            for (;;) {
                const std::uint64_t across = fullRows | FileA | FileH | ((stable << 1) & NotFileA) | ((stable >> 1) & NotFileH);
                const std::uint64_t down = fullColumns | Row1 | Row8 | (stable << 8) | (stable >> 8);
                const std::uint64_t diagonal = fullDiagonals | Edges | ((stable << 9) & NotFileA) | ((stable >> 9) & NotFileH);
                const std::uint64_t antiDiagonal = fullAntiDiagonals | Edges | ((stable << 7) & NotFileH) | ((stable >> 7) & NotFileA);
                const std::uint64_t next = own & across & down & diagonal & antiDiagonal;
                if (next == stable)
                    return stable;
                stable = next;
            }
        }

        int Count(std::uint64_t bits) { return std::popcount(bits); }

        // A difference as a share of a total, from -100 to 100.
        int Share(int own, int other) { return own + other > 0 ? 100 * (own - other) / (own + other) : 0; }

        // The weights of the evaluation's terms; the sum is divided by eight.
        struct Weights {
            int mobility = 237;     // times the difference in moves as a share of all moves, -100 to 100
            int frontier = 166;     // the same for the discs next to empty squares, which count against
            int corner = 8900;      // for each corner
            int xSquare = 3183;     // against each disc diagonally next to an empty corner
            int cSquare = 1415;     // against each disc beside an empty corner on the edge
            int stable = 1250;      // for each disc that can no longer be turned
            int discs = 15;         // times the difference in discs as a share
            int squares = 10;       // times the worth of the squares held
        };

        constexpr Weights Weight{};

        int EvaluateWith(std::uint64_t player, std::uint64_t opponent, std::uint64_t playerMoves)
        {
            const std::uint64_t empty = ~(player | opponent);
            int score = 0;

            // Room to move, now and later: the moves each side has, and the discs each has next to empty squares,
            // which give the other side something to play against.
            score += Weight.mobility * Share(Count(playerMoves), Count(MovesOf(opponent, player)));
            const std::uint64_t frontier = Neighbours(empty);
            score -= Weight.frontier * Share(Count(player & frontier), Count(opponent & frontier));

            // The corners, and the squares that give them away while they are empty.
            score += Weight.corner * (Count(player & Corners) - Count(opponent & Corners));
            for (const CornerArea& area : CornerAreas) {
                if (empty & area.corner) {
                    score -= Weight.xSquare * (Count(player & area.x) - Count(opponent & area.x));
                    score -= Weight.cSquare * (Count(player & area.c) - Count(opponent & area.c));
                }
            }

            // Discs that are there for good.
            const std::uint64_t occupied = player | opponent;
            score += Weight.stable * (Count(StableDiscs(player, occupied)) - Count(StableDiscs(opponent, occupied)));

            // The discs themselves, and where they stand.
            score += Weight.discs * Share(Count(player), Count(opponent));
            int squares = 0;
            for (const ValueMask& value : ValueMasks)
                squares += value.value * (Count(player & value.mask) - Count(opponent & value.mask));
            score += Weight.squares * squares;
            return std::clamp(score / 8, -Engine::Win + 1000, Engine::Win - 1000);
        }

        int FinalScore(std::uint64_t player, std::uint64_t opponent)
        {
            const int difference = Count(player) - Count(opponent);
            return difference > 0 ? Engine::Win + difference : difference < 0 ? -Engine::Win + difference : 0;
        }

        // The last empty square, filled by whichever side can: the final difference in discs.
        int LastMove(std::uint64_t player, std::uint64_t opponent)
        {
            const Square square = std::countr_zero(~(player | opponent));
            const int before = 2 * Count(player) - 63;
            if (const std::uint64_t flips = FlipsOf(square, player, opponent))
                return before + 2 * Count(flips) + 1;
            if (const std::uint64_t flips = FlipsOf(square, opponent, player))
                return before - 2 * Count(flips) - 1;
            return before;
        }

        struct Candidate {
            Square square;
            std::uint64_t flips;
            int order;
        };
    }

    Engine::Engine(int hashMegabytes)
    {
        std::size_t entries = 1;
        while (entries * 2 * sizeof(Entry) <= static_cast<std::size_t>(hashMegabytes) * 1024 * 1024)
            entries *= 2;
        m_table.resize(entries);
    }

    void Engine::Clear()
    {
        std::ranges::fill(m_table, Entry{});
    }

    int Engine::Evaluate(std::uint64_t player, std::uint64_t opponent)
    {
        return EvaluateWith(player, opponent, MovesOf(player, opponent));
    }

    bool Engine::ShouldStop() { return m_stop.stop_requested() || std::chrono::steady_clock::now() >= m_deadline; }

    void Engine::Store(std::uint64_t key, int depth, int score, int bound, int move, bool solved)
    {
        Entry& entry = Slot(key);
        if (entry.key == key && entry.depth > depth && entry.solved == solved && bound != ExactBound)
            return;
        entry.key = key;
        entry.score = score;
        entry.depth = static_cast<std::int8_t>(std::clamp(depth, 0, 120));
        entry.bound = static_cast<std::uint8_t>(bound);
        entry.move = static_cast<std::int8_t>(move);
        entry.solved = solved ? 1 : 0;
    }

    int Engine::Negamax(std::uint64_t player, std::uint64_t opponent, int depth, int alpha, int beta, bool passed)
    {
        if ((++m_nodes & 4095) == 0 && ShouldStop())
            m_aborted = true;
        if (m_aborted)
            return 0;

        const std::uint64_t moves = MovesOf(player, opponent);
        if (!moves) {
            if (passed)
                return FinalScore(player, opponent);
            return -Negamax(opponent, player, depth, -beta, -alpha, true); // a pass costs no depth
        }
        if (depth <= 0)
            return EvaluateWith(player, opponent, moves);

        const std::uint64_t key = Hash(player, opponent);
        const Entry& entry = Slot(key);
        int ttMove = -1;

        if (entry.key == key && !entry.solved) {
            ttMove = entry.move;
            if (entry.depth >= depth) {
                const int score = entry.score;
                if (entry.bound == ExactBound || (entry.bound == LowerBound && score >= beta) || (entry.bound == UpperBound && score <= alpha))
                    return score;
            }
        }

        // Best first: the move that was best here before, then those that leave the opponent the fewest replies.
        std::array<Candidate, 64> list;
        int count = 0;
        for (std::uint64_t bits = moves; bits; bits &= bits - 1) {
            const Square square = std::countr_zero(bits);
            const std::uint64_t flips = FlipsOf(square, player, opponent);
            int order = SquareValue[static_cast<std::size_t>(square)];
            if (square == ttMove)
                order = Infinity;
            else if (depth >= 2)
                order = 8 * order - 64 * Count(MovesOf(opponent & ~flips, player | flips | Bit(square)));
            list[static_cast<std::size_t>(count++)] = { square, flips, order };
        }

        const int alphaBefore = alpha;
        int best = -Infinity, bestMove = -1;
        for (int i = 0; i < count; ++i) {
            int top = i;
            for (int j = i + 1; j < count; ++j)
                if (list[static_cast<std::size_t>(j)].order > list[static_cast<std::size_t>(top)].order)
                    top = j;
            std::swap(list[static_cast<std::size_t>(i)], list[static_cast<std::size_t>(top)]);
            const Candidate& move = list[static_cast<std::size_t>(i)];
            const std::uint64_t nextPlayer = opponent & ~move.flips, nextOpponent = player | move.flips | Bit(move.square);
            int score;
            if (i == 0) {
                score = -Negamax(nextPlayer, nextOpponent, depth - 1, -beta, -alpha, false);
            }
            else {
                score = -Negamax(nextPlayer, nextOpponent, depth - 1, -alpha - 1, -alpha, false);
                if (score > alpha && score < beta)
                    score = -Negamax(nextPlayer, nextOpponent, depth - 1, -beta, -alpha, false);
            }
            if (m_aborted)
                return 0;
            if (score > best) {
                best = score;
                bestMove = move.square;
            }
            if (score > alpha) {
                alpha = score;
                if (alpha >= beta)
                    break;
            }
        }
        Store(key, depth, best, best >= beta ? LowerBound : best > alphaBefore ? ExactBound : UpperBound, bestMove, false);
        return best;
    }

    // The last few empty squares: no table and no sorting, only the quarters with an odd number of empty squares
    // first; the empty squares are tried one by one rather than the moves generated.
    int Engine::SolveSmall(std::uint64_t player, std::uint64_t opponent, int alpha, int beta, int empties, bool passed)
    {
        ++m_nodes;
        if (empties == 1)
            return LastMove(player, opponent);
        const std::uint64_t empty = ~(player | opponent);
        std::uint64_t odd = 0;
        for (const std::uint64_t quarter : Quarters)
            if (Count(empty & quarter) & 1)
                odd |= quarter;
        int best = -Infinity;
        for (const std::uint64_t group : {empty& odd, empty & ~odd}) {
            for (std::uint64_t bits = group; bits; bits &= bits - 1) {
                const Square square = std::countr_zero(bits);
                if ((NeighbourMasks[static_cast<std::size_t>(square)] & opponent) == 0)
                    continue;
                const std::uint64_t flips = FlipsOf(square, player, opponent);
                if (flips == 0)
                    continue;
                const int score = -SolveSmall(opponent & ~flips, player | flips | Bit(square), -beta, -alpha, empties - 1, false);
                if (score > best) {
                    best = score;
                    if (score > alpha) {
                        alpha = score;
                        if (alpha >= beta)
                            return best;
                    }
                }
            }
        }
        if (best == -Infinity) {
            // No move: a pass, or the end.
            if (passed)
                return Count(player) - Count(opponent);
            return -SolveSmall(opponent, player, -beta, -alpha, empties, true);
        }
        return best;
    }

    int Engine::Solve(std::uint64_t player, std::uint64_t opponent, int alpha, int beta, int empties, bool passed)
    {
        if (empties <= 6)
            return SolveSmall(player, opponent, alpha, beta, empties, passed);
        if ((++m_nodes & 4095) == 0 && ShouldStop())
            m_aborted = true;
        if (m_aborted)
            return 0;

        const std::uint64_t moves = MovesOf(player, opponent);
        if (!moves) {
            if (passed)
                return Count(player) - Count(opponent);
            return -Solve(opponent, player, -beta, -alpha, empties, true);
        }

        const std::uint64_t key = Hash(player, opponent);
        const Entry& entry = Slot(key);
        int ttMove = -1;

        if (entry.key == key && entry.solved) {
            ttMove = entry.move;
            const int score = entry.score;
            if (entry.bound == ExactBound || (entry.bound == LowerBound && score >= beta) || (entry.bound == UpperBound && score <= alpha))
                return score;
            if (entry.bound == LowerBound)
                alpha = std::max(alpha, score);
            else if (entry.bound == UpperBound)
                beta = std::min(beta, score);
        }

        // Fastest first: the moves that leave the opponent the fewest replies, corners and the odd quarters breaking ties.
        const std::uint64_t empty = ~(player | opponent);
        std::uint64_t odd = 0;
        for (const std::uint64_t quarter : Quarters)
            if (Count(empty & quarter) & 1)
                odd |= quarter;
        std::array<Candidate, 64> list;
        int count = 0;
        for (std::uint64_t bits = moves; bits; bits &= bits - 1) {
            const Square square = std::countr_zero(bits);
            const std::uint64_t flips = FlipsOf(square, player, opponent);
            const std::uint64_t nextPlayer = opponent & ~flips, nextOpponent = player | flips | Bit(square);
            int order;
            if (empties >= 12) {
                const std::uint64_t replies = MovesOf(nextPlayer, nextOpponent);
                order = -EvaluateWith(nextPlayer, nextOpponent, replies) - 200 * Count(replies);
            }
            else
                order = -16 * Count(MovesOf(nextPlayer, nextOpponent)) + ((Bit(square) & Corners) ? 12 : 0) + ((Bit(square) & odd) ? 3 : 0);
            if (square == ttMove)
                order = Infinity;
            list[static_cast<std::size_t>(count++)] = { square, flips, order };
        }

        const int alphaBefore = alpha;
        int best = -Infinity, bestMove = -1;
        for (int i = 0; i < count; ++i) {
            int top = i;
            for (int j = i + 1; j < count; ++j)
                if (list[static_cast<std::size_t>(j)].order > list[static_cast<std::size_t>(top)].order)
                    top = j;
            std::swap(list[static_cast<std::size_t>(i)], list[static_cast<std::size_t>(top)]);
            const Candidate& move = list[static_cast<std::size_t>(i)];
            const std::uint64_t nextPlayer = opponent & ~move.flips, nextOpponent = player | move.flips | Bit(move.square);
            int score;
            if (i == 0) {
                score = -Solve(nextPlayer, nextOpponent, -beta, -alpha, empties - 1, false);
            }
            else {
                score = -Solve(nextPlayer, nextOpponent, -alpha - 1, -alpha, empties - 1, false);
                if (score > alpha && score < beta)
                    score = -Solve(nextPlayer, nextOpponent, -beta, -alpha, empties - 1, false);
            }
            if (m_aborted)
                return 0;
            if (score > best) {
                best = score;
                bestMove = move.square;
            }
            if (score > alpha) {
                alpha = score;
                if (alpha >= beta)
                    break;
            }
        }
        Store(key, empties, best, best >= beta ? LowerBound : best > alphaBefore ? ExactBound : UpperBound, bestMove, true);
        return best;
    }

    bool Engine::SearchMiddle(const Position& root, std::vector<RootMove>& moves, const SearchLimits& limits, SearchResult& result,
        std::chrono::steady_clock::time_point start, std::chrono::milliseconds budget)
    {
        const std::uint64_t player = root.Player(), opponent = root.Opponent();
        const bool exactScores = limits.randomMargin > 0;
        const int empties = root.EmptyCount();
        for (int depth = 1; depth <= limits.maxDepth; ++depth) {
            int alpha = -Infinity;
            Square iterationBest = Pass;
            int iterationScore = -Infinity;
            bool complete = true;
            for (std::size_t i = 0; i < moves.size(); ++i) {
                RootMove& move = moves[i];
                const std::uint64_t flips = FlipsOf(move.square, player, opponent);
                const std::uint64_t nextPlayer = opponent & ~flips, nextOpponent = player | flips | Bit(move.square);
                int score;
                if (i == 0 || exactScores) {
                    score = -Negamax(nextPlayer, nextOpponent, depth - 1, -Infinity, exactScores ? Infinity : -alpha, false);
                }
                else {
                    score = -Negamax(nextPlayer, nextOpponent, depth - 1, -alpha - 1, -alpha, false);
                    if (!m_aborted && score > alpha)
                        score = -Negamax(nextPlayer, nextOpponent, depth - 1, -Infinity, -alpha, false);
                }
                if (m_aborted) {
                    complete = false;
                    break;
                }
                move.score = score;
                if (score > iterationScore) {
                    iterationScore = score;
                    iterationBest = move.square;
                }
                alpha = std::max(alpha, score);
            }

            // An unfinished iteration still counts when its first move, the best before, was searched: a move
            // that then came out best beat it.
            if (iterationBest != Pass) {
                result.best = iterationBest;
                result.score = iterationScore;
            }
            if (!complete)
                return false;
            result.depth = depth;
            std::ranges::stable_sort(moves, [](const RootMove& a, const RootMove& b) { return a.score > b.score; });
            const auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed > budget / 2 || IsWinScore(iterationScore) || depth >= empties)
                break;
        }
        return true;
    }

    bool Engine::SolveRoot(const Position& root, std::vector<RootMove>& moves, bool exactScores, SearchResult& result)
    {
        const std::uint64_t player = root.Player(), opponent = root.Opponent();
        const int empties = root.EmptyCount();
        int alpha = -SquareCount - 1;
        const int beta = SquareCount + 1;
        std::vector<RootMove> solved = moves;
        for (std::size_t i = 0; i < solved.size(); ++i) {
            RootMove& move = solved[i];
            const std::uint64_t flips = FlipsOf(move.square, player, opponent);
            const std::uint64_t nextPlayer = opponent & ~flips, nextOpponent = player | flips | Bit(move.square);
            int score;
            if (i == 0 || exactScores) {
                score = -Solve(nextPlayer, nextOpponent, -beta, exactScores ? beta : -alpha, empties - 1, false);
            }
            else {
                score = -Solve(nextPlayer, nextOpponent, -alpha - 1, -alpha, empties - 1, false);
                if (!m_aborted && score > alpha)
                    score = -Solve(nextPlayer, nextOpponent, -beta, -alpha, empties - 1, false);
            }
            if (m_aborted)
                return false;
            move.score = score;
            alpha = std::max(alpha, score);
        }
        std::ranges::stable_sort(solved, [](const RootMove& a, const RootMove& b) { return a.score > b.score; });
        moves = std::move(solved);
        result.best = moves.front().square;
        result.score = moves.front().score;
        result.solved = true;
        result.depth = empties;
        return true;
    }

    SearchResult Engine::Search(const Position& root, const SearchLimits& limits, std::stop_token stop)
    {
        const auto start = std::chrono::steady_clock::now();
        m_deadline = start + std::chrono::milliseconds(limits.milliseconds);
        m_stop = stop;
        m_aborted = false;
        m_nodes = 0;

        const std::uint64_t player = root.Player(), opponent = root.Opponent();
        std::vector<RootMove> moves;
        for (std::uint64_t bits = root.Moves(); bits; bits &= bits - 1)
            moves.push_back({ std::countr_zero(bits), 0 });
        if (moves.empty())
            return {};
        // To begin with, the moves that leave the opponent the fewest replies first.
        for (RootMove& move : moves) {
            const std::uint64_t flips = FlipsOf(move.square, player, opponent);
            move.score = 8 * SquareValue[static_cast<std::size_t>(move.square)] - 64 * Count(MovesOf(opponent & ~flips, player | flips | Bit(move.square)));
        }
        std::ranges::stable_sort(moves, [](const RootMove& a, const RootMove& b) { return a.score > b.score; });

        SearchResult result;
        result.best = moves.front().square;
        if (moves.size() == 1 && limits.randomMargin == 0)
            return result;

        // Before an exact solution of the end, a short search puts the likely best moves first for it.
        const bool solve = root.EmptyCount() <= limits.exactEmpties;
        const auto budget = std::chrono::milliseconds(solve ? limits.milliseconds / 5 : limits.milliseconds);
        SearchLimits middle = limits;
        if (solve)
            middle.maxDepth = std::min(middle.maxDepth, 10);
        m_deadline = start + budget;
        SearchMiddle(root, moves, middle, result, start, budget);
        if (solve && !stop.stop_requested()) {
            m_deadline = start + std::chrono::milliseconds(limits.milliseconds);
            m_aborted = false;
            SolveRoot(root, moves, limits.randomMargin > 0, result);
        }

        if (limits.randomMargin > 0 && result.depth > 0) {
            // The weaker levels pick among the good moves, the better ones more often. When the end is solved, the
            // margin is counted in discs.
            const int margin = result.solved ? limits.randomMargin / 25 : limits.randomMargin;
            std::mt19937_64 random(limits.seed ^ root.Key());
            std::vector<RootMove> candidates;
            for (const RootMove& move : moves)
                if (move.score >= moves.front().score - margin)
                    candidates.push_back(move);
            std::vector<double> weights;
            for (const RootMove& candidate : candidates)
                weights.push_back(1.0 + static_cast<double>(margin - (moves.front().score - candidate.score)) / std::max(1, margin) * 3.0);
            std::discrete_distribution<std::size_t> pick(weights.begin(), weights.end());
            const RootMove& chosen = candidates[pick(random)];
            result.best = chosen.square;
            result.score = chosen.score;
        }
        result.nodes = m_nodes;
        return result;
    }

    void AiPlayer::Start(const Position& position, const SearchLimits& limits)
    {
        Cancel();
        m_busy = true;
        m_worker = std::jthread([this, position, limits](std::stop_token stop) {
            SearchResult result = m_engine.Search(position, limits, stop);
            if (!stop.stop_requested()) {
                std::scoped_lock lock(m_mutex);
                m_result = result;
            }
            m_busy = false;
            });
    }

    void AiPlayer::Cancel()
    {
        if (m_worker.joinable()) {
            m_worker.request_stop();
            m_worker.join();
        }
        m_busy = false;
        std::scoped_lock lock(m_mutex);
        m_result.reset();
    }

    void AiPlayer::NewGame()
    {
        Cancel();
        m_engine.Clear();
    }

    std::optional<SearchResult> AiPlayer::TakeResult()
    {
        std::scoped_lock lock(m_mutex);
        return std::exchange(m_result, std::nullopt);
    }
}
