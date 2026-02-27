#pragma once
#include "GeneticAlgorithm.h"
#include <atomic>
#include <functional>
#include <string>
#ifdef QT_CORE_LIB
#  include <QMetaType>
#endif

class QString;

enum class NestingMode { Fast, Optimal, Auto };

// ─── Технологическая карта ────────────────────────────────────────────────────
struct TechCard {
    double totalCutLength    = 0;   // Суммарная длина резки (мм)
    double estimatedCutTime  = 0;   // Оценочное время резки (с)
    double materialUsed      = 0;   // Площадь материала (мм²)
    double materialWaste     = 0;   // Отходы (мм²)
    double wastePercent      = 0;   // Процент отходов
    int    sheetsUsed        = 0;   // Листов использовано
    int    partsPlaced       = 0;
    int    partsTotal        = 0;
};

// ─── Результат раскладки ──────────────────────────────────────────────────────
struct NestingResult {
    std::vector<Sheet> sheets;
    int       totalParts       = 0;
    int       placedParts      = 0;
    double    avgUtilization   = 0;
    double    timeSeconds      = 0;
    TechCard  techCard;
    NestingMode modeUsed       = NestingMode::Fast;
};

// ─── NestingEngine — Профессиональный движок раскладки ───────────────────────
class NestingEngine {
public:
    int    gaPopulation   = 60;
    int    gaGenerations  = 300;
    double gaTarget       = 0.96;

    std::vector<double> angles = {0, 90, 180, 270};

    std::function<void(int, const std::string&)> onProgress;
    std::atomic<bool> cancelRequested{false};

    // Скорость резки (мм/с) для оценки времени в технокарте
    double cuttingSpeedMmPerSec = 50.0;
    bool verboseLogging = false;

    NestingResult nest(
        const std::vector<std::unique_ptr<Part>>& parts,
        double sheetW, double sheetH,
        double margin, double gap,
        NestingMode mode
    );

private:
    NFPCache globalCache_;

    // Автовыбор режима на основе анализа задачи
    NestingMode analyzeAndChooseMode(
        const std::vector<std::unique_ptr<Part>>& parts,
        double sheetW, double sheetH) const;

    std::vector<Sheet> nestGreedy(
        std::vector<Part*> parts,
        double w, double h, double margin, double gap
    );
    std::vector<Sheet> nestOptimal(
        const std::vector<std::unique_ptr<Part>>& parts,
        double w, double h, double margin, double gap
    );
    std::vector<Part*> greedyFillSheet(Sheet& sheet, std::vector<Part*>& parts);

    // Построение технологической карты
    TechCard buildTechCard(const NestingResult& r,
                            double sheetW, double sheetH) const;
};

#ifdef QT_CORE_LIB
Q_DECLARE_METATYPE(NestingResult)
#endif
