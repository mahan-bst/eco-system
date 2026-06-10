#include "SliderPanel.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    constexpr float PAD      = 14.f;
    constexpr float TITLE_H  = 32.f;
    constexpr float ROW_H    = 34.f;
    constexpr float LABEL_W  = 104.f;
    constexpr float TRACK_W  = 186.f;
    constexpr float VALUE_W  = 56.f;
    constexpr float TRACK_H  = 5.f;
    constexpr float HANDLE_R = 7.f;

    sf::Text makeText(const sf::Font& font, const std::string& str,
                      unsigned size, sf::Vector2f pos, sf::Color color)
    {
        sf::Text t(font, str, size);
        t.setFillColor(color);
        t.setPosition(pos);
        return t;
    }
}

void SliderPanel::add(const char* name, float* value, float lo, float hi, const char* fmt)
{
    m_sliders.push_back({ name, value, lo, hi, fmt });
}

float SliderPanel::width() const
{
    return PAD + LABEL_W + TRACK_W + 10.f + VALUE_W + PAD;
}

float SliderPanel::height() const
{
    return TITLE_H + ROW_H * float(m_sliders.size()) + PAD;
}

sf::FloatRect SliderPanel::trackRect(std::size_t i) const
{
    return { { m_pos.x + PAD + LABEL_W,
               m_pos.y + TITLE_H + ROW_H * float(i) + (ROW_H - TRACK_H) / 2.f },
             { TRACK_W, TRACK_H } };
}

void SliderPanel::applyDrag(std::size_t i, float x)
{
    const sf::FloatRect t = trackRect(i);
    const float f = std::clamp((x - t.position.x) / t.size.x, 0.f, 1.f);
    Slider& s = m_sliders[i];
    *s.value = s.lo + f * (s.hi - s.lo);
}

bool SliderPanel::onMousePressed(sf::Vector2f p)
{
    if (!visible) return false;
    const sf::FloatRect bounds(m_pos, { width(), height() });
    if (!bounds.contains(p)) return false;

    for (std::size_t i = 0; i < m_sliders.size(); ++i)
    {
        const sf::FloatRect t = trackRect(i);
        const float cy = t.position.y + t.size.y / 2.f;
        if (p.x >= t.position.x - 10.f && p.x <= t.position.x + t.size.x + 10.f &&
            std::abs(p.y - cy) <= 13.f)
        {
            m_active = int(i);
            applyDrag(i, p.x);
            break;
        }
    }
    return true;   // swallow every click on the panel
}

void SliderPanel::onMouseMoved(sf::Vector2f p)
{
    if (m_active >= 0) applyDrag(std::size_t(m_active), p.x);
}

void SliderPanel::draw(sf::RenderTarget& rt, const sf::Font* font) const
{
    if (!visible) return;

    sf::RectangleShape bg({ width(), height() });
    bg.setPosition(m_pos);
    bg.setFillColor(sf::Color(15, 20, 26, 240));
    bg.setOutlineColor(sf::Color(46, 60, 72));
    bg.setOutlineThickness(1.f);
    rt.draw(bg);

    if (font)
        rt.draw(makeText(*font, "TUNING - live  (D = reset defaults)", 13,
                         { m_pos.x + PAD, m_pos.y + 8.f }, sf::Color(214, 224, 234)));

    char buf[32];
    for (std::size_t i = 0; i < m_sliders.size(); ++i)
    {
        const Slider& s = m_sliders[i];
        const sf::FloatRect t = trackRect(i);
        const float cy = t.position.y + t.size.y / 2.f;
        const float f  = std::clamp((*s.value - s.lo) / (s.hi - s.lo), 0.f, 1.f);

        sf::RectangleShape track(t.size);
        track.setPosition(t.position);
        track.setFillColor(sf::Color(50, 62, 74));
        rt.draw(track);

        sf::RectangleShape fill({ t.size.x * f, t.size.y });
        fill.setPosition(t.position);
        fill.setFillColor(sf::Color(96, 196, 208));
        rt.draw(fill);

        sf::CircleShape handle(HANDLE_R);
        handle.setOrigin({ HANDLE_R, HANDLE_R });
        handle.setPosition({ t.position.x + t.size.x * f, cy });
        handle.setFillColor(int(i) == m_active ? sf::Color(150, 226, 236)
                                               : sf::Color(214, 224, 234));
        rt.draw(handle);

        if (font)
        {
            const float rowTop = m_pos.y + TITLE_H + ROW_H * float(i);
            rt.draw(makeText(*font, s.name, 12, { m_pos.x + PAD, rowTop + 8.f },
                             sf::Color(168, 180, 192)));

            std::snprintf(buf, sizeof(buf), s.fmt, double(*s.value));
            rt.draw(makeText(*font, buf, 12,
                             { t.position.x + t.size.x + 14.f, rowTop + 8.f },
                             sf::Color(214, 224, 234)));
        }
    }
}
