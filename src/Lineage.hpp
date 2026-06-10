#pragma once

#include <SFML/Graphics/Color.hpp>

#include <cmath>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

#include "Brain.hpp"

// Colour a creature by the *similarity of its brain* to others, so lineages
// show up as visibly distinct tribes and a brain "taking over" the gene pool
// repaints the world in its colour.
//
// Method: project the high-dimensional weight vector onto two fixed random
// directions (a locality-sensitive hash). Nearby weight vectors land at
// nearby (x, y), and atan2(y, x) turns that into a hue that wraps naturally.
// Because offspring are near-copies of their parent, a lineage clusters
// tightly in hue; an unrelated lineage sits elsewhere on the wheel.
namespace lineage
{
    inline const std::pair<std::vector<float>, std::vector<float>>& projection()
    {
        static const auto p = []
        {
            std::mt19937 gen(0xC0FFEEu);          // fixed seed: stable hues
            std::normal_distribution<float> nd(0.f, 1.f);
            const std::size_t n = std::size_t(Brain::paramCount());
            std::vector<float> a(n), b(n);
            for (std::size_t i = 0; i < n; ++i) { a[i] = nd(gen); b[i] = nd(gen); }
            return std::make_pair(std::move(a), std::move(b));
        }();
        return p;
    }

    inline float hue(const Brain& brain)
    {
        const auto& [ax, by] = projection();
        const auto& w = brain.weights();
        float x = 0.f, y = 0.f;
        for (std::size_t i = 0; i < w.size(); ++i) { x += w[i] * ax[i]; y += w[i] * by[i]; }
        const float h = std::atan2(y, x) / 6.2831853f;   // -0.5 .. 0.5
        return h < 0.f ? h + 1.f : h;                     //  0 .. 1
    }

    inline sf::Color hsv(float h, float s, float v)
    {
        h = (h - std::floor(h)) * 6.f;
        const int   i = int(h);
        const float f = h - float(i);
        const float p = v * (1.f - s);
        const float q = v * (1.f - s * f);
        const float t = v * (1.f - s * (1.f - f));
        float r = v, g = t, b = p;
        switch (i % 6)
        {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            default: r = v; g = p; b = q; break;
        }
        return sf::Color(std::uint8_t(r * 255), std::uint8_t(g * 255),
                         std::uint8_t(b * 255));
    }

    // brightness still carries energy, so "dim = starving" reads the same
    inline sf::Color color(const Brain& brain, float energyFrac)
    {
        return hsv(hue(brain), 0.72f, 0.40f + 0.60f * energyFrac);
    }
}
