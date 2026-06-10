#pragma once

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

    std::vector<Sample> m_samples;
    float m_interval = 0.5f;
    float m_accum    = 0.5f;
};
