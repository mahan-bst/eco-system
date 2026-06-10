#pragma once

#include <SFML/System/Vector2.hpp>
#include <cstdint>

#include "Brain.hpp"

struct Food
{
    sf::Vector2f pos;
    bool alive = true;
};

// One creature. Bodies are identical within a species — the only thing that
// is inherited and evolves is the neural network.
struct Animal
{
    std::uint64_t id = 0;     // stable identity (used by the selection UI)
    sf::Vector2f pos;
    sf::Vector2f vel;         // last tick's velocity (sensed by other animals)
    float heading = 0.f;      // radians
    float energy  = 0.f;
    float age     = 0.f;      // seconds alive
    Brain brain;
    bool alive = true;
};
