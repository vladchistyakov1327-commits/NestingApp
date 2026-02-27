#include "NestingEngine.h"
#include <algorithm>
#include <chrono>
#include <numeric>
#include <sstream>
#include <thread>
#include <cmath>
#include <iostream>

static std::string fmtFit(double f) {
    std::ostringstream s;
    s << "утилизация " << (int)(f*100) << "%";
    return s.str();
}

static std::string fmtFit2(double f) {
    std::ostringstream s;
    s << std::fixed;
    s.precision(1);
    s << f * 100 << "%";
    return s.str();
}

// ─── analyzeAndChooseMode ────────────────────────────────────────────────────
NestingMode NestingEngine::analyzeAndChooseMode(
    const std::vector<std::unique_ptr<Part>>& parts,
    double sheetW, double sheetH) const
{
    int totalParts = 0;
    for (auto& p : parts) totalParts += p->requiredCount;

    double sheetArea = sheetW * sheetH;
    double totalArea = 0;
    bool allSimple = true;
    for (auto& p : parts) {
        totalArea += p->area() * p->requiredCount;
        if (p->shape.verts.size() > 8) allSimple = false;
    }

    double density = (sheetArea > 0) ? totalArea / sheetArea : 0;
    (void)density;

    if (totalParts <= 5 || allSimple) return NestingMode::Fast;
    return NestingMode::Optimal;
}

// ─── greedyFillSheet ────────────────────────────────────────────────────────
std::vector<Part*> NestingEngine::greedyFillSheet(
    Sheet& sheet, std::vector<Part*>& parts)
{
    std::vector<Part*> unplaced;
    int total = (int)parts.size();

    for (int i = 0; i < total; ++i) {
        Part* p = parts[i];

        // Статус для каждой детали
        if (onProgress) {
            std::ostringstream msg;
            msg << "Размещение детали " << (i+1) << "/" << total
                << ": " << p->name;
            onProgress(-1, msg.str());
        }

        if (verboseLogging) {
            std::cerr << "[GREEDY] деталь " << (i+1) << "/" << total
                      << " id=" << p->id << " name=" << p->name << "\n";
        }

        auto pl = sheet.findBestPlacement(*p, angles, globalCache_);
        if (pl.valid) {
            sheet.place(*p, pl.pos, pl.angle);
        } else {
            unplaced.push_back(p);
        }
    }
    return unplaced;
}

// ─── nestGreedy ─────────────────────────────────────────────────────────────
std::vector<Sheet> NestingEngine::nestGreedy(
    std::vector<Part*> parts, double w, double h, double margin, double gap)
{
    std::sort(parts.begin(), parts.end(), [](Part* a, Part* b) {
        return a->area() > b->area();
    });

    std::vector<Sheet> sheets;
    int total = (int)parts.size(), done = 0;

    while (!parts.empty() && !cancelRequested) {
        Sheet sheet(w, h, margin, gap);
        sheet.verboseLog = verboseLogging;

        if (onProgress) {
            std::ostringstream msg;
            msg << "Лист " << (sheets.size()+1) << ": жадная раскладка "
                << parts.size() << " деталей...";
            onProgress(-1, msg.str());
        }

        std::vector<Part*> remaining = greedyFillSheet(sheet, parts);

        if (remaining.size() == parts.size()) {
            if (onProgress) onProgress(100, "Детали не помещаются на лист");
            break;
        }

        done += (int)parts.size() - (int)remaining.size();
        parts = std::move(remaining);
        sheets.push_back(std::move(sheet));

        if (onProgress) {
            int pct = total > 0 ? done * 100 / total : 100;
            std::ostringstream msg;
            msg << "Лист " << sheets.size() << " готов"
                << " | " << done << "/" << total << " дет."
                << " | утил. " << fmtFit2(sheets.back().utilization());
            onProgress(pct, msg.str());
        }
    }
    return sheets;
}

// ─── nestOptimal ────────────────────────────────────────────────────────────
std::vector<Sheet> NestingEngine::nestOptimal(
    const std::vector<std::unique_ptr<Part>>& parts,
    double w, double h, double margin, double gap)
{
    std::vector<Sheet> sheets;
    int total = (int)parts.size();
    std::vector<int> remaining(total);
    std::iota(remaining.begin(), remaining.end(), 0);

    while (!remaining.empty() && !cancelRequested) {
        std::vector<std::unique_ptr<Part>> batch;
        for (int idx : remaining)
            batch.push_back(parts[idx]->clone());

        Sheet emptySheet(w, h, margin, gap);
        emptySheet.verboseLog = verboseLogging;

        GeneticAlgorithm ga;
        ga.populationSize  = gaPopulation;
        ga.maxGenerations  = gaGenerations;
        ga.targetFitness   = gaTarget;
        ga.allowedAngles   = angles;
        ga.cancelFlag      = &cancelRequested;
        ga.verboseLogging  = verboseLogging;

        int numIslands = ga.islandCount > 0 ? ga.islandCount
            : std::max(2, std::min(4, (int)std::thread::hardware_concurrency()));

        // Прогресс поколения ГА
        ga.onProgress = [&, numIslands](int gen, double fitness) {
            if (cancelRequested) return;
            if (onProgress) {
                int done = total - (int)remaining.size();
                int pct = std::min(95,
                    done * 100 / std::max(1, total) +
                    gen  *  40 / std::max(1, gaGenerations));
                std::ostringstream msg;
                msg << "ГА пок. " << gen+1 << "/" << gaGenerations
                    << " | " << fmtFit(fitness)
                    << " | " << numIslands << " островов";
                onProgress(pct, msg.str());
            }
        };

        // Детальный прогресс острова (каждые 10 поколений)
        ga.onIslandProgress = [&, numIslands](int isl, int gen, double fit) {
            if (cancelRequested) return;
            if (onProgress) {
                std::ostringstream msg;
                msg << "Остров " << (isl+1) << "/" << numIslands
                    << ", пок. " << (gen+1) << "/" << gaGenerations
                    << ", лучший: " << (int)(fit*100) << "%";
                onProgress(-1, msg.str());
            }
        };

        if (onProgress) {
            std::ostringstream msg;
            msg << "Лист " << (sheets.size()+1)
                << ": запуск ГА для " << batch.size() << " деталей...";
            onProgress(0, msg.str());
        }

        Chromosome best = ga.run(batch, emptySheet);

        if (best.order.empty()) {
            // Fallback: жадная раскладка
            if (onProgress) onProgress(-1, "ГА: fallback на жадный алгоритм");
            std::vector<Part*> ptrs;
            for (auto& p : batch) ptrs.push_back(p.get());
            auto fallback = nestGreedy(ptrs, w, h, margin, gap);
            for (auto& s : fallback) sheets.push_back(std::move(s));
            break;
        }

        // Применяем результат ГА
        Sheet sheet(w, h, margin, gap);
        sheet.verboseLog = verboseLogging;
        NFPCache localCache = ga.sharedCache_;
        std::vector<bool> placedFlags(batch.size(), false);

        if (onProgress) onProgress(-1, "Применяем результат ГА...");

        for (int i = 0; i < (int)best.order.size(); ++i) {
            int batchIdx = best.order[i];
            const Part& part = *batch[batchIdx];
            auto pl = sheet.findBestPlacement(part, {best.angles[i]}, localCache);
            if (pl.valid) {
                sheet.place(part, pl.pos, pl.angle);
                placedFlags[batchIdx] = true;
            }
        }

        std::vector<int> newRemaining;
        for (int bi = 0; bi < (int)batch.size(); ++bi)
            if (!placedFlags[bi])
                newRemaining.push_back(remaining[bi]);

        sheets.push_back(std::move(sheet));

        if (newRemaining.size() == remaining.size()) {
            if (onProgress)
                onProgress(100, "Часть деталей не помещается на лист");
            break;
        }
        remaining = std::move(newRemaining);

        if (onProgress) {
            int done = total - (int)remaining.size();
            std::ostringstream msg;
            msg << "Лист " << sheets.size() << " готов"
                << " | " << done << "/" << total << " дет."
                << " | утил. " << fmtFit2(sheets.back().utilization());
            onProgress(done * 100 / std::max(1, total), msg.str());
        }
    }
    return sheets;
}

// ─── buildTechCard ────────────────────────────────────────────────────────────
TechCard NestingEngine::buildTechCard(
    const NestingResult& r, double sheetW, double sheetH) const
{
    TechCard tc;
    tc.sheetsUsed  = (int)r.sheets.size();
    tc.partsPlaced = r.placedParts;
    tc.partsTotal  = r.totalParts;

    double sheetArea = sheetW * sheetH;
    tc.materialUsed  = sheetArea * r.sheets.size();

    for (const auto& sheet : r.sheets) {
        for (const auto& pp : sheet.placed) {
            const auto& verts = pp.shape.verts;
            int n = (int)verts.size();
            for (int i = 0; i < n; ++i) {
                tc.totalCutLength += verts[i].distanceTo(verts[(i+1)%n]);
            }
        }
    }

    tc.estimatedCutTime = (cuttingSpeedMmPerSec > 0)
                        ? tc.totalCutLength / cuttingSpeedMmPerSec
                        : 0;

    double usedArea = 0;
    for (const auto& s : r.sheets) usedArea += s.placedArea();
    tc.materialWaste  = tc.materialUsed - usedArea;
    tc.wastePercent   = (tc.materialUsed > 0)
                       ? tc.materialWaste * 100.0 / tc.materialUsed : 0;
    return tc;
}

// ─── nest ────────────────────────────────────────────────────────────────────
NestingResult NestingEngine::nest(
    const std::vector<std::unique_ptr<Part>>& srcParts,
    double sheetW, double sheetH, double margin, double gap,
    NestingMode mode)
{
    auto t0 = std::chrono::steady_clock::now();
    cancelRequested = false;
    globalCache_.clear();

    NestingMode actualMode = (mode == NestingMode::Auto)
        ? analyzeAndChooseMode(srcParts, sheetW, sheetH) : mode;

    if (onProgress) {
        std::ostringstream msg;
        msg << "Режим: " << (actualMode == NestingMode::Fast ? "БЫСТРЫЙ" : "ОПТИМАЛЬНЫЙ")
            << " | подготовка деталей...";
        onProgress(0, msg.str());
    }

    // Раскрываем по requiredCount
    std::vector<std::unique_ptr<Part>> expanded;
    for (const auto& p : srcParts) {
        for (int i = 0; i < p->requiredCount; ++i) {
            auto copy = p->clone();
            copy->requiredCount = 1;
            copy->placedCount   = 0;
            copy->normalize();
            expanded.push_back(std::move(copy));
        }
    }

    if (verboseLogging) {
        std::cerr << "[ENGINE] Запуск раскладки: " << expanded.size()
                  << " деталей, лист " << sheetW << "x" << sheetH
                  << ", margin=" << margin << ", gap=" << gap << "\n";
    }

    NestingResult result;
    result.totalParts = (int)expanded.size();
    result.modeUsed   = actualMode;

    if (actualMode == NestingMode::Fast) {
        std::vector<Part*> ptrs;
        for (auto& p : expanded) ptrs.push_back(p.get());
        result.sheets = nestGreedy(ptrs, sheetW, sheetH, margin, gap);
    } else {
        result.sheets = nestOptimal(expanded, sheetW, sheetH, margin, gap);
    }

    for (const auto& s : result.sheets) {
        result.placedParts    += (int)s.placed.size();
        result.avgUtilization += s.utilization();
    }
    if (!result.sheets.empty())
        result.avgUtilization /= result.sheets.size();

    auto t1 = std::chrono::steady_clock::now();
    result.timeSeconds = std::chrono::duration<double>(t1-t0).count();
    result.techCard    = buildTechCard(result, sheetW, sheetH);

    if (verboseLogging) {
        std::cerr << "[ENGINE] Готово: " << result.placedParts << "/" << result.totalParts
                  << " деталей, " << result.sheets.size() << " листов"
                  << ", утил. " << fmtFit2(result.avgUtilization)
                  << ", время=" << result.timeSeconds << "с\n";
    }

    return result;
}
