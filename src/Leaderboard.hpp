#pragma once

#include <SFML/Graphics.hpp>
#include <cstdint>
#include <vector>

class Simulation;

// Toggleable (L) hall of fame: the longest-living creatures that are alive
// right now. Clicking a row selects that creature and engages the follow cam.
class Leaderboard
{
public:
    bool visible = false;

    void setPosition(sf::Vector2f pos) { m_pos = pos; }

    // rebuild the rankings; call once per frame while visible
    void update(const Simulation& sim);

    void draw(sf::RenderTarget& rt, const sf::Font* font,
              std::uint64_t selectedId) const;

    // window coords; returns true if the click hit the panel,
    // filling id/isPred when an actual row was clicked
    bool onMousePressed(sf::Vector2f p, std::uint64_t& id, bool& isPred) const;

private:
    struct Entry
    {
        std::uint64_t id = 0;
        float age = 0.f;
        float energy = 0.f;
        bool isPred = false;
        float rowY = 0.f;       // window-space y of the row, set by update()
    };

    static constexpr int   TOP_N = 5;
    static constexpr float WIDTH = 252.f;

    std::vector<Entry> m_rows;  // prey block first, then predators
    int m_preyCount = 0;        // how many of m_rows are prey
    sf::Vector2f m_pos;
    float m_height = 0.f;
};
