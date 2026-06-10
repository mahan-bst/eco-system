#include "Leaderboard.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

#include "Simulation.hpp"

namespace
{
    constexpr float TITLE_H   = 30.f;
    constexpr float SECTION_H = 20.f;
    constexpr float ROW_H     = 22.f;
    constexpr float PAD       = 10.f;

    const sf::Color PREY_COL(96, 196, 208);
    const sf::Color PRED_COL(235, 105, 86);
    const sf::Color GOLD(255, 196, 84);

    sf::Text makeText(const sf::Font& font, const std::string& str,
                      unsigned size, sf::Vector2f pos, sf::Color color)
    {
        sf::Text t(font, str, size);
        t.setFillColor(color);
        t.setPosition(pos);
        return t;
    }
}

void Leaderboard::update(const Simulation& sim)
{
    m_rows.clear();

    const auto add = [&](const std::vector<Animal>& list, bool isPred)
    {
        std::vector<const Animal*> sorted;
        sorted.reserve(list.size());
        for (const auto& a : list) sorted.push_back(&a);

        const std::size_t n = std::min<std::size_t>(TOP_N, sorted.size());
        std::partial_sort(sorted.begin(), sorted.begin() + n, sorted.end(),
                          [](const Animal* x, const Animal* y)
                          { return x->age > y->age; });
        for (std::size_t i = 0; i < n; ++i)
            m_rows.push_back({ sorted[i]->id, sorted[i]->age,
                               sorted[i]->energy, isPred, 0.f });
        return int(n);
    };

    m_preyCount = add(sim.preyList(), false);
    add(sim.predList(), true);

    // layout: title, "PREY" header, prey rows, gap, "PREDATORS" header, rows
    float y = m_pos.y + TITLE_H + SECTION_H;
    for (int i = 0; i < int(m_rows.size()); ++i)
    {
        if (i == m_preyCount)
            y += 6.f + SECTION_H;
        m_rows[std::size_t(i)].rowY = y;
        y += ROW_H;
    }
    if (m_rows.empty() || m_preyCount == int(m_rows.size()))
        y += 6.f + SECTION_H;       // an empty predator section still shows
    m_height = (y + 18.f + PAD) - m_pos.y;   // + the "click to follow" hint
}

void Leaderboard::draw(sf::RenderTarget& rt, const sf::Font* font,
                       std::uint64_t selectedId) const
{
    if (!visible) return;

    sf::RectangleShape bg({ WIDTH, m_height });
    bg.setPosition(m_pos);
    bg.setFillColor(sf::Color(15, 20, 26, 235));
    bg.setOutlineColor(sf::Color(46, 60, 72));
    bg.setOutlineThickness(1.f);
    rt.draw(bg);

    if (!font) return;

    rt.draw(makeText(*font, "LEADERBOARD - oldest alive", 13,
                     { m_pos.x + PAD, m_pos.y + 7.f }, sf::Color(214, 224, 234)));

    const float preyHeaderY = m_pos.y + TITLE_H;
    const float predHeaderY = preyHeaderY + SECTION_H +
                              float(m_preyCount) * ROW_H + 6.f;
    rt.draw(makeText(*font, "PREY", 12, { m_pos.x + PAD, preyHeaderY }, PREY_COL));
    rt.draw(makeText(*font, "PREDATORS", 12, { m_pos.x + PAD, predHeaderY }, PRED_COL));

    char buf[48];
    for (int i = 0; i < int(m_rows.size()); ++i)
    {
        const Entry& e = m_rows[std::size_t(i)];
        const int rank = e.isPred ? i - m_preyCount + 1 : i + 1;

        if (e.id == selectedId)
        {
            sf::RectangleShape hl({ WIDTH - 8.f, ROW_H - 2.f });
            hl.setPosition({ m_pos.x + 4.f, e.rowY });
            hl.setFillColor(sf::Color(255, 255, 255, 18));
            rt.draw(hl);
        }

        std::snprintf(buf, sizeof(buf), "%d.", rank);
        rt.draw(makeText(*font, buf, 12, { m_pos.x + PAD, e.rowY + 2.f },
                         rank == 1 ? GOLD : sf::Color(130, 142, 154)));

        std::snprintf(buf, sizeof(buf), "#%llu",
                      static_cast<unsigned long long>(e.id));
        rt.draw(makeText(*font, buf, 12, { m_pos.x + PAD + 24.f, e.rowY + 2.f },
                         e.isPred ? PRED_COL : PREY_COL));

        std::snprintf(buf, sizeof(buf), "%.0fs", e.age);
        sf::Text age = makeText(*font, buf, 12, { 0.f, 0.f },
                                sf::Color(208, 218, 228));
        age.setPosition({ m_pos.x + WIDTH - PAD - age.getLocalBounds().size.x,
                          e.rowY + 2.f });
        rt.draw(age);
    }

    rt.draw(makeText(*font, "click a row to follow", 10,
                     { m_pos.x + PAD, m_pos.y + m_height - PAD - 14.f },
                     sf::Color(120, 132, 144)));
}

bool Leaderboard::onMousePressed(sf::Vector2f p, std::uint64_t& id,
                                 bool& isPred) const
{
    if (!visible) return false;

    const sf::FloatRect bounds(m_pos, { WIDTH, m_height });
    if (!bounds.contains(p)) return false;

    for (const auto& e : m_rows)
    {
        if (p.y >= e.rowY && p.y < e.rowY + ROW_H)
        {
            id = e.id;
            isPred = e.isPred;
            break;
        }
    }
    return true;   // swallow every click on the panel
}
