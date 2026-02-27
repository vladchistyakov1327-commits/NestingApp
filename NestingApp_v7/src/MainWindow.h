#pragma once
#include <QMainWindow>
#include <QThread>
#include <QObject>
#include <vector>
#include <memory>
#include <atomic>
#include "NestingEngine.h"
#include "DXFLoader.h"
#include "LXDExporter.h"

class QTableWidget;
class QGraphicsScene;
class QGraphicsView;
class QLineEdit;
class QPushButton;
class QLabel;
class QProgressBar;
class QComboBox;
class QTextEdit;
class QTabWidget;
class QSplitter;
class QDoubleSpinBox;
class QSpinBox;

// ─── Worker ───────────────────────────────────────────────────────────────────
class NestingWorker : public QObject {
    Q_OBJECT
public:
    NestingWorker(std::vector<std::unique_ptr<Part>> parts,
                  double w, double h, double margin, double gap,
                  NestingMode mode,
                  std::atomic<bool>* cancelFlag,
                  int gaPopulation, int gaGenerations, double gaTarget,
                  double cutSpeed,
                  std::vector<double> angles);

signals:
    void finished(NestingResult result);
    void progress(int pct, QString msg);

public slots:
    void process();

private:
    std::vector<std::unique_ptr<Part>> parts_;
    double w_, h_, margin_, gap_, cutSpeed_;
    NestingMode mode_;
    std::atomic<bool>* cancelFlag_;
    int    gaPopulation_, gaGenerations_;
    double gaTarget_;
    std::vector<double> angles_;
};

// ─── MainWindow ───────────────────────────────────────────────────────────────
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onAddDXF();
    void onLoadCSV();
    void onFastNest();
    void onOptimalNest();
    void onAutoNest();
    void onExportLXD();
    void onExportDXF();
    void onCancel();
    void onNestingFinished(NestingResult result);
    void onNestingProgress(int pct, QString msg);
    void onPartCountChanged(int row, int col);
    void onClearParts();
    void onPrevSheet();
    void onNextSheet();
    void onPresetChanged(int idx);

private:
    void setupUI();
    void setupMenuBar();
    void loadDXFFiles(const QStringList& files);
    void updatePartsTable();
    void displaySheet(int idx);
    void drawSheet(const Sheet& sheet);
    void runNesting(NestingMode mode);
    void setRunning(bool on);
    void updateTechCard(const NestingResult& r);
    bool eventFilter(QObject* obj, QEvent* ev) override;

    // ── Центральный сплиттер ─────────────────────────────────────────────────
    QTabWidget*     tabs_        = nullptr;   // Вид / Технокарта / Лог

    // Вкладка «Вид»
    QGraphicsScene* scene_       = nullptr;
    QGraphicsView*  view_        = nullptr;

    // Вкладка «Технологическая карта»
    QTextEdit*      techCardView_= nullptr;

    // Вкладка «Лог»
    QTextEdit*      logView_     = nullptr;

    // ── Левая панель ─────────────────────────────────────────────────────────
    QTableWidget*   partsTable_  = nullptr;

    // ── Правая панель ────────────────────────────────────────────────────────
    // Лист
    QComboBox*      preset_      = nullptr;
    QLineEdit*      sheetW_      = nullptr;
    QLineEdit*      sheetH_      = nullptr;
    QLineEdit*      marginEdit_  = nullptr;
    QLineEdit*      gapEdit_     = nullptr;
    QLineEdit*      cutSpeedEdit_= nullptr;

    // Углы
    QComboBox*      anglesCombo_ = nullptr;

    // ГА
    QLineEdit*      gaPop_       = nullptr;
    QLineEdit*      gaGen_       = nullptr;

    // Кнопки
    QPushButton*    fastBtn_     = nullptr;
    QPushButton*    autoBtn_     = nullptr;
    QPushButton*    optBtn_      = nullptr;
    QPushButton*    cancelBtn_   = nullptr;
    QPushButton*    exportLxdBtn_= nullptr;
    QPushButton*    exportDxfBtn_= nullptr;

    // Навигация листов
    QPushButton*    prevBtn_     = nullptr;
    QPushButton*    nextBtn_     = nullptr;
    QLabel*         sheetLabel_  = nullptr;
    QLabel*         utilLabel_   = nullptr;

    // Статус
    QLabel*         statusLabel_ = nullptr;
    QProgressBar*   progressBar_ = nullptr;

    // Мини-статистика под видом
    QLabel*         statsLabel_  = nullptr;

    // ── Данные ───────────────────────────────────────────────────────────────
    std::vector<std::unique_ptr<Part>> parts_;
    NestingResult  result_;
    int            currentSheet_ = 0;
    int            nextPartId_   = 0;

    // ── Поток ────────────────────────────────────────────────────────────────
    QThread*       thread_    = nullptr;
    NestingWorker* worker_    = nullptr;
    std::atomic<bool> cancelFlag_{false};
};
