#pragma once

// ---------------------------------------------------------------------------
//  Every tunable number of the ecosystem lives here.
//  Bodies are fixed per species; evolution happens in the brains (Brain.hpp).
// ---------------------------------------------------------------------------

namespace cfg
{
    // ----- window / layout --------------------------------------------------
    inline constexpr unsigned WINDOW_W = 1920;
    inline constexpr unsigned WINDOW_H = 1080;

    // Simulation space (the coordinates all sim logic and saves use).
    // Deliberately unchanged when the window grew: the world is *displayed*
    // scaled up instead, so older .eco saves keep working unmodified.
    inline constexpr float WORLD_W = 1080.f;
    inline constexpr float WORLD_H = 868.f;

    // where the world is displayed inside the window (aspect-correct upscale)
    inline constexpr float VIEW_X = 16.f;
    inline constexpr float VIEW_Y = 16.f;
    inline constexpr float VIEW_H = float(WINDOW_H) - 32.f;        // 1048
    inline constexpr float VIEW_W = VIEW_H * WORLD_W / WORLD_H;    // ~1304

    // right-hand column: header, docked tool panels, charts, controls
    inline constexpr float PANEL_X = VIEW_X + VIEW_W + 16.f;
    inline constexpr float PANEL_Y = 16.f;
    inline constexpr float PANEL_W = float(WINDOW_W) - PANEL_X - 16.f;   // ~568

    // ----- time -------------------------------------------------------------
    inline constexpr float FIXED_DT = 1.f / 60.f;          // one simulation tick
    inline constexpr int   SPEED_STEPS[] = { 1, 2, 4, 8, 16, 32 };
    inline constexpr int   SPEED_COUNT = 6;

    // ----- food -------------------------------------------------------------
    // Generous defaults on purpose: generation-zero brains are random, so the
    // world has to be forgiving enough for them to bootstrap.
    inline constexpr int   FOOD_START   = 500;
    inline constexpr float FOOD_PER_SEC = 20.f;   // natural spawn rate
    inline constexpr int   FOOD_MAX     = 1000;
    inline constexpr float FOOD_ENERGY  = 30.f;   // energy per food item
    inline constexpr float FOOD_RADIUS  = 2.5f;

    // ----- populations ------------------------------------------------------
    inline constexpr int PREY_START = 160;
    inline constexpr int PRED_START = 14;
    inline constexpr int PREY_MAX   = 900;
    inline constexpr int PRED_MAX   = 250;

    // "Immigration": if a species falls below its floor, a few newcomers
    // appear (mutated descendants of survivors, or fresh random brains if the
    // species died out). Keeps the experiment from ending in a dead world.
    //
    // Keep these LOW. The floor is an extinction safety-net, not a steady
    // supply: if the prey floor is high relative to predation, predators feed
    // on the immigration drip forever, prey never rise above the floor to
    // reproduce, and prey evolution stalls. A small floor lets prey actually
    // bloom (and evolve) in the gaps between predator booms.
    inline constexpr int PREY_FLOOR = 12;
    inline constexpr int PRED_FLOOR = 4;

    // normalisation for the relative-velocity brain inputs (px/s -> ~[-1,1])
    inline constexpr float VEL_NORM = 200.f;

    // ----- evolution ----------------------------------------------------------
    // (fresh weights use Xavier/fan-in scaling, computed per layer in Brain.cpp)
    inline constexpr float MUTATION_SIGMA   = 0.08f;   // gaussian noise per weight
    inline constexpr float BRAIN_RESET_PROB = 0.005f;  // chance to rewire a weight
                                                       // ~1 of 194 weights per birth
                                                       // (was 0.02 = ~4, too violent)

    // ----- species bodies (identical for every individual) -------------------
    struct SpeciesCfg
    {
        float maxSpeed;     // px/s at full throttle
        float vision;       // sense radius, px
        float size;         // body radius, px
        float turnRate;     // rad/s at full turn output

        float baseCost;     // energy/s for being alive
        float moveCost;     // extra energy/s at full throttle (scales with throttle^2)

        float capacity;     // max stored energy
        float reproFrac;    // reproduce when energy >= reproFrac * capacity
        float childFrac;    // child receives childFrac * parent energy
        float parentKeep;   // parent keeps parentKeep * energy (rest = birth cost)
    };

    // Defaults reasoning (prey): cruising at ~60 % throttle burns
    // 1.4 + 3.8 * 0.36 ≈ 2.8 energy/s, one food item is worth 30, so a prey
    // must stumble into food roughly every 10 s to break even — easy once
    // brains learn to steer, survivable while they are still random.
    //
    // maxSpeed 100 is close to the predator's 105, and prey out-turn
    // predators (4.5 vs 3.5 rad/s): a *skilled* prey can escape, so survival
    // correlates with brain quality. That correlation is what lets prey
    // evolve at all. reproFrac 0.72 lets a lucky survivor breed sooner and
    // seed the next, better generation faster.
    inline constexpr SpeciesCfg PREY {
        /* maxSpeed   */ 100.f,
        /* vision     */ 110.f,
        /* size       */   5.f,
        /* turnRate   */   4.5f,
        /* baseCost   */   1.4f,
        /* moveCost   */   3.8f,
        /* capacity   */ 110.f,
        /* reproFrac  */   0.72f,
        /* childFrac  */   0.40f,
        /* parentKeep */   0.45f,
    };

    // Defaults reasoning (predators): faster and longer-sighted than prey
    // (else hunting can never work), but they MUST depend on catching prey
    // or their numbers never self-limit. baseCost 2.4 means an idle predator
    // starves in 170 / 2.4 ≈ 70 s (was a lazy ~140 s), so when prey get
    // scarce the predator population actually crashes instead of pinning at
    // the cap — restoring the boom/bust cycle that gives prey room to evolve.
    // A full-throttle chase burns 2.4 + 6.0 = 8.4 energy/s; a kill pays ~60,
    // so a hunter has ~7 s of chasing per meal: sprint-and-rest wins.
    inline constexpr SpeciesCfg PRED {
        /* maxSpeed   */ 105.f,
        /* vision     */ 150.f,
        /* size       */   7.f,
        /* turnRate   */   3.5f,
        /* baseCost   */   2.4f,
        /* moveCost   */   6.0f,
        /* capacity   */ 170.f,
        /* reproFrac  */   0.82f,
        /* childFrac  */   0.40f,
        /* parentKeep */   0.45f,
    };

    // ----- feeding ------------------------------------------------------------
    // Predator meal: gain = PRED_EAT_BASE + PRED_EAT_FRAC * preyEnergy
    inline constexpr float PRED_EAT_BASE = 25.f;
    inline constexpr float PRED_EAT_FRAC = 0.60f;

    // ----- live tunables ----------------------------------------------------
    // Values bound to the in-app slider panel (T key). They start at the
    // constants above and can be changed while the simulation runs.
    struct Tunables
    {
        float foodPerSec    = FOOD_PER_SEC;
        float foodEnergy    = FOOD_ENERGY;
        float mutationSigma = MUTATION_SIGMA;
        float preyCostMul   = 1.f;   // multiplies the whole prey energy bill
        float predCostMul   = 1.f;   // same for predators
        float predGainMul   = 1.f;   // multiplies energy gained per kill
        float preyVision    = PREY.vision;     // sense radius, px
        float preySpeed     = PREY.maxSpeed;   // px/s at full throttle
        float predVision    = PRED.vision;
        float predSpeed     = PRED.maxSpeed;
        bool  floorRefill   = true;   // immigration: respawn when a species is few
    };
    inline Tunables tune;

    inline float maxSpeedOf(const SpeciesCfg& s)
    {
        return (&s == &PRED) ? tune.predSpeed : tune.preySpeed;
    }

    // Energy burned per second at the given throttle (0..1).
    inline float metabolicCost(const SpeciesCfg& s, float throttle)
    {
        const float mul = (&s == &PRED) ? tune.predCostMul : tune.preyCostMul;
        return (s.baseCost + s.moveCost * throttle * throttle) * mul;
    }
}
