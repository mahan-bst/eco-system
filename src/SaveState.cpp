#include "SaveState.hpp"

#include <fstream>
#include <iostream>

#include "History.hpp"
#include "Simulation.hpp"
#include "Timeline.hpp"

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
    #include <commdlg.h>
    #pragma comment(lib, "comdlg32.lib")
#endif

namespace
{
#ifdef _WIN32
    constexpr const char* FILTER =
        "EcoSim save (*.eco)\0*.eco\0All files (*.*)\0*.*\0";

    std::string dialog(bool saving)
    {
        char file[MAX_PATH] = "ecosim.eco";

        OPENFILENAMEA ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = FILTER;
        ofn.lpstrFile   = file;
        ofn.nMaxFile    = MAX_PATH;
        ofn.lpstrDefExt = "eco";
        // OFN_NOCHANGEDIR: the dialog must not change the working directory,
        // relative asset paths would silently break
        ofn.Flags = OFN_NOCHANGEDIR |
                    (saving ? OFN_OVERWRITEPROMPT
                            : OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST);

        const BOOL ok = saving ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn);
        return ok ? std::string(file) : std::string();
    }
#else
    // no native dialog available: fall back to the console
    std::string dialog(bool saving)
    {
        std::cout << (saving ? "save to path: " : "load from path: ") << std::flush;
        std::string path;
        std::getline(std::cin, path);
        return path;
    }
#endif
}

std::string SaveState::askSavePath() { return dialog(true); }
std::string SaveState::askOpenPath() { return dialog(false); }

bool SaveState::save(const Simulation& sim, const Timeline& timeline,
                     const History& history, const std::string& path)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    sim.serialize(f);           // the world
    timeline.serialize(f);      // the scrubbable snapshots
    history.serialize(f);       // the chart series
    return bool(f);
}

bool SaveState::load(Simulation& sim, Timeline& timeline,
                     History& history, const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    if (!sim.deserialize(f)) return false;
    timeline.deserialize(f);    // optional: absent in older files -> empty
    history.deserialize(f);     // optional: absent in older files -> empty
    return true;
}
