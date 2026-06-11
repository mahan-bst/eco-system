#pragma once

// Headless warm-up / training mode.
//
// Usage:  MyGame --train <minutes> [outfile.eco] [-j workers]
//         MyGame --train 5                 -> 5 sim-minutes -> trained_0.eco ..N
//         MyGame --train 10 hunters.eco    -> hunters_0.eco, hunters_1.eco, ...
//         MyGame --train=3 -o world.eco -j 8
//
// Runs many independent worlds in parallel (one per hardware thread by
// default, or -j N), with no window/rendering/chart sampling, as fast as the
// CPU allows — best-of-N evolution that actually uses all your cores. Each
// world is saved to its own numbered .eco file (the base name with _<index>
// inserted), and the scoreboard tells you which is the best. Open any of them
// in the GUI with `O`.
namespace Headless
{
    // Returns an exit code (0/1) if this was a --train invocation and it ran;
    // returns -1 if no --train flag was present, so main() should launch the
    // normal interactive app.
    int run(int argc, char** argv);
}
