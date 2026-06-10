#pragma once

#include <SFML/Graphics.hpp>
#include <cstdint>
#include <vector>

class Simulation;

// Toggleable (L) panel with two tabs:
//   Alive        - the longest-living creatures right now; click to follow.
//   Hall of Fame - the best brains ever recorded (champion archive);
//                  click to spawn a live copy of that legend.
class Leaderboard
{
public:
    bool visible = false;

    enum class Mode { Alive, HallOfFame };

    // what a click on the panel asks the app to do
    struct Click
    {
        enum Type { None, Follow, Spawn } type = None;
        std::uint64_t id = 0;       // Follow: which live creature
        bool isPred = false;
        int  champIndex = -1;       // Spawn: index into that species' archive
    };

    void setPosition(sf::Vector2f pos) { m_pos = pos; }

    void markDirty() { m_sinceRebuild = REBUILD_EVERY; }   // force a rebuild
    void update(const Simulation& sim);   // rebuild rows for the active tab
    void draw(sf::RenderTarget& rt, const sf::Font* font,
              std::uint64_t selectedId) const;

    bool contains(sf::Vector2f p) const;             // hit-test only
    bool onMousePressed(sf::Vector2f p, Click& out); // switch tab / pick a row

private:
    struct Entry
    {
        std::uint64_t id = 0;
        float age = 0.f;
        int   offspring = 0;
        bool  isPred = false;
        int   champIndex = -1;      // >= 0 only in Hall of Fame mode
        float rowY = 0.f;           // window-space y, filled by update()
    };

    float contentTop() const;
    sf::FloatRect tabRowRect() const;

    static constexpr int   TOP_N = 5;
    static constexpr float WIDTH = 252.f;
    static constexpr int   REBUILD_EVERY = 8;   // rebuild ~7x/sec, not 60x

    std::vector<Entry> m_rows;      // prey block first, then predators
    int  m_preyCount = 0;
    Mode m_mode = Mode::Alive;
    sf::Vector2f m_pos;
    float m_height = 0.f;
    int  m_sinceRebuild = REBUILD_EVERY;   // frames since last rebuild
};
