#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _CRT_SECURE_NO_WARNINGS

#include "MainWindow.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsPolygonItem>
#include <QProgressBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QComboBox>
#include <QMenuBar>
#include <QStatusBar>
#include <QPolygonF>
#include <QSplitter>
#include <QTextStream>
#include <QTextEdit>
#include <QTabWidget>
#include <QPen>
#include <QBrush>
#include <QWheelEvent>
#include <QDir>
#include <QFileInfo>
#include <QCheckBox>
#include <QCoreApplication>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <thread>
#include <atomic>

// ─── NestingWorker ────────────────────────────────────────────────────────────
NestingWorker::NestingWorker(std::vector<std::unique_ptr<Part>> parts,
                              double w, double h, double margin, double gap,
                              NestingMode mode,
                              std::atomic<bool>* cancelFlag,
                              int gaPopulation, int gaGenerations, double gaTarget,
                              double cutSpeed,
                              std::vector<double> angles,
                              bool verboseLogging)
    : parts_(std::move(parts))
    , w_(w), h_(h), margin_(margin), gap_(gap)
    , mode_(mode), cancelFlag_(cancelFlag)
    , gaPopulation_(gaPopulation), gaGenerations_(gaGenerations)
    , gaTarget_(gaTarget), cutSpeed_(cutSpeed)
    , angles_(std::move(angles))
    , verboseLogging_(verboseLogging)
{}

void NestingWorker::process() {
    NestingEngine engine;
    engine.cancelRequested.store(false);
    engine.angles               = angles_;
    engine.gaPopulation         = gaPopulation_;
    engine.gaGenerations        = gaGenerations_;
    engine.gaTarget             = gaTarget_;
    engine.verboseLogging       = verboseLogging_;
    engine.cuttingSpeedMmPerSec = cutSpeed_ / 60.0;

    engine.onProgress = [this, &engine](int pct, const std::string& msg) {
        if (cancelFlag_->load())
            engine.cancelRequested.store(true);
        emit progress(pct, QString::fromStdString(msg));
    };

    NestingResult res = engine.nest(parts_, w_, h_, margin_, gap_, mode_);
    emit finished(std::move(res));
}

// ─── MainWindow ───────────────────────────────────────────────────────────────
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("NestingApp — Раскладка листового металла");
    resize(1440, 880);
    setupUI();
    setupMenuBar();
    statusBar()->showMessage("Готов к работе");
}

MainWindow::~MainWindow() {
    cancelFlag_.store(true);
    if (thread_) { thread_->quit(); thread_->wait(3000); }
}

// ─── Меню ─────────────────────────────────────────────────────────────────────
void MainWindow::setupMenuBar() {
    auto* file = menuBar()->addMenu("Файл");
    file->addAction("Добавить DXF...",   this, &MainWindow::onAddDXF,    QKeySequence::Open);
    file->addAction("Загрузить CSV...",  this, &MainWindow::onLoadCSV);
    file->addAction("Очистить список",  this, &MainWindow::onClearParts);
    file->addSeparator();
    file->addAction("Экспорт в LXD...", this, &MainWindow::onExportLXD);
    file->addAction("Экспорт в DXF...", this, &MainWindow::onExportDXF);
    file->addSeparator();
    file->addAction("Выход", qApp, &QApplication::quit, QKeySequence::Quit);

    menuBar()->addAction("О программе", this, [this]() {
        QMessageBox::about(this, "О NestingApp",
            "<b>NestingApp v2.0</b><br><br>"
            "Раскладка деталей из листового металла.<br>"
            "Алгоритмы: NFP (орбитальный метод) + "
            "Генетическая оптимизация (островная модель).<br><br>"
            "Исправления v2.0:<br>"
            "• Функция evaluate: полное размещение всегда > частичного<br>"
            "• addGapToPolygon: убран convexHull, сохраняется вогнутость<br>"
            "• NFP для невыпуклых: прямой orbital → fallback с логом<br>"
            "• findBestPlacement: счётчики диагностики, лимит кандидатов<br>"
            "• UI: превью детали, предупреждения DXF, verbose-режим");
    });
}

// ─── UI ───────────────────────────────────────────────────────────────────────
void MainWindow::setupUI() {
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* root = new QHBoxLayout(central);
    root->setSpacing(6);
    root->setContentsMargins(6, 6, 6, 6);

    // ── Левая панель: список деталей + превью ─────────────────────────────────
    auto* leftBox = new QGroupBox("Детали");
    auto* leftLay = new QVBoxLayout(leftBox);

    auto* btns   = new QHBoxLayout;
    auto* addBtn = new QPushButton("➕ DXF");
    auto* csvBtn = new QPushButton("📂 CSV");
    auto* clrBtn = new QPushButton("🗑 Очист.");
    btns->addWidget(addBtn);
    btns->addWidget(csvBtn);
    btns->addWidget(clrBtn);
    leftLay->addLayout(btns);

    partsTable_ = new QTableWidget(0, 3);
    partsTable_->setHorizontalHeaderLabels({"Деталь", "Пл.мм²", "Кол-во"});
    partsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    partsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    partsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    partsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    partsTable_->setEditTriggers(QAbstractItemView::DoubleClicked);
    leftLay->addWidget(partsTable_);
    leftLay->addWidget(new QLabel("Дважды щёлкни «Кол-во» для изменения"));

    // ── Превью выбранной детали ───────────────────────────────────────────────
    auto* previewBox = new QGroupBox("Превью детали");
    auto* previewLay = new QVBoxLayout(previewBox);
    previewLay->setContentsMargins(4, 4, 4, 4);
    previewLay->setSpacing(3);

    previewScene_ = new QGraphicsScene(this);
    previewView_  = new QGraphicsView(previewScene_);
    previewView_->setFixedHeight(150);
    previewView_->setRenderHint(QPainter::Antialiasing);
    previewView_->setBackgroundBrush(QColor(35, 35, 45));
    previewView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    previewView_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    previewInfo_ = new QLabel("—");
    previewInfo_->setAlignment(Qt::AlignCenter);
    previewInfo_->setStyleSheet("color:#aaa;font-size:10px;");
    previewInfo_->setWordWrap(true);

    previewLay->addWidget(previewView_);
    previewLay->addWidget(previewInfo_);
    leftLay->addWidget(previewBox);
    leftBox->setFixedWidth(290);

    // ── Центр: вкладки Вид / Технокарта / Лог ────────────────────────────────
    tabs_ = new QTabWidget;

    // Вкладка «Вид»
    auto* viewTab = new QWidget;
    auto* viewLay = new QVBoxLayout(viewTab);
    viewLay->setContentsMargins(0, 4, 0, 0);
    viewLay->setSpacing(4);

    auto* navRow = new QHBoxLayout;
    prevBtn_    = new QPushButton("◀");
    nextBtn_    = new QPushButton("▶");
    sheetLabel_ = new QLabel("Лист 0 из 0");
    utilLabel_  = new QLabel("—");
    sheetLabel_->setAlignment(Qt::AlignCenter);
    utilLabel_->setAlignment(Qt::AlignCenter);
    prevBtn_->setFixedWidth(36);
    nextBtn_->setFixedWidth(36);
    navRow->addWidget(prevBtn_);
    navRow->addWidget(sheetLabel_, 2);
    navRow->addWidget(utilLabel_,  2);
    navRow->addWidget(nextBtn_);
    viewLay->addLayout(navRow);

    scene_ = new QGraphicsScene(this);
    view_  = new QGraphicsView(scene_);
    view_->setRenderHint(QPainter::Antialiasing);
    view_->setDragMode(QGraphicsView::ScrollHandDrag);
    view_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    view_->setBackgroundBrush(QColor(28, 28, 28));
    view_->installEventFilter(this);
    viewLay->addWidget(view_, 1);

    statsLabel_ = new QLabel("—");
    statsLabel_->setAlignment(Qt::AlignCenter);
    statsLabel_->setStyleSheet("color:#aaa;font-size:11px;");
    viewLay->addWidget(statsLabel_);

    tabs_->addTab(viewTab, "📐 Вид");

    // Вкладка «Технологическая карта»
    techCardView_ = new QTextEdit;
    techCardView_->setReadOnly(true);
    techCardView_->setFontFamily("Courier New");
    tabs_->addTab(techCardView_, "📋 Технокарта");

    // Вкладка «Лог»
    logView_ = new QTextEdit;
    logView_->setReadOnly(true);
    logView_->setFontFamily("Courier New");
    tabs_->addTab(logView_, "📜 Лог");

    // ── Правая панель: параметры ──────────────────────────────────────────────
    auto* rightPanel = new QWidget;
    auto* rightLay   = new QVBoxLayout(rightPanel);
    rightLay->setSpacing(8);
    rightPanel->setFixedWidth(230);

    // Лист
    auto* sheetBox = new QGroupBox("Лист металла");
    auto* sg = new QGridLayout(sheetBox);
    preset_ = new QComboBox;
    preset_->addItems({"Пользовательский", "3000×1500", "2500×1250",
                       "2000×1000", "1500×750", "1000×500"});
    sg->addWidget(new QLabel("Пресет:"),      0, 0); sg->addWidget(preset_,     0, 1);
    sheetW_ = new QLineEdit("3000");
    sheetH_ = new QLineEdit("1500");
    sg->addWidget(new QLabel("Длина (мм):"),  1, 0); sg->addWidget(sheetW_,     1, 1);
    sg->addWidget(new QLabel("Ширина (мм):"), 2, 0); sg->addWidget(sheetH_,     2, 1);
    marginEdit_   = new QLineEdit("10");
    gapEdit_      = new QLineEdit("5");
    cutSpeedEdit_ = new QLineEdit("3000");
    sg->addWidget(new QLabel("Отступ (мм):"), 3, 0); sg->addWidget(marginEdit_,   3, 1);
    sg->addWidget(new QLabel("Зазор (мм):"),  4, 0); sg->addWidget(gapEdit_,      4, 1);
    sg->addWidget(new QLabel("Скорость (мм/мин):"), 5, 0, 1, 2);
    sg->addWidget(cutSpeedEdit_, 6, 0, 1, 2);
    rightLay->addWidget(sheetBox);

    // Углы поворота
    auto* angBox = new QGroupBox("Углы поворота");
    auto* angLay = new QVBoxLayout(angBox);
    anglesCombo_ = new QComboBox;
    anglesCombo_->addItem("0°, 90°, 180°, 270°");
    anglesCombo_->addItem("0°, 180°");
    anglesCombo_->addItem("0° только");
    angLay->addWidget(anglesCombo_);
    rightLay->addWidget(angBox);

    // ГА параметры
    auto* gaBox = new QGroupBox("Параметры ГА (оптим. режим)");
    auto* gaG   = new QGridLayout(gaBox);
    gaPop_ = new QLineEdit("200");
    gaGen_ = new QLineEdit("500");
    gaG->addWidget(new QLabel("Популяция:"), 0, 0); gaG->addWidget(gaPop_, 0, 1);
    gaG->addWidget(new QLabel("Поколений:"), 1, 0); gaG->addWidget(gaGen_, 1, 1);
    rightLay->addWidget(gaBox);

    // Verbose checkbox
    verboseChk_ = new QCheckBox("Подробный лог (отладка)");
    verboseChk_->setStyleSheet("color:#888;font-size:11px;");
    verboseChk_->setToolTip("Выводить диагностику NFP и кандидатов размещения в лог и stderr");
    rightLay->addWidget(verboseChk_);

    // Кнопки раскладки
    fastBtn_ = new QPushButton("🔵  БЫСТРО");
    fastBtn_->setStyleSheet(
        "background:#2475b0;color:white;font-size:13px;padding:10px;border-radius:4px;");
    fastBtn_->setToolTip("Жадный Bottom-Left. Секунды, ~75-85%");

    autoBtn_ = new QPushButton("🟡  АВТО");
    autoBtn_->setStyleSheet(
        "background:#b7770d;color:white;font-size:13px;padding:10px;border-radius:4px;");
    autoBtn_->setToolTip("Автовыбор алгоритма по числу деталей");

    optBtn_ = new QPushButton("🟢  ОПТИМАЛЬНО");
    optBtn_->setStyleSheet(
        "background:#1e8449;color:white;font-size:13px;padding:10px;border-radius:4px;");
    optBtn_->setToolTip("NFP + ГА (островная модель). 95-99%");

    cancelBtn_ = new QPushButton("⛔  ОТМЕНИТЬ");
    cancelBtn_->setStyleSheet(
        "background:#922b21;color:white;font-size:12px;padding:8px;border-radius:4px;");
    cancelBtn_->setEnabled(false);

    exportLxdBtn_ = new QPushButton("💾  ЭКСПОРТ LXD");
    exportLxdBtn_->setStyleSheet(
        "background:#6c3483;color:white;font-size:12px;padding:8px;border-radius:4px;");
    exportLxdBtn_->setEnabled(false);

    exportDxfBtn_ = new QPushButton("📄  ЭКСПОРТ DXF");
    exportDxfBtn_->setStyleSheet(
        "background:#1a5276;color:white;font-size:12px;padding:8px;border-radius:4px;");
    exportDxfBtn_->setEnabled(false);

    rightLay->addWidget(fastBtn_);
    rightLay->addWidget(autoBtn_);
    rightLay->addWidget(optBtn_);
    rightLay->addWidget(cancelBtn_);
    rightLay->addSpacing(8);
    rightLay->addWidget(exportLxdBtn_);
    rightLay->addWidget(exportDxfBtn_);
    rightLay->addStretch();

    progressBar_ = new QProgressBar;
    progressBar_->setRange(0, 100);
    statusLabel_ = new QLabel("Готов");
    statusLabel_->setWordWrap(true);
    statusLabel_->setStyleSheet("color:#aaa;font-size:11px;");
    rightLay->addWidget(progressBar_);
    rightLay->addWidget(statusLabel_);

    // Сборка макета
    root->addWidget(leftBox);
    root->addWidget(tabs_, 1);
    root->addWidget(rightPanel);

    // Сигналы
    connect(addBtn,        &QPushButton::clicked, this, &MainWindow::onAddDXF);
    connect(csvBtn,        &QPushButton::clicked, this, &MainWindow::onLoadCSV);
    connect(clrBtn,        &QPushButton::clicked, this, &MainWindow::onClearParts);
    connect(fastBtn_,      &QPushButton::clicked, this, &MainWindow::onFastNest);
    connect(autoBtn_,      &QPushButton::clicked, this, &MainWindow::onAutoNest);
    connect(optBtn_,       &QPushButton::clicked, this, &MainWindow::onOptimalNest);
    connect(cancelBtn_,    &QPushButton::clicked, this, &MainWindow::onCancel);
    connect(exportLxdBtn_, &QPushButton::clicked, this, &MainWindow::onExportLXD);
    connect(exportDxfBtn_, &QPushButton::clicked, this, &MainWindow::onExportDXF);
    connect(prevBtn_,      &QPushButton::clicked, this, &MainWindow::onPrevSheet);
    connect(nextBtn_,      &QPushButton::clicked, this, &MainWindow::onNextSheet);
    connect(preset_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onPresetChanged);
    connect(partsTable_, &QTableWidget::cellChanged,
            this, &MainWindow::onPartCountChanged);
    connect(partsTable_, &QTableWidget::itemSelectionChanged,
            this, &MainWindow::onPartSelected);
}

// ─── Зум колёсиком ────────────────────────────────────────────────────────────
bool MainWindow::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == view_ && ev->type() == QEvent::Wheel) {
        auto*  we     = static_cast<QWheelEvent*>(ev);
        double factor = we->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
        view_->scale(factor, factor);
        return true;
    }
    return QMainWindow::eventFilter(obj, ev);
}

// ─── Добавить DXF ────────────────────────────────────────────────────────────
void MainWindow::onAddDXF() {
    QStringList files = QFileDialog::getOpenFileNames(
        this, "Загрузить DXF детали", "", "DXF (*.dxf);;All (*)");
    if (!files.isEmpty()) loadDXFFiles(files);
}

// ─── CSV ──────────────────────────────────────────────────────────────────────
void MainWindow::onLoadCSV() {
    QString file = QFileDialog::getOpenFileName(
        this, "Список деталей CSV", "", "CSV (*.csv);;All (*)");
    if (file.isEmpty()) return;

    QFile f(file);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть CSV файл.");
        return;
    }

    QTextStream in(&f);
    QDir csvDir(QFileInfo(file).dir());
    int loaded = 0;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        auto cols = line.split(',');
        if (cols.size() < 2) continue;

        QString dxfPath;
        int     count       = 1;
        bool    secondIsNum = false;
        cols[1].trimmed().toInt(&secondIsNum);

        if (secondIsNum && cols.size() == 2) {
            dxfPath = cols[0].trimmed();
            count   = cols[1].trimmed().toInt();
        } else {
            dxfPath = cols[1].trimmed();
            count   = cols.size() > 2 ? cols[2].trimmed().toInt() : 1;
        }
        count = std::max(1, count);

        QFileInfo fi(dxfPath);
        if (!fi.exists()) fi = QFileInfo(csvDir, dxfPath);
        if (!fi.exists()) {
            logView_->append(QString("⚠ CSV: файл не найден: %1").arg(dxfPath));
            continue;
        }

        int prevSize = (int)parts_.size();
        loadDXFFiles({fi.absoluteFilePath()});
        for (int i = prevSize; i < (int)parts_.size(); ++i) {
            parts_[i]->requiredCount = count;
            ++loaded;
        }
    }
    updatePartsTable();
    statusBar()->showMessage(QString("CSV загружен: %1 позиций").arg(loaded), 4000);
}

// ─── loadDXFFiles ────────────────────────────────────────────────────────────
void MainWindow::loadDXFFiles(const QStringList& files) {
    for (const auto& file : files) {
        auto res = DXFLoader::loadFile(file.toStdString());
        QString baseName = QFileInfo(file).fileName();

        if (!res.success) {
            QMessageBox::warning(this, "Ошибка DXF",
                QString("Не удалось загрузить:\n%1\n\n%2")
                .arg(file)
                .arg(res.warnings.empty()
                     ? "Нет замкнутых контуров"
                     : QString::fromStdString(res.warnings[0])));
            logView_->append(QString("✖ %1: ошибка загрузки").arg(baseName));
            continue;
        }

        for (size_t i = 0; i < res.cutContours.size(); ++i) {
            auto part = std::make_unique<Part>();
            part->id   = nextPartId_++;
            part->name = QFileInfo(file).baseName().toStdString();
            if (res.cutContours.size() > 1)
                part->name += "_" + std::to_string(i + 1);
            part->sourceFile    = file.toStdString();
            part->shape         = res.cutContours[i];
            part->requiredCount = 1;

            if (res.cutContours.size() == 1) {
                part->marks = res.markContours;
            } else {
                Point partCenter = res.cutContours[i].centroid();
                for (const auto& mark : res.markContours) {
                    Point mc = mark.centroid();
                    double myDist = mc.distanceTo(partCenter);
                    bool nearest = true;
                    for (size_t j = 0; j < res.cutContours.size(); ++j) {
                        if (j == i) continue;
                        if (mc.distanceTo(res.cutContours[j].centroid()) < myDist) {
                            nearest = false; break;
                        }
                    }
                    if (nearest) part->marks.push_back(mark);
                }
            }

            part->normalize();
            parts_.push_back(std::move(part));
        }

        // ── Обратная связь о загрузке ─────────────────────────────────────────
        if (!res.warnings.empty()) {
            // Формируем строку предупреждений
            QString warnText = QString("⚠ Предупреждения при загрузке %1:\n").arg(baseName);
            for (const auto& w : res.warnings)
                warnText += "  • " + QString::fromStdString(w) + "\n";

            logView_->append(warnText);

            // Всплывающий диалог (только если файлов немного — иначе надоедает)
            if (files.size() <= 3) {
                QMessageBox msgBox(this);
                msgBox.setWindowTitle("Предупреждения DXF");
                msgBox.setIcon(QMessageBox::Warning);
                msgBox.setText(QString("Файл загружен, но есть %1 предупреждение(й):\n%2")
                               .arg(res.warnings.size()).arg(baseName));
                msgBox.setDetailedText(warnText);
                msgBox.exec();
            }

            // Подсветить строки этого файла в таблице оранжевым
            for (int row = 0; row < (int)parts_.size(); ++row) {
                if (parts_[row]->sourceFile == file.toStdString()) {
                    for (int col = 0; col < partsTable_->columnCount(); ++col) {
                        auto* it = partsTable_->item(row, col);
                        if (it) it->setBackground(QColor(255, 230, 180));
                    }
                }
            }
        } else {
            // Успешная загрузка — зелёный фон
            logView_->append(QString("✔ Загружено: %1 (%2 контуров)")
                             .arg(baseName).arg(res.cutContours.size()));
        }
    }
    updatePartsTable();
}

// ─── updatePartsTable ────────────────────────────────────────────────────────
void MainWindow::updatePartsTable() {
    partsTable_->blockSignals(true);
    partsTable_->setRowCount((int)parts_.size());
    for (int i = 0; i < (int)parts_.size(); ++i) {
        const auto& p = parts_[i];
        auto* nameItem = new QTableWidgetItem(QString::fromStdString(p->name));
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        auto* areaItem = new QTableWidgetItem(QString::number((int)p->area()));
        areaItem->setFlags(areaItem->flags() & ~Qt::ItemIsEditable);
        auto* cntItem  = new QTableWidgetItem(QString::number(p->requiredCount));
        partsTable_->setItem(i, 0, nameItem);
        partsTable_->setItem(i, 1, areaItem);
        partsTable_->setItem(i, 2, cntItem);
    }
    partsTable_->blockSignals(false);
}

// ─── onPartSelected — превью детали ─────────────────────────────────────────
void MainWindow::onPartSelected() {
    previewScene_->clear();

    auto rows = partsTable_->selectionModel()->selectedRows();
    if (rows.isEmpty() || rows[0].row() >= (int)parts_.size()) {
        previewInfo_->setText("—");
        return;
    }

    const auto& part = parts_[rows[0].row()];
    const Polygon& shape = part->shape;

    if (shape.verts.empty()) {
        previewInfo_->setText("Нет контура");
        return;
    }

    // Строим QPolygonF (инвертируем Y для Qt)
    QPolygonF poly;
    for (const auto& v : shape.verts)
        poly << QPointF(v.x, -v.y);

    QRectF bb = poly.boundingRect();
    if (bb.width() < 1e-6 || bb.height() < 1e-6) {
        previewInfo_->setText("Вырожденный контур");
        return;
    }

    // Центрируем
    double cx = bb.center().x(), cy = bb.center().y();
    QPolygonF centered;
    for (auto& pt : poly)
        centered << QPointF(pt.x() - cx, pt.y() - cy);

    auto* item = previewScene_->addPolygon(
        centered,
        QPen(QColor(0x27, 0xAE, 0x60), 1.5),
        QBrush(QColor(0x1E, 0x8B, 0x4C, 70))
    );
    previewScene_->setSceneRect(previewScene_->itemsBoundingRect().adjusted(-5,-5,5,5));
    previewView_->fitInView(previewScene_->sceneRect(), Qt::KeepAspectRatio);

    // Метки (гравировка)
    for (const auto& mark : part->marks) {
        QPolygonF qm;
        for (const auto& v : mark.verts)
            qm << QPointF(v.x - cx, -(v.y) - cy);
        previewScene_->addPolygon(qm, QPen(Qt::cyan, 0.8), QBrush(Qt::NoBrush));
    }

    // Информация
    Rect bbox = shape.boundingBox();
    previewInfo_->setText(QString("%1\n%2 × %3 мм  |  Пл: %4 мм²  |  Вершин: %5")
        .arg(QString::fromStdString(part->name))
        .arg(bbox.w, 0, 'f', 1)
        .arg(bbox.h, 0, 'f', 1)
        .arg(part->area(), 0, 'f', 1)
        .arg(shape.verts.size()));

    (void)item;
}

// ─── Очистить ────────────────────────────────────────────────────────────────
void MainWindow::onClearParts() {
    parts_.clear();
    updatePartsTable();
    scene_->clear();
    previewScene_->clear();
    previewInfo_->setText("—");
    result_ = {};
    sheetLabel_->setText("Лист 0 из 0");
    utilLabel_->setText("—");
    statsLabel_->setText("—");
    techCardView_->clear();
    logView_->append("─── Список очищен ───");
    exportLxdBtn_->setEnabled(false);
    exportDxfBtn_->setEnabled(false);
}

void MainWindow::onPartCountChanged(int row, int col) {
    if (col != 2 || row >= (int)parts_.size()) return;
    bool ok;
    int cnt = partsTable_->item(row, col)->text().toInt(&ok);
    if (ok && cnt > 0) parts_[row]->requiredCount = cnt;
}

// ─── Запуск раскладки ─────────────────────────────────────────────────────────
void MainWindow::onFastNest()    { runNesting(NestingMode::Fast);    }
void MainWindow::onOptimalNest() { runNesting(NestingMode::Optimal); }

void MainWindow::onAutoNest() {
    int total = 0;
    for (const auto& p : parts_) total += p->requiredCount;
    NestingMode mode = (total <= 20) ? NestingMode::Fast : NestingMode::Optimal;
    logView_->append(QString("Авто: %1 деталей → %2")
        .arg(total)
        .arg(mode == NestingMode::Fast ? "БЫСТРО" : "ОПТИМАЛЬНО"));
    runNesting(mode);
}

static std::vector<double> anglesFromCombo(int idx) {
    if (idx == 0) return {0, 90, 180, 270};
    if (idx == 1) return {0, 180};
    return {0};
}

void MainWindow::runNesting(NestingMode mode) {
    if (parts_.empty()) {
        QMessageBox::information(this, "Нет деталей",
            "Загрузите хотя бы один DXF файл.");
        return;
    }

    double w      = sheetW_->text().toDouble();
    double h      = sheetH_->text().toDouble();
    double margin = marginEdit_->text().toDouble();
    double gap    = gapEdit_->text().toDouble();
    double speed  = cutSpeedEdit_->text().toDouble();

    if (w <= 0 || h <= 0) {
        QMessageBox::warning(this, "Ошибка", "Размеры листа должны быть > 0.");
        return;
    }
    if (margin < 0) margin = 0;
    if (gap    < 0) gap    = 0;
    if (margin * 2 >= w || margin * 2 >= h) {
        QMessageBox::warning(this, "Ошибка",
            "Отступ слишком велик — рабочая область нулевая.");
        return;
    }
    if (speed <= 0) speed = 3000;

    cancelFlag_.store(false);
    setRunning(true);

    // Прогресс-бар в неопределённый режим на этапах вычисления NFP
    progressBar_->setRange(0, 0);
    statusLabel_->setText("Подготовка...");

    int totalParts = 0;
    for (const auto& p : parts_) totalParts += p->requiredCount;
    logView_->append(QString("─── Запуск: %1 деталей, лист %2×%3 мм ───")
        .arg(totalParts).arg(w).arg(h));
    tabs_->setCurrentIndex(2); // Лог

    std::vector<std::unique_ptr<Part>> partsClone;
    for (const auto& p : parts_)
        partsClone.push_back(p->clone());

    bool verbose = verboseChk_->isChecked();

    worker_ = new NestingWorker(
        std::move(partsClone), w, h, margin, gap, mode, &cancelFlag_,
        std::max(10, gaPop_->text().toInt()),
        std::max(10, gaGen_->text().toInt()),
        0.96, speed,
        anglesFromCombo(anglesCombo_->currentIndex()),
        verbose
    );

    thread_ = new QThread(this);
    worker_->moveToThread(thread_);

    connect(thread_,  &QThread::started,        worker_,  &NestingWorker::process);
    connect(worker_,  &NestingWorker::finished,  this,     &MainWindow::onNestingFinished);
    connect(worker_,  &NestingWorker::progress,  this,     &MainWindow::onNestingProgress);
    connect(worker_,  &NestingWorker::finished,  thread_,  &QThread::quit);
    connect(worker_,  &NestingWorker::finished,  worker_,  &QObject::deleteLater);
    connect(thread_,  &QThread::finished,        thread_,  &QThread::deleteLater);

    thread_->start();
}

void MainWindow::onCancel() {
    cancelFlag_.store(true);
    statusLabel_->setText("Отмена...");
    cancelBtn_->setEnabled(false);
}

void MainWindow::onNestingProgress(int pct, QString msg) {
    // pct == -1: не меняем значение прогресс-бара, только статус
    if (pct >= 0) {
        if (progressBar_->maximum() == 0) {
            // Возвращаем из неопределённого режима
            progressBar_->setRange(0, 100);
        }
        progressBar_->setValue(pct);
    }
    statusLabel_->setText(msg);
    logView_->append(msg);
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void MainWindow::onNestingFinished(NestingResult result) {
    thread_ = nullptr;
    worker_ = nullptr;
    setRunning(false);

    // Возвращаем прогресс-бар в нормальный режим
    progressBar_->setRange(0, 100);
    progressBar_->setValue(100);

    result_ = std::move(result);

    if (result_.sheets.empty()) {
        statusLabel_->setText("Не удалось разложить ни одной детали");
        QMessageBox::warning(this, "Результат",
            "Ни одна деталь не была размещена.\n"
            "Проверьте размеры листа и деталей.\n\n"
            "Совет: включите 'Подробный лог' для диагностики.");
        return;
    }

    currentSheet_ = 0;
    displaySheet(0);
    exportLxdBtn_->setEnabled(true);
    exportDxfBtn_->setEnabled(true);
    tabs_->setCurrentIndex(0); // Вид

    QString summary = QString("%1 лист(ов) | %2/%3 дет. | утил. %4% | %5с")
        .arg(result_.sheets.size())
        .arg(result_.placedParts)
        .arg(result_.totalParts)
        .arg(result_.avgUtilization * 100, 0, 'f', 1)
        .arg(result_.timeSeconds, 0, 'f', 1);

    statusLabel_->setText(summary);
    logView_->append("─── Готово: " + summary + " ───");

    updateTechCard(result_);
}

// ─── Технологическая карта ────────────────────────────────────────────────────
void MainWindow::updateTechCard(const NestingResult& r) {
    const TechCard& tc = r.techCard;

    std::ostringstream out;
    out << std::fixed << std::setprecision(1);
    out << "=================================================\n";
    out << "  ТЕХНОЛОГИЧЕСКАЯ КАРТА РАСКРОЯ\n";
    out << "=================================================\n\n";
    out << "Листов:              " << r.sheets.size()  << "\n";
    out << "Деталей размещено:   " << r.placedParts << " / " << r.totalParts << "\n";
    out << "Ср. утилизация:      " << r.avgUtilization * 100 << " %\n";
    out << "Время расчёта:       " << r.timeSeconds << " с\n\n";

    for (size_t si = 0; si < r.sheets.size(); ++si) {
        const Sheet& s = r.sheets[si];
        out << "-------------------------------------------------\n";
        out << "  ЛИСТ " << (si + 1)
            << "  (" << s.width << " × " << s.height << " мм)\n";
        out << "  Деталей: " << s.placed.size()
            << "  |  Утилизация: " << s.utilization() * 100 << " %\n";
        out << "-------------------------------------------------\n";

        double sheetCut = 0;
        int piercings = 0;
        for (size_t di = 0; di < s.placed.size(); ++di) {
            const auto& pp = s.placed[di];
            double perim = 0;
            const auto& vv = pp.shape.verts;
            for (size_t vi = 0; vi < vv.size(); ++vi) {
                const auto& a = vv[vi];
                const auto& b = vv[(vi + 1) % vv.size()];
                double dx = b.x - a.x, dy = b.y - a.y;
                perim += std::sqrt(dx*dx + dy*dy);
            }
            sheetCut += perim;
            piercings++;  // 1 пробивка на деталь (внешний контур)
            piercings += (int)pp.marks.size();  // + пробивки под метки/отверстия
            out << "  [" << (di + 1) << "] partId=" << pp.partId
                << "  X=" << pp.position.x << " Y=" << pp.position.y
                << "  A=" << pp.angle << "°"
                << "  Рез=" << perim << " мм"
                << "  Контуров=" << (1 + pp.marks.size()) << "\n";
        }

        double sheetTimeSec = (tc.totalCutLength > 0 && tc.estimatedCutTime > 0)
            ? tc.estimatedCutTime * sheetCut / tc.totalCutLength : 0;
        out << "\n  Длина реза:  " << sheetCut << " мм\n";
        out << "  Пробивок:    " << piercings << "\n";
        out << "  Время (~):   "
            << (int)(sheetTimeSec / 60) << " мин " << (int)sheetTimeSec % 60 << " с\n\n";
    }

    double speed = cutSpeedEdit_->text().toDouble();
    if (speed <= 0) speed = 3000;

    out << "=================================================\n";
    out << "  ИТОГО\n";
    out << "  Общая длина реза:  " << tc.totalCutLength << " мм\n";

    int totalMin = (int)(tc.estimatedCutTime / 60);
    int totalSec = (int)tc.estimatedCutTime % 60;
    out << "  Общее время (~):   " << totalMin << " мин " << totalSec << " с\n";
    out << "  Скорость резки:    " << speed << " мм/мин\n";
    out << "  Материал использ.: " << std::setprecision(0) << tc.materialUsed << " мм²\n";
    out << "  Отходы:            " << tc.materialWaste << " мм²"
        << "  (" << std::setprecision(1) << tc.wastePercent << " %)\n";
    out << "=================================================\n";

    techCardView_->setPlainText(QString::fromStdString(out.str()));
}

// ─── Отображение листа ────────────────────────────────────────────────────────
void MainWindow::displaySheet(int idx) {
    if (result_.sheets.empty()) return;
    idx = std::clamp(idx, 0, (int)result_.sheets.size() - 1);
    currentSheet_ = idx;
    const Sheet& s = result_.sheets[idx];

    sheetLabel_->setText(
        QString("Лист %1 из %2").arg(idx + 1).arg(result_.sheets.size()));
    utilLabel_->setText(
        QString("Утилизация: %1%").arg(s.utilization() * 100, 0, 'f', 1));
    statsLabel_->setText(
        QString("Деталей: %1  |  Площадь деталей: %2 мм²")
        .arg(s.placed.size()).arg((int)s.placedArea()));
    prevBtn_->setEnabled(idx > 0);
    nextBtn_->setEnabled(idx < (int)result_.sheets.size() - 1);

    drawSheet(s);
}

void MainWindow::drawSheet(const Sheet& s) {
    scene_->clear();
    double W = s.width, H = s.height, m = s.margin;

    scene_->addRect(0, 0, W, H,
        QPen(QColor(80, 80, 80)), QBrush(QColor(55, 55, 65)));
    scene_->addRect(m, m, W - 2*m, H - 2*m,
        QPen(QColor(100, 200, 100, 120), 0.5, Qt::DashLine), QBrush(Qt::NoBrush));

    static const QColor palette[] = {
        {52,152,219}, {231,76,60},  {46,204,113}, {155,89,182},
        {241,196,15}, {26,188,156}, {230,126,34}, {149,165,166},
        {236,112,99}, {93,173,226}, {82,190,128}, {187,143,206}
    };
    constexpr int PAL = 12;

    for (size_t i = 0; i < s.placed.size(); ++i) {
        const auto& pp  = s.placed[i];
        QColor       col = palette[i % PAL];

        QPolygonF qpoly;
        for (const auto& v : pp.shape.verts)
            qpoly << QPointF(v.x, H - v.y);

        auto* item = scene_->addPolygon(qpoly,
            QPen(col.darker(160), 0.4),
            QBrush(QColor(col.red(), col.green(), col.blue(), 110)));
        item->setToolTip(
            QString("Деталь #%1  partId=%2\nX=%3  Y=%4  Угол=%5°")
            .arg(i + 1).arg(pp.partId)
            .arg(pp.position.x, 0, 'f', 1)
            .arg(pp.position.y, 0, 'f', 1)
            .arg(pp.angle));

        for (const auto& mark : pp.marks) {
            QPolygonF qm;
            for (const auto& v : mark.verts)
                qm << QPointF(v.x, H - v.y);
            scene_->addPolygon(qm, QPen(Qt::cyan, 0.3), QBrush(Qt::NoBrush));
        }
    }

    view_->setSceneRect(scene_->itemsBoundingRect().adjusted(-30, -30, 30, 30));
    view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
}

void MainWindow::onPrevSheet() { displaySheet(currentSheet_ - 1); }
void MainWindow::onNextSheet() { displaySheet(currentSheet_ + 1); }

void MainWindow::onPresetChanged(int idx) {
    static const char* W[] = {"", "3000", "2500", "2000", "1500", "1000"};
    static const char* H[] = {"", "1500", "1250", "1000",  "750",  "500"};
    if (idx > 0) { sheetW_->setText(W[idx]); sheetH_->setText(H[idx]); }
}

// ─── Экспорт ──────────────────────────────────────────────────────────────────
void MainWindow::onExportLXD() {
    if (result_.sheets.empty()) return;
    QString folder = QFileDialog::getExistingDirectory(this, "Папка для LXD файлов");
    if (folder.isEmpty()) return;

    bool ok = LXDExporter::exportAllSheets(result_.sheets, folder.toStdString());
    if (ok) {
        QMessageBox::information(this, "Экспорт LXD",
            QString("Экспортировано %1 файл(ов) в:\n%2")
            .arg(result_.sheets.size()).arg(folder));
        logView_->append(QString("✔ LXD → %1 файлов → %2")
            .arg(result_.sheets.size()).arg(folder));
    } else {
        QMessageBox::warning(this, "Ошибка", "Ошибка при записи LXD файлов.");
    }
}

void MainWindow::onExportDXF() {
    if (result_.sheets.empty()) return;
    QString folder = QFileDialog::getExistingDirectory(this, "Папка для DXF файлов");
    if (folder.isEmpty()) return;

    int exported = 0;
    for (size_t si = 0; si < result_.sheets.size(); ++si) {
        const Sheet& s = result_.sheets[si];
        QString path   = folder + QString("/sheet_%1.dxf").arg(si + 1);
        QFile   f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) continue;

        QTextStream out(&f);
        out.setLocale(QLocale::c());
        out.setRealNumberPrecision(6);
        out.setRealNumberNotation(QTextStream::FixedNotation);

        out << "0\nSECTION\n2\nENTITIES\n";
        for (const auto& pp : s.placed) {
            out << "0\nLWPOLYLINE\n8\n0\n70\n1\n90\n"
                << pp.shape.verts.size() << "\n";
            for (const auto& v : pp.shape.verts)
                out << "10\n" << v.x << "\n20\n" << v.y << "\n";
            for (const auto& mark : pp.marks) {
                out << "0\nLWPOLYLINE\n8\nMARK\n70\n1\n90\n"
                    << mark.verts.size() << "\n";
                for (const auto& v : mark.verts)
                    out << "10\n" << v.x << "\n20\n" << v.y << "\n";
            }
        }
        out << "0\nENDSEC\n0\nEOF\n";
        ++exported;
    }

    QMessageBox::information(this, "Экспорт DXF",
        QString("Экспортировано %1 DXF файлов в:\n%2").arg(exported).arg(folder));
    logView_->append(QString("✔ DXF → %1 файлов → %2")
        .arg(exported).arg(folder));
}

// ─── Блокировка UI ────────────────────────────────────────────────────────────
void MainWindow::setRunning(bool on) {
    fastBtn_->setEnabled(!on);
    autoBtn_->setEnabled(!on);
    optBtn_->setEnabled(!on);
    cancelBtn_->setEnabled(on);
    sheetW_->setEnabled(!on);
    sheetH_->setEnabled(!on);
    marginEdit_->setEnabled(!on);
    gapEdit_->setEnabled(!on);
    cutSpeedEdit_->setEnabled(!on);
    gaPop_->setEnabled(!on);
    gaGen_->setEnabled(!on);
    verboseChk_->setEnabled(!on);
}
