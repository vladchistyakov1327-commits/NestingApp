#pragma once
#include "Sheet.h"
#include "ThreadPool.h"
#include <vector>
#include <memory>
#include <functional>
#include <random>
#include <atomic>
#include <mutex>

// ─── Хромосома ────────────────────────────────────────────────────────────────
struct Chromosome {
    std::vector<int>    order;
    std::vector<double> angles;
    double fitness = 0.0;
    bool operator<(const Chromosome& o) const { return fitness > o.fitness; }
};

// ─── Адаптивные параметры ГА ─────────────────────────────────────────────────
struct AdaptiveParams {
    double mutationRate   = 0.12;
    double crossoverRate  = 0.85;

    void adapt(int stagnation) {
        if (stagnation > 10) {
            mutationRate  = std::min(0.40, mutationRate  * 1.15);
            crossoverRate = std::max(0.60, crossoverRate * 0.97);
        } else if (stagnation < 3) {
            mutationRate  = std::max(0.05, mutationRate  * 0.95);
            crossoverRate = std::min(0.95, crossoverRate * 1.01);
        }
    }
    void reset() { mutationRate = 0.12; crossoverRate = 0.85; }
};

// ─── GeneticAlgorithm — Островная модель + адаптивные параметры ──────────────
class GeneticAlgorithm {
public:
    int    populationSize  = 60;
    int    maxGenerations  = 400;
    double targetFitness   = 0.97;
    int    stagnationLimit = 40;
    int    eliteCount      = 2;
    int    tournamentK     = 4;
    int    islandCount     = 0;   // 0 = auto (CPU cores, max 4)
    int    migrationEvery  = 20;
    int    migrationCount  = 2;

    std::vector<double> allowedAngles = {0, 90, 180, 270};
    std::function<void(int, double)> onProgress;
    std::atomic<bool>* cancelFlag = nullptr;

    Chromosome run(const std::vector<std::unique_ptr<Part>>& parts,
                   const Sheet& sheetTemplate);

    NFPCache sharedCache_;

private:
    const std::vector<std::unique_ptr<Part>>* parts_ = nullptr;
    Sheet sheetTemplate_;

    struct Island {
        std::vector<Chromosome> pop;
        NFPCache                cache;
        AdaptiveParams          params;
        std::mt19937            rng{std::random_device{}()};
        int                     stagnation = 0;
        Chromosome              best;
    };

    std::vector<Chromosome> initPopulation(int n, std::mt19937& rng,
                                            const AdaptiveParams& params) const;

    double evaluate(const Chromosome& c, NFPCache& cache) const;
    std::pair<int,double> simulate(const std::vector<int>& order,
                                    const std::vector<double>& angles,
                                    NFPCache& cache) const;

    const Chromosome& tournament(const std::vector<Chromosome>& pop,
                                  std::mt19937& rng, int k) const;
    Chromosome crossover(const Chromosome& p1, const Chromosome& p2,
                          std::mt19937& rng) const;
    void mutate(Chromosome& c, std::mt19937& rng,
                const AdaptiveParams& params) const;
    void localSearch(Chromosome& c, int maxIter,
                     NFPCache& cache, std::mt19937& rng) const;

    static std::vector<int> pmxCrossover(const std::vector<int>& p1,
                                          const std::vector<int>& p2,
                                          int s, int e);

    void    evolveIsland(Island& island, int generation);
    void    migrate(std::vector<Island>& islands);
};
