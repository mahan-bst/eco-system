#include "Charts.hpp"

#include <algorithm>
#include <cstdio>

static sf::Text makeText(const sf::Font& font, const std::string& str,
                         unsigned size, sf::Vector2f pos, sf::Color color)
{
    sf::Text t(font, str, size);
    t.setFillColor(color);
    t.setPosition(pos);
    return t;
}

void drawChart(sf::RenderTarget& rt, const sf::FloatRect& rect, const char* title,
               const std::vector<Sample>& samples, const std::vector<Series>& series,
               bool sharedScale, const sf::Font* font, float markerT)
{
    // frame
    sf::RectangleShape bg(rect.size);
    bg.setPosition(rect.position);
    bg.setFillColor(sf::Color(20, 26, 34));
    bg.setOutlineColor(sf::Color(46, 60, 72));
    bg.setOutlineThickness(1.f);
    rt.draw(bg);

    const float padL = 10.f, padR = 10.f, padT = 46.f, padB = 20.f;
    const sf::FloatRect plot({ rect.position.x + padL, rect.position.y + padT },
                             { rect.size.x - padL - padR, rect.size.y - padT - padB });

    if (font)
        rt.draw(makeText(*font, title, 14,
                         { rect.position.x + 10.f, rect.position.y + 6.f },
                         sf::Color(214, 224, 234)));

    // legend with the latest value of each series
    if (font)
    {
        float lx = rect.position.x + 10.f;
        const float ly = rect.position.y + 26.f;
        char buf[64];
        for (const auto& s : series)
        {
            const float v = samples.empty() ? 0.f : samples.back().*(s.member);
            std::snprintf(buf, sizeof(buf), s.integer ? "%s %.0f" : "%s %.1f", s.name, v);

            sf::RectangleShape swatch({ 9.f, 9.f });
            swatch.setPosition({ lx, ly + 4.f });
            swatch.setFillColor(s.color);
            rt.draw(swatch);

            sf::Text label = makeText(*font, buf, 12, { lx + 14.f, ly },
                                      sf::Color(168, 180, 192));
            rt.draw(label);
            lx += 14.f + label.getLocalBounds().size.x + 16.f;
        }
    }

    // faint horizontal grid lines
    for (int i = 1; i <= 3; ++i)
    {
        sf::RectangleShape line({ plot.size.x, 1.f });
        line.setPosition({ plot.position.x, plot.position.y + plot.size.y * (i / 4.f) });
        line.setFillColor(sf::Color(255, 255, 255, 10));
        rt.draw(line);
    }

    if (samples.size() < 2) return;

    // shared scale (population chart): one 0-based axis for all series
    float sharedMin = 0.f, sharedMax = 1.f;
    if (sharedScale)
    {
        for (const auto& s : series)
            for (const auto& smp : samples)
                sharedMax = std::max(sharedMax, smp.*(s.member));
        sharedMax *= 1.05f;
    }

    const std::size_t n = samples.size();
    for (const auto& s : series)
    {
        float lo = sharedMin, hi = sharedMax;
        if (!sharedScale)
        {
            lo = 1e18f; hi = -1e18f;
            for (const auto& smp : samples)
            {
                lo = std::min(lo, smp.*(s.member));
                hi = std::max(hi, smp.*(s.member));
            }
            const float pad = std::max((hi - lo) * 0.08f, 0.5f);
            lo -= pad; hi += pad;
        }
        const float range = std::max(hi - lo, 1e-6f);

        sf::VertexArray strip(sf::PrimitiveType::LineStrip, n);
        for (std::size_t i = 0; i < n; ++i)
        {
            const float x = plot.position.x +
                            plot.size.x * (n > 1 ? float(i) / float(n - 1) : 0.f);
            const float y = plot.position.y + plot.size.y *
                            (1.f - (samples[i].*(s.member) - lo) / range);
            strip[i] = sf::Vertex{ { x, y }, s.color };
        }
        rt.draw(strip);
    }

    // "you are here" marker: a vertical line at markerT while rewinding.
    // We locate markerT by interpolating the samples' times into a fractional
    // index, so the line lands exactly on the plotted curves.
    if (markerT >= 0.f)
    {
        float f;
        if (markerT <= samples.front().t)      f = 0.f;
        else if (markerT >= samples.back().t)  f = float(n - 1);
        else
        {
            std::size_t i = 0;
            while (i + 1 < n && samples[i + 1].t < markerT) ++i;
            const float t0 = samples[i].t, t1 = samples[i + 1].t;
            f = float(i) + (t1 > t0 ? (markerT - t0) / (t1 - t0) : 0.f);
        }
        const float x = plot.position.x + plot.size.x * (f / float(n - 1));

        sf::RectangleShape line({ 1.5f, plot.size.y });
        line.setPosition({ x, plot.position.y });
        line.setFillColor(sf::Color(255, 196, 84, 235));
        rt.draw(line);

        sf::CircleShape dot(3.f);
        dot.setOrigin({ 3.f, 3.f });
        dot.setPosition({ x, plot.position.y });
        dot.setFillColor(sf::Color(255, 196, 84));
        rt.draw(dot);
    }

    // axis labels
    if (font)
    {
        char buf[32];
        const sf::Color dim(120, 132, 144);
        const float labelY = rect.position.y + rect.size.y - 17.f;

        std::snprintf(buf, sizeof(buf), "%.0fs", samples.front().t);
        rt.draw(makeText(*font, buf, 11, { plot.position.x, labelY }, dim));

        std::snprintf(buf, sizeof(buf), "%.0fs", samples.back().t);
        sf::Text tEnd = makeText(*font, buf, 11, { 0.f, 0.f }, dim);
        tEnd.setPosition({ plot.position.x + plot.size.x - tEnd.getLocalBounds().size.x,
                           labelY });
        rt.draw(tEnd);

        if (sharedScale)
        {
            std::snprintf(buf, sizeof(buf), "%.0f", sharedMax);
            rt.draw(makeText(*font, buf, 11,
                             { plot.position.x, plot.position.y - 2.f }, dim));
        }
    }
}
