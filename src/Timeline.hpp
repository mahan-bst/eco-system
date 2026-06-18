#pragma once

#include <SFML/Graphics.hpp>
#include <iosfwd>
#include <string>
#include <vector>

class Simulation;

// Records the whole evolution so you can scrub back and forward in time.
//
// While live, it snapshots the *entire* world state (reusing Simulation's
// serializer) into an in-memory ring at a fixed sim-time interval; when the
// ring fills it halves resolution (keeps every other snapshot, doubles the
// interval) so the full run always fits in bounded memory.
//
// Dragging the timeline bar enters non-destructive REVIEW: the live present is
// backed up, and scrubbing restores past snapshots into the sim for viewing
// (charts, brains, lineage colours, leaderboard all follow automatically).
// Leaving review restores the present; "branch" instead adopts the viewed
// moment as the new present and continues from there.
class Timeline
{
public:
    void reset();                          // wipe history (restart)
    void record(const Simulation& sim);    // call after each live sim step

    // the recorded history is saved inside the .eco file, after the world
    void serialize(std::ostream& out) const;
    bool deserialize(std::istream& in);    // leaves the timeline empty on failure

    bool reviewing() const { return m_reviewing; }
    int  count() const { return int(m_snaps.size()); }

    void setBarRect(const sf::FloatRect& r) { m_bar = r; }
    void draw(sf::RenderTarget& rt, const sf::Font* font, float liveTime) const;

    // mouse in window coords; onPress returns true if it grabbed the bar
    bool onPress(sf::Vector2f p, Simulation& sim);
    void onDrag(sf::Vector2f p, Simulation& sim);
    void onRelease() { m_dragging = false; }

    void stepSnapshot(Simulation& sim, int dir);      // <- / -> fine scrubbing
    void returnToPresent(Simulation& sim);            // exit review at "now"
    float branch(Simulation& sim);                    // adopt viewed moment; returns its time

    float viewedTime() const;

private:
    struct Snap { float t = 0.f; std::string data; };

    void enterReview(const Simulation& sim);
    void restoreIndex(Simulation& sim, int i);
    int  indexAtFraction(float f) const;
    float handleFraction() const;
    void thin();

    std::vector<Snap> m_snaps;
    std::string m_present;            // exact live state, backed up during review
    float m_interval = 1.0f;          // sim-seconds between snapshots
    bool  m_reviewing = false;
    bool  m_dragging  = false;
    int   m_viewIndex = 0;
    int   m_loadedIndex = -1;         // snapshot currently in the sim (drag dedup)
    sf::FloatRect m_bar;

    static constexpr int MAX_SNAPS = 220;
};
