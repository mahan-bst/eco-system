#pragma once

#include <vector>

// A small fixed-topology feed-forward network with recurrent memory — the
// whole genome of an animal.
//
//   14 inputs  ->  10 hidden (tanh)  ->  4 outputs
//
// Inputs (all egocentric, so the same brain works anywhere on the map):
//    0  target signal     1 - dist/vision for the thing this species eats
//    1  target sin        sine of the angle to it, relative to our heading
//    2  target cos        cosine of that angle
//    3  other signal      prey: nearest predator / predator: nearest rival
//    4  other sin
//    5  other cos
//    6  energy            own energy / capacity
//    7  wall              proximity of the nearest wall (0 far .. 1 touching)
//    8  centre sin        direction of the world centre, relative to heading
//    9  centre cos
//   10  rel vel fwd       relative velocity of the key moving agent
//   11  rel vel side      (prey: the predator, predator: the prey),
//                         in our frame -> enables interception and dodging
//   12  memory 1          own memory outputs from the previous tick:
//   13  memory 2          short-term state ("something was here a moment ago")
//
// Outputs:
//    0  turn      tanh    -1..1, scaled by the species turn rate
//    1  throttle  sigmoid  0..1, scaled by the species max speed
//    2  memory 1  tanh    fed back into input 12 next tick
//    3  memory 2  tanh    fed back into input 13 next tick
//
// There is no crossover: a child gets a mutated copy of its parent's weights
// (and starts with a blank memory).
class Brain
{
public:
    static constexpr int IN  = 14;
    static constexpr int HID = 10;
    static constexpr int OUT = 4;

    Brain();                      // fresh random weights
    Brain offspring() const;      // copy + mutation (uses cfg::tune.mutationSigma)

    // inputs 12/13 may be left zero — the brain fills them from its own memory
    void think(const float (&inputs)[IN]);

    float turn()     const { return m_out[0]; }
    float throttle() const { return m_out[1]; }

    // genome / state access for save & load
    static constexpr int paramCount() { return N_PARAMS; }
    const std::vector<float>& weights() const { return m_w; }
    void setWeights(std::vector<float> w)
    {
        if (int(w.size()) == N_PARAMS) m_w = std::move(w);
    }
    const float* memory() const { return m_mem; }
    void setMemory(float a, float b) { m_mem[0] = a; m_mem[1] = b; }

    // live activations and weights, exposed for the brain visualiser
    const float* inputs()  const { return m_in;  }
    const float* hidden()  const { return m_hid; }
    const float* outputs() const { return m_out; }
    float w1(int h, int i) const { return m_w[h * IN + i]; }
    float w2(int o, int h) const { return m_w[W1 + HID + o * HID + h]; }

private:
    static constexpr int W1 = HID * IN;   // input -> hidden weights
    static constexpr int W2 = OUT * HID;  // hidden -> output weights
    static constexpr int N_PARAMS = W1 + HID + W2 + OUT;

    float b1(int h) const { return m_w[W1 + h]; }
    float b2(int o) const { return m_w[W1 + HID + W2 + o]; }

    // layout: [w1][b1][w2][b2]
    std::vector<float> m_w;

    float m_in[IN]   {};
    float m_hid[HID] {};
    float m_out[OUT] {};
    float m_mem[2]   {};   // recurrent state carried between ticks
};
