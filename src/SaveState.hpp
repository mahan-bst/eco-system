#pragma once

#include <string>

class Simulation;

// Saving/loading the whole world to a binary .eco file, with native file
// dialogs to pick the path (S = save, O = open in the app).
namespace SaveState
{
    // Show a path picker. Returns an empty string if the user cancels.
    std::string askSavePath();
    std::string askOpenPath();

    bool save(const Simulation& sim, const std::string& path);
    bool load(Simulation& sim, const std::string& path);
}
