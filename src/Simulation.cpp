#include "Simulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <istream>
#include <ostream>

#include "Config.hpp"
#include "Utils.hpp"

// ---------------------------------------------------------------------------
//  sensing helpers
// ---------------------------------------------------------------------------

namespace
{
    // Writes a 3-value sensor channel (signal, sin, cos) for a seen object.
    // Angles are relative to the animal's heading, so the brain's world is
    // always "forward is forward".
    void writeChannel(float* in, const Animal& a, sf::Vector2f target,
                      float dist, float vision)
    {
        const sf::Vector2f d = target - a.pos;
        const float rel = wrapAngle(std::atan2(d.y, d.x) - a.heading);
        in[0] = 1.f - dist / vision;       // closer = stronger
        in[1] = std::sin(rel);
        in[2] = std::cos(rel);
    }

    // Inputs 10/11: how the key moving agent (the predator for prey, the prey
    // for predators) moves relative to us, expressed in our own frame as
    // forward / lateral components. This is what makes interception and
    // dodging learnable instead of pure tail-chasing.
    void writeVelocity(float* in, const Animal& a, const Animal& other)
    {
        const sf::Vector2f rv = other.vel - a.vel;
        const sf::Vector2f fwd(std::cos(a.heading), std::sin(a.heading));
        const sf::Vector2f side(-fwd.y, fwd.x);
        in[10] = clampf((rv.x * fwd.x + rv.y * fwd.y) / cfg::VEL_NORM, -1.f, 1.f);
        in[11] = clampf((rv.x * side.x + rv.y * side.y) / cfg::VEL_NORM, -1.f, 1.f);
    }

    void writeCommonSenses(float* in, const Animal& a, const cfg::SpeciesCfg& s)
    {
        in[6] = clampf(a.energy / s.capacity, 0.f, 1.f);

        const float dWall = std::min(std::min(a.pos.x, cfg::WORLD_W - a.pos.x),
                                     std::min(a.pos.y, cfg::WORLD_H - a.pos.y));
        in[7] = clampf(1.f - dWall / 120.f, 0.f, 1.f);

        const sf::Vector2f toCentre =
            sf::Vector2f(cfg::WORLD_W / 2.f, cfg::WORLD_H / 2.f) - a.pos;
        const float rel = wrapAngle(std::atan2(toCentre.y, toCentre.x) - a.heading);
        in[8] = std::sin(rel);
        in[9] = std::cos(rel);
    }
}

// ---------------------------------------------------------------------------
//  setup
// ---------------------------------------------------------------------------

Simulation::Simulation()
{
    m_foodGrid.configure(64.f, cfg::WORLD_W, cfg::WORLD_H);
    m_preyGrid.configure(64.f, cfg::WORLD_W, cfg::WORLD_H);
    m_predGrid.configure(64.f, cfg::WORLD_W, cfg::WORLD_H);
    reset();
}

Animal Simulation::spawnAnimal(const cfg::SpeciesCfg& s)
{
    Animal a;
    a.id      = m_nextId++;
    a.pos     = { frand(20.f, cfg::WORLD_W - 20.f), frand(20.f, cfg::WORLD_H - 20.f) };
    a.heading = frand(-PI, PI);
    a.energy  = 0.7f * s.capacity;
    return a;   // a.brain default-constructs with random weights
}

void Simulation::reset()
{
    m_time = 0.f;
    m_foodAccum = 0.f;

    m_preyHof.clear();
    m_predHof.clear();

    m_food.clear();
    m_food.reserve(cfg::FOOD_MAX);
    for (int i = 0; i < cfg::FOOD_START; ++i)
        m_food.push_back({ { frand(8.f, cfg::WORLD_W - 8.f),
                             frand(8.f, cfg::WORLD_H - 8.f) } });

    m_prey.clear();
    m_prey.reserve(cfg::PREY_MAX);
    for (int i = 0; i < cfg::PREY_START; ++i)
        m_prey.push_back(spawnAnimal(cfg::PREY));

    m_pred.clear();
    m_pred.reserve(cfg::PRED_MAX);
    for (int i = 0; i < cfg::PRED_START; ++i)
        m_pred.push_back(spawnAnimal(cfg::PRED));
}

// ---------------------------------------------------------------------------
//  main tick
// ---------------------------------------------------------------------------

void Simulation::step(float dt)
{
    m_time += dt;
    spawnFood(dt);
    rebuildGrids();

    std::vector<Animal> babyPrey, babyPred;
    updatePrey(dt, babyPrey);
    updatePredators(dt, babyPred);
    compact(babyPrey, babyPred);
}

void Simulation::spawnFood(float dt)
{
    m_foodAccum += cfg::tune.foodPerSec * dt;
    while (m_foodAccum >= 1.f)
    {
        m_foodAccum -= 1.f;
        if (int(m_food.size()) < cfg::FOOD_MAX)
            m_food.push_back({ { frand(8.f, cfg::WORLD_W - 8.f),
                                 frand(8.f, cfg::WORLD_H - 8.f) } });
    }
}

void Simulation::spawnFoodBurst(sf::Vector2f pos, int count)
{
    for (int i = 0; i < count && int(m_food.size()) < cfg::FOOD_MAX; ++i)
    {
        const float ang = frand(-PI, PI);
        const float rad = frand(0.f, 45.f);
        sf::Vector2f p = pos + sf::Vector2f(std::cos(ang) * rad, std::sin(ang) * rad);
        p.x = clampf(p.x, 4.f, cfg::WORLD_W - 4.f);
        p.y = clampf(p.y, 4.f, cfg::WORLD_H - 4.f);
        m_food.push_back({ p });
    }
}

void Simulation::rebuildGrids()
{
    m_foodGrid.clear();
    for (int i = 0; i < int(m_food.size()); ++i)
        if (m_food[i].alive) m_foodGrid.insert(m_food[i].pos, i);

    m_preyGrid.clear();
    for (int i = 0; i < int(m_prey.size()); ++i)
        if (m_prey[i].alive) m_preyGrid.insert(m_prey[i].pos, i);

    m_predGrid.clear();
    for (int i = 0; i < int(m_pred.size()); ++i)
        if (m_pred[i].alive) m_predGrid.insert(m_pred[i].pos, i);
}

// ---------------------------------------------------------------------------
//  prey: sense food + nearest predator, think, move, graze, reproduce
// ---------------------------------------------------------------------------

void Simulation::updatePrey(float dt, std::vector<Animal>& babies)
{
    const auto& S = cfg::PREY;
    const float vis = cfg::tune.preyVision;   // slider-controlled sense radius

    for (auto& a : m_prey)
    {
        if (!a.alive) continue;
        a.age += dt;

        float in[Brain::IN] = {};

        // channel A: nearest food
        {
            float bestD2 = vis * vis;
            int best = -1;
            m_foodGrid.query(a.pos, vis, [&](int j)
            {
                if (!m_food[j].alive) return;
                const float d2 = dist2(a.pos, m_food[j].pos);
                if (d2 < bestD2) { bestD2 = d2; best = j; }
            });
            if (best >= 0)
                writeChannel(in, a, m_food[best].pos, std::sqrt(bestD2), vis);
        }

        // channel B: nearest predator
        {
            float bestD2 = vis * vis;
            const Animal* threat = nullptr;
            m_predGrid.query(a.pos, vis, [&](int j)
            {
                const Animal& p = m_pred[j];
                if (!p.alive) return;
                const float d2 = dist2(a.pos, p.pos);
                if (d2 < bestD2) { bestD2 = d2; threat = &p; }
            });
            if (threat)
            {
                writeChannel(in + 3, a, threat->pos, std::sqrt(bestD2), vis);
                writeVelocity(in, a, *threat);   // see the predator's movement
            }
        }

        writeCommonSenses(in, a, S);
        a.brain.think(in);
        act(a, S, dt);

        // graze: eating is automatic on contact, the brain only has to get here
        const float reach = S.size + cfg::FOOD_RADIUS + 1.5f;
        m_foodGrid.query(a.pos, reach, [&](int j)
        {
            Food& f = m_food[j];
            if (!f.alive || dist2(a.pos, f.pos) > reach * reach) return;
            f.alive  = false;
            a.energy = std::min(a.energy + cfg::tune.foodEnergy, S.capacity);
        });

        tryReproduce(a, S, babies, m_prey.size(), cfg::PREY_MAX);

        if (a.energy <= 0.f) a.alive = false;   // starved
    }
}

// ---------------------------------------------------------------------------
//  predators: sense nearest prey + nearest rival, think, move, catch, reproduce
// ---------------------------------------------------------------------------

void Simulation::updatePredators(float dt, std::vector<Animal>& babies)
{
    const auto& S = cfg::PRED;
    const float vis = cfg::tune.predVision;   // slider-controlled sense radius

    for (auto& a : m_pred)
    {
        if (!a.alive) continue;
        a.age += dt;

        float in[Brain::IN] = {};

        // channel A: nearest prey (this is dinner)
        int targetIdx = -1;
        {
            float bestD2 = vis * vis;
            m_preyGrid.query(a.pos, vis, [&](int j)
            {
                if (!m_prey[j].alive) return;
                const float d2 = dist2(a.pos, m_prey[j].pos);
                if (d2 < bestD2) { bestD2 = d2; targetIdx = j; }
            });
            if (targetIdx >= 0)
            {
                writeChannel(in, a, m_prey[targetIdx].pos, std::sqrt(bestD2), vis);
                writeVelocity(in, a, m_prey[targetIdx]);   // lead the target
            }
        }

        // channel B: nearest rival predator (competition awareness)
        {
            float bestD2 = vis * vis;
            const Animal* rival = nullptr;
            m_predGrid.query(a.pos, vis, [&](int j)
            {
                const Animal& p = m_pred[j];
                if (!p.alive || p.id == a.id) return;
                const float d2 = dist2(a.pos, p.pos);
                if (d2 < bestD2) { bestD2 = d2; rival = &p; }
            });
            if (rival)
                writeChannel(in + 3, a, rival->pos, std::sqrt(bestD2), vis);
        }

        writeCommonSenses(in, a, S);
        a.brain.think(in);
        act(a, S, dt);

        // catch: contact with the prey we were tracking
        if (targetIdx >= 0)
        {
            Animal& q = m_prey[targetIdx];
            const float r = S.size + cfg::PREY.size;
            if (q.alive && dist2(a.pos, q.pos) <= r * r)
            {
                q.alive = false;
                const float gain = (cfg::PRED_EAT_BASE +
                                    cfg::PRED_EAT_FRAC * std::max(q.energy, 0.f)) *
                                   cfg::tune.predGainMul;
                a.energy = std::min(a.energy + gain, S.capacity);
            }
        }

        tryReproduce(a, S, babies, m_pred.size(), cfg::PRED_MAX);

        if (a.energy <= 0.f) a.alive = false;   // starved
    }
}

// ---------------------------------------------------------------------------
//  shared mechanics
// ---------------------------------------------------------------------------

void Simulation::act(Animal& a, const cfg::SpeciesCfg& s, float dt)
{
    const float throttle = a.brain.throttle();

    a.heading = wrapAngle(a.heading + a.brain.turn() * s.turnRate * dt);

    const float spd = throttle * cfg::maxSpeedOf(s);
    const sf::Vector2f dir(std::cos(a.heading), std::sin(a.heading));
    a.vel  = dir * spd;
    a.pos += dir * (spd * dt);
    a.pos.x = clampf(a.pos.x, s.size, cfg::WORLD_W - s.size);
    a.pos.y = clampf(a.pos.y, s.size, cfg::WORLD_H - s.size);

    a.energy -= cfg::metabolicCost(s, throttle) * dt;
}

void Simulation::tryReproduce(Animal& a, const cfg::SpeciesCfg& s,
                              std::vector<Animal>& babies,
                              std::size_t population, std::size_t cap)
{
    if (population + babies.size() >= cap) return;
    if (a.energy < s.reproFrac * s.capacity) return;

    Animal child;
    child.id  = m_nextId++;
    child.pos = a.pos + sf::Vector2f(frand(-12.f, 12.f), frand(-12.f, 12.f));
    child.pos.x   = clampf(child.pos.x, 4.f, cfg::WORLD_W - 4.f);
    child.pos.y   = clampf(child.pos.y, 4.f, cfg::WORLD_H - 4.f);
    child.heading = frand(-PI, PI);
    child.brain   = a.brain.offspring();   // <- this is where evolution happens

    // the energy split: child gets a share, parent keeps a share,
    // the remainder is the cost of giving birth
    const float e = a.energy;
    child.energy  = std::min(s.childFrac * e, s.capacity);
    a.energy      = s.parentKeep * e;

    babies.push_back(child);

    // reproduction is the fitness currency: count it and offer the proven
    // parent to the hall of fame
    ++a.offspring;
    hofFor(s).consider(a.brain, a.id, a.offspring, a.age);
}

ChampionArchive& Simulation::hofFor(const cfg::SpeciesCfg& s)
{
    return (&s == &cfg::PRED) ? m_predHof : m_preyHof;
}

void Simulation::topUp(std::vector<Animal>& pop, const cfg::SpeciesCfg& s,
                       int floor, const ChampionArchive& archive)
{
    const std::size_t survivors = pop.size();   // pick parents from these only
    while (int(pop.size()) < floor)
    {
        Animal a = spawnAnimal(s);
        // Reseed priority: after a wipe, recover from the hall of fame so the
        // species comes back competent instead of random; while merely thin,
        // mix proven champions with live survivors to keep diversity.
        if (!archive.empty() && (survivors == 0 || frand(0.f, 1.f) < 0.5f))
            a.brain = archive.reseed();
        else if (survivors > 0)
            a.brain = pop[std::size_t(frand(0.f, float(survivors) - 0.001f))]
                          .brain.offspring();
        // else: a.brain keeps its fresh random weights (true cold start)
        pop.push_back(a);
    }
}

void Simulation::compact(std::vector<Animal>& babyPrey, std::vector<Animal>& babyPred)
{
    const auto deadAnimal = [](const Animal& a) { return !a.alive; };
    const auto deadFood   = [](const Food& f)   { return !f.alive; };

    // a creature's final lifetime stats are its truest fitness — capture the
    // dead before they are swept away
    for (const auto& a : m_prey)
        if (!a.alive) m_preyHof.consider(a.brain, a.id, a.offspring, a.age);
    for (const auto& a : m_pred)
        if (!a.alive) m_predHof.consider(a.brain, a.id, a.offspring, a.age);

    m_food.erase(std::remove_if(m_food.begin(), m_food.end(), deadFood),   m_food.end());
    m_prey.erase(std::remove_if(m_prey.begin(), m_prey.end(), deadAnimal), m_prey.end());
    m_pred.erase(std::remove_if(m_pred.begin(), m_pred.end(), deadAnimal), m_pred.end());

    m_prey.insert(m_prey.end(), babyPrey.begin(), babyPrey.end());
    m_pred.insert(m_pred.end(), babyPred.begin(), babyPred.end());

    topUp(m_prey, cfg::PREY, cfg::PREY_FLOOR, m_preyHof);
    topUp(m_pred, cfg::PRED, cfg::PRED_FLOOR, m_predHof);
}

std::uint64_t Simulation::spawnChampion(bool predator, int index)
{
    const auto& s        = predator ? cfg::PRED : cfg::PREY;
    auto&       pop      = predator ? m_pred : m_prey;
    const auto& archive  = predator ? m_predHof : m_preyHof;
    const int   cap      = predator ? cfg::PRED_MAX : cfg::PREY_MAX;

    if (index < 0 || index >= archive.size() || int(pop.size()) >= cap)
        return 0;

    Animal a = spawnAnimal(s);
    a.brain  = archive.at(index).brain;   // an exact copy of the legend
    a.brain.setMemory(0.f, 0.f);
    a.energy = 0.9f * s.capacity;          // arrive in fine form
    pop.push_back(a);
    return a.id;
}

// ---------------------------------------------------------------------------
//  save / load
// ---------------------------------------------------------------------------

namespace
{
    // version history (the last byte of the magic):
    //   2 - first brain-era format
    //   3 - adds the vision/speed tunables; older files load with defaults
    //   4 - animal records gain velocity + brain memory state
    //       (v2/v3 brains had a different topology, so the paramCount check
    //        rejects them anyway)
    //   5 - per-animal offspring count + the two champion archives;
    //       v4 files load fine (offspring defaults to 0, archives start empty)
    constexpr char SAVE_MAGIC[8] = { 'E','C','O','S','I','M','0','5' };
    constexpr int  SAVE_VERSION_MIN = 2;
    constexpr int  SAVE_VERSION_MAX = 5;

    template <class T>
    void wr(std::ostream& out, const T& v)
    {
        out.write(reinterpret_cast<const char*>(&v), sizeof v);
    }

    template <class T>
    bool rd(std::istream& in, T& v)
    {
        in.read(reinterpret_cast<char*>(&v), sizeof v);
        return bool(in);
    }

    void writeAnimal(std::ostream& out, const Animal& a)
    {
        wr(out, a.id);
        wr(out, a.pos.x); wr(out, a.pos.y);
        wr(out, a.vel.x); wr(out, a.vel.y);
        wr(out, a.heading);
        wr(out, a.energy);
        wr(out, a.age);
        wr(out, std::int32_t(a.offspring));
        wr(out, a.brain.memory()[0]);
        wr(out, a.brain.memory()[1]);
        for (float w : a.brain.weights()) wr(out, w);
    }

    bool readAnimal(std::istream& in, Animal& a, int version)
    {
        if (!rd(in, a.id) ||
            !rd(in, a.pos.x) || !rd(in, a.pos.y))
            return false;
        if (version >= 4 && (!rd(in, a.vel.x) || !rd(in, a.vel.y)))
            return false;
        if (!rd(in, a.heading) || !rd(in, a.energy) || !rd(in, a.age))
            return false;
        if (version >= 5)
        {
            std::int32_t off = 0;
            if (!rd(in, off)) return false;
            a.offspring = off;
        }
        if (version >= 4)
        {
            float m0 = 0.f, m1 = 0.f;
            if (!rd(in, m0) || !rd(in, m1)) return false;
            a.brain.setMemory(m0, m1);
        }
        std::vector<float> w(static_cast<std::size_t>(Brain::paramCount()));
        for (float& x : w)
            if (!rd(in, x)) return false;
        a.brain.setWeights(std::move(w));
        a.alive = true;
        return true;
    }

    bool readAnimals(std::istream& in, std::vector<Animal>& out,
                     std::uint32_t maxCount, int version)
    {
        std::uint32_t n = 0;
        if (!rd(in, n) || n > maxCount) return false;
        out.resize(n);
        for (auto& a : out)
            if (!readAnimal(in, a, version)) return false;
        return true;
    }
}

void Simulation::serialize(std::ostream& out) const
{
    out.write(SAVE_MAGIC, sizeof SAVE_MAGIC);
    wr(out, std::uint32_t(Brain::paramCount()));

    wr(out, cfg::tune.foodPerSec);
    wr(out, cfg::tune.foodEnergy);
    wr(out, cfg::tune.mutationSigma);
    wr(out, cfg::tune.preyCostMul);
    wr(out, cfg::tune.predCostMul);
    wr(out, cfg::tune.predGainMul);
    wr(out, cfg::tune.preyVision);
    wr(out, cfg::tune.preySpeed);
    wr(out, cfg::tune.predVision);
    wr(out, cfg::tune.predSpeed);

    wr(out, m_time);
    wr(out, m_foodAccum);
    wr(out, m_nextId);

    wr(out, std::uint32_t(m_food.size()));
    for (const auto& f : m_food)
    {
        wr(out, f.pos.x);
        wr(out, f.pos.y);
    }

    wr(out, std::uint32_t(m_prey.size()));
    for (const auto& a : m_prey) writeAnimal(out, a);
    wr(out, std::uint32_t(m_pred.size()));
    for (const auto& a : m_pred) writeAnimal(out, a);

    m_preyHof.serialize(out);
    m_predHof.serialize(out);
}

bool Simulation::deserialize(std::istream& in)
{
    char magic[8];
    in.read(magic, sizeof magic);
    if (!in || std::memcmp(magic, SAVE_MAGIC, 7) != 0) return false;   // "ECOSIM0"
    const int version = magic[7] - '0';
    if (version < SAVE_VERSION_MIN || version > SAVE_VERSION_MAX) return false;

    std::uint32_t params = 0;
    if (!rd(in, params) || params != std::uint32_t(Brain::paramCount())) return false;

    // everything goes into temporaries first, so a truncated or corrupt
    // file leaves the running simulation untouched
    cfg::Tunables tune;   // default-initialised, so fields a v2 file lacks
                          // (vision/speed) keep their standard values
    if (!rd(in, tune.foodPerSec) || !rd(in, tune.foodEnergy) ||
        !rd(in, tune.mutationSigma) || !rd(in, tune.preyCostMul) ||
        !rd(in, tune.predCostMul) || !rd(in, tune.predGainMul))
        return false;
    if (version >= 3)
    {
        if (!rd(in, tune.preyVision) || !rd(in, tune.preySpeed) ||
            !rd(in, tune.predVision) || !rd(in, tune.predSpeed))
            return false;
    }

    float time = 0.f, foodAccum = 0.f;
    std::uint64_t nextId = 1;
    if (!rd(in, time) || !rd(in, foodAccum) || !rd(in, nextId)) return false;

    std::uint32_t nFood = 0;
    if (!rd(in, nFood) || nFood > 100000) return false;
    std::vector<Food> food(nFood);
    for (auto& f : food)
    {
        if (!rd(in, f.pos.x) || !rd(in, f.pos.y)) return false;
        f.alive = true;
    }

    std::vector<Animal> prey, pred;
    if (!readAnimals(in, prey, 100000, version)) return false;
    if (!readAnimals(in, pred, 100000, version)) return false;

    ChampionArchive preyHof, predHof;   // empty for v4-; loaded for v5+
    if (version >= 5)
    {
        if (!preyHof.deserialize(in) || !predHof.deserialize(in)) return false;
    }

    // commit
    cfg::tune    = tune;
    m_time       = time;
    m_foodAccum  = foodAccum;
    m_nextId     = nextId;
    m_food       = std::move(food);
    m_prey       = std::move(prey);
    m_pred       = std::move(pred);
    m_preyHof    = std::move(preyHof);
    m_predHof    = std::move(predHof);
    return true;
}

Simulation::Averages Simulation::average(const std::vector<Animal>& animals)
{
    Averages avg;
    if (animals.empty()) return avg;
    for (const auto& a : animals)
    {
        avg.energy += a.energy;
        avg.age    += a.age;
    }
    const float n = float(animals.size());
    avg.energy /= n;
    avg.age    /= n;
    return avg;
}
