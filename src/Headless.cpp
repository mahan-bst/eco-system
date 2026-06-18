#include "Headless.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "Config.hpp"
#include "History.hpp"
#include "SaveState.hpp"
#include "Simulation.hpp"
#include "Timeline.hpp"

namespace
{
    bool startsWith(const std::string& s, const char* p)
    {
        return s.rfind(p, 0) == 0;
    }

    // How "evolved" a finished world is: how good its best brain of each
    // species got (offspring is the real fitness), with total population as a
    // gentle tiebreaker. Used to pick which parallel world to keep.
    double worldScore(const Simulation& s)
    {
        double sc = 0.0;
        if (!s.preyChampions().empty()) sc += s.preyChampions().at(0).offspring;
        if (!s.predChampions().empty()) sc += s.predChampions().at(0).offspring;
        sc += 1e-4 * double(s.preyList().size() + s.predList().size());
        return sc;
    }

    int topOffspring(const ChampionArchive& a)
    {
        return a.empty() ? 0 : a.at(0).offspring;
    }

    // "trained.eco" + 3 -> "trained_3.eco"   |   "world" + 0 -> "world_0"
    std::string numbered(const std::string& path, int i)
    {
        const std::size_t slash = path.find_last_of("/\\");
        const std::size_t dot   = path.find_last_of('.');
        const bool hasExt = dot != std::string::npos &&
                            (slash == std::string::npos || dot > slash);
        if (hasExt)
            return path.substr(0, dot) + "_" + std::to_string(i) + path.substr(dot);
        return path + "_" + std::to_string(i);
    }
}

int Headless::run(int argc, char** argv)
{
    float minutes = -1.f;
    std::string path = "trained.eco";
    int workers = 0;   // 0 -> auto (all hardware threads)

    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--train" && i + 1 < argc)
            minutes = float(std::atof(argv[++i]));
        else if (startsWith(a, "--train="))
            minutes = float(std::atof(a.c_str() + 8));
        else if ((a == "-o" || a == "--out") && i + 1 < argc)
            path = argv[++i];
        else if ((a == "-j" || a == "--workers") && i + 1 < argc)
            workers = std::atoi(argv[++i]);
        else if (minutes >= 0.f && !a.empty() && a[0] != '-')
            path = a;                       // positional output path
    }

    if (minutes < 0.f)
        return -1;                          // not a training run -> GUI

    if (workers <= 0)
        workers = int(std::max(1u, std::thread::hardware_concurrency()));

    const long long steps = std::llround(double(minutes) * 60.0 / cfg::FIXED_DT);
    std::printf("[train] %d parallel worlds x %.1f sim-min (%lld steps each) -> \"%s\"\n",
                workers, minutes, steps, path.c_str());
    std::fflush(stdout);

    // each worker evolves its own independent world (random generation zero),
    // recording its evolution into its own timeline (saved with the world)
    const std::size_t nWorlds = std::size_t(workers);
    std::vector<Simulation> worlds(nWorlds);   // 'nWorlds' (a variable) avoids
                                               // the most-vexing-parse
    std::vector<Timeline> timelines(nWorlds);
    std::vector<History>  histories(nWorlds);

    std::atomic<long long> doneSteps{ 0 };
    std::atomic<int> finished{ 0 };

    const auto t0 = std::chrono::steady_clock::now();

    std::vector<std::thread> pool;
    pool.reserve(std::size_t(workers));
    for (int w = 0; w < workers; ++w)
    {
        pool.emplace_back([&, w]
        {
            Simulation& s = worlds[std::size_t(w)];
            Timeline& tl  = timelines[std::size_t(w)];
            History&  hi  = histories[std::size_t(w)];
            for (long long i = 0; i < steps; ++i)
            {
                s.step(cfg::FIXED_DT);
                tl.record(s);                   // snapshot the evolution
                hi.update(s, cfg::FIXED_DT);     // and the chart series
                if ((i & 1023) == 1023)     // report in batches, not every step
                    doneSteps.fetch_add(1024, std::memory_order_relaxed);
            }
            doneSteps.fetch_add(steps & 1023, std::memory_order_relaxed);
            finished.fetch_add(1, std::memory_order_relaxed);
        });
    }

    // progress monitor on the main thread while the workers run
    const long long total = std::max<long long>(1, steps * workers);
    while (finished.load(std::memory_order_relaxed) < workers)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        const double wall = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t0).count();
        const long long done = doneSteps.load(std::memory_order_relaxed);
        const double simSec = double(done) * cfg::FIXED_DT;
        std::printf("\r[train] %3.0f%% | %d workers | %7.0fx realtime   ",
                    100.0 * double(done) / double(total),
                    workers, wall > 1e-6 ? simSec / wall : 0.0);
        std::fflush(stdout);
    }
    for (auto& t : pool) t.join();

    const double wall = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t0).count();
    const double simSecTotal = double(minutes) * 60.0 * workers;
    std::printf("\r[train] finished %d worlds in %.1f s wall (%.0fx realtime aggregate)   \n",
                workers, wall, wall > 1e-6 ? simSecTotal / wall : 0.0);

    // save every world to its own numbered file, and report the spread
    int best = 0;
    double bestScore = -1.0;
    int saved = 0;
    for (int w = 0; w < workers; ++w)
    {
        const Simulation& s = worlds[std::size_t(w)];
        const double sc = worldScore(s);
        const std::string out = numbered(path, w);
        const bool ok = SaveState::save(s, timelines[std::size_t(w)],
                                        histories[std::size_t(w)], out);
        if (ok) ++saved;
        std::printf("[train]   world %2d | prey %4zu pred %4zu | best %d / %d kids | score %5.1f | %s%s\n",
                    w, s.preyList().size(), s.predList().size(),
                    topOffspring(s.preyChampions()), topOffspring(s.predChampions()),
                    sc, out.c_str(), ok ? "" : "  (WRITE FAILED)");
        if (sc > bestScore) { bestScore = sc; best = w; }
    }

    std::printf("[train] saved %d/%d worlds. best = world %d (%s, score %.1f)\n",
                saved, workers, best, numbered(path, best).c_str(), bestScore);
    return saved == workers ? 0 : 1;
}
