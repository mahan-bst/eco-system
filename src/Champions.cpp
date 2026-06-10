#include "Champions.hpp"

#include <algorithm>
#include <istream>
#include <ostream>

#include "Utils.hpp"

namespace
{
    float fitnessOf(int offspring, float age)
    {
        // offspring (reproductive success) dominates; age only breaks ties
        return float(offspring) + 0.001f * age;
    }

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
}

void ChampionArchive::sortDesc()
{
    std::sort(m_list.begin(), m_list.end(),
              [](const Champion& a, const Champion& b) { return a.fitness > b.fitness; });
}

void ChampionArchive::consider(const Brain& brain, std::uint64_t id,
                               int offspring, float age)
{
    const float fit = fitnessOf(offspring, age);

    // already in the hall of fame? keep its best-ever snapshot, no duplicates
    for (auto& c : m_list)
    {
        if (c.id == id)
        {
            if (fit > c.fitness)
            {
                c.brain = brain; c.offspring = offspring; c.age = age; c.fitness = fit;
                sortDesc();
            }
            return;
        }
    }

    if (int(m_list.size()) < CAP)
        m_list.push_back({ brain, id, offspring, age, fit });
    else if (fit > m_list.back().fitness)
        m_list.back() = { brain, id, offspring, age, fit };
    else
        return;

    sortDesc();
}

Brain ChampionArchive::reseed() const
{
    const std::size_t i = std::size_t(frand(0.f, float(m_list.size()) - 0.001f));
    return m_list[i].brain.offspring();
}

void ChampionArchive::serialize(std::ostream& out) const
{
    wr(out, std::uint32_t(m_list.size()));
    for (const auto& c : m_list)
    {
        wr(out, c.id);
        wr(out, std::int32_t(c.offspring));
        wr(out, c.age);
        wr(out, c.fitness);
        for (float w : c.brain.weights()) wr(out, w);
    }
}

bool ChampionArchive::deserialize(std::istream& in)
{
    clear();
    std::uint32_t n = 0;
    if (!rd(in, n) || n > std::uint32_t(CAP)) return false;
    for (std::uint32_t k = 0; k < n; ++k)
    {
        Champion c;
        std::int32_t off = 0;
        if (!rd(in, c.id) || !rd(in, off) || !rd(in, c.age) || !rd(in, c.fitness))
            return false;
        c.offspring = off;
        std::vector<float> w(static_cast<std::size_t>(Brain::paramCount()));
        for (float& x : w)
            if (!rd(in, x)) return false;
        c.brain.setWeights(std::move(w));
        m_list.push_back(std::move(c));
    }
    sortDesc();
    return true;
}
