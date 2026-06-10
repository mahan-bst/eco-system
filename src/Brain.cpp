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

    for (int h = 0; h < HID; ++h)
    {
        float sum = b1(h);
        for (int i = 0; i < IN; ++i)
            sum += w1(h, i) * m_in[i];
        m_hid[h] = std::tanh(sum);
    }

    float t = b2(0);
    for (int h = 0; h < HID; ++h) t += w2(0, h) * m_hid[h];
    m_out[0] = std::tanh(t);                       // turn: -1..1

    float s = b2(1);
    for (int h = 0; h < HID; ++h) s += w2(1, h) * m_hid[h];
    m_out[1] = 1.f / (1.f + std::exp(-s));         // throttle: 0..1
}
