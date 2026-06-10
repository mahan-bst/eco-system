#pragma once

#include <SFML/Graphics.hpp>

#include "Entities.hpp"

// Overlay that visualises the selected creature's neural network live:
// node colours show current activations, connection colours/intensity show
// the signal flowing through each weight this very tick.
namespace BrainView
{
    constexpr float WIDTH  = 440.f;
    constexpr float HEIGHT = 336.f;

    inline sf::FloatRect bounds(sf::Vector2f pos)
    {
        return { pos, { WIDTH, HEIGHT } };
    }

    void draw(sf::RenderTarget& rt, sf::Vector2f pos, const Animal& a,
              bool isPredator, const sf::Font* font);
}
