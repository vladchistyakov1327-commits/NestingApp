#include "GeneticAlgorithm.h"
#include <algorithm>
#include <numeric>
#include <future>
#include <sstream>
#include <unordered_map>
#include <iomanip>
#include <iostream>

// ─── initPopulation ──────────────────────────────────────────────────────────
std::vector<Chromosome> GeneticAlgorithm::initPopulation(
    int n, std::mt19937& rng, const AdaptiveParams& /*params*/) const
{
    int np = (int)parts_->size();
    std::vector<int> base(np);
    std::iota(base.begin(), base.end(), 0);

    std::vector<Chromosome> pop;
    pop.reserve(n);

    for (int i = 0; i < n; ++i) {
        Chromosome c;
        c.order = base;

        if (i == 0) {
            // Хромосома 0: по убыванию площади
            std::sort(c.order.begin(), c.order.end(), [&](int a, int b) {
                return (*parts_)[a]->area() > (*parts_)[b]->area();
            });
        } else if (i == 1) {
            // Хромосома 1: по убыванию соотношения сторон (длинные первыми)
            std::sort(c.order.begin(), c.order.end(), [&](int a, int b) {
                Rect ra = (*parts_)[a]->boundingBox();
                Rect rb = (*parts_)[b]->boundingBox();
                double aw = std::max(ra.w, ra.h) / std::max(GEO_EPS, std::min(ra.w, ra.h));
                double bw = std::max(rb.w, rb.h) / std::max(GEO_EPS, std::min(rb.w, rb.h));
                return aw > bw;
            });
        } else if (i == 2) {
            // Хромосома 2: по периметру bounding box (большой периметр первым)
            std::sort(c.order.begin(), c.order.end(), [&](int a, int b) {
                Rect ra = (*parts_)[a]->boundingBox();
                Rect rb = (*parts_)[b]->boundingBox();
                return (ra.w + ra.h) > (rb.w + rb.h);
            });
        } else {
            std::shuffle(c.order.begin(), c.order.end(), rng);
        }

        c.angles.resize(np);
        for (int j = 0; j < np; ++j) {
            if (i < 3 || allowedAngles.empty())
                c.angles[j] = 0;
            else
                c.angles[j] = allowedAngles[rng() % allowedAngles.size()];
        }

        pop.push_back(std::move(c));
    }
    return pop;
}

// ─── simulate ────────────────────────────────────────────────────────────────
std::pair<int,double> GeneticAlgorithm::simulate(
    const std::vector<int>& order,
    const std::vector<double>& angles,
    NFPCache& cache) const
{
    Sheet sheet = sheetTemplate_;
    sheet.placed.clear();
    int placed = 0;

    for (int i = 0; i < (int)order.size(); ++i) {
        const Part& part = *(*parts_)[order[i]];
        auto pl = sheet.findBestPlacement(part, {angles[i]}, cache);
        if (pl.valid) {
            sheet.place(part, pl.pos, pl.angle);
            ++placed;
        }
    }
    return {placed, sheet.utilization()};
}

// ─── evaluate ────────────────────────────────────────────────────────────────
// ИСПРАВЛЕНИЕ от v8:
//   Старая схема при placed<total: r*0.3 + util*0.2 = макс 0.5
//   Но полное размещение возвращало util~0.7-0.9 → ГА правильно ранжировал.
//   Проблема: когда placed==0 для ВСЕХ хромосом, все fitness==0,
//   tournament вырождается в случайный выбор — ГА стоит на месте.
//
//   Новая схема:
//     placed == 0:          1e-6        (чуть больше нуля — tournament работает)
//     0 < placed < total:   ratio*0.8 + util*0.2
//     placed == total:      0.5 + util*0.5  => всегда [0.5, 1.0]
//
//   Гарантия: полное размещение > частичного (даже при util=0.01 → 0.505 > 0.8)
double GeneticAlgorithm::evaluate(const Chromosome& c, NFPCache& cache) const {
    auto [placed, util] = simulate(c.order, c.angles, cache);
    int total = (int)c.order.size();
    if (total == 0) return 0.0;

    if (placed == 0) {
        return 1e-6;
    }
    double ratio = (double)placed / total;
    if (placed < total) {
        return ratio * 0.8 + util * 0.2;
    }
    return 0.5 + util * 0.5;
}

// ─── tournament ───────────────────────────────────────────────────────────────
const Chromosome& GeneticAlgorithm::tournament(
    const std::vector<Chromosome>& pop, std::mt19937& rng, int k) const
{
    std::uniform_int_distribution<int> dist(0, (int)pop.size()-1);
    int best = dist(rng);
    for (int i = 1; i < k; ++i) {
        int idx = dist(rng);
        if (pop[idx].fitness > pop[best].fitness) best = idx;
    }
    return pop[best];
}

// ─── PMX crossover ────────────────────────────────────────────────────────────
std::vector<int> GeneticAlgorithm::pmxCrossover(
    const std::vector<int>& p1, const std::vector<int>& p2, int s, int e)
{
    int n = (int)p1.size();
    std::vector<int> child(n, -1);
    for (int i = s; i <= e; ++i) child[i] = p1[i];

    for (int i = 0; i < n; ++i) {
        if (i >= s && i <= e) continue;
        int val = p2[i], tries = 0;
        while (std::find(child.begin(), child.end(), val) != child.end()) {
            int pos = (int)(std::find(p1.begin(), p1.end(), val) - p1.begin());
            if (pos >= n) break;
            val = p2[pos];
            if (++tries > n) { val = -1; break; }
        }
        if (val != -1) child[i] = val;
    }

    std::vector<int> missing;
    for (int v = 0; v < n; ++v)
        if (std::find(child.begin(), child.end(), v) == child.end())
            missing.push_back(v);
    int mi = 0;
    for (int& cv : child)
        if (cv == -1 && mi < (int)missing.size())
            cv = missing[mi++];

    return child;
}

Chromosome GeneticAlgorithm::crossover(
    const Chromosome& p1, const Chromosome& p2, std::mt19937& rng) const
{
    int n = (int)p1.order.size();
    if (n == 0) return p1;
    std::uniform_int_distribution<int> dist(0, n-1);
    int s = dist(rng), e = dist(rng);
    if (s > e) std::swap(s, e);

    Chromosome child;
    child.order  = pmxCrossover(p1.order, p2.order, s, e);
    child.angles.resize(n);

    // Углы привязаны к деталям (partIndex), а не к позициям в хромосоме
    std::unordered_map<int,double> angleMap;
    for (int i = 0; i < n; ++i)
        angleMap[p2.order[i]] = p2.angles[i];
    for (int i = s; i <= e; ++i)
        angleMap[p1.order[i]] = p1.angles[i];
    for (int i = 0; i < n; ++i)
        child.angles[i] = angleMap[child.order[i]];

    return child;
}

// ─── mutate ───────────────────────────────────────────────────────────────────
void GeneticAlgorithm::mutate(
    Chromosome& c, std::mt19937& rng, const AdaptiveParams& params) const
{
    int n = (int)c.order.size();
    if (n < 2) return;

    std::uniform_real_distribution<double> prob(0.0, 1.0);
    std::uniform_int_distribution<int>     idx(0, n-1);
    double mr = params.mutationRate;

    if (prob(rng) < mr) {
        int a = idx(rng), b = idx(rng);
        std::swap(c.order[a],  c.order[b]);
        std::swap(c.angles[a], c.angles[b]);
    }
    if (prob(rng) < mr * 0.5) {
        int a = idx(rng), b = idx(rng);
        if (a > b) std::swap(a, b);
        if (b-a > 1) {
            std::reverse(c.order.begin()+a,  c.order.begin()+b+1);
            std::reverse(c.angles.begin()+a, c.angles.begin()+b+1);
        }
    }
    if (prob(rng) < mr * 0.3) {
        int from = idx(rng), to = idx(rng);
        if (from != to) {
            int    valO = c.order[from];
            double valA = c.angles[from];
            c.order.erase(c.order.begin()+from);
            c.angles.erase(c.angles.begin()+from);
            if (to >= (int)c.order.size()) to = (int)c.order.size()-1;
            c.order.insert(c.order.begin()+to, valO);
            c.angles.insert(c.angles.begin()+to, valA);
        }
    }
    if (!allowedAngles.empty())
        for (int i = 0; i < n; ++i)
            if (prob(rng) < mr * 0.25)
                c.angles[i] = allowedAngles[rng() % allowedAngles.size()];
}

// ─── localSearch ─────────────────────────────────────────────────────────────
void GeneticAlgorithm::localSearch(
    Chromosome& c, int maxIter, NFPCache& cache, std::mt19937& rng) const
{
    int n = (int)c.order.size();
    if (n < 2) return;
    std::uniform_int_distribution<int> idx(0, n-1);
    std::uniform_real_distribution<double> prob(0.0, 1.0);

    for (int k = 0; k < maxIter; ++k) {
        Chromosome cand = c;
        int a = idx(rng), b = idx(rng);
        if (a == b) continue;

        std::swap(cand.order[a],  cand.order[b]);
        std::swap(cand.angles[a], cand.angles[b]);

        if (!allowedAngles.empty() && prob(rng) < 0.3) {
            int pick = (prob(rng) < 0.5) ? a : b;
            cand.angles[pick] = allowedAngles[rng() % allowedAngles.size()];
        }

        cand.fitness = evaluate(cand, cache);
        if (cand.fitness > c.fitness) c = cand;
    }
}

// ─── evolveIsland ─────────────────────────────────────────────────────────────
void GeneticAlgorithm::evolveIsland(Island& island, int generation) {
    auto& pop    = island.pop;
    auto& rng    = island.rng;
    auto& params = island.params;
    auto& cache  = island.cache;
    int   ps     = (int)pop.size();

    std::vector<Chromosome> newPop;
    newPop.reserve(ps);

    for (int e = 0; e < eliteCount && e < ps; ++e)
        newPop.push_back(pop[e]);

    std::uniform_real_distribution<double> prob(0.0, 1.0);
    while ((int)newPop.size() < ps) {
        Chromosome child;
        if (prob(rng) < params.crossoverRate)
            child = crossover(tournament(pop, rng, tournamentK),
                              tournament(pop, rng, tournamentK), rng);
        else
            child = tournament(pop, rng, tournamentK);

        mutate(child, rng, params);
        child.fitness = evaluate(child, cache);
        newPop.push_back(std::move(child));
    }

    pop = std::move(newPop);
    std::sort(pop.begin(), pop.end());

    if (pop[0].fitness > island.best.fitness + 1e-5) {
        island.best       = pop[0];
        island.stagnation = 0;
        localSearch(island.best, 8, cache, rng);
        island.best.fitness = evaluate(island.best, cache);
        if (island.best.fitness > pop[0].fitness) pop[0] = island.best;
    } else {
        ++island.stagnation;
    }

    params.adapt(island.stagnation);

    // Callback детального прогресса (остров 0, каждые 10 поколений)
    if (onIslandProgress && island.islandId == 0 && generation % 10 == 0) {
        onIslandProgress(island.islandId, generation, island.best.fitness);
    }

    (void)generation;
}

// ─── migrate ──────────────────────────────────────────────────────────────────
void GeneticAlgorithm::migrate(std::vector<Island>& islands) {
    int ni = (int)islands.size();
    if (ni < 2) return;

    std::vector<std::vector<Chromosome>> migrants(ni);
    for (int i = 0; i < ni; ++i) {
        int mc = std::min(migrationCount, (int)islands[i].pop.size());
        for (int j = 0; j < mc; ++j)
            migrants[i].push_back(islands[i].pop[j]);
    }

    for (int i = 0; i < ni; ++i) {
        int src = (i - 1 + ni) % ni;
        auto& pop = islands[i].pop;
        for (int j = 0; j < (int)migrants[src].size(); ++j) {
            int worst = (int)pop.size() - 1 - j;
            if (worst >= 0 && migrants[src][j].fitness > pop[worst].fitness)
                pop[worst] = migrants[src][j];
        }
        std::sort(pop.begin(), pop.end());
    }
}

// ─── run ─────────────────────────────────────────────────────────────────────
Chromosome GeneticAlgorithm::run(
    const std::vector<std::unique_ptr<Part>>& parts,
    const Sheet& sheetTemplate)
{
    parts_         = &parts;
    sheetTemplate_ = sheetTemplate;
    sheetTemplate_.placed.clear();
    sharedCache_.clear();

    if (parts.empty()) return {};

    int ni = islandCount > 0 ? islandCount
           : std::max(2, std::min(4, (int)std::thread::hardware_concurrency()));
    int islandPop = std::max(10, populationSize / ni);

    std::vector<Island> islands(ni);
    for (int i = 0; i < ni; ++i) {
        islands[i].islandId = i;
        islands[i].pop = initPopulation(islandPop, islands[i].rng, islands[i].params);
        for (auto& c : islands[i].pop) c.fitness = evaluate(c, islands[i].cache);
        std::sort(islands[i].pop.begin(), islands[i].pop.end());
        if (!islands[i].pop.empty()) islands[i].best = islands[i].pop[0];
    }

    Chromosome globalBest;
    auto updateGlobal = [&]() {
        for (auto& isl : islands)
            if (isl.best.fitness > globalBest.fitness)
                globalBest = isl.best;
    };
    updateGlobal();

    if (verboseLogging) {
        std::cerr << "[GA] Старт: " << ni << " островов x " << islandPop
                  << " особей, " << parts.size() << " деталей\n";
        std::cerr << "[GA] Начальный fitness: "
                  << std::fixed << std::setprecision(4) << globalBest.fitness << "\n";
    }

    ThreadPool pool(ni);
    int globalStagnation = 0;

    for (int gen = 0; gen < maxGenerations; ++gen) {
        if (cancelFlag && cancelFlag->load()) break;

        std::vector<std::future<void>> futures;
        futures.reserve(ni);
        for (int i = 0; i < ni; ++i) {
            futures.push_back(pool.enqueue([this, &islands, i, gen]() {
                evolveIsland(islands[i], gen);
            }));
        }
        for (auto& f : futures) f.get();

        if ((gen + 1) % migrationEvery == 0)
            migrate(islands);

        double prevBest = globalBest.fitness;
        updateGlobal();

        if (globalBest.fitness > prevBest + 1e-5)
            globalStagnation = 0;
        else
            ++globalStagnation;

        if (onProgress) onProgress(gen, globalBest.fitness);

        if (verboseLogging && gen % 50 == 0) {
            std::cerr << "[GA] пок." << gen << " fitness="
                      << std::fixed << std::setprecision(4) << globalBest.fitness
                      << " stagnation=" << globalStagnation << "\n";
        }

        if (globalBest.fitness >= targetFitness)   break;
        if (globalStagnation  >= stagnationLimit)  break;
    }

    for (auto& isl : islands)
        for (auto& kv : isl.cache)
            sharedCache_.emplace(kv.first, kv.second);

    if (verboseLogging) {
        std::cerr << "[GA] Завершён. Итоговый fitness: "
                  << std::fixed << std::setprecision(4) << globalBest.fitness << "\n";
    }

    return globalBest;
}
