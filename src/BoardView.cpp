#include "BoardView.h"

#include "BoardGeometry.h"
#include "PieceModels.h"

#include <algorithm>
#include <bit>
#include <cmath>

using namespace math;
using reversi::Color;

namespace {

float Ease(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
float Approach(float value, float target, float step) { return value < target ? std::min(value + step, target) : std::max(value - step, target); }

constexpr float FadeTime = 0.45f;
constexpr float FlightTime = 0.55f;
constexpr float TurnTime = 0.36f;
constexpr float TurnLift = 0.42f;   // enough for a disc on its edge to clear the board
constexpr float WaveStep = 0.075f;  // between the discs one square further from the one placed
constexpr int StackHeight = 10;     // discs in a stack beside the board
constexpr float Thickness = 2 * pieces::HalfHeight;

constexpr int PileOf(Color color) { return color == Color::Black ? 0 : 1; }

// The level axis a disc turns about to roll away from a point.
Vec3 AxisAwayFrom(Vec3 from, Vec3 to)
{
    Vec3 direction = to - from;
    direction.y = 0;
    if (Length(direction) < 1e-4f)
        return {1, 0, 0};
    direction = Normalize(direction);
    return {direction.z, 0, -direction.x};
}

} // namespace

float BoardView::Random()
{
    m_seed = m_seed * 1664525u + 1013904223u;
    return static_cast<float>(m_seed >> 8) / static_cast<float>(1u << 24);
}

float BoardView::GroundAt(Vec3 position)
{
    return std::abs(position.x) < board::FrameOuter && std::abs(position.z) < board::FrameOuter ? 0.0f : board::TableHeight;
}

// Each player's discs stand in stacks beside the board, on the player's right: Black's on the right of the
// default view, the stack nearest the player used first.
Vec3 BoardView::PilePosition(int pile, int level)
{
    const float stack = (static_cast<float>(level / StackHeight) - 1) * 1.05f;
    const float y = board::TableHeight + static_cast<float>(level % StackHeight) * Thickness;
    if (pile == 0)
        return {5.35f, y, stack};
    return {-5.35f, y, -stack};
}

BoardView::Disc& BoardView::Add(bool whiteUp, Vec3 position, bool appear, float delay)
{
    Disc disc;
    disc.whiteUp = whiteUp;
    disc.position = position;
    disc.yaw = Random() * 2 * Pi;
    disc.seed = Random();
    if (appear) {
        disc.dissolve = 1;
        disc.dissolveDelay = delay;
    }
    m_discs.push_back(disc);
    return m_discs.back();
}

BoardView::Disc* BoardView::OnSquare(int square)
{
    for (Disc& disc : m_discs)
        if (disc.square == square)
            return &disc;
    return nullptr;
}

BoardView::Disc* BoardView::TopOfPile(int pile)
{
    Disc* top = nullptr;
    for (Disc& disc : m_discs)
        if (disc.square == -1 && disc.pile == pile && (!top || disc.level > top->level))
            top = &disc;
    return top;
}

void BoardView::Fly(Disc& disc, Vec3 to, float duration, float arc, float delay, Event landing)
{
    disc.from = disc.position;
    disc.to = to;
    disc.flightTime = 0;
    disc.flightDuration = duration;
    disc.flightDelay = delay;
    disc.arc = arc;
    disc.flying = true;
    disc.landing = landing;
}

void BoardView::TurnTo(Disc& disc, bool whiteUp, Vec3 axis, float duration, float lift, float delay)
{
    const bool ending = disc.turning ? !disc.whiteUp : disc.whiteUp;
    if (ending == whiteUp)
        return;
    if (disc.turning) {
        disc.turning = false;
        if (disc.turnDelay > 0)
            return; // a turn not yet begun is simply called off
        disc.whiteUp = !disc.whiteUp; // one under way is finished at once, and the disc turns back
    }
    disc.turning = true;
    disc.turnAxis = axis;
    disc.turnTime = 0;
    disc.turnDuration = duration;
    disc.turnDelay = delay;
    disc.turnLift = lift;
}

void BoardView::Rebuild(const reversi::Game& game, bool animate)
{
    if (animate) {
        for (Disc& disc : m_discs) {
            disc.dissolveTarget = 1;
            disc.dissolveDelay = 0;
            disc.removeWhenGone = true;
            disc.square = -2;
            disc.pile = -1;
        }
    }
    else {
        m_discs.clear();
    }

    // The discs off the board at the start, shared out between the players, and then the pile each disc
    // played came from: a player whose own pile is used up takes from the other's.
    const reversi::Position& start = game.Start();
    const int total = reversi::SquareCount - std::popcount(~start.Empty());
    m_pileCounts[0] = (total + (start.SideToMove() == Color::Black ? 1 : 0)) / 2;
    m_pileCounts[1] = total - m_pileCounts[0];
    std::array<int, reversi::SquareCount> origin{};
    origin.fill(-1);
    for (const reversi::MoveRecord& record : game.Moves()) {
        if (record.IsPass())
            continue;
        int pile = PileOf(record.side);
        if (m_pileCounts[static_cast<std::size_t>(pile)] == 0)
            pile = 1 - pile;
        --m_pileCounts[static_cast<std::size_t>(pile)];
        origin[static_cast<std::size_t>(record.square)] = pile;
    }

    const reversi::Position& position = game.Current();
    for (reversi::Square square = 0; square < reversi::SquareCount; ++square) {
        if (const auto color = position.At(square)) {
            Disc& disc = Add(*color == Color::White, board::SquareCenter(square), animate, 0.3f + 0.01f * static_cast<float>(square));
            disc.square = square;
            disc.pile = origin[static_cast<std::size_t>(square)];
        }
    }
    for (int pile = 0; pile < 2; ++pile) {
        for (int level = 0; level < m_pileCounts[static_cast<std::size_t>(pile)]; ++level) {
            Disc& disc = Add(pile == 1, PilePosition(pile, level), animate, 0.5f + 0.012f * static_cast<float>(level));
            disc.pile = pile;
            disc.level = level;
        }
    }
}

void BoardView::PlayMove(const reversi::MoveRecord& record, bool forward)
{
    if (record.IsPass())
        return;
    const Vec3 center = board::SquareCenter(record.square);
    const bool moverWhite = record.side == Color::White;
    auto distance = [&](int square) {
        return std::max(std::abs(reversi::FileOf(square) - reversi::FileOf(record.square)), std::abs(reversi::RowOf(square) - reversi::RowOf(record.square)));
    };

    if (forward) {
        // A disc from the player's pile, or the other's when that is used up, flies to the square.
        int pile = PileOf(record.side);
        if (m_pileCounts[static_cast<std::size_t>(pile)] == 0)
            pile = 1 - pile;
        Disc* disc = TopOfPile(pile);
        if (!disc)
            disc = &Add(moverWhite, PilePosition(pile, 0), false, 0);
        m_pileCounts[static_cast<std::size_t>(pile)] = std::max(0, m_pileCounts[static_cast<std::size_t>(pile)] - 1);
        disc->square = record.square;
        disc->pile = pile;
        Fly(*disc, center, FlightTime, 1.1f, 0, Event::Place);
        TurnTo(*disc, moverWhite, AxisAwayFrom(disc->position, center), FlightTime * 0.8f, 0, 0.05f);

        // Then the discs it brackets turn over, the nearest first.
        for (std::uint64_t bits = record.flipped; bits; bits &= bits - 1) {
            const int square = std::countr_zero(bits);
            if (Disc* turned = OnSquare(square)) {
                const float delay = FlightTime + WaveStep * static_cast<float>(distance(square) - 1);
                TurnTo(*turned, moverWhite, AxisAwayFrom(center, board::SquareCenter(square)), TurnTime, TurnLift, delay);
            }
        }
        return;
    }

    // Back: the turned discs turn back, the furthest first, and then the disc placed returns to its pile.
    int furthest = 0;
    for (std::uint64_t bits = record.flipped; bits; bits &= bits - 1)
        furthest = std::max(furthest, distance(std::countr_zero(bits)));
    for (std::uint64_t bits = record.flipped; bits; bits &= bits - 1) {
        const int square = std::countr_zero(bits);
        if (Disc* turned = OnSquare(square)) {
            const float delay = WaveStep * static_cast<float>(furthest - distance(square));
            TurnTo(*turned, !moverWhite, AxisAwayFrom(board::SquareCenter(square), center), TurnTime, TurnLift, delay);
        }
    }
    if (Disc* disc = OnSquare(record.square)) {
        const int pile = disc->pile >= 0 ? disc->pile : PileOf(record.side);
        disc->square = -1;
        disc->pile = pile;
        disc->level = m_pileCounts[static_cast<std::size_t>(pile)]++;
        const Vec3 place = PilePosition(pile, disc->level);
        const float delay = WaveStep * static_cast<float>(furthest) + (record.flipped ? TurnTime * 0.6f : 0.0f);
        Fly(*disc, place, FlightTime, 1.1f, delay, Event::Stack);
        TurnTo(*disc, pile == 1, AxisAwayFrom(center, place), FlightTime * 0.8f, 0, delay + 0.05f);
    }
}

void BoardView::Preview(reversi::Square square, std::uint64_t turning)
{
    for (Disc& disc : m_discs) {
        const bool leaning = square >= 0 && disc.square >= 0 && (turning & reversi::Bit(disc.square)) != 0;
        disc.leanTarget = leaning ? 1.0f : 0.0f;
        if (leaning)
            disc.leanAxis = AxisAwayFrom(board::SquareCenter(square), board::SquareCenter(disc.square));
    }
}

void BoardView::Update(float seconds, std::vector<Event>& events)
{
    for (Disc& disc : m_discs) {
        disc.lean = Approach(disc.lean, disc.leanTarget, seconds / 0.15f);
        if (disc.dissolveDelay > 0)
            disc.dissolveDelay -= seconds;
        else
            disc.dissolve = Approach(disc.dissolve, disc.dissolveTarget, seconds / FadeTime);

        if (disc.flying) {
            if (disc.flightDelay > 0) {
                disc.flightDelay -= seconds;
            }
            else {
                disc.flightTime = std::min(disc.flightTime + seconds, disc.flightDuration);
                const float t = disc.flightTime / disc.flightDuration;
                disc.position = Lerp(disc.from, disc.to, Ease(t)) + Vec3{0, disc.arc * std::sin(Pi * t), 0};
                if (t >= 1) {
                    disc.position = disc.to;
                    disc.flying = false;
                    events.push_back(disc.landing);
                }
            }
        }

        if (disc.turning) {
            if (disc.turnDelay > 0) {
                disc.turnDelay -= seconds;
            }
            else {
                disc.turnTime += seconds;
                if (disc.turnTime >= disc.turnDuration) {
                    disc.whiteUp = !disc.whiteUp;
                    disc.turning = false;
                    if (disc.turnLift > 0)
                        events.push_back(Event::Flip);
                }
            }
        }
    }
    std::erase_if(m_discs, [](const Disc& disc) { return disc.removeWhenGone && disc.dissolve >= 1; });
}

bool BoardView::Settled() const
{
    return std::ranges::none_of(m_discs, [](const Disc& disc) { return disc.flying || disc.turning || disc.dissolve != disc.dissolveTarget; });
}

bool BoardView::Animating() const
{
    return !Settled() || std::ranges::any_of(m_discs, [](const Disc& disc) { return disc.lean != disc.leanTarget; });
}

float BoardView::Lift(const Disc& disc)
{
    if (!disc.turning || disc.turnDelay > 0)
        return 0;
    return disc.turnLift * std::sin(Pi * std::min(disc.turnTime / disc.turnDuration, 1.0f));
}

std::vector<render::DiscDraw> BoardView::Draws(reversi::Square lastMove, Vec3 accent, float time) const
{
    std::vector<render::DiscDraw> draws;
    draws.reserve(m_discs.size());
    for (const Disc& disc : m_discs) {
        float angle = 0;
        if (disc.turning && disc.turnDelay <= 0)
            angle = Pi * Ease(std::min(disc.turnTime / disc.turnDuration, 1.0f));
        const float lean = Ease(disc.lean) * (0.85f + 0.15f * std::sin(time * 5.0f));
        render::DiscDraw draw;
        const Mat4 face = disc.whiteUp ? Mat4::Identity() : Mat4::RotationX(Pi);
        draw.model = Mat4::Translation(disc.position + Vec3{0, pieces::HalfHeight + Lift(disc) + 0.09f * lean, 0}) * Mat4::Rotation(disc.leanAxis, 0.28f * lean) *
                     Mat4::Rotation(disc.turnAxis, angle) * face * Mat4::RotationY(disc.yaw);
        draw.ground = GroundAt(disc.position);
        draw.dissolve = disc.dissolve;
        draw.seed = disc.seed;
        draw.glowColor = accent;
        draw.glow = 0.7f * lean;
        if (disc.square >= 0 && disc.square == lastMove && !disc.flying)
            draw.mark = 1;
        draws.push_back(draw);
    }
    return draws;
}

std::vector<render::ContactShadow> BoardView::Contacts() const
{
    std::vector<render::ContactShadow> contacts;
    for (const Disc& disc : m_discs) {
        // In a stack only the lowest disc touches the table.
        if (disc.square == -1 && !disc.flying && disc.level % StackHeight != 0)
            continue;
        const float height = disc.position.y - GroundAt(disc.position) + Lift(disc) + 0.09f * disc.lean;
        const float strength = 0.72f * (1 - SmoothStep(0.0f, 0.8f, height)) * (1 - disc.dissolve);
        if (strength > 0.01f)
            contacts.push_back({disc.position.x, disc.position.z, pieces::Radius * (1.05f + height * 0.6f), strength});
    }
    return contacts;
}
