#pragma once

#include "Reversi.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <vector>

namespace reversi {

    struct SearchLimits {
        int maxDepth = 60;
        int milliseconds = 1000; // iterative deepening stops when this runs out
        int exactEmpties = 0;    // from this many empty squares the game is played out to the end, exactly
        int randomMargin = 0;    // a random move among those this close to the best, for the weaker levels
        std::uint64_t seed = 0;
    };

    struct SearchResult {
        Square best = Pass;
        int score = 0;       // for the side to move: the evaluation, or the final difference in discs when solved
        int depth = 0;
        bool solved = false; // the score is exact
        std::uint64_t nodes = 0;
    };

    // An alpha-beta searcher: iterative deepening, principal variation search and a transposition table in the
    // middle game, and an exact solver for the end, which orders its moves to leave the opponent the fewest replies.
    class Engine {
    public:
        explicit Engine(int hashMegabytes = 32);

        SearchResult Search(const Position& root, const SearchLimits& limits, std::stop_token stop);
        void Clear();

        // Corners, the squares next to them, mobility, frontier, stable discs and position, for the side to move.
        static int Evaluate(std::uint64_t player, std::uint64_t opponent);

        static constexpr int Win = 30000; // a won game, and then the discs it is won by
        static bool IsWinScore(int score) { return score > Win - 100 || score < -Win + 100; }

    private:
        struct Entry {
            std::uint64_t key = 0;
            std::int32_t score = 0;
            std::int8_t depth = 0;
            std::uint8_t bound = 0;
            std::int8_t move = -1;
            std::uint8_t solved = 0; // the score is the final difference in discs, not an evaluation
        };

        struct RootMove {
            Square square = Pass;
            int score = 0;
        };

        int Negamax(std::uint64_t player, std::uint64_t opponent, int depth, int alpha, int beta, bool passed);
        int Solve(std::uint64_t player, std::uint64_t opponent, int alpha, int beta, int empties, bool passed);
        int SolveSmall(std::uint64_t player, std::uint64_t opponent, int alpha, int beta, int empties, bool passed);
        bool SearchMiddle(const Position& root, std::vector<RootMove>& moves, const SearchLimits& limits, SearchResult& result,
            std::chrono::steady_clock::time_point start, std::chrono::milliseconds budget);
        bool SolveRoot(const Position& root, std::vector<RootMove>& moves, bool exactScores, SearchResult& result);
        bool ShouldStop();
        Entry& Slot(std::uint64_t key) { return m_table[key & (m_table.size() - 1)]; }
        void Store(std::uint64_t key, int depth, int score, int bound, int move, bool solved);

        std::vector<Entry> m_table;
        std::uint64_t m_nodes = 0;
        std::chrono::steady_clock::time_point m_deadline;
        std::stop_token m_stop;
        bool m_aborted = false;
    };

    // Runs an engine on a thread of its own; the window polls for the result.
    class AiPlayer {
    public:
        explicit AiPlayer(int hashMegabytes = 32) : m_engine(hashMegabytes) {}
        ~AiPlayer() { Cancel(); }
        AiPlayer(const AiPlayer&) = delete;
        AiPlayer& operator=(const AiPlayer&) = delete;

        void Start(const Position& position, const SearchLimits& limits);
        void Cancel();
        void NewGame();
        bool Busy() const { return m_busy; }
        std::optional<SearchResult> TakeResult();

    private:
        Engine m_engine;
        std::jthread m_worker;
        std::mutex m_mutex;
        std::optional<SearchResult> m_result;
        std::atomic<bool> m_busy = false;
    };
}
