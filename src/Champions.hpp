#pragma once

#include <cstdint>
#include <iosfwd>
#include <vector>

#include "Brain.hpp"

// One archived elite brain plus the stats that earned it a slot.
struct Champion
{
    Brain brain;
    std::uint64_t id = 0;      // the original creature's id (for display)
    int   offspring = 0;       // reproductive success = the real fitness
    float age = 0.f;           // lifespan, seconds (tiebreaker)
    float fitness = 0.f;       // offspring + tiny age term
};

// A per-species "hall of fame": the best brains seen so far, kept aside from
// the live population. Used to reseed after a crash so a species recovers
// competent instead of from random noise, and surfaced in the UI so you can
// spawn a copy of the greatest creature on demand.
//
// Diversity safeguards (see the discussion): we keep several distinct
// champions (deduped by id) rather than one, and reseeding always mutates the
// archived brain — so the archive is a safety net, not a monoculture pump.
class ChampionArchive
{
public:
    static constexpr int CAP = 8;

    void clear() { m_list.clear(); }
    bool empty() const { return m_list.empty(); }
    int  size()  const { return int(m_list.size()); }

    // offer a creature for the hall of fame; stored if it beats the weakest
    void consider(const Brain& brain, std::uint64_t id, int offspring, float age);

    const std::vector<Champion>& list() const { return m_list; }
    const Champion& at(int i) const { return m_list[std::size_t(i)]; }

    // a random archived brain, mutated — for crash reseeding (caller: !empty)
    Brain reseed() const;

    void serialize(std::ostream& out) const;
    bool deserialize(std::istream& in);   // leaves the archive empty on failure

private:
    void sortDesc();
    std::vector<Champion> m_list;   // kept sorted best-first; back() is weakest
};
