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
    double mutationRate   = 0.14;  // Текст1: 10-20%
    double crossoverRate  = 0.88;  // Текст1: 80-90%

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
    int    populationSize  = 200;  // Текст1: 200-500
    int    maxGenerations  = 500;  // Текст1: 500-1000
    double targetFitness   = 0.98;
    int    stagnationLimit = 60;
    int    eliteCount      = 4;   // Текст1: 5-10% элитизм
    int    tournamentK     = 4;
    int    islandCount     = 0;   // 0 = auto (CPU cores, max 4)
    int    migrationEvery  = 20;
    int    migrationCount  = 2;

    bool   verboseLogging  = false;

    std::vector<double> allowedAngles = {0, 90, 180, 270};

    // Прогресс: (generation, bestFitness)
    std::function<void(int, double)> onProgress;

    // Детальный прогресс острова: (islandId, generation, fitness)
    std::function<void(int, int, double)> onIslandProgress;

    std::atomic<bool>* cancelFlag = nullptr;

    Chromosome run(const std::vector<std::unique_ptr<Part>>& parts,
                   const Sheet& sheetTemplate);

    NFPCache sharedCache_;

private:
    const std::vector<std::unique_ptr<Part>>* parts_ = nullptr;
    Sheet sheetTemplate_;

    struct Island {
        int                     islandId   = 0;
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
