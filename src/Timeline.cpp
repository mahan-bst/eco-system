#include "Timeline.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <istream>
#include <ostream>
#include <sstream>

#include "Simulation.hpp"

namespace
{
    // marks the start of the timeline section appended after the world in .eco
    constexpr char TL_MAGIC[6] = { 'E', 'C', 'O', 'T', 'L', '1' };

    template <class T>
    void wr(std::ostream& out, const T& v)
    {
        out.write(reinterpret_cast<const char*>(&v), sizeof v);
    }

    template <class T>
    bool rd(std::istream& in, T& v)
    {
        in.read(reinterpret_cast<char*>(&v), sizeof v);
        return bool(in);
    }

    std::string snap(const Simulation& sim)
    {
        return sim.snapshot();   // fast raw-buffer serialization (no ostream)
    }

    void unsnap(Simulation& sim, const std::string& data)
    {
        std::istringstream i(data, std::ios::binary);
        sim.deserialize(i);
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

void Timeline::reset()
{
    m_snaps.clear();
    m_present.clear();
    m_interval = 1.0f;
    m_reviewing = false;
    m_dragging = false;
    m_viewIndex = 0;
    m_loadedIndex = -1;
}

void Timeline::record(const Simulation& sim)
{
    if (m_reviewing) return;
    if (!m_snaps.empty() && sim.time() - m_snaps.back().t < m_interval) return;
    m_snaps.push_back({ sim.time(), snap(sim) });
    if (int(m_snaps.size()) > MAX_SNAPS) thin();
}

void Timeline::thin()
{
    // keep every other snapshot, double the interval -> whole run still covered
    std::vector<Snap> kept;
    kept.reserve(m_snaps.size() / 2 + 1);
    for (std::size_t i = 0; i < m_snaps.size(); i += 2)
        kept.push_back(std::move(m_snaps[i]));
    m_snaps = std::move(kept);
    m_interval *= 2.f;
}

int Timeline::indexAtFraction(float f) const
{
    if (m_snaps.size() <= 1) return 0;
    const int i = int(std::lround(f * float(m_snaps.size() - 1)));
    return std::clamp(i, 0, int(m_snaps.size()) - 1);
}

float Timeline::handleFraction() const
{
    if (!m_reviewing || m_snaps.size() <= 1) return 1.f;   // live -> present (right end)
    return float(m_viewIndex) / float(m_snaps.size() - 1);
}

float Timeline::viewedTime() const
{
    if (m_snaps.empty()) return 0.f;
    return m_snaps[std::size_t(std::clamp(m_viewIndex, 0, int(m_snaps.size()) - 1))].t;
}

void Timeline::enterReview(const Simulation& sim)
{
    m_present = snap(sim);               // exact present, restored on exit
    m_reviewing = true;
    m_viewIndex = int(m_snaps.size()) - 1;
    m_loadedIndex = -1;                  // force the first scrub to actually load
}

void Timeline::restoreIndex(Simulation& sim, int i)
{
    if (m_snaps.empty()) return;
    m_viewIndex = std::clamp(i, 0, int(m_snaps.size()) - 1);
    if (m_viewIndex == m_loadedIndex) return;   // already showing this snapshot
    m_loadedIndex = m_viewIndex;
    unsnap(sim, m_snaps[std::size_t(m_viewIndex)].data);
}

bool Timeline::onPress(sf::Vector2f p, Simulation& sim)
{
    if (!m_bar.contains(p)) return false;
    if (m_snaps.empty()) return true;            // nothing recorded yet, but eat the click

    if (!m_reviewing) enterReview(sim);
    m_dragging = true;

    const float f = (p.x - m_bar.position.x) / m_bar.size.x;
    restoreIndex(sim, indexAtFraction(std::clamp(f, 0.f, 1.f)));
    return true;
}

void Timeline::onDrag(sf::Vector2f p, Simulation& sim)
{
    if (!m_dragging || !m_reviewing) return;
    const float f = (p.x - m_bar.position.x) / m_bar.size.x;
    restoreIndex(sim, indexAtFraction(std::clamp(f, 0.f, 1.f)));
}

void Timeline::stepSnapshot(Simulation& sim, int dir)
{
    if (!m_reviewing) return;
    restoreIndex(sim, m_viewIndex + dir);
}

void Timeline::returnToPresent(Simulation& sim)
{
    if (!m_reviewing) return;
    if (!m_present.empty()) unsnap(sim, m_present);
    m_reviewing = false;
    m_dragging = false;
}

float Timeline::branch(Simulation& sim)
{
    if (!m_reviewing) return sim.time();
    // sim already holds the viewed snapshot; make it the new present and
    // throw away everything that came after it
    const float t = viewedTime();
    if (m_viewIndex + 1 < int(m_snaps.size()))
        m_snaps.erase(m_snaps.begin() + (m_viewIndex + 1), m_snaps.end());
    m_present.clear();
    m_reviewing = false;
    m_dragging = false;
    return t;
}

void Timeline::serialize(std::ostream& out) const
{
    out.write(TL_MAGIC, sizeof TL_MAGIC);
    wr(out, std::uint32_t(m_snaps.size()));
    wr(out, m_interval);
    for (const auto& s : m_snaps)
    {
        wr(out, s.t);
        wr(out, std::uint32_t(s.data.size()));
        out.write(s.data.data(), std::streamsize(s.data.size()));
    }
}

bool Timeline::deserialize(std::istream& in)
{
    reset();

    char magic[6];
    in.read(magic, sizeof magic);
    if (!in || std::memcmp(magic, TL_MAGIC, sizeof magic) != 0)
        return false;                       // no timeline section (e.g. old file)

    std::uint32_t n = 0;
    float interval = 1.f;
    if (!rd(in, n) || n > 1000000u || !rd(in, interval)) { reset(); return false; }

    std::vector<Snap> snaps;
    snaps.reserve(n);
    for (std::uint32_t i = 0; i < n; ++i)
    {
        float t = 0.f;
        std::uint32_t len = 0;
        if (!rd(in, t) || !rd(in, len) || len > 64u * 1024u * 1024u)
        {
            reset();
            return false;
        }
        std::string data(len, '\0');
        if (len) in.read(&data[0], std::streamsize(len));
        if (!in) { reset(); return false; }
        snaps.push_back({ t, std::move(data) });
    }

    m_snaps = std::move(snaps);
    m_interval = interval;
    m_viewIndex = m_snaps.empty() ? 0 : int(m_snaps.size()) - 1;
    return true;
}

void Timeline::draw(sf::RenderTarget& rt, const sf::Font* font, float liveTime) const
{
    // ----- bar -----
    sf::RectangleShape track(m_bar.size);
    track.setPosition(m_bar.position);
    track.setFillColor(sf::Color(12, 16, 22, 210));
    track.setOutlineColor(sf::Color(46, 60, 72));
    track.setOutlineThickness(1.f);
    rt.draw(track);

    const float midY = m_bar.position.y + m_bar.size.y * 0.5f;
    const float left = m_bar.position.x + 6.f;
    const float right = m_bar.position.x + m_bar.size.x - 6.f;
    const float w = right - left;

    // groove
    sf::RectangleShape groove({ w, 3.f });
    groove.setOrigin({ 0.f, 1.5f });
    groove.setPosition({ left, midY });
    groove.setFillColor(sf::Color(50, 62, 74));
    rt.draw(groove);

    if (!m_snaps.empty())
    {
        const float frac = handleFraction();
        const sf::Color accent = m_reviewing ? sf::Color(255, 196, 84)
                                             : sf::Color(96, 196, 208);

        // filled portion up to the playhead
        sf::RectangleShape fill({ w * frac, 3.f });
        fill.setOrigin({ 0.f, 1.5f });
        fill.setPosition({ left, midY });
        fill.setFillColor(accent);
        rt.draw(fill);

        // snapshot tick marks
        const int n = int(m_snaps.size());
        const float step = n > 1 ? w / float(n - 1) : 0.f;
        if (step > 2.5f)
            for (int i = 0; i < n; ++i)
            {
                sf::RectangleShape tick({ 1.f, 6.f });
                tick.setOrigin({ 0.5f, 3.f });
                tick.setPosition({ left + step * float(i), midY });
                tick.setFillColor(sf::Color(255, 255, 255, 28));
                rt.draw(tick);
            }

        // playhead handle
        const float hx = left + w * frac;
        sf::CircleShape handle(6.f);
        handle.setOrigin({ 6.f, 6.f });
        handle.setPosition({ hx, midY });
        handle.setFillColor(accent);
        handle.setOutlineColor(sf::Color(12, 16, 22));
        handle.setOutlineThickness(1.5f);
        rt.draw(handle);
    }

    if (!font) return;

    char buf[96];
    const float span = m_snaps.empty() ? liveTime : m_snaps.back().t;

    // left/right time labels
    rt.draw(makeText(*font, "0s", 11, { left, m_bar.position.y + 2.f },
                     sf::Color(120, 132, 144)));
    std::snprintf(buf, sizeof(buf), "%.0fs", span);
    sf::Text end = makeText(*font, buf, 11, { 0.f, 0.f }, sf::Color(120, 132, 144));
    end.setPosition({ right - end.getLocalBounds().size.x, m_bar.position.y + 2.f });
    rt.draw(end);

    // ----- status banner just above the bar -----
    if (m_reviewing)
    {
        std::snprintf(buf, sizeof(buf),
                      "REVIEWING  t = %.0fs / %.0fs    <- ->  step    Space  resume present    B  branch here",
                      viewedTime(), span);
        sf::Text t = makeText(*font, buf, 14, { 0.f, 0.f }, sf::Color(255, 210, 120));
        t.setPosition({ m_bar.position.x + (m_bar.size.x - t.getLocalBounds().size.x) / 2.f,
                        m_bar.position.y - 26.f });
        sf::RectangleShape bg({ t.getLocalBounds().size.x + 20.f, 22.f });
        bg.setOrigin({ 10.f, 4.f });
        bg.setPosition(t.getPosition());
        bg.setFillColor(sf::Color(0, 0, 0, 170));
        rt.draw(bg);
        rt.draw(t);
    }
    else
    {
        std::snprintf(buf, sizeof(buf), "REC  %d snapshots  -  drag to rewind",
                      int(m_snaps.size()));
        rt.draw(makeText(*font, buf, 11,
                         { left, m_bar.position.y + m_bar.size.y - 14.f },
                         sf::Color(110, 122, 134)));
    }
}
