#pragma once

#include <SFML/System/Vector2.hpp>
#include <cmath>
#include <vector>

// Uniform spatial hash used for "what is near me?" queries.
// Rebuilt every tick; keeps the simulation fast even with ~2000 agents
// at 32x time speed.
class SpatialGrid
{
public:
    void configure(float cellSize, float worldW, float worldH)
    {
        m_cell = cellSize;
        m_cols = std::max(1, int(std::ceil(worldW / cellSize)));
        m_rows = std::max(1, int(std::ceil(worldH / cellSize)));
        m_cells.assign(std::size_t(m_cols) * m_rows, {});
    }

    void clear()
    {
        for (auto& c : m_cells) c.clear();   // keeps allocations
    }

    void insert(sf::Vector2f p, int idx)
    {
        m_cells[cellIndex(p)].push_back(idx);
    }

    // Calls fn(index) for every item whose cell intersects the circle (p, r).
    // The caller still has to do the exact distance check.
    template <class F>
    void query(sf::Vector2f p, float r, F&& fn) const
    {
        const int x0 = clampi(int((p.x - r) / m_cell), 0, m_cols - 1);
        const int x1 = clampi(int((p.x + r) / m_cell), 0, m_cols - 1);
        const int y0 = clampi(int((p.y - r) / m_cell), 0, m_rows - 1);
        const int y1 = clampi(int((p.y + r) / m_cell), 0, m_rows - 1);
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
                for (int idx : m_cells[std::size_t(y) * m_cols + x])
                    fn(idx);
    }

private:
    static int clampi(int v, int lo, int hi)
    {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    std::size_t cellIndex(sf::Vector2f p) const
    {
        const int cx = clampi(int(p.x / m_cell), 0, m_cols - 1);
        const int cy = clampi(int(p.y / m_cell), 0, m_rows - 1);
        return std::size_t(cy) * m_cols + cx;
    }

    float m_cell = 64.f;
    int m_cols = 1, m_rows = 1;
    std::vector<std::vector<int>> m_cells;
};
