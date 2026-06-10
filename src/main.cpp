// ---------------------------------------------------------------------------
//  EcoSim — a neuro-evolution sandbox
//
//  Food spawns on the map. Prey eat food, predators eat prey. Bodies are
//  identical within a species — every creature is driven by its own small
//  neural network, inherited with mutation. Whatever brain survives and
//  reproduces best takes over the gene pool.
//
//  Click a creature to watch its brain think. Written against SFML 3.
// ---------------------------------------------------------------------------

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>

#include "BrainView.hpp"
#include "Config.hpp"
#include "Charts.hpp"
#include "History.hpp"
#include "Leaderboard.hpp"
#include "SaveState.hpp"
#include "Simulation.hpp"
#include "SliderPanel.hpp"
#include "Utils.hpp"

// ----- palette --------------------------------------------------------------
namespace pal
{
    const sf::Color background(13, 17, 23);
    const sf::Color worldBg(18, 24, 30);
    const sf::Color worldGrid(255, 255, 255, 7);
    const sf::Color worldBorder(46, 60, 72);

    const sf::Color food(124, 196, 88);
    const sf::Color prey(96, 196, 208);
    const sf::Color predator(235, 105, 86);

    const sf::Color text(208, 218, 228);
    const sf::Color textDim(130, 142, 154);

    const sf::Color statEnergy(255, 196, 84);
    const sf::Color statAge(140, 162, 255);
}

static sf::Color scaled(sf::Color c, float f)
{
    return { std::uint8_t(c.r * f), std::uint8_t(c.g * f), std::uint8_t(c.b * f), c.a };
}

static sf::Text makeText(const sf::Font& font, const std::string& str,
                         unsigned size, sf::Vector2f pos, sf::Color color)
{
    sf::Text t(font, str, size);
    t.setFillColor(color);
    t.setPosition(pos);
    return t;
}

// ----- selection ----------------------------------------------------------------

struct Selection
{
    std::uint64_t id = 0;
    bool isPred = false;

    void clear() { id = 0; }

    // re-find the selected animal each frame (vectors reshuffle every tick)
    const Animal* resolve(const Simulation& sim)
    {
        if (id == 0) return nullptr;
        const auto& list = isPred ? sim.predList() : sim.preyList();
        for (const auto& a : list)
            if (a.id == id) return &a;
        id = 0;   // it died — deselect
        return nullptr;
    }

    // pick the creature nearest to a world-space click
    void pick(const Simulation& sim, sf::Vector2f p)
    {
        float bestD2 = 1e18f;
        id = 0;
        const auto consider = [&](const std::vector<Animal>& list, float size, bool pred)
        {
            const float r = size + 10.f;
            for (const auto& a : list)
            {
                const float d2 = dist2(a.pos, p);
                if (d2 <= r * r && d2 < bestD2)
                {
                    bestD2 = d2;
                    id = a.id;
                    isPred = pred;
                }
            }
        };
        consider(sim.preyList(), cfg::PREY.size, false);
        consider(sim.predList(), cfg::PRED.size, true);
    }
};

// ----- world rendering --------------------------------------------------------

static void drawWorld(sf::RenderWindow& window, const Simulation& sim,
                      bool showVision, const Animal* selected, bool selectedIsPred)
{
    sf::RectangleShape bg({ cfg::WORLD_W, cfg::WORLD_H });
    bg.setFillColor(pal::worldBg);
    window.draw(bg);

    // subtle grid so movement reads better on screen
    sf::VertexArray grid(sf::PrimitiveType::Lines);
    for (float x = 80.f; x < cfg::WORLD_W; x += 80.f)
    {
        grid.append(sf::Vertex{ { x, 0.f }, pal::worldGrid });
        grid.append(sf::Vertex{ { x, cfg::WORLD_H }, pal::worldGrid });
    }
    for (float y = 80.f; y < cfg::WORLD_H; y += 80.f)
    {
        grid.append(sf::Vertex{ { 0.f, y }, pal::worldGrid });
        grid.append(sf::Vertex{ { cfg::WORLD_W, y }, pal::worldGrid });
    }
    window.draw(grid);

    // optional vision rings
    if (showVision)
    {
        sf::CircleShape ring;
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineThickness(1.f);
        const float pv = cfg::tune.preyVision;
        ring.setRadius(pv);
        ring.setOrigin({ pv, pv });
        ring.setOutlineColor(sf::Color(96, 196, 208, 18));
        for (const auto& a : sim.preyList())
        {
            ring.setPosition(a.pos);
            window.draw(ring);
        }
        const float dv = cfg::tune.predVision;
        ring.setRadius(dv);
        ring.setOrigin({ dv, dv });
        ring.setOutlineColor(sf::Color(235, 105, 86, 22));
        for (const auto& a : sim.predList())
        {
            ring.setPosition(a.pos);
            window.draw(ring);
        }
    }

    // food
    sf::CircleShape dot(cfg::FOOD_RADIUS);
    dot.setOrigin({ cfg::FOOD_RADIUS, cfg::FOOD_RADIUS });
    dot.setFillColor(pal::food);
    for (const auto& f : sim.foodList())
    {
        dot.setPosition(f.pos);
        window.draw(dot);
    }

    // prey: circle + small "eye" showing the heading; brightness = energy
    sf::CircleShape body(cfg::PREY.size);
    body.setOrigin({ cfg::PREY.size, cfg::PREY.size });
    const float er = cfg::PREY.size * 0.32f;
    sf::CircleShape eye(er);
    eye.setOrigin({ er, er });
    for (const auto& a : sim.preyList())
    {
        const float bright = 0.45f + 0.55f * clampf(a.energy / cfg::PREY.capacity, 0.f, 1.f);
        body.setPosition(a.pos);
        body.setFillColor(scaled(pal::prey, bright));
        window.draw(body);

        const sf::Vector2f dir(std::cos(a.heading), std::sin(a.heading));
        eye.setPosition(a.pos + dir * (cfg::PREY.size * 0.55f));
        eye.setFillColor(scaled(pal::prey, bright * 0.45f));
        window.draw(eye);
    }

    // predators: triangle pointing at the heading; brightness = energy
    const float s = cfg::PRED.size;
    sf::ConvexShape tri(3);
    tri.setPoint(0, {  1.7f * s,  0.f });
    tri.setPoint(1, { -1.0f * s,  0.95f * s });
    tri.setPoint(2, { -1.0f * s, -0.95f * s });
    for (const auto& a : sim.predList())
    {
        const float bright = 0.45f + 0.55f * clampf(a.energy / cfg::PRED.capacity, 0.f, 1.f);
        tri.setPosition(a.pos);
        tri.setRotation(sf::radians(a.heading));
        tri.setFillColor(scaled(pal::predator, bright));
        window.draw(tri);
    }

    // selection marker: white ring + that creature's vision circle
    if (selected)
    {
        const auto& S = selectedIsPred ? cfg::PRED : cfg::PREY;
        const float vr = selectedIsPred ? cfg::tune.predVision : cfg::tune.preyVision;

        sf::CircleShape vis(vr);
        vis.setOrigin({ vr, vr });
        vis.setPosition(selected->pos);
        vis.setFillColor(sf::Color::Transparent);
        vis.setOutlineColor(sf::Color(220, 230, 240, 45));
        vis.setOutlineThickness(1.f);
        window.draw(vis);

        const float r = S.size + 5.f;
        sf::CircleShape mark(r);
        mark.setOrigin({ r, r });
        mark.setPosition(selected->pos);
        mark.setFillColor(sf::Color::Transparent);
        mark.setOutlineColor(sf::Color(230, 240, 250, 210));
        mark.setOutlineThickness(2.f);
        window.draw(mark);

        // little energy bar floating above the star of the show
        const float frac = clampf(selected->energy / S.capacity, 0.f, 1.f);
        const sf::Vector2f barPos = selected->pos + sf::Vector2f(0.f, -(S.size + 13.f));
        sf::RectangleShape barBg({ 30.f, 4.f });
        barBg.setOrigin({ 15.f, 2.f });
        barBg.setPosition(barPos);
        barBg.setFillColor(sf::Color(0, 0, 0, 150));
        window.draw(barBg);
        sf::RectangleShape bar({ 30.f * frac, 4.f });
        bar.setOrigin({ 15.f, 2.f });
        bar.setPosition(barPos);
        bar.setFillColor(sf::Color(std::uint8_t(230 - 150.f * frac),
                                   std::uint8_t(80 + 140.f * frac), 80));
        window.draw(bar);
    }
}

// ----- side panel ---------------------------------------------------------------

static void drawPanel(sf::RenderWindow& window, const Simulation& sim,
                      const History& history, const sf::Font* font,
                      bool paused, int speedMult, bool showVision, bool showTuning,
                      const std::string& status)
{
    const float x = cfg::PANEL_X;
    char buf[128];

    if (font)
    {
        window.draw(makeText(*font, "ECOSYSTEM", 20, { x, 14.f }, pal::text));
        window.draw(makeText(*font, "neuro-evolution sandbox", 12,
                             { x + 132.f, 21.f }, pal::textDim));

        if (paused)
            std::snprintf(buf, sizeof(buf), "t = %.0f s    PAUSED  ( . to step)", sim.time());
        else
            std::snprintf(buf, sizeof(buf), "t = %.0f s    speed x%d", sim.time(), speedMult);
        window.draw(makeText(*font, buf, 14, { x, 46.f }, pal::textDim));

        // counts, colour-coded to match the world
        float cx = x;
        std::snprintf(buf, sizeof(buf), "prey %zu", sim.preyList().size());
        sf::Text t1 = makeText(*font, buf, 15, { cx, 70.f }, pal::prey);
        window.draw(t1);
        cx += t1.getLocalBounds().size.x + 24.f;

        std::snprintf(buf, sizeof(buf), "predators %zu", sim.predList().size());
        sf::Text t2 = makeText(*font, buf, 15, { cx, 70.f }, pal::predator);
        window.draw(t2);
        cx += t2.getLocalBounds().size.x + 24.f;

        std::snprintf(buf, sizeof(buf), "food %zu", sim.foodList().size());
        window.draw(makeText(*font, buf, 15, { cx, 70.f }, pal::food));

        if (!status.empty())
            window.draw(makeText(*font, status, 13, { x, 94.f },
                                 sf::Color(255, 196, 84)));
    }

    // charts
    const float chartH = 218.f, gap = 10.f;
    float cy = 118.f;

    drawChart(window, { { x, cy }, { cfg::PANEL_W, chartH } }, "Population",
              history.samples(),
              { { &Sample::preyCount, pal::prey,     "prey",      true },
                { &Sample::predCount, pal::predator, "predators", true },
                { &Sample::foodCount, scaled(pal::food, 0.55f), "food", true } },
              true, font);
    cy += chartH + gap;

    drawChart(window, { { x, cy }, { cfg::PANEL_W, chartH } },
              "Prey - avg energy & lifespan (auto-scaled)",
              history.samples(),
              { { &Sample::preyEnergy, pal::statEnergy, "energy" },
                { &Sample::preyAge,    pal::statAge,    "age"    } },
              false, font);
    cy += chartH + gap;

    drawChart(window, { { x, cy }, { cfg::PANEL_W, chartH } },
              "Predator - avg energy & lifespan (auto-scaled)",
              history.samples(),
              { { &Sample::predEnergy, pal::statEnergy, "energy" },
                { &Sample::predAge,    pal::statAge,    "age"    } },
              false, font);
    cy += chartH + 14.f;

    if (font)
    {
        window.draw(makeText(*font, "Space  pause / resume      + / -  faster / slower",
                             12, { x, cy }, pal::textDim));
        window.draw(makeText(*font,
                             ".  single step (paused)      R  restart      S / O  save / load",
                             12, { x, cy + 18.f }, pal::textDim));
        std::snprintf(buf, sizeof(buf), "V  vision rings (%s)      T  tuning sliders (%s)",
                      showVision ? "on" : "off", showTuning ? "on" : "off");
        window.draw(makeText(*font, buf, 12, { x, cy + 36.f }, pal::textDim));
        window.draw(makeText(*font,
                             "left-click  inspect a brain      right-click  drop food",
                             12, { x, cy + 54.f }, pal::textDim));
        window.draw(makeText(*font,
                             "F  follow cam      L  leaderboard      scroll  zoom      Esc  deselect",
                             12, { x, cy + 72.f }, pal::textDim));
    }
}

// ----- entry point ----------------------------------------------------------------

int main()
{
    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;

    sf::RenderWindow window(sf::VideoMode({ cfg::WINDOW_W, cfg::WINDOW_H }),
                            "EcoSim - neuro-evolution",
                            sf::Style::Titlebar | sf::Style::Close,
                            sf::State::Windowed, settings);
    window.setFramerateLimit(60);

    // any reasonable system font will do; layout fails gracefully without one
    sf::Font font;
    bool hasFont = false;
    const char* fontPaths[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "assets/font.ttf",
    };
    for (const char* p : fontPaths)
        if (font.openFromFile(p)) { hasFont = true; break; }

    // the world is drawn through its own view, so it is clipped to its
    // viewport and entities can be drawn in local (0..W, 0..H) coordinates
    sf::View worldView(sf::FloatRect({ 0.f, 0.f }, { cfg::WORLD_W, cfg::WORLD_H }));
    worldView.setViewport(sf::FloatRect(
        { cfg::WORLD_X / cfg::WINDOW_W, cfg::WORLD_Y / cfg::WINDOW_H },
        { cfg::WORLD_W / cfg::WINDOW_W, cfg::WORLD_H / cfg::WINDOW_H }));

    Simulation sim;
    History history;
    history.reset();

    // live-tuning sliders, bound straight into cfg::tune (toggle with T)
    SliderPanel tuning;
    tuning.add("food / sec",  &cfg::tune.foodPerSec,    0.f,   60.f,  "%.0f");
    tuning.add("food energy", &cfg::tune.foodEnergy,    5.f,   60.f,  "%.0f");
    tuning.add("mutation",    &cfg::tune.mutationSigma, 0.f,   0.30f, "%.2f");
    tuning.add("prey cost x", &cfg::tune.preyCostMul,   0.4f,  2.0f,  "%.2f");
    tuning.add("pred cost x", &cfg::tune.predCostMul,   0.4f,  2.0f,  "%.2f");
    tuning.add("pred meal x", &cfg::tune.predGainMul,   0.4f,  2.0f,  "%.2f");
    tuning.add("prey vision", &cfg::tune.preyVision,   30.f, 250.f,  "%.0f");
    tuning.add("prey speed",  &cfg::tune.preySpeed,    30.f, 180.f,  "%.0f");
    tuning.add("pred vision", &cfg::tune.predVision,   30.f, 250.f,  "%.0f");
    tuning.add("pred speed",  &cfg::tune.predSpeed,    30.f, 180.f,  "%.0f");
    tuning.setPosition({ cfg::WORLD_X + 12.f,
                         cfg::WORLD_Y + cfg::WORLD_H - tuning.height() - 12.f });

    const sf::Vector2f brainViewPos(cfg::WORLD_X + 12.f, cfg::WORLD_Y + 12.f);

    Leaderboard leaderboard;
    leaderboard.setPosition({ cfg::WORLD_X + cfg::WORLD_W - 252.f - 12.f,
                              cfg::WORLD_Y + 12.f });

    Selection selection;
    bool paused = false;
    bool showVision = false;
    int  speedIdx = 0;

    // ----- follow cam ("nature documentary mode") -----
    bool  follow = false;          // F toggles; engages while something is selected
    float followZoom = 3.2f;       // mouse-wheel adjustable
    float camZoom = 1.f;           // smoothed actual zoom
    sf::Vector2f camCenter(cfg::WORLD_W / 2.f, cfg::WORLD_H / 2.f);
    bool  hadSelection = false;    // for the "your creature died" drama
    float lastSelEnergy = 0.f;
    bool  lastSelWasPred = false;
    float liveBlink = 0.f;         // timer for the blinking LIVE tag

    // transient feedback line ("state saved", ...) shown in the panel header
    std::string status;
    float statusTimer = 0.f;
    const auto setStatus = [&](std::string s)
    {
        status = std::move(s);
        statusTimer = 4.f;
    };

    while (window.isOpen())
    {
        // ----- input -----
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            else if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                switch (key->code)
                {
                case sf::Keyboard::Key::Space:
                    paused = !paused;
                    break;
                case sf::Keyboard::Key::Equal:
                case sf::Keyboard::Key::Add:
                case sf::Keyboard::Key::Up:
                    speedIdx = std::min(speedIdx + 1, cfg::SPEED_COUNT - 1);
                    break;
                case sf::Keyboard::Key::Hyphen:
                case sf::Keyboard::Key::Subtract:
                case sf::Keyboard::Key::Down:
                    speedIdx = std::max(speedIdx - 1, 0);
                    break;
                case sf::Keyboard::Key::Period:
                    if (paused)
                    {
                        sim.step(cfg::FIXED_DT);
                        history.update(sim, cfg::FIXED_DT);
                    }
                    break;
                case sf::Keyboard::Key::V:
                    showVision = !showVision;
                    break;
                case sf::Keyboard::Key::L:
                    leaderboard.visible = !leaderboard.visible;
                    break;
                case sf::Keyboard::Key::T:
                    tuning.visible = !tuning.visible;
                    break;
                case sf::Keyboard::Key::F:
                    follow = !follow;
                    if (follow)
                        setStatus(selection.id != 0
                                      ? "follow cam ON"
                                      : "follow cam armed - click a creature");
                    else
                        setStatus("follow cam off");
                    break;
                case sf::Keyboard::Key::Escape:
                    selection.clear();
                    break;
                case sf::Keyboard::Key::R:
                    sim.reset();
                    history.reset();
                    selection.clear();
                    break;
                case sf::Keyboard::Key::S:
                {
                    const std::string path = SaveState::askSavePath();
                    if (!path.empty())
                        setStatus(SaveState::save(sim, path)
                                      ? "state saved"
                                      : "save FAILED - could not write file");
                    break;
                }
                case sf::Keyboard::Key::O:
                {
                    const std::string path = SaveState::askOpenPath();
                    if (!path.empty())
                    {
                        if (SaveState::load(sim, path))
                        {
                            history.reset();      // charts restart at the loaded time
                            selection.clear();
                            setStatus("state loaded");
                        }
                        else
                            setStatus("load FAILED - not a valid save file");
                    }
                    break;
                }
                default:
                    break;
                }
            }

            else if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>())
            {
                const sf::Vector2f pix(float(mb->position.x), float(mb->position.y));
                const sf::Vector2f p = window.mapPixelToCoords(mb->position, worldView);
                const bool inWorld = p.x >= 0.f && p.x <= cfg::WORLD_W &&
                                     p.y >= 0.f && p.y <= cfg::WORLD_H;
                const bool onBrainView =
                    selection.id != 0 && BrainView::bounds(brainViewPos).contains(pix);

                if (mb->button == sf::Mouse::Button::Left)
                {
                    std::uint64_t lbId = 0;
                    bool lbPred = false;
                    if (tuning.onMousePressed(pix))
                    {
                        // sliders ate the click
                    }
                    else if (leaderboard.onMousePressed(pix, lbId, lbPred))
                    {
                        if (lbId != 0)              // a row: follow that creature
                        {
                            selection.id = lbId;
                            selection.isPred = lbPred;
                            follow = true;
                            setStatus("follow cam ON");
                        }
                    }
                    else if (!onBrainView && inWorld)
                        selection.pick(sim, p);     // empty space deselects
                }
                else if (mb->button == sf::Mouse::Button::Right)
                {
                    std::uint64_t d1 = 0;
                    bool d2 = false;
                    if (inWorld && !onBrainView &&
                        !leaderboard.onMousePressed(pix, d1, d2))
                        sim.spawnFoodBurst(p, 20);
                }
            }

            else if (const auto* mw = event->getIf<sf::Event::MouseWheelScrolled>())
            {
                if (follow)
                    followZoom = clampf(followZoom * (1.f + 0.12f * mw->delta),
                                        1.6f, 6.5f);
            }

            else if (const auto* mm = event->getIf<sf::Event::MouseMoved>())
                tuning.onMouseMoved({ float(mm->position.x), float(mm->position.y) });

            else if (event->is<sf::Event::MouseButtonReleased>())
                tuning.onMouseReleased();
        }

        // ----- simulate -----
        if (!paused)
        {
            const int steps = cfg::SPEED_STEPS[speedIdx];
            for (int i = 0; i < steps; ++i)
            {
                sim.step(cfg::FIXED_DT);
                history.update(sim, cfg::FIXED_DT);
            }
        }

        const Animal* selected = selection.resolve(sim);

        if (leaderboard.visible)
            leaderboard.update(sim);

        // the creature we were filming just vanished — break the bad news
        if (hadSelection && !selected && follow)
        {
            if (lastSelWasPred)
                setStatus("your predator starved...");
            else
                setStatus(lastSelEnergy > 8.f ? "your prey got EATEN!"
                                              : "your prey starved...");
        }
        hadSelection = selected != nullptr;
        if (selected)
        {
            lastSelEnergy  = selected->energy;
            lastSelWasPred = selection.isPred;
        }

        // ----- camera: glide toward the followed creature, else zoom out -----
        const bool filming = follow && selected;
        const float targetZoom = filming ? followZoom : 1.f;
        sf::Vector2f targetCenter(cfg::WORLD_W / 2.f, cfg::WORLD_H / 2.f);
        if (filming)
        {
            // lead the shot a little toward where the creature is heading
            const sf::Vector2f dir(std::cos(selected->heading),
                                   std::sin(selected->heading));
            targetCenter = selected->pos + dir * 26.f;
        }
        const float k = 1.f - std::exp(-5.f * cfg::FIXED_DT);
        camZoom   += (targetZoom - camZoom) * k;
        camCenter += (targetCenter - camCenter) * k;

        // never show the void outside the world
        const float hw = cfg::WORLD_W / (2.f * camZoom);
        const float hh = cfg::WORLD_H / (2.f * camZoom);
        camCenter.x = clampf(camCenter.x, hw, cfg::WORLD_W - hw);
        camCenter.y = clampf(camCenter.y, hh, cfg::WORLD_H - hh);

        worldView.setCenter(camCenter);
        worldView.setSize({ cfg::WORLD_W / camZoom, cfg::WORLD_H / camZoom });

        liveBlink += cfg::FIXED_DT;

        if (statusTimer > 0.f)
        {
            statusTimer -= cfg::FIXED_DT;   // one frame of wall time
            if (statusTimer <= 0.f) status.clear();
        }

        // ----- render -----
        window.clear(pal::background);

        window.setView(worldView);
        drawWorld(window, sim, showVision, selected, selection.isPred);
        window.setView(window.getDefaultView());

        sf::RectangleShape border({ cfg::WORLD_W, cfg::WORLD_H });
        border.setPosition({ cfg::WORLD_X, cfg::WORLD_Y });
        border.setFillColor(sf::Color::Transparent);
        border.setOutlineColor(pal::worldBorder);
        border.setOutlineThickness(1.f);
        window.draw(border);

        // cinematic letterbox while the follow cam is engaged
        const float follow01 = clampf((camZoom - 1.f) / 1.8f, 0.f, 1.f);
        if (follow01 > 0.02f)
        {
            const float bh = 44.f * follow01;
            sf::RectangleShape barShape({ cfg::WORLD_W, bh });
            barShape.setFillColor(sf::Color(0, 0, 0, std::uint8_t(165 * follow01)));
            barShape.setPosition({ cfg::WORLD_X, cfg::WORLD_Y });
            window.draw(barShape);
            barShape.setPosition({ cfg::WORLD_X, cfg::WORLD_Y + cfg::WORLD_H - bh });
            window.draw(barShape);

            if (hasFont && selected && follow01 > 0.6f)
            {
                const std::uint8_t a = std::uint8_t(255 * follow01);
                char buf[96];
                std::snprintf(buf, sizeof(buf),
                              "FOLLOWING %s #%llu      energy %.0f      age %.0fs",
                              selection.isPred ? "PREDATOR" : "PREY",
                              static_cast<unsigned long long>(selected->id),
                              selected->energy, selected->age);
                sf::Text title = makeText(font, buf, 15, { 0.f, 0.f },
                                          selection.isPred
                                              ? sf::Color(235, 105, 86, a)
                                              : sf::Color(96, 196, 208, a));
                title.setPosition({ cfg::WORLD_X +
                                        (cfg::WORLD_W -
                                         title.getLocalBounds().size.x) / 2.f,
                                    cfg::WORLD_Y + bh / 2.f - 11.f });
                window.draw(title);

                // blinking LIVE tag, because every nature documentary needs one
                const std::uint8_t blink =
                    std::sin(liveBlink * 4.f) > 0.f ? a : std::uint8_t(a / 5);
                window.draw(makeText(font, "LIVE", 14,
                                     { cfg::WORLD_X + 18.f,
                                       cfg::WORLD_Y + bh / 2.f - 10.f },
                                     sf::Color(255, 70, 70, blink)));

                window.draw(makeText(font, "scroll to zoom", 11,
                                     { cfg::WORLD_X + 18.f,
                                       cfg::WORLD_Y + cfg::WORLD_H - bh / 2.f - 8.f },
                                     sf::Color(190, 200, 210, std::uint8_t(a / 2))));
            }
        }

        drawPanel(window, sim, history, hasFont ? &font : nullptr,
                  paused, cfg::SPEED_STEPS[speedIdx], showVision, tuning.visible,
                  status);

        if (selected)
            BrainView::draw(window, brainViewPos, *selected, selection.isPred,
                            hasFont ? &font : nullptr);

        leaderboard.draw(window, hasFont ? &font : nullptr, selection.id);
        tuning.draw(window, hasFont ? &font : nullptr);

        window.display();
    }
    return 0;
}
