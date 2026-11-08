#pragma once

#include "MathTypes.h"
#include "Renderer.h"
#include "Reversi.h"

#include <array>
#include <cstdint>
#include <vector>

// The discs as they lie and move: flying from the piles beside the board to their squares, turning over in a
// wave that runs out from the disc placed, flying back when a move is taken back, and dissolving in when a
// game is set out.
class BoardView {
public:
    // What happened in a frame, for the sounds.
    enum class Event { Place, Flip, Stack };

    // Sets out the game's position, and beside the board the discs not yet played.
    void Rebuild(const reversi::Game& game, bool animate);
    // A move played (forward) or taken back.
    void PlayMove(const reversi::MoveRecord& record, bool forward);
    // The discs that a disc placed on `square` would turn over lean away from it, about to go, and glow.
    void Preview(reversi::Square square, std::uint64_t turning);
    void Update(float seconds, std::vector<Event>& events);
    // Nothing flying, turning or dissolving: the position shown is the game's.
    bool Settled() const;
    bool Animating() const;

    std::vector<render::DiscDraw> Draws(reversi::Square lastMove, math::Vec3 accent, float time) const;
    std::vector<render::ContactShadow> Contacts() const;

private:
    struct Disc {
        int square = -1;      // on the board; -1 in a pile; -2 leaving
        int pile = -1;        // in a pile, Black's (0) or White's (1); on the board, the pile it came from
        int level = 0;        // its place in the pile, counted from the bottom
        bool whiteUp = false; // the face up when it lies still
        float yaw = 0;        // each disc turned its own way, so that the grain differs
        float seed = 0;
        math::Vec3 position;  // the middle of its underside
        // A flight from one place to another, in an arc.
        math::Vec3 from, to;
        float flightTime = 0, flightDuration = 0, flightDelay = 0, arc = 0;
        bool flying = false;
        Event landing = Event::Place;
        // Turning over about a level axis, lifted off the board for the turn.
        math::Vec3 turnAxis{1, 0, 0};
        float turnTime = 0, turnDuration = 0, turnDelay = 0, turnLift = 0;
        bool turning = false;
        float dissolve = 0, dissolveTarget = 0, dissolveDelay = 0;
        bool removeWhenGone = false;
        // Leaning in the preview of a move that would turn it.
        math::Vec3 leanAxis{1, 0, 0};
        float lean = 0, leanTarget = 0;
    };

    Disc& Add(bool whiteUp, math::Vec3 position, bool appear, float delay);
    Disc* OnSquare(int square);
    Disc* TopOfPile(int pile);
    static math::Vec3 PilePosition(int pile, int level);
    static void Fly(Disc& disc, math::Vec3 to, float duration, float arc, float delay, Event landing);
    // Turns a disc to show a face, unless it shows it already or is about to.
    static void TurnTo(Disc& disc, bool whiteUp, math::Vec3 axis, float duration, float lift, float delay);
    static float GroundAt(math::Vec3 position);
    static float Lift(const Disc& disc);
    float Random();

    std::vector<Disc> m_discs;
    std::array<int, 2> m_pileCounts{};
    std::uint32_t m_seed = 1;
};
