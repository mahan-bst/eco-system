#pragma once

#include <string>

class Simulation;
class Timeline;
class History;

// Saving/loading the whole world to a binary .eco file, with native file
// dialogs to pick the path (S = save, O = open in the app). The file holds the
// world, then the scrubbable timeline, then the chart history.
namespace SaveState
{
    // Show a path picker. Returns an empty string if the user cancels.
    std::string askSavePath();
    std::string askOpenPath();

    bool save(const Simulation& sim, const Timeline& timeline,
              const History& history, const std::string& path);
    bool load(Simulation& sim, Timeline& timeline,
              History& history, const std::string& path);
}
