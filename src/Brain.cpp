#include "Brain.hpp"

#include <cmath>

#include "Config.hpp"
#include "Utils.hpp"

namespace
{
    // Xavier-style per-parameter init scale. With tanh neurons, drawing each
    // weight from N(0, 1/sqrt(fan_in)) keeps the pre-activation sum near unit
    // variance, so neurons start in their *responsive* range instead of
    // saturated at +/-1 (where mutations barely change behaviour). Biases
    // start small. The flat 0.5 it replaces saturated every hidden neuron at
    // birth, which made early evolution slow and rugged.
    float initSigma(int i)
    {
        constexpr int w1End = Brain::HID * Brain::IN;                 // input->hidden
        constexpr int b1End = w1End + Brain::HID;                     // hidden biases
        constexpr int w2End = b1End + Brain::OUT * Brain::HID;        // hidden->output
        if (i < w1End) return 1.f / std::sqrt(float(Brain::IN));      // fan-in = IN
        if (i < b1End) return 0.1f;                                   // hidden bias
        if (i < w2End) return 1.f / std::sqrt(float(Brain::HID));     // fan-in = HID
        return 0.1f;                                                  // output bias
    }
}

Brain::Brain()
{
    m_w.resize(N_PARAMS);
    for (int i = 0; i < N_PARAMS; ++i)
        m_w[i] = nrand(0.f, initSigma(i));
}

Brain Brain::offspring() const
{
    Brain child = *this;
    child.m_mem[0] = 0.f;            // a newborn starts with a blank memory
    child.m_mem[1] = 0.f;
    for (int i = 0; i < N_PARAMS; ++i)
    {
        child.m_w[i] += nrand(0.f, cfg::tune.mutationSigma);
        // rare macro-mutation: rewire one connection — drawn at the layer's
        // own scale so a reset weight isn't an outlier that wrecks the brain
        if (frand(0.f, 1.f) < cfg::BRAIN_RESET_PROB)
            child.m_w[i] = nrand(0.f, initSigma(i));
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
