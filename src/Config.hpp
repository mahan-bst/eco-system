#pragma once

// ---------------------------------------------------------------------------
//  Every tunable number of the ecosystem lives here.
//  Bodies are fixed per species; evolution happens in the brains (Brain.hpp).
// ---------------------------------------------------------------------------

namespace cfg
{
    // ----- window / layout --------------------------------------------------
    inline constexpr unsigned WINDOW_W = 1600;
    inline constexpr unsigned WINDOW_H = 900;

    inline constexpr float WORLD_X = 16.f;    // world viewport inside the window
    inline constexpr float WORLD_Y = 16.f;
    inline constexpr float WORLD_W = 1080.f;  // simulation space (local coords 0..W, 0..H)
    inline constexpr float WORLD_H = 868.f;

    inline constexpr float PANEL_X = WORLD_X + WORLD_W + 16.f;
    inline constexpr float PANEL_W = WINDOW_W - PANEL_X - 16.f;

    // ----- time -------------------------------------------------------------
    inline constexpr float FIXED_DT = 1.f / 60.f;          // one simulation tick
    inline constexpr int   SPEED_STEPS[] = { 1, 2, 4, 8, 16, 32 };
    inline constexpr int   SPEED_COUNT = 6;

    // ----- food -------------------------------------------------------------
    inline constexpr int   FOOD_START   = 400;
    inline constexpr float FOOD_PER_SEC = 20.f;   // natural spawn rate
    inline constexpr int   FOOD_MAX     = 1000;
    inline constexpr float FOOD_ENERGY  = 28.f;   // energy per food item
    inline constexpr float FOOD_RADIUS  = 2.5f;

    // ----- populations ------------------------------------------------------
    inline constexpr int PREY_START = 160;
    inline constexpr int PRED_START = 24;
    inline constexpr int PREY_MAX   = 900;
    inline constexpr int PRED_MAX   = 400;

    // "Immigration": if a species falls below its floor, a few newcomers
    // appear (mutated descendants of survivors, or fresh random brains if the
    // species died out). Keeps the experiment from ending in a dead world.
    inline constexpr int PREY_FLOOR = 20;
    inline constexpr int PRED_FLOOR = 6;

    // normalisation for the relative-velocity brain inputs (px/s -> ~[-1,1])
    inline constexpr float VEL_NORM = 200.f;

    // ----- evolution ----------------------------------------------------------
    inline constexpr float BRAIN_INIT_SIGMA = 0.5f;   // fresh random weights
    inline constexpr float MUTATION_SIGMA   = 0.08f;  // gaussian noise per weight
    inline constexpr float BRAIN_RESET_PROB = 0.02f;  // chance to rewire a weight

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

    inline constexpr SpeciesCfg PREY {
        /* maxSpeed   */  90.f,
        /* vision     */ 110.f,
        /* size       */   5.f,
        /* turnRate   */   4.5f,
        /* baseCost   */   1.6f,
        /* moveCost   */   4.0f,
        /* capacity   */ 110.f,
        /* reproFrac  */   0.80f,
        /* childFrac  */   0.40f,
        /* parentKeep */   0.45f,
    };

    inline constexpr SpeciesCfg PRED {
        /* maxSpeed   */ 105.f,
        /* vision     */ 150.f,
        /* size       */   7.f,
        /* turnRate   */   3.5f,
        /* baseCost   */   1.2f,
        /* moveCost   */   4.5f,
        /* capacity   */ 170.f,
        /* reproFrac  */   0.80f,
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
