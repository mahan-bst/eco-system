#include "Brain.hpp"

#include <cmath>

#include "Config.hpp"
#include "Utils.hpp"

Brain::Brain()
{
    m_w.resize(N_PARAMS);
    for (float& w : m_w)
        w = nrand(0.f, cfg::BRAIN_INIT_SIGMA);
}

Brain Brain::offspring() const
{
    Brain child = *this;
    child.m_mem[0] = 0.f;            // a newborn starts with a blank memory
    child.m_mem[1] = 0.f;
    for (float& w : child.m_w)
    {
        w += nrand(0.f, cfg::tune.mutationSigma);
        // rare macro-mutation: rewire the connection completely
        if (frand(0.f, 1.f) < cfg::BRAIN_RESET_PROB)
            w = nrand(0.f, cfg::BRAIN_INIT_SIGMA);
    }
    return child;
}

void Brain::think(const float (&inputs)[IN])
{
    for (int i = 0; i < IN; ++i) m_in[i] = inputs[i];

    // recurrence: the last two inputs are our own memory from last tick
    m_in[IN - 2] = m_mem[0];
    m_in[IN - 1] = m_mem[1];

    for (int h = 0; h < HID; ++h)
    {
        float sum = b1(h);
        for (int i = 0; i < IN; ++i)
            sum += w1(h, i) * m_in[i];
        m_hid[h] = std::tanh(sum);
    }

    for (int o = 0; o < OUT; ++o)
    {
        float sum = b2(o);
        for (int h = 0; h < HID; ++h)
            sum += w2(o, h) * m_hid[h];
        // throttle is the only 0..1 output; everything else is tanh
        m_out[o] = (o == 1) ? 1.f / (1.f + std::exp(-sum)) : std::tanh(sum);
    }

    m_mem[0] = m_out[2];   // remembered until the next think()
    m_mem[1] = m_out[3];
}
