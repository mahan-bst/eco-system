#pragma once

#include <cstdint>
#include <cstring>
#include <istream>
#include <ostream>
#include <vector>

#include "Simulation.hpp"

// One data point for the charts.
struct Sample
{
    float t = 0.f;
    float preyCount = 0.f, predCount = 0.f, foodCount = 0.f;
    float preyEnergy = 0.f, preyAge = 0.f;
    float predEnergy = 0.f, predAge = 0.f;
};

// Records the world state at a fixed sim-time interval. When the buffer
// fills up it throws away every second sample and doubles the interval,
// so a chart always shows the *whole* run at bounded memory cost.
class History
{
public:
    void reset()
    {
        m_samples.clear();
        m_interval = 0.5f;
        m_accum    = m_interval;   // record immediately on the first update
    }

    void update(const Simulation& sim, float dt)
    {
        m_accum += dt;
        if (m_accum < m_interval) return;
        m_accum -= m_interval;
        record(sim);
        if (m_samples.size() > MAX_SAMPLES) compactSamples();
    }

    const std::vector<Sample>& samples() const { return m_samples; }

    // drop chart samples after time t (used when branching the timeline)
    void truncate(float t)
    {
        while (!m_samples.empty() && m_samples.back().t > t)
            m_samples.pop_back();
        m_accum = 0.f;
    }

    // The chart series is saved inside the .eco file (after the timeline) so
    // the charts survive a save/open round-trip. Sample is plain floats, so a
    // flat blob is fine.
    void serialize(std::ostream& out) const
    {
        out.write(HS_MAGIC, 6);
        const std::uint32_t n = std::uint32_t(m_samples.size());
        out.write(reinterpret_cast<const char*>(&n), sizeof n);
        out.write(reinterpret_cast<const char*>(&m_interval), sizeof m_interval);
        if (n)
            out.write(reinterpret_cast<const char*>(m_samples.data()),
                      std::streamsize(n * sizeof(Sample)));
    }

    bool deserialize(std::istream& in)   // leaves charts empty on failure
    {
        reset();
        char magic[6];
        in.read(magic, 6);
        if (!in || std::memcmp(magic, HS_MAGIC, 6) != 0) { reset(); return false; }

        std::uint32_t n = 0;
        float interval = 0.5f;
        in.read(reinterpret_cast<char*>(&n), sizeof n);
        in.read(reinterpret_cast<char*>(&interval), sizeof interval);
        if (!in || n > 1000000u) { reset(); return false; }

        std::vector<Sample> samples(n);
        if (n)
            in.read(reinterpret_cast<char*>(samples.data()),
                    std::streamsize(n * sizeof(Sample)));
        if (!in) { reset(); return false; }

        m_samples = std::move(samples);
        m_interval = interval;
        m_accum = 0.f;
        return true;
    }

private:
    void record(const Simulation& sim)
    {
        Sample s;
        s.t         = sim.time();
        s.preyCount = float(sim.preyList().size());
        s.predCount = float(sim.predList().size());
        s.foodCount = float(sim.foodList().size());

        const auto pa = Simulation::average(sim.preyList());
        const auto da = Simulation::average(sim.predList());
        s.preyEnergy = pa.energy; s.preyAge = pa.age;
        s.predEnergy = da.energy; s.predAge = da.age;

        m_samples.push_back(s);
    }

    void compactSamples()
    {
        for (std::size_t i = 0; i * 2 < m_samples.size(); ++i)
            m_samples[i] = m_samples[i * 2];
        m_samples.resize((m_samples.size() + 1) / 2);
        m_interval *= 2.f;
    }

    static constexpr std::size_t MAX_SAMPLES = 1400;
    static constexpr char HS_MAGIC[6] = { 'E', 'C', 'O', 'H', 'S', '1' };

    std::vector<Sample> m_samples;
    float m_interval = 0.5f;
    float m_accum    = 0.5f;
};
