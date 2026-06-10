#include "Leaderboard.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

#include "Champions.hpp"
#include "Simulation.hpp"

namespace
{
    constexpr float TITLE_H   = 26.f;
    constexpr float TAB_H     = 24.f;
    constexpr float SECTION_H = 20.f;
    constexpr float ROW_H     = 22.f;
    constexpr float PAD       = 10.f;
    constexpr float HINT_H    = 18.f;

    const sf::Color PREY_COL(96, 196, 208);
    const sf::Color PRED_COL(235, 105, 86);
    const sf::Color GOLD(255, 196, 84);
    const sf::Color DIM(120, 132, 144);

    sf::Text makeText(const sf::Font& font, const std::string& str,
                      unsigned size, sf::Vector2f pos, sf::Color color)
    {
        sf::Text t(font, str, size);
        t.setFillColor(color);
        t.setPosition(pos);
        return t;
    }
}

float Leaderboard::contentTop() const
{
    return m_pos.y + TITLE_H + TAB_H;
}

sf::FloatRect Leaderboard::tabRowRect() const
{
    return { { m_pos.x, m_pos.y + TITLE_H }, { WIDTH, TAB_H } };
}

void Leaderboard::update(const Simulation& sim)
{
    // rankings change slowly, so rebuild a few times per second instead of
    // every frame — the per-frame sort + allocation was the cost here
    if (++m_sinceRebuild < REBUILD_EVERY) return;
    m_sinceRebuild = 0;

    m_rows.clear();

    if (m_mode == Mode::Alive)
    {
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
            {
                Entry e;
                e.id = sorted[i]->id;
                e.age = sorted[i]->age;
                e.offspring = sorted[i]->offspring;
                e.isPred = isPred;
                m_rows.push_back(e);
            }
            return int(n);
        };
        m_preyCount = add(sim.preyList(), false);
        add(sim.predList(), true);
    }
    else   // Hall of Fame: straight from the champion archives (best-first)
    {
        const auto add = [&](const ChampionArchive& arc, bool isPred)
        {
            const int n = arc.size();
            for (int i = 0; i < n; ++i)
            {
                Entry e;
                e.id = arc.at(i).id;
                e.age = arc.at(i).age;
                e.offspring = arc.at(i).offspring;
                e.isPred = isPred;
                e.champIndex = i;
                m_rows.push_back(e);
            }
            return n;
        };
        m_preyCount = add(sim.preyChampions(), false);
        add(sim.predChampions(), true);
    }

    // layout: prey header, prey rows, gap + predator header, predator rows
    float y = contentTop() + SECTION_H;
    for (int i = 0; i < int(m_rows.size()); ++i)
    {
        if (i == m_preyCount)
            y += 6.f + SECTION_H;
        m_rows[std::size_t(i)].rowY = y;
        y += ROW_H;
    }
    if (m_rows.empty() || m_preyCount == int(m_rows.size()))
        y += 6.f + SECTION_H;       // an empty predator section still shows
    m_height = (y + HINT_H + PAD) - m_pos.y;
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

    rt.draw(makeText(*font, "LEADERBOARD", 13,
                     { m_pos.x + PAD, m_pos.y + 6.f }, sf::Color(214, 224, 234)));

    // ----- tabs -----
    const float tabY = m_pos.y + TITLE_H + 3.f;
    const bool alive = m_mode == Mode::Alive;
    rt.draw(makeText(*font, "Alive", 13, { m_pos.x + PAD, tabY },
                     alive ? sf::Color(214, 224, 234) : DIM));
    rt.draw(makeText(*font, "Hall of Fame", 13, { m_pos.x + PAD + 70.f, tabY },
                     alive ? DIM : GOLD));
    {   // underline the active tab
        const float ux = alive ? m_pos.x + PAD : m_pos.x + PAD + 70.f;
        const float uw = alive ? 36.f : 86.f;
        sf::RectangleShape u({ uw, 2.f });
        u.setPosition({ ux, tabY + 18.f });
        u.setFillColor(alive ? sf::Color(214, 224, 234) : GOLD);
        rt.draw(u);
    }

    // ----- section headers -----
    const float preyHeaderY = contentTop();
    const float predHeaderY = preyHeaderY + SECTION_H + float(m_preyCount) * ROW_H + 6.f;
    const char* preyLbl = alive ? "PREY" : "PREY - best ever";
    const char* predLbl = alive ? "PREDATORS" : "PREDATORS - best ever";
    rt.draw(makeText(*font, preyLbl, 12, { m_pos.x + PAD, preyHeaderY }, PREY_COL));
    rt.draw(makeText(*font, predLbl, 12, { m_pos.x + PAD, predHeaderY }, PRED_COL));

    if (m_rows.empty())
        rt.draw(makeText(*font, "no champions yet - keep evolving", 11,
                         { m_pos.x + PAD, preyHeaderY + SECTION_H }, DIM));

    // ----- rows -----
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
                         rank == 1 ? GOLD : DIM));

        std::snprintf(buf, sizeof(buf), "#%llu",
                      static_cast<unsigned long long>(e.id));
        rt.draw(makeText(*font, buf, 12, { m_pos.x + PAD + 24.f, e.rowY + 2.f },
                         e.isPred ? PRED_COL : PREY_COL));

        // right-aligned headline: lifespan (Alive) or offspring (Hall of Fame)
        if (alive) std::snprintf(buf, sizeof(buf), "%.0fs", e.age);
        else       std::snprintf(buf, sizeof(buf), "%d kids", e.offspring);
        sf::Text val = makeText(*font, buf, 12, { 0.f, 0.f },
                                sf::Color(208, 218, 228));
        val.setPosition({ m_pos.x + WIDTH - PAD - val.getLocalBounds().size.x,
                          e.rowY + 2.f });
        rt.draw(val);
    }

    rt.draw(makeText(*font, alive ? "click a row to follow"
                                   : "click a row to spawn a copy",
                     10, { m_pos.x + PAD, m_pos.y + m_height - PAD - 12.f }, DIM));
}

bool Leaderboard::contains(sf::Vector2f p) const
{
    if (!visible) return false;
    return sf::FloatRect(m_pos, { WIDTH, m_height }).contains(p);
}

bool Leaderboard::onMousePressed(sf::Vector2f p, Click& out)
{
    out = Click{};
    if (!contains(p)) return false;

    // tab row: switch view
    if (tabRowRect().contains(p))
    {
        m_mode = (p.x < m_pos.x + PAD + 64.f) ? Mode::Alive : Mode::HallOfFame;
        m_sinceRebuild = REBUILD_EVERY;   // refresh rows for the new tab now
        return true;
    }

    // a data row?
    for (const auto& e : m_rows)
    {
        if (p.y >= e.rowY && p.y < e.rowY + ROW_H)
        {
            if (m_mode == Mode::Alive)
            {
                out.type = Click::Follow;
                out.id = e.id;
                out.isPred = e.isPred;
            }
            else
            {
                out.type = Click::Spawn;
                out.isPred = e.isPred;
                out.champIndex = e.champIndex;
            }
            break;
        }
    }
    return true;   // swallow every click on the panel
}
