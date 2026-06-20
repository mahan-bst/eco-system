#include "BrainView.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <string>

#include "Config.hpp"

namespace
{
    // signal colours: excitation = teal, inhibition = orange
    const sf::Color POS(96, 196, 208);
    const sf::Color NEG(235, 140, 80);

    const char* PREY_INPUTS[Brain::IN] = {
        "food", "food sin", "food cos",
        "danger", "dng sin", "dng cos",
        "energy", "wall", "ctr sin", "ctr cos",
        "dng vel fw", "dng vel sd", "mem 1", "mem 2"
    };
    const char* PRED_INPUTS[Brain::IN] = {
        "prey", "prey sin", "prey cos",
        "rival", "rvl sin", "rvl cos",
        "energy", "wall", "ctr sin", "ctr cos",
        "prey vel fw", "prey vel sd", "mem 1", "mem 2"
    };
    const char* OUTPUTS[Brain::OUT] = { "turn", "speed", "mem 1", "mem 2" };

    sf::Color scaledColor(sf::Color c, float f)
    {
        return { std::uint8_t(c.r * f), std::uint8_t(c.g * f),
                 std::uint8_t(c.b * f), c.a };
    }

    // activation in [-1, 1] -> node fill colour
    sf::Color actColor(float a)
    {
        const float m = std::min(std::abs(a), 1.f);
        return scaledColor(a >= 0.f ? POS : NEG, 0.22f + 0.78f * m);
    }

    sf::Text makeText(const sf::Font& font, const std::string& str,
                      unsigned size, sf::Vector2f pos, sf::Color color)
    {
        sf::Text t(font, str, size);
        t.setFillColor(color);
        t.setPosition(pos);
        return t;
    }
}

void BrainView::draw(sf::RenderTarget& rt, sf::Vector2f pos, const Animal& a,
                     bool isPredator, const sf::Font* font)
{
    const Brain& b = a.brain;
    const auto& S  = isPredator ? cfg::PRED : cfg::PREY;
    const char** inputNames = isPredator ? PRED_INPUTS : PREY_INPUTS;

    // ----- panel ------------------------------------------------------------
    sf::RectangleShape bg({ WIDTH, HEIGHT });
    bg.setPosition(pos);
    bg.setFillColor(sf::Color(15, 20, 26, 238));
    bg.setOutlineColor(sf::Color(46, 60, 72));
    bg.setOutlineThickness(1.f);
    rt.draw(bg);

    if (font)
    {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%s #%llu    energy %.0f / %.0f    age %.0fs",
                      isPredator ? "PREDATOR" : "PREY",
                      static_cast<unsigned long long>(a.id),
                      a.energy, S.capacity, a.age);
        rt.draw(makeText(*font, buf, 14, { pos.x + 12.f, pos.y + 8.f },
                         isPredator ? sf::Color(235, 105, 86)
                                    : sf::Color(96, 196, 208)));

        std::snprintf(buf, sizeof(buf), "turn %+.2f    speed %.2f",
                      b.turn(), b.throttle());
        rt.draw(makeText(*font, buf, 12, { pos.x + 12.f, pos.y + 30.f },
                         sf::Color(168, 180, 192)));
    }

    // ----- node layout --------------------------------------------------------
    const float top = pos.y + 60.f;
    const float inX = pos.x + 104.f, hidX = pos.x + 252.f, outX = pos.x + 352.f;
    const float inSpan = 26.f * (Brain::IN - 1);   // 338

    sf::Vector2f inP[Brain::IN], hidP[Brain::HID], outP[Brain::OUT];
    for (int i = 0; i < Brain::IN; ++i)
        inP[i] = { inX, top + 26.f * float(i) };

    const float hidTop = top + (inSpan - 30.f * (Brain::HID - 1)) / 2.f;
    for (int h = 0; h < Brain::HID; ++h)
        hidP[h] = { hidX, hidTop + 30.f * float(h) };

    for (int o = 0; o < Brain::OUT; ++o)
        outP[o] = { outX, top + inSpan / 2.f +
                          (float(o) - (Brain::OUT - 1) / 2.f) * 52.f };

    // ----- connections (drawn first, under the nodes) --------------------------
    // colour = sign of the live signal (weight x source activation),
    // intensity = its magnitude, so you can watch information flow
    sf::VertexArray lines(sf::PrimitiveType::Lines);
    const auto link = [&](sf::Vector2f from, sf::Vector2f to, float w, float src)
    {
        if (std::abs(w) < 0.03f) return;            // skip near-dead synapses
        const float signal = w * src;
        const float alpha  = std::clamp(18.f + 150.f * std::abs(signal), 0.f, 190.f);
        sf::Color c = signal >= 0.f ? POS : NEG;
        c.a = std::uint8_t(alpha);
        lines.append(sf::Vertex{ from, c });
        lines.append(sf::Vertex{ to, c });
    };

    for (int h = 0; h < Brain::HID; ++h)
        for (int i = 0; i < Brain::IN; ++i)
            link(inP[i], hidP[h], b.w1(h, i), b.inputs()[i]);
    for (int o = 0; o < Brain::OUT; ++o)
        for (int h = 0; h < Brain::HID; ++h)
            link(hidP[h], outP[o], b.w2(o, h), b.hidden()[h]);
    rt.draw(lines);

    // ----- nodes ---------------------------------------------------------------
    const auto node = [&](sf::Vector2f p, float act, float radius)
    {
        sf::CircleShape c(radius);
        c.setOrigin({ radius, radius });
        c.setPosition(p);
        c.setFillColor(actColor(act));
        c.setOutlineColor(sf::Color(70, 84, 96));
        c.setOutlineThickness(1.f);
        rt.draw(c);
    };

    for (int i = 0; i < Brain::IN; ++i)  node(inP[i],  b.inputs()[i],  7.f);
    for (int h = 0; h < Brain::HID; ++h) node(hidP[h], b.hidden()[h],  8.f);
    for (int o = 0; o < Brain::OUT; ++o) node(outP[o], b.outputs()[o], 9.f);

    // ----- labels -----------------------------------------------------------------
    if (font)
    {
        for (int i = 0; i < Brain::IN; ++i)
            rt.draw(makeText(*font, inputNames[i], 11,
                             { pos.x + 12.f, inP[i].y - 7.f },
                             sf::Color(150, 162, 174)));
        for (int o = 0; o < Brain::OUT; ++o)
        {
            char buf[24];
            std::snprintf(buf, sizeof(buf), "%s %+.2f", OUTPUTS[o], b.outputs()[o]);
            rt.draw(makeText(*font, buf, 11,
                             { outP[o].x + 16.f, outP[o].y - 7.f },
                             sf::Color(190, 200, 210)));
        }
    }

    // ----- short-term memory readout ------------------------------------------
    // Turns the amber wiring into plain language: for each memory channel, how
    // many hidden neurons read it / write it, which sensory inputs end up stored
    // in it, and which motor output it ends up steering. Computed from the
    // weights (the fixed wiring), not this tick's signal, so the readout is
    // stable while you watch the creature move.
    if (font)
    {
        const sf::Color MEM(255, 196, 84);
        const float secY = pos.y + 414.f;
        rt.draw(makeText(*font, "SHORT-TERM MEMORY", 12,
                         { pos.x + 12.f, secY }, MEM));

        for (int k = 0; k < 2; ++k)
        {
            const int inIdx  = Brain::IN - 2 + k;   // memory input  12 / 13
            const int outIdx = 2 + k;               // memory output  2 /  3

            int readN = 0, writeN = 0;
            float stored[Brain::IN] = {};
            float drives[2] = {};   // 0 = turn, 1 = speed
            for (int h = 0; h < Brain::HID; ++h)
            {
                const float wRead  = std::abs(b.w1(h, inIdx));     // mem -> hidden
                const float wWrite = std::abs(b.w2(outIdx, h));    // hidden -> mem
                if (wRead  > 0.15f) ++readN;
                if (wWrite > 0.15f) ++writeN;

                // stored: sensory inputs feeding the neurons that WRITE this channel
                for (int i = 0; i < Brain::IN - 2; ++i)            // skip mem inputs
                    stored[i] += wWrite * std::abs(b.w1(h, i));
                // steered: motor outputs driven by neurons that READ this channel
                drives[0] += wRead * std::abs(b.w2(0, h));
                drives[1] += wRead * std::abs(b.w2(1, h));
            }

            int best1 = 0, best2 = 1;
            for (int i = 0; i < Brain::IN - 2; ++i)
            {
                if (stored[i] > stored[best1]) { best2 = best1; best1 = i; }
                else if (i != best1 && stored[i] > stored[best2]) best2 = i;
            }

            char buf[128];
            const float base = secY + 20.f + float(k) * 50.f;

            std::snprintf(buf, sizeof(buf),
                          "mem %d   reads %d hidden / writes %d hidden",
                          k + 1, readN, writeN);
            rt.draw(makeText(*font, buf, 12, { pos.x + 12.f, base },
                             sf::Color(214, 224, 234)));

            std::snprintf(buf, sizeof(buf), "stores: %s, %s",
                          inputNames[best1], inputNames[best2]);
            rt.draw(makeText(*font, buf, 11, { pos.x + 22.f, base + 16.f },
                             sf::Color(168, 180, 192)));

            std::snprintf(buf, sizeof(buf), "steers: %s",
                          drives[0] >= drives[1] ? "turn" : "speed");
            rt.draw(makeText(*font, buf, 11, { pos.x + 22.f, base + 31.f },
                             sf::Color(168, 180, 192)));
        }
    }
}
