#pragma once

#include <cstdint>
#include <iosfwd>
#include <vector>

#include "Entities.hpp"
#include "Grid.hpp"

namespace cfg { struct SpeciesCfg; }

class Simulation
{
public:
    struct Averages { float energy = 0.f, age = 0.f; };

    Simulation();

    void reset();
    void step(float dt);
    void spawnFoodBurst(sf::Vector2f pos, int count);   // demo helper (mouse)

    // full world snapshot: time, food, every animal incl. its brain weights,
    // and the live tunables. deserialize leaves the sim untouched on failure.
    void serialize(std::ostream& out) const;
    bool deserialize(std::istream& in);

    float time() const { return m_time; }
    const std::vector<Food>&   foodList() const { return m_food; }
    const std::vector<Animal>& preyList() const { return m_prey; }
    const std::vector<Animal>& predList() const { return m_pred; }

    static Averages average(const std::vector<Animal>& animals);

private:
    void spawnFood(float dt);
    void rebuildGrids();
    void updatePrey(float dt, std::vector<Animal>& babies);
    void updatePredators(float dt, std::vector<Animal>& babies);

    // apply the brain's outputs: steer, move, clamp to the world, pay energy
    void act(Animal& a, const cfg::SpeciesCfg& s, float dt);

    void tryReproduce(Animal& a, const cfg::SpeciesCfg& s,
                      std::vector<Animal>& babies,
                      std::size_t population, std::size_t cap);

    Animal spawnAnimal(const cfg::SpeciesCfg& s);
    void topUp(std::vector<Animal>& pop, const cfg::SpeciesCfg& s, int floor);
    void compact(std::vector<Animal>& babyPrey, std::vector<Animal>& babyPred);

    std::vector<Food>   m_food;
    std::vector<Animal> m_prey;
    std::vector<Animal> m_pred;

    SpatialGrid m_foodGrid, m_preyGrid, m_predGrid;

    float m_time = 0.f;
    float m_foodAccum = 0.f;
    std::uint64_t m_nextId = 1;
};
