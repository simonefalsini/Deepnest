#include "TestApplication.h"
#include "SVGLoader.h"
#include "../include/deepnest/converters/QtBoostConverter.h"
#include "../include/deepnest/geometry/PolygonOperations.h"
#include "../include/deepnest/geometry/GeometryUtil.h"
#include <QThread>

#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QApplication>
#include <QDateTime>
#include <QSvgGenerator>
#include <QPainter>
#include <QGraphicsTextItem>
#include <random>
#include <chrono>
#include <iomanip>
#include <iostream>

TestApplication::TestApplication(QWidget* parent)
    : QMainWindow(parent)
    , scene_(nullptr)
    , view_(nullptr)
    , logWidget_(nullptr)
    , statusLabel_(nullptr)
    , progressBar_(nullptr)
    , generationLabel_(nullptr)
    , fitnessLabel_(nullptr)
    , solver_(nullptr)
    , stepTimer_(nullptr)
    , running_(false)
    , currentGeneration_(0)
    , bestFitness_(std::numeric_limits<double>::max())
    // lastResult_ is unique_ptr, auto-initialized to nullptr
    , maxGenerations_(100)
    , stepTimerInterval_(50)  // 50ms between steps
    , showShapeIds_(false)  // Hide IDs by default
    , currentSheetIndex_(0)
{
    LOG_MEMORY("TestApplication constructor started");
    setWindowTitle("DeepNest C++ Test Application");
    resize(1400, 900);

    // Register custom types for Qt signal/slot system
    qRegisterMetaType<deepnest::NestResult>("deepnest::NestResult");
    qRegisterMetaType<deepnest::NestProgress>("deepnest::NestProgress");

    // Initialize UI
    initUI();

    // Create solver
    solver_ = std::make_unique<deepnest::DeepNestSolver>();

    // Configure solver with sensible defaults
    // TEMPORARY DEBUG: spacing=0 to test algorithm without spacing mismatch
    solver_->setSpacing(0.0);           // 0 units spacing (DEBUG: will fix visualization later)
    solver_->setRotations(4);           // Allow 0°, 90°, 180°, 270°
    solver_->setPopulationSize(10);     // GA population size
    solver_->setMutationRate(10);       // 10% mutation rate
    log(QString("Solver configured: spacing=0.0 (DEBUG), rotations=4, pop=10"));

    // Setup callbacks
    solver_->setProgressCallback([this](const deepnest::NestProgress& progress) {
        // Use Qt::QueuedConnection for thread safety
        QMetaObject::invokeMethod(this, "onProgress", Qt::QueuedConnection,
                                Q_ARG(deepnest::NestProgress, progress));
    });

    solver_->setResultCallback([this](const deepnest::NestResult& result) {
        // Use Qt::QueuedConnection for thread safety
        QMetaObject::invokeMethod(this, "onResult", Qt::QueuedConnection,
                                Q_ARG(deepnest::NestResult, result));
    });

    // Create step timer
    stepTimer_ = new QTimer(this);
    connect(stepTimer_, &QTimer::timeout, this, &TestApplication::onStepTimer);

    log("DeepNest Test Application initialized");
    log("Ready to load parts or generate test shapes");

    LOG_MEMORY("TestApplication constructor completed");
}

TestApplication::~TestApplication() {
    LOG_MEMORY("TestApplication destructor entered");

    // CRITICAL: Stop nesting and wait for all threads to terminate
    if (running_) {
        LOG_NESTING("Destructor: Stopping nesting that was still running");
        stopNesting();  // This now waits for threads
    }

    // CRITICAL: Clear callbacks to prevent them from being called with dangling 'this'
    LOG_MEMORY("Clearing callbacks to prevent dangling pointer access");
    solver_->setProgressCallback(nullptr);
    solver_->setResultCallback(nullptr);

    // lastResult_ is unique_ptr, auto-deleted here
    LOG_MEMORY("TestApplication destructor completed");
}

void TestApplication::initUI() {
    // Create central widget with layout
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Create graphics view
    view_ = new ZoomableGraphicsView(this);
    scene_ = new QGraphicsScene(this);
    view_->setScene(scene_);
    view_->setRenderHint(QPainter::Antialiasing);
    view_->setDragMode(QGraphicsView::ScrollHandDrag);
    
    // Add view to layout
    mainLayout->addWidget(view_);

    // Create bottom controls
    QHBoxLayout* controlsLayout = new QHBoxLayout();
    controlsLayout->setContentsMargins(10, 5, 10, 5);

    controlsLayout->addStretch(); // Push to right

    sheetInfoLabel_ = new QLabel("Sheet 0 of 0", this);
    controlsLayout->addWidget(sheetInfoLabel_);

    prevSheetBtn_ = new QPushButton("< Prev", this);
    prevSheetBtn_->setEnabled(false);
    connect(prevSheetBtn_, &QPushButton::clicked, this, &TestApplication::prevSheet);
    controlsLayout->addWidget(prevSheetBtn_);

    nextSheetBtn_ = new QPushButton("Next >", this);
    nextSheetBtn_->setEnabled(false);
    connect(nextSheetBtn_, &QPushButton::clicked, this, &TestApplication::nextSheet);
    controlsLayout->addWidget(nextSheetBtn_);

    mainLayout->addLayout(controlsLayout);

    setCentralWidget(centralWidget);

    // Create log dock widget
    QDockWidget* logDock = new QDockWidget("Log", this);
    logWidget_ = new QTextEdit(logDock);
    logWidget_->setReadOnly(true);
    logWidget_->setMaximumHeight(200);
    logDock->setWidget(logWidget_);
    addDockWidget(Qt::BottomDockWidgetArea, logDock);

    // Create menus and toolbars
    createMenus();
    createToolbar();
    createStatusBar();
}

void TestApplication::createMenus() {
    QMenu* fileMenu = menuBar()->addMenu("&File");

    QAction* loadAction = fileMenu->addAction("&Load SVG...");
    loadAction->setShortcut(QKeySequence::Open);
    connect(loadAction, &QAction::triggered, this, &TestApplication::loadSVG);

    QAction* exportAction = fileMenu->addAction("&Export Result...");
    exportAction->setShortcut(QKeySequence::Save);
    connect(exportAction, &QAction::triggered, this, &TestApplication::exportSVG);

    fileMenu->addSeparator();

    QAction* importAction = fileMenu->addAction("&Import SVG (Append)...");
    importAction->setShortcut(QKeySequence("Ctrl+I"));
    connect(importAction, &QAction::triggered, this, &TestApplication::importSVG);

    fileMenu->addSeparator();

    QAction* exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    QMenu* testMenu = menuBar()->addMenu("&Test");

    QAction* rectAction = testMenu->addAction("Random &Rectangles");
    connect(rectAction, &QAction::triggered, this, &TestApplication::testRandomRectangles);

    QAction* polyAction = testMenu->addAction("Random &Polygons");
    connect(polyAction, &QAction::triggered, this, &TestApplication::testRandomPolygons);

    QMenu* nestingMenu = menuBar()->addMenu("&Nesting");

    QAction* configAction = nestingMenu->addAction("&Configuration...");
    connect(configAction, &QAction::triggered, this, &TestApplication::openConfigDialog);

    nestingMenu->addSeparator();

    QAction* startAction = nestingMenu->addAction("&Start");
    startAction->setShortcut(QKeySequence(Qt::Key_F5));
    connect(startAction, &QAction::triggered, this, &TestApplication::startNesting);

    QAction* stopAction = nestingMenu->addAction("St&op");
    stopAction->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(stopAction, &QAction::triggered, this, &TestApplication::stopNesting);

    QAction* resetAction = nestingMenu->addAction("&Reset");
    connect(resetAction, &QAction::triggered, this, &TestApplication::reset);

    // Container menu
    QMenu* containerMenu = menuBar()->addMenu("&Container");

    QAction* setContainerAction = containerMenu->addAction("Set Container &Size...");
    connect(setContainerAction, &QAction::triggered, this, &TestApplication::setContainerSize);

    QAction* viewContainerAction = containerMenu->addAction("&View Container Info");
    connect(viewContainerAction, &QAction::triggered, this, &TestApplication::viewContainerInfo);
}

void TestApplication::createToolbar() {
    QToolBar* toolbar = addToolBar("Main");
    toolbar->setMovable(false);

    QPushButton* loadBtn = new QPushButton("Load SVG", toolbar);
    connect(loadBtn, &QPushButton::clicked, this, &TestApplication::loadSVG);
    toolbar->addWidget(loadBtn);

    QPushButton* importBtn = new QPushButton("Import SVG", toolbar);
    connect(importBtn, &QPushButton::clicked, this, &TestApplication::importSVG);
    toolbar->addWidget(importBtn);

    toolbar->addSeparator();

    QPushButton* generateBtn = new QPushButton("Test Polygon", toolbar);
    connect(generateBtn, &QPushButton::clicked, this, &TestApplication::generatePolygons);
    toolbar->addWidget(generateBtn);

    QPushButton* containerBtn = new QPushButton("Container", toolbar);
    connect(containerBtn, &QPushButton::clicked, this, &TestApplication::setContainerSize);
    toolbar->addWidget(containerBtn);

    QPushButton* rectBtn = new QPushButton("Test Rectangles", toolbar);
    connect(rectBtn, &QPushButton::clicked, this, &TestApplication::testRandomRectangles);
    toolbar->addWidget(rectBtn);

    toolbar->addSeparator();

    QPushButton* configBtn = new QPushButton("Config", toolbar);
    connect(configBtn, &QPushButton::clicked, this, &TestApplication::openConfigDialog);
    toolbar->addWidget(configBtn);

    toolbar->addSeparator();

    QPushButton* startBtn = new QPushButton("Start", toolbar);
    startBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; padding: 5px 15px; }");
    connect(startBtn, &QPushButton::clicked, this, &TestApplication::startNesting);
    toolbar->addWidget(startBtn);

    QPushButton* stopBtn = new QPushButton("Stop", toolbar);
    stopBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; padding: 5px 15px; }");
    connect(stopBtn, &QPushButton::clicked, this, &TestApplication::stopNesting);
    toolbar->addWidget(stopBtn);

    QPushButton* resetBtn = new QPushButton("Reset", toolbar);
    connect(resetBtn, &QPushButton::clicked, this, &TestApplication::reset);
    toolbar->addWidget(resetBtn);

    toolbar->addSeparator();

    QPushButton* toggleIdsBtn = new QPushButton("Toggle IDs", toolbar);
    toggleIdsBtn->setCheckable(true);
    toggleIdsBtn->setChecked(showShapeIds_);
    connect(toggleIdsBtn, &QPushButton::clicked, this, &TestApplication::toggleShapeIds);
    toolbar->addWidget(toggleIdsBtn);
}

void TestApplication::createStatusBar() {
    statusLabel_ = new QLabel("Ready");
    statusBar()->addWidget(statusLabel_);

    generationLabel_ = new QLabel("Generation: 0");
    statusBar()->addWidget(generationLabel_);

    fitnessLabel_ = new QLabel("Fitness: N/A");
    statusBar()->addWidget(fitnessLabel_);

    progressBar_ = new QProgressBar();
    progressBar_->setMaximum(100);
    progressBar_->setMinimumWidth(200);
    statusBar()->addPermanentWidget(progressBar_);
}

void TestApplication::loadSVG() {
    QString filename = QFileDialog::getOpenFileName(
        this,
        "Load SVG File",
        QString(),
        "SVG Files (*.svg);;All Files (*)"
    );

    if (filename.isEmpty()) {
        return;
    }

    reset();
    log("Loading SVG: " + filename);

    // Load SVG file
    SVGLoader::LoadResult result = SVGLoader::loadFile(filename);

    if (!result.success()) {
        log("Error loading SVG: " + result.errorMessage);
        QMessageBox::critical(this, "Load Error", "Failed to load SVG file:\n" + result.errorMessage);
        return;
    }

    // Add container/sheet if found
    int sheetCount = 0;
    if (result.container) {
        QPainterPath containerPath = result.container->path;
        if (!result.container->transform.isIdentity()) {
            containerPath = result.container->transform.map(containerPath);
        }

        deepnest::Polygon sheetPoly = deepnest::QtBoostConverter::fromQPainterPath(containerPath, 0);
        sheets_.push_back(sheetPoly);  // Save for visualization
        solver_->addSheet(sheetPoly, config_.quantityPerSheet, result.container->id.isEmpty() ? "Sheet" : result.container->id.toStdString());
        sheetCount++;

        log(QString("Added sheet: %1").arg(result.container->id.isEmpty() ? "Sheet" : result.container->id));
    }

    // Add all shapes as parts
    int partCount = 0;
    for (const auto& shape : result.shapes) {
        QPainterPath shapePath = shape.path;
        if (!shape.transform.isIdentity()) {
            shapePath = shape.transform.map(shapePath);
        }

        deepnest::Polygon partPoly = deepnest::QtBoostConverter::fromQPainterPath(shapePath, partCount);

        if (partPoly.isValid()) {
            QString partName = shape.id.isEmpty() ? QString("Part_%1").arg(partCount) : shape.id;
            parts_.push_back(partPoly);  // Save for visualization
            solver_->addPart(partPoly, 1, partName.toStdString());
            partCount++;
        }
    }

    log(QString("Loaded %1 parts and %2 sheet(s) from SVG").arg(partCount).arg(sheetCount));

    if (partCount == 0) {
        QMessageBox::warning(this, "No Parts", "No valid parts found in SVG file");
        return;
    }

    if (sheetCount == 0) {
        // No container found - ask user to define one
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "No Container Found",
            "No container found in SVG.\n\n"
            "Would you like to define a container now?",
            QMessageBox::Yes | QMessageBox::No
        );

        if (reply == QMessageBox::Yes) {
            ContainerDialog dialog(this);
            if (dialog.exec() == QDialog::Accepted) {
                double width = dialog.getWidth();
                double height = dialog.getHeight();

                // Create container
                std::vector<deepnest::Point> points = {
                    {0, 0},
                    {width, 0},
                    {width, height},
                    {0, height}
                };

                deepnest::Polygon container(points, -1);
                sheets_.push_back(container);
                solver_->addSheet(container, 1, "Container");

                log(QString("Container created: %1 x %2 mm").arg(width).arg(height));
            } else {
                log("Container creation cancelled");
            }
        }
    }

    // Visualize loaded shapes with uniform layout (container left, parts right)
    clearScene();

    // Draw container on the left if present
    if (!sheets_.empty()) {
        drawPolygon(sheets_[0], Qt::black, 0.0, true);  // Show border for container

        // Add container label
        auto bbox = deepnest::GeometryUtil::getPolygonBounds(sheets_[0].points);
        QGraphicsTextItem* containerLabel = scene_->addText("Container", QFont("Arial", 12, QFont::Bold));
        containerLabel->setDefaultTextColor(Qt::black);
        containerLabel->setPos(bbox.x + bbox.width/2 - 40, bbox.y - 30);
    }

    // Draw parts on the right in a grid layout
    double xOffset = sheets_.empty() ? 50 : 550;  // Start at 550 if we have a container
    double yOffset = 50;
    double maxHeight = 0;

    for (size_t i = 0; i < parts_.size(); ++i) {
        // Get bounding box
        auto bbox = deepnest::GeometryUtil::getPolygonBounds(parts_[i].points);
        double width = bbox.width;
        double height = bbox.height;

        // Check if we need to wrap to next row
        if (xOffset + width > 1400) {
            xOffset = sheets_.empty() ? 50 : 550;
            yOffset += maxHeight + 20;
            maxHeight = 0;
        }

        // Translate part to current position
        deepnest::Polygon translated = parts_[i].translate(xOffset - bbox.x, yOffset - bbox.y);

        // Draw part (blue with transparency)
        drawPolygon(translated, QColor(100, 150, 200), 0.3);

        // Add ID label on the part (if enabled)
        if (showShapeIds_) {
            QGraphicsTextItem* idLabel = scene_->addText(QString::number(i), QFont("Arial", 6));
            idLabel->setDefaultTextColor(Qt::darkBlue);
            idLabel->setPos(xOffset + width/2 - 5, yOffset + height/2 - 5);
        }

        xOffset += width + 20;
        maxHeight = std::max(maxHeight, height);
    }

    view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
    log("Ready to start nesting - click 'Start' button");
}

void TestApplication::startNesting() {
    if (running_) {
        log("Nesting is already running");
        return;
    }

    if (solver_->getPartCount() == 0) {
        QMessageBox::warning(this, "No Parts", "Please add some parts first");
        return;
    }

    if (solver_->getSheetCount() == 0) {
        QMessageBox::warning(this, "No Sheets", "Please add a sheet first");
        return;
    }

    bool ok;
    int gens = QInputDialog::getInt(this, "Max Generations",
                                    "Enter maximum generations (0 = unlimited):",
                                    maxGenerations_, 0, 10000, 10, &ok);
    if (!ok) {
        return;
    }

    maxGenerations_ = gens;
    running_ = true;
    currentGeneration_ = 0;
    bestFitness_ = std::numeric_limits<double>::max();

    log(QString("Starting nesting (max %1 generations)...").arg(maxGenerations_));
    log(QString("Configuration: Spacing=%1, Rotations=%2, Pop=%3, SheetQty=%4")
        .arg(config_.spacing)
        .arg(config_.rotations)
        .arg(config_.populationSize)
        .arg(config_.quantityPerSheet));

    try {
        solver_->start(maxGenerations_);
        stepTimer_->start(stepTimerInterval_);
        statusLabel_->setText("Running");
    }
    catch (const std::exception& e) {
        log(QString("Error starting nesting: %1").arg(e.what()));
        running_ = false;
        QMessageBox::critical(this, "Error", QString("Failed to start nesting:\n%1").arg(e.what()));
    }
}

void TestApplication::stopNesting() {
    if (!running_) {
        LOG_NESTING("stopNesting() called but not running, ignoring");
        return;
    }

    LOG_NESTING("Stopping nesting...");
    log("Stopping nesting...");

    // Stop the step timer first
    stepTimer_->stop();
    LOG_THREAD("Step timer stopped");

    // Signal solver to stop
    solver_->stop();
    LOG_THREAD("Solver stop() called, waiting for threads to finish...");

    // CRITICAL: Wait for solver threads to actually terminate
    // This prevents race conditions where callbacks are called after stopNesting() returns
    // Give threads time to finish and callbacks to complete
    // TODO: Implement solver_->waitForCompletion() for proper synchronization
    QThread::msleep(100);  // Wait 100ms for threads to wind down
    LOG_THREAD("Waited 100ms for threads to terminate");

    // Process any remaining Qt events to handle queued callbacks
    QCoreApplication::processEvents();
    LOG_THREAD("Processed remaining Qt events");

    running_ = false;
    statusLabel_->setText("Stopped");

    LOG_NESTING("Nesting stopped, all threads terminated");
    log("Nesting stopped");
}

void TestApplication::reset() {
    LOG_NESTING("Reset called");

    if (running_) {
        LOG_NESTING("Reset: stopping nesting first");
        stopNesting();
    }

    LOG_MEMORY("Clearing solver state");
    solver_->clear();
    parts_.clear();
    sheets_.clear();
    clearScene();
    currentGeneration_ = 0;
    bestFitness_ = std::numeric_limits<double>::max();

    // CRITICAL: Thread-safe reset of lastResult_
    {
        std::lock_guard<std::mutex> lock(resultMutex_);
        lastResult_.reset();  // unique_ptr, no need for delete
        LOG_MEMORY("lastResult_ reset to nullptr (thread-safe)");
    }

    currentGeneration_ = 0;
    bestFitness_ = std::numeric_limits<double>::max();
    currentSheetIndex_ = 0;

    generationLabel_->setText("Generation: 0");
    fitnessLabel_->setText("Best Fitness: -");
    // statusLabel_->setText("Ready"); // This line was commented out in the original snippet, keeping it that way
    progressBar_->setValue(0);
    sheetInfoLabel_->setText("Sheet 0 of 0");
    prevSheetBtn_->setEnabled(false);
    nextSheetBtn_->setEnabled(false);

    log("Application reset");

    updateStatus();

    log("Reset complete");
    LOG_NESTING("Reset completed");
}

void TestApplication::testRandomRectangles() {
    // Ask user for number of rectangles
    bool ok;
    int count = QInputDialog::getInt(this, "Test Rectangles",
                                     "Number of rectangles to generate:", 10, 1, 100, 1, &ok);
    if (!ok) {
        return;
    }

    reset();

    log(QString("Generating %1 random rectangles...").arg(count));

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> widthDist(config_.minPartWidth, config_.maxPartWidth);
    std::uniform_real_distribution<> heightDist(config_.minPartHeight, config_.maxPartHeight);

    // Create N random rectangles
    for (int i = 0; i < count; ++i) {
        double w = widthDist(gen);
        double h = heightDist(gen);

        std::vector<deepnest::Point> points;
        // Create counter-clockwise rectangle
        points.push_back(deepnest::Point(0, 0));
        points.push_back(deepnest::Point(0, h));  // Up
        points.push_back(deepnest::Point(w, h));  // Right
        points.push_back(deepnest::Point(w, 0));  // Down

        deepnest::Polygon rect(points, i);  // Assign id

        // Validate polygon
        if (!rect.isValid()) {
            log(QString("WARNING: Rectangle %1 is invalid!").arg(i));
        }
        if (!rect.isCounterClockwise()) {
            log(QString("WARNING: Rectangle %1 is clockwise, reversing").arg(i));
            rect.reverse();  // Fix orientation
        }

        log(QString("Created rect %1: %2x%3, area=%4, CCW=%5")
            .arg(i)
            .arg(w, 0, 'f', 1)
            .arg(h, 0, 'f', 1)
            .arg(std::abs(rect.area()), 0, 'f', 1)
            .arg(rect.isCounterClockwise() ? "yes" : "no"));

        parts_.push_back(rect);  // Save for visualization
        solver_->addPart(rect, config_.quantityPerPart, QString("Rect_%1").arg(i).toStdString());
    }

    // Create sheet with configured dimensions - counter-clockwise
    std::vector<deepnest::Point> sheetPoints;
    sheetPoints.push_back(deepnest::Point(0, 0));
    sheetPoints.push_back(deepnest::Point(0, config_.sheetHeight));    // Up
    sheetPoints.push_back(deepnest::Point(config_.sheetWidth, config_.sheetHeight));  // Right
    sheetPoints.push_back(deepnest::Point(config_.sheetWidth, 0));    // Down

    deepnest::Polygon sheet(sheetPoints, 0);  // Assign id

    // Validate sheet
    if (!sheet.isValid()) {
        log("ERROR: Sheet is invalid!");
    }
    if (!sheet.isCounterClockwise()) {
        log("WARNING: Sheet is clockwise, reversing");
        sheet.reverse();
    }

    log(QString("Created sheet: %1x%2, area=%3, CCW=%4")
        .arg(config_.sheetWidth, 0, 'f', 0)
        .arg(config_.sheetHeight, 0, 'f', 0)
        .arg(std::abs(sheet.area()), 0, 'f', 1)
        .arg(sheet.isCounterClockwise() ? "yes" : "no"));

    sheets_.push_back(sheet);  // Save for visualization
    solver_->addSheet(sheet, config_.quantityPerSheet,
                      QString("Sheet_%1x%2").arg(config_.sheetWidth, 0, 'f', 0)
                                            .arg(config_.sheetHeight, 0, 'f', 0)
                                            .toStdString());

    log(QString("Created %1 part types (%2 total parts) and 1 sheet type (%3 sheets)")
        .arg(config_.numPartTypes)
        .arg(config_.numPartTypes * config_.quantityPerPart)
        .arg(config_.quantityPerSheet));

    // Visualize the parts BEFORE nesting - arranged side by side
    clearScene();

    // Draw sheet boundary first (green outline, no fill)
    QPainterPath sheetPath = sheet.toQPainterPath();
    scene_->addPath(sheetPath, QPen(Qt::green, 3), QBrush(Qt::NoBrush));

    // Add sheet label
    QGraphicsTextItem* sheetLabel = scene_->addText("SHEET 500x400", QFont("Arial", 12));
    sheetLabel->setDefaultTextColor(Qt::darkGreen);
    sheetLabel->setPos(10, -30);

    // Draw parts arranged in a grid OUTSIDE the sheet area
    double xOffset = 550;  // Start after sheet
    double yOffset = 50;
    double maxHeight = 0;

    for (size_t i = 0; i < parts_.size(); ++i) {
        // Get bounding box
        auto bbox = deepnest::GeometryUtil::getPolygonBounds(parts_[i].points);
        double width = bbox.width;
        double height = bbox.height;

        // Check if we need to wrap to next row
        if (xOffset + width > 1400) {
            xOffset = 550;
            yOffset += maxHeight + 20;
            maxHeight = 0;
        }

        // Translate part to current position
        deepnest::Polygon translated = parts_[i].translate(xOffset - bbox.x, yOffset - bbox.y);

        // Draw part (blue with transparency)
        drawPolygon(translated, QColor(100, 150, 200), 0.3);

        // Add ID label on the part
        QGraphicsTextItem* idLabel = scene_->addText(QString::number(i), QFont("Arial", 10, QFont::Bold));
        idLabel->setDefaultTextColor(Qt::darkBlue);
        idLabel->setPos(xOffset + width/2 - 10, yOffset + height/2 - 10);

        xOffset += width + 20;
        maxHeight = std::max(maxHeight, height);
    }

    view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
    log("Ready to start nesting - click 'Start' button");
}

void TestApplication::testRandomPolygons() {
    reset();

    log(QString("Generating %1 random polygon types (%2 total parts)...")
        .arg(config_.numPartTypes).arg(config_.numPartTypes * config_.quantityPerPart));

    std::random_device rd;
    std::mt19937 gen(rd());

    // Calculate radius from configured part dimensions
    double minRadius = std::min(config_.minPartWidth, config_.minPartHeight) / 2.0;
    double maxRadius = std::min(config_.maxPartWidth, config_.maxPartHeight) / 2.0;
    std::uniform_real_distribution<> sizeDist(minRadius, maxRadius);
    std::uniform_int_distribution<> sidesDist(config_.minPolySides, config_.maxPolySides);

    // Create random polygons
    for (int i = 0; i < config_.numPartTypes; ++i) {
        int sides = sidesDist(gen);
        double radius = sizeDist(gen);

        std::vector<deepnest::Point> points;
        for (int j = 0; j < sides; ++j) {
            double angle = 2.0 * M_PI * j / sides;
            double x = radius * std::cos(angle);
            double y = radius * std::sin(angle);
            points.push_back(deepnest::Point(x + radius, y + radius));
        }

        deepnest::Polygon poly(points, i);  // Assign id
        parts_.push_back(poly);  // Save for visualization
        solver_->addPart(poly, config_.quantityPerPart, QString("Poly_%1").arg(i).toStdString());
    }

    // Create sheet with configured dimensions - counter-clockwise
    std::vector<deepnest::Point> sheetPoints;
    sheetPoints.push_back(deepnest::Point(0, 0));
    sheetPoints.push_back(deepnest::Point(0, config_.sheetHeight));    // Up
    sheetPoints.push_back(deepnest::Point(config_.sheetWidth, config_.sheetHeight));  // Right
    sheetPoints.push_back(deepnest::Point(config_.sheetWidth, 0));    // Down

    deepnest::Polygon sheet(sheetPoints, 0);  // Assign id

    // Validate sheet
    if (!sheet.isValid()) {
        log("ERROR: Sheet is invalid!");
    }
    if (!sheet.isCounterClockwise()) {
        log("WARNING: Sheet is clockwise, reversing");
        sheet.reverse();
    }

    log(QString("Created sheet: %1x%2, area=%3, CCW=%4")
        .arg(config_.sheetWidth, 0, 'f', 0)
        .arg(config_.sheetHeight, 0, 'f', 0)
        .arg(std::abs(sheet.area()), 0, 'f', 1)
        .arg(sheet.isCounterClockwise() ? "yes" : "no"));

    sheets_.push_back(sheet);  // Save for visualization
    solver_->addSheet(sheet, config_.quantityPerSheet,
                      QString("Sheet_%1x%2").arg(config_.sheetWidth, 0, 'f', 0)
                                            .arg(config_.sheetHeight, 0, 'f', 0)
                                            .toStdString());

    log(QString("Created %1 part types (%2 total parts) and 1 sheet type (%3 sheets)")
        .arg(config_.numPartTypes)
        .arg(config_.numPartTypes * config_.quantityPerPart)
        .arg(config_.quantityPerSheet));

    // Visualize the parts BEFORE nesting - arranged side by side
    clearScene();

    // Draw sheet boundary first (green outline, no fill)
    QPainterPath sheetPath = sheet.toQPainterPath();
    scene_->addPath(sheetPath, QPen(Qt::green, 3), QBrush(Qt::NoBrush));

    // Add sheet label
    QGraphicsTextItem* sheetLabel = scene_->addText("SHEET 600x400", QFont("Arial", 12));
    sheetLabel->setDefaultTextColor(Qt::darkGreen);
    sheetLabel->setPos(10, -30);

    // Draw parts arranged in a grid OUTSIDE the sheet area
    double xOffset = 650;  // Start after sheet (600 + 50 margin)
    double yOffset = 50;
    double maxHeight = 0;

    for (size_t i = 0; i < parts_.size(); ++i) {
        // Get bounding box
        auto bbox = deepnest::GeometryUtil::getPolygonBounds(parts_[i].points);
        double width = bbox.width;
        double height = bbox.height;

        // Check if we need to wrap to next row
        if (xOffset + width > 1400) {
            xOffset = 650;
            yOffset += maxHeight + 20;
            maxHeight = 0;
        }

        // Translate part to current position
        deepnest::Polygon translated = parts_[i].translate(xOffset - bbox.x, yOffset - bbox.y);

        // Draw part (blue with transparency)
        drawPolygon(translated, QColor(100, 150, 200), 0.3);

        // Add ID label on the part
        QGraphicsTextItem* idLabel = scene_->addText(QString::number(i), QFont("Arial", 10, QFont::Bold));
        idLabel->setDefaultTextColor(Qt::darkBlue);
        idLabel->setPos(xOffset + width/2 - 10, yOffset + height/2 - 10);

        xOffset += width + 20;
        maxHeight = std::max(maxHeight, height);
    }

    view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
    log("Ready to start nesting - click 'Start' button");
}

void TestApplication::openConfigDialog() {
    // Open unified configuration dialog
    ConfigDialog dialog(config_, this);

    if (dialog.exec() == QDialog::Accepted) {
        // Update configuration
        config_ = dialog.getConfig();

        // Apply nesting parameters to solver
        solver_->setSpacing(config_.spacing);
        solver_->setCurveTolerance(config_.curveTolerance);
        solver_->setRotations(config_.rotations);
        solver_->setPopulationSize(config_.populationSize);
        solver_->setMutationRate(config_.mutationRate);
        solver_->setThreads(config_.threads);
        solver_->setPlacementType(config_.placementType.toStdString());
        
        // Convert gravity direction string to enum and apply
        deepnest::GravityDirection gravityDir = deepnest::GravityDirection::LEFT; // Default
        if (config_.gravityDirection == "left") {
            gravityDir = deepnest::GravityDirection::LEFT;
        } else if (config_.gravityDirection == "right") {
            gravityDir = deepnest::GravityDirection::RIGHT;
        } else if (config_.gravityDirection == "bottom") {
            gravityDir = deepnest::GravityDirection::BOTTOM;
        } else if (config_.gravityDirection == "top") {
            gravityDir = deepnest::GravityDirection::TOP;
        } else if (config_.gravityDirection == "bottom_left") {
            gravityDir = deepnest::GravityDirection::BOTTOM_LEFT;
        }
        
        // Apply gravity direction to DeepNestConfig
        deepnest::DeepNestConfig& deepnestConfig = deepnest::DeepNestConfig::getInstance();
        deepnestConfig.gravityDirection = gravityDir;
        
        // Apply overlap tolerance to DeepNestConfig
        deepnestConfig.overlapTolerance = config_.overlapTolerance;
        
        solver_->setMergeLines(config_.mergeLines);
        solver_->setTimeRatio(config_.timeRatio);

        // Update max generations
        maxGenerations_ = config_.maxGenerations;

        log(QString("Configuration updated: spacing=%1, curveTol=%2, rotations=%3, popSize=%4")
            .arg(config_.spacing, 0, 'f', 1)
            .arg(config_.curveTolerance, 0, 'f', 2)
            .arg(config_.rotations)
            .arg(config_.populationSize));

        log(QString("  Algorithm: placementType=%1, gravityDir=%2, mergeLines=%3, timeRatio=%4, threads=%5")
            .arg(config_.placementType)
            .arg(config_.gravityDirection)
            .arg(config_.mergeLines ? "ON" : "OFF")
            .arg(config_.timeRatio, 0, 'f', 2)
            .arg(config_.threads));

        log(QString("  Precision: overlapTolerance=%1")
            .arg(config_.overlapTolerance, 0, 'e', 4));  // Scientific notation for small values

        log(QString("  Generation: %1 part types × %2 qty, %3 sheet types × %4 qty")
            .arg(config_.numPartTypes)
            .arg(config_.quantityPerPart)
            .arg(config_.numSheetTypes)
            .arg(config_.quantityPerSheet));
    }
}

void TestApplication::onProgress(const deepnest::NestProgress& progress) {
    currentGeneration_ = progress.generation;
    bestFitness_ = progress.bestFitness;

    generationLabel_->setText(QString("Generation: %1").arg(progress.generation));
    fitnessLabel_->setText(QString("Fitness: %1").arg(progress.bestFitness, 0, 'f', 2));
    progressBar_->setValue(static_cast<int>(progress.percentComplete));

    if (progress.generation % 10 == 0) {
        log(QString("Gen %1: Fitness = %2, Progress = %3%")
            .arg(progress.generation)
            .arg(progress.bestFitness, 0, 'f', 2)
            .arg(progress.percentComplete, 0, 'f', 1));
    }
}

void TestApplication::onResult(const deepnest::NestResult& result) {
    LOG_MEMORY("onResult callback invoked from worker thread");

    // CRITICAL: Thread-safe update of lastResult_ using mutex
    {
        std::lock_guard<std::mutex> lock(resultMutex_);
        lastResult_ = std::make_unique<deepnest::NestResult>(result);
        LOG_MEMORY("lastResult_ updated (thread-safe with unique_ptr)");
    }

    log(QString("New best result! Fitness: %1 at generation %2")
        .arg(result.fitness, 0, 'f', 2)
        .arg(result.generation));

    // CRITICAL: Update visualization on UI thread, not worker thread
    // Use QueuedConnection to marshal back to main thread safely
    QMetaObject::invokeMethod(this, [this, result]() {
        LOG_THREAD("updateVisualization executing on UI thread");
        updateVisualization(result);
    }, Qt::QueuedConnection);

    LOG_MEMORY("onResult callback completed");
}

void TestApplication::onStepTimer() {
    if (!solver_->isRunning()) {
        stepTimer_->stop();
        running_ = false;
        statusLabel_->setText("Complete");
        log("Nesting complete!");

        const deepnest::NestResult* best = solver_->getBestResult();
        if (best) {
            log(QString("Final best fitness: %1").arg(best->fitness, 0, 'f', 2));
            // Visualize the final best result
            updateVisualization(*best);
        }
        return;
    }

    // Perform one step
    solver_->step();
}

void TestApplication::exportSVG() {
    // CRITICAL: Thread-safe check of lastResult_
    {
        std::lock_guard<std::mutex> lock(resultMutex_);
        if (!lastResult_) {
            QMessageBox::warning(this, "No Result", "No nesting result available to export");
            return;
        }
    }

    QString filename = QFileDialog::getSaveFileName(
        this,
        "Export SVG",
        QString(),
        "SVG Files (*.svg);;All Files (*)"
    );

    if (filename.isEmpty()) {
        return;
    }

    log("Exporting result to: " + filename);

    // Export using QSvgGenerator
    QSvgGenerator generator;
    generator.setFileName(filename);
    generator.setSize(QSize(800, 600));
    generator.setViewBox(QRect(0, 0, 800, 600));
    generator.setTitle("DeepNest Result");
    generator.setDescription("Nesting result from DeepNest C++");

    QPainter painter(&generator);
    scene_->render(&painter);

    log("Export complete");
}

void TestApplication::updateVisualization(const deepnest::NestResult& result) {
    // Detailed diagnostics
    log(QString("=== Visualization Update ==="));
    log(QString("Result fitness: %1, generation: %2")
        .arg(result.fitness, 0, 'f', 2)
        .arg(result.generation));
    log(QString("Result has %1 sheets in placements vector")
        .arg(result.placements.size()));

    // Count total placements to see if we have anything to show
    int totalPlacements = 0;
    for (size_t i = 0; i < result.placements.size(); ++i) {
        const auto& sheetPlacements = result.placements[i];
        log(QString("  Sheet %1: %2 placements").arg(i).arg(sheetPlacements.size()));
        totalPlacements += sheetPlacements.size();
    }

    log(QString("Total placements: %1").arg(totalPlacements));

    // Only update visualization if we have placements to show
    if (totalPlacements == 0) {
        log(QString("Result has no placements yet, keeping current view"));
        return;
    }

    log(QString("Visualizing %1 placed parts on %2 sheets")
        .arg(totalPlacements)
        .arg(result.placements.size()));

    // Clamp index
    if (currentSheetIndex_ >= static_cast<int>(result.placements.size())) {
        currentSheetIndex_ = static_cast<int>(result.placements.size()) - 1;
    }
    if (currentSheetIndex_ < 0) currentSheetIndex_ = 0;

    // Update controls
    sheetInfoLabel_->setText(QString("Sheet %1 of %2")
        .arg(currentSheetIndex_ + 1)
        .arg(result.placements.size()));

    prevSheetBtn_->setEnabled(currentSheetIndex_ > 0);
    nextSheetBtn_->setEnabled(currentSheetIndex_ < static_cast<int>(result.placements.size()) - 1);

    clearScene();

    // Draw each sheet with placements
    double sheetOffsetX = 0;
    size_t sheetIdx = currentSheetIndex_;
    //for (size_t sheetIdx = 0; sheetIdx < result.placements.size(); ++sheetIdx) 
    {
        const auto& sheetPlacements = result.placements[sheetIdx];

        log(QString("--- Sheet %1 ---").arg(sheetIdx));

        // Draw sheet boundary (green, thick border, no fill)
        // All sheets have the same dimensions, so we use the first sheet as a template
        if (!sheets_.empty()) {
            deepnest::Polygon sheetTranslated = sheets_[0].translate(sheetOffsetX, 0);
            QPainterPath sheetPath = sheetTranslated.toQPainterPath();
            scene_->addPath(sheetPath, QPen(Qt::darkGreen, 4), QBrush(Qt::NoBrush));

            // Add sheet label
            QGraphicsTextItem* sheetLabel = scene_->addText(
                QString("SHEET %1").arg(sheetIdx + 1),  // +1 for 1-based display
                QFont("Arial", 14, QFont::Bold));
            sheetLabel->setDefaultTextColor(Qt::darkGreen);
            sheetLabel->setPos(sheetOffsetX + 10, -35);
        }

        // Define distinct colors for each part type (by source ID)
        QColor partColors[10] = {
            QColor(255, 100, 100),  // Red
            QColor(100, 255, 100),  // Green
            QColor(100, 100, 255),  // Blue
            QColor(255, 255, 100),  // Yellow
            QColor(255, 100, 255),  // Magenta
            QColor(100, 255, 255),  // Cyan
            QColor(255, 150, 100),  // Orange
            QColor(150, 100, 255),  // Purple
            QColor(100, 255, 150),  // Lime
            QColor(255, 100, 150)   // Pink
        };

        // Draw each placed part
        int placementCount = 0;
        for (const auto& placement : sheetPlacements) {
            // Log first 5 placements for diagnostics
            if (placementCount < 5) {
                log(QString("  Placement %1: id=%2, source=%3, pos=(%4, %5), rot=%6")
                    .arg(placementCount)
                    .arg(placement.id)
                    .arg(placement.source)
                    .arg(placement.position.x, 0, 'f', 1)
                    .arg(placement.position.y, 0, 'f', 1)
                    .arg(placement.rotation, 0, 'f', 1));
            }
            placementCount++;

            // Use source ID to find the original polygon type
            int sourceId = (placement.source >= 0) ? placement.source : placement.id;

            if (sourceId >= 0 && sourceId < static_cast<int>(parts_.size())) {
                deepnest::Polygon part = parts_[sourceId];

                // DETAILED LOGGING for first 3 placements
                if (placementCount <= 3) {
                    log(QString("=== VISUALIZATION DEBUG Placement %1 ===").arg(placementCount - 1));
                    log(QString("  Source ID: %1, Rotation: %2°, Position: (%3, %4)")
                        .arg(sourceId)
                        .arg(placement.rotation)
                        .arg(placement.position.x, 0, 'f', 2)
                        .arg(placement.position.y, 0, 'f', 2));

                    // Log original part (first 4 points)
                    QString origPoints;
                    for (size_t i = 0; i < std::min(size_t(4), part.points.size()); ++i) {
                        origPoints += QString("(%1,%2) ")
                            .arg(part.points[i].x, 0, 'f', 1)
                            .arg(part.points[i].y, 0, 'f', 1);
                    }
                    log(QString("  Original part points: %1").arg(origPoints));
                }

                // Apply transformation: rotate FIRST, then translate
                // Step 1: Rotate around origin
                deepnest::Polygon rotated = part.rotate(placement.rotation);

                if (placementCount <= 3) {
                    // Log rotated part (first 4 points)
                    QString rotPoints;
                    for (size_t i = 0; i < std::min(size_t(4), rotated.points.size()); ++i) {
                        rotPoints += QString("(%1,%2) ")
                            .arg(rotated.points[i].x, 0, 'f', 1)
                            .arg(rotated.points[i].y, 0, 'f', 1);
                    }
                    log(QString("  After rotation points: %1").arg(rotPoints));
                }

                // Step 2: Translate to final position + sheet offset
                deepnest::Polygon transformed = rotated.translate(
                    placement.position.x + sheetOffsetX,
                    placement.position.y);

                if (placementCount <= 3) {
                    // Log transformed part (first 4 points)
                    QString transPoints;
                    for (size_t i = 0; i < std::min(size_t(4), transformed.points.size()); ++i) {
                        transPoints += QString("(%1,%2) ")
                            .arg(transformed.points[i].x, 0, 'f', 1)
                            .arg(transformed.points[i].y, 0, 'f', 1);
                    }
                    log(QString("  Final transformed points: %1").arg(transPoints));

                    // Log bounding box
                    auto bbox = deepnest::GeometryUtil::getPolygonBounds(transformed.points);
                    log(QString("  Final bbox: (%1,%2) to (%3,%4)")
                        .arg(bbox.x, 0, 'f', 1)
                        .arg(bbox.y, 0, 'f', 1)
                        .arg(bbox.x + bbox.width, 0, 'f', 1)
                        .arg(bbox.y + bbox.height, 0, 'f', 1));
                }

                // Draw with color based on source ID
                QColor color = partColors[sourceId % 10];
                drawPolygon(transformed, color, 0.5);

                // Add PLACEMENT INDEX label on the part (if enabled)
                if (showShapeIds_) {
                    auto bbox = deepnest::GeometryUtil::getPolygonBounds(transformed.points);
                    double centerX = bbox.x + bbox.width / 2.0;
                    double centerY = bbox.y + bbox.height / 2.0;

                    QGraphicsTextItem* idLabel = scene_->addText(
                        QString::number(placementCount - 1),  // Show placement index (0-based)
                        QFont("Arial", 6));
                    idLabel->setDefaultTextColor(Qt::black);
                    idLabel->setPos(centerX - 5, centerY - 5);
                }
            }
        }

        // Move to next sheet position
        if (sheetIdx < sheets_.size()) {
            auto bbox = deepnest::GeometryUtil::getPolygonBounds(sheets_[sheetIdx].points);
            sheetOffsetX += bbox.width + 50;  // 50 units spacing between sheets
        }
    }

    // Fit view to show all sheets with margin
    if (!sheets_.empty()) {
        // Calculate bounds of first sheet with 10% margin
        auto bbox = deepnest::GeometryUtil::getPolygonBounds(sheets_[0].points);
        double margin = std::max(bbox.width, bbox.height) * 0.1;  // 10% margin
        QRectF sheetRect(
            bbox.x - margin,
            bbox.y - margin,
            bbox.width + 2 * margin,
            bbox.height + 2 * margin
        );
        view_->fitInView(sheetRect, Qt::KeepAspectRatio);
    } else {
        view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
    }
}

void TestApplication::drawPolygon(const deepnest::Polygon& polygon, const QColor& color, double fillOpacity, bool showBorder) {
    QPainterPath path = polygon.toQPainterPath();

    QColor fillColor = color;
    fillColor.setAlphaF(fillOpacity);

    // Draw with or without border based on parameter
    if (showBorder) {
        scene_->addPath(path, QPen(color, 2), QBrush(fillColor));
    } else {
        scene_->addPath(path, QPen(Qt::NoPen), QBrush(fillColor));
    }

    // Draw holes if any
    for (const auto& hole : polygon.children) {
        QPainterPath holePath = hole.toQPainterPath();
        scene_->addPath(holePath, QPen(Qt::red, 1), QBrush(Qt::white));
    }
}

void TestApplication::clearScene() {
    scene_->clear();
}

void TestApplication::updateStatus() {
    generationLabel_->setText(QString("Generation: %1").arg(currentGeneration_));

    if (bestFitness_ < std::numeric_limits<double>::max()) {
        fitnessLabel_->setText(QString("Fitness: %1").arg(bestFitness_, 0, 'f', 2));
    } else {
        fitnessLabel_->setText("Fitness: N/A");
    }

    progressBar_->setValue(0);
}



void TestApplication::log(const QString& message) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    logWidget_->append(QString("[%1] %2").arg(timestamp).arg(message));
}

// ========== Automatic Testing Methods ==========

bool TestApplication::loadSVGFromPath(const QString& filepath) {
    if (filepath.isEmpty()) {
        std::cerr << "ERROR: Empty filepath provided" << std::endl;
        return false;
    }

    QFileInfo fileInfo(filepath);
    if (!fileInfo.exists()) {
        std::cerr << "ERROR: File does not exist: " << filepath.toStdString() << std::endl;
        return false;
    }

    reset();
    std::cout << "Loading SVG: " << filepath.toStdString() << std::endl;

    // Load SVG file
    SVGLoader::LoadResult result = SVGLoader::loadFile(filepath);

    if (!result.success()) {
        std::cerr << "ERROR: Failed to load SVG: " << result.errorMessage.toStdString() << std::endl;
        return false;
    }

    // Add container/sheet if found
    int sheetCount = 0;
    if (result.container) {
        QPainterPath containerPath = result.container->path;
        if (!result.container->transform.isIdentity()) {
            containerPath = result.container->transform.map(containerPath);
        }

        deepnest::Polygon sheetPoly = deepnest::QtBoostConverter::fromQPainterPath(containerPath, 0);
        sheets_.push_back(sheetPoly);
        solver_->addSheet(sheetPoly, config_.quantityPerSheet, result.container->id.isEmpty() ? "Sheet" : result.container->id.toStdString());
        sheetCount++;

        std::cout << "  Added sheet: " << (result.container->id.isEmpty() ? "Sheet" : result.container->id.toStdString()) << std::endl;
    }

    // Add all shapes as parts
    // Note: SVGLoader has already split multi-subpath elements and identified holes
    // Each shape here is already a complete polygon (potentially with holes)
    int partCount = 0;
    std::cout << "  Processing " << result.shapes.size() << " SVG shape(s)..." << std::endl;
    
    for (const auto& shape : result.shapes) {
        QPainterPath shapePath = shape.path;
        if (!shape.transform.isIdentity()) {
            shapePath = shape.transform.map(shapePath);
        }

        // Convert to Polygon - this preserves holes that are already in the QPainterPath
        deepnest::Polygon partPoly = deepnest::QtBoostConverter::fromQPainterPath(shapePath, partCount);
        
        if (partPoly.isValid()) {
            QString partName = shape.id.isEmpty() ? QString("Part_%1").arg(partCount) : shape.id;
            parts_.push_back(partPoly);
            solver_->addPart(partPoly, 1, partName.toStdString());
            
            std::cout << "    Added part: " << partName.toStdString() 
                      << " (" << partPoly.points.size() << " points";
            if (!partPoly.children.empty()) {
                std::cout << ", " << partPoly.children.size() << " hole(s)";
            }
            std::cout << ")" << std::endl;
            
            partCount++;
        } else {
            std::cout << "    Skipped invalid polygon from shape '" 
                      << (shape.id.isEmpty() ? "unnamed" : shape.id.toStdString()) << "'" << std::endl;
        }
    }

    std::cout << "  Loaded " << partCount << " parts and " << sheetCount << " sheet(s)" << std::endl;

    if (partCount == 0) {
        std::cerr << "ERROR: No valid parts found in SVG file" << std::endl;
        return false;
    }

    if (sheetCount == 0) {
        std::cerr << "WARNING: No sheet/container found in SVG" << std::endl;
    }

    return true;
}

double TestApplication::runAutomaticTest(int generations) {
    if (running_) {
        std::cerr << "ERROR: Nesting is already running" << std::endl;
        return bestFitness_;
    }

    // Validate we have parts and sheets
    if (solver_->getPartCount() == 0) {
        std::cerr << "ERROR: No parts loaded. Cannot start nesting." << std::endl;
        return std::numeric_limits<double>::max();
    }

    if (solver_->getSheetCount() == 0) {
        std::cerr << "ERROR: No sheets loaded. Cannot start nesting." << std::endl;
        return std::numeric_limits<double>::max();
    }

    // For automatic testing, use single thread to avoid threading issues during debugging
    config_.threads = 1;

    std::cout << "\n========================================" << std::endl;
    std::cout << "  AUTOMATIC NESTING TEST" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Generations: " << generations << std::endl;
    std::cout << "Parts: " << solver_->getPartCount() << std::endl;
    std::cout << "Sheets: " << solver_->getSheetCount() << std::endl;
    std::cout << "Population size: " << config_.populationSize << std::endl;
    std::cout << "Threads: " << config_.threads << " (forced to 1 for debugging)" << std::endl;
    std::cout << "Spacing: " << config_.spacing << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    running_ = true;
    currentGeneration_ = 0;
    bestFitness_ = std::numeric_limits<double>::max();

    // Configure solver with current settings
    solver_->setPopulationSize(config_.populationSize);
    solver_->setMutationRate(config_.mutationRate);
    solver_->setThreads(config_.threads);

    // Start nesting
    solver_->start(0);  // 0 = unlimited generations, we'll control manually

    // Run for specified generations
    int lastProgressGen = -1;
    auto startTime = std::chrono::high_resolution_clock::now();

    while (currentGeneration_ < generations && running_ && solver_->isRunning()) {
        // Step the solver
        solver_->step();

        // Get current state
        const deepnest::NestResult* result = solver_->getBestResult();
        if (result != nullptr) {
            bestFitness_ = result->fitness;
            currentGeneration_ = result->generation;

            // Print progress every 10 generations
            if (currentGeneration_ - lastProgressGen >= 10) {
                auto currentTime = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
                double gensPerSec = (elapsed > 0) ? (currentGeneration_ * 1000.0 / elapsed) : 0.0;

                std::cout << "Gen " << currentGeneration_ << "/" << generations
                          << " | Fitness: " << bestFitness_
                          << " | Speed: " << std::fixed << std::setprecision(2) << gensPerSec << " gen/s"
                          << std::endl;
                lastProgressGen = currentGeneration_;
            }
        }

        // Process Qt events to handle signals
        QCoreApplication::processEvents();
    }

    // Stop nesting
    solver_->stop();
    running_ = false;

    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    std::cout << "\n=== Test Complete ===" << std::endl;
    std::cout << "Total time: " << totalTime << "ms" << std::endl;
    std::cout << "Final generation: " << currentGeneration_ << std::endl;
    std::cout << "Best fitness: " << bestFitness_ << std::endl;

    return bestFitness_;
}

void TestApplication::generatePolygons() {
    bool ok;
    int count = QInputDialog::getInt(this, "Test Polygon",
                                     "Number of polygons to generate:", 10, 1, 100, 1, &ok);
    if (!ok) {
        return;
    }

    reset();  // Clear existing data like testRandomRectangles does

    log(QString("Generating %1 random polygons...").arg(count));

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> sizeDist(20.0, 100.0);
    std::uniform_int_distribution<> sidesDist(3, 8);

    // Generate polygons
    for (int i = 0; i < count; i++) {
        int sides = sidesDist(gen);
        double size = sizeDist(gen);

        std::vector<deepnest::Point> points;
        for (int j = 0; j < sides; j++) {
            double angle = (2.0 * M_PI * j) / sides;
            double radius = size * (0.7 + 0.3 * std::uniform_real_distribution<>(0.0, 1.0)(gen));
            points.push_back(deepnest::Point(
                radius * cos(angle),
                radius * sin(angle)
            ));
        }

        deepnest::Polygon poly(points, i);
        parts_.push_back(poly);
        solver_->addPart(poly, 1, QString("GenPoly_%1").arg(i).toStdString());
    }

    // Create sheet (500x400 like testRandomRectangles)
    std::vector<deepnest::Point> sheetPoints = {
        {0, 0},
        {500, 0},
        {500, 400},
        {0, 400}
    };
    deepnest::Polygon sheet(sheetPoints, -1);
    sheets_.push_back(sheet);
    solver_->addSheet(sheet, config_.quantityPerSheet, "Sheet");

    log(QString("Generated %1 polygons and 500x400 sheet").arg(count));

    // Visualize with grid layout (container left, parts right)
    clearScene();

    // Draw sheet on the left
    drawPolygon(sheet, Qt::black, 0.0, true);

    // Add sheet label
    QGraphicsTextItem* sheetLabel = scene_->addText("SHEET 500x400", QFont("Arial", 12));
    sheetLabel->setDefaultTextColor(Qt::black);
    sheetLabel->setPos(10, -30);

    // Draw parts in grid on the right
    double xOffset = 550;  // Start after sheet
    double yOffset = 50;
    double maxHeight = 0;

    for (size_t i = 0; i < parts_.size(); ++i) {
        auto bbox = deepnest::GeometryUtil::getPolygonBounds(parts_[i].points);
        double width = bbox.width;
        double height = bbox.height;

        // Wrap to next row if needed
        if (xOffset + width > 1400) {
            xOffset = 550;
            yOffset += maxHeight + 20;
            maxHeight = 0;
        }

        // Translate part to grid position
        deepnest::Polygon translated = parts_[i].translate(xOffset - bbox.x, yOffset - bbox.y);
        drawPolygon(translated, QColor(100, 150, 200), 0.3);

        // Add ID label if enabled
        if (showShapeIds_) {
            QGraphicsTextItem* idLabel = scene_->addText(QString::number(i), QFont("Arial", 6));
            idLabel->setDefaultTextColor(Qt::darkBlue);
            idLabel->setPos(xOffset + width/2 - 5, yOffset + height/2 - 5);
        }

        xOffset += width + 20;
        maxHeight = std::max(maxHeight, height);
    }

    view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
    log("Ready to start nesting - click 'Start' button");
}

void TestApplication::setContainerSize() {
    ContainerDialog dialog(this);

    // Pre-fill with current container size if exists
    if (!sheets_.empty()) {
        const auto& container = sheets_[0];
        if (!container.points.empty()) {
            // Calculate bounds
            deepnest::CoordType minX = container.points[0].x, maxX = container.points[0].x;
            deepnest::CoordType minY = container.points[0].y, maxY = container.points[0].y;

            for (const auto& p : container.points) {
                minX = std::min(minX, p.x);
                maxX = std::max(maxX, p.x);
                minY = std::min(minY, p.y);
                maxY = std::max(maxY, p.y);
            }

            dialog.setWidth(static_cast<double>(maxX - minX));
            dialog.setHeight(static_cast<double>(maxY - minY));
        }
    }

    if (dialog.exec() == QDialog::Accepted) {
        double width = dialog.getWidth();
        double height = dialog.getHeight();

        // Create new container
        std::vector<deepnest::Point> points = {
            {0, 0},
            {width, 0},
            {width, height},
            {0, height}
        };

        deepnest::Polygon container(points, -1);

        if (sheets_.empty()) {
            sheets_.push_back(container);
            solver_->addSheet(container, 1, "Container");  // Add to solver
            log(QString("Container created: %1 x %2 mm").arg(width).arg(height));
        } else {
            sheets_[0] = container;
            // Note: Cannot update existing sheet in solver, would need reset
            log(QString("Container updated: %1 x %2 mm (restart nesting to apply)").arg(width).arg(height));
        }

        // Update visualization
        clearScene();
        for (const auto& part : parts_) {
            drawPolygon(part, Qt::blue, 0.3);
        }
        drawPolygon(sheets_[0], Qt::black, 0.0, true);  // Show border
        view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
    }
}

void TestApplication::viewContainerInfo() {
    if (sheets_.empty()) {
        QMessageBox::information(this, "Container Info",
                                "No container defined.\n\n"
                                "Use 'Container > Set Container Size' to create one.");
        return;
    }

    const auto& container = sheets_[0];
    if (container.points.empty()) {
        QMessageBox::warning(this, "Container Info", "Container has no points!");
        return;
    }

    // Calculate bounds
    deepnest::CoordType minX = container.points[0].x, maxX = container.points[0].x;
    deepnest::CoordType minY = container.points[0].y, maxY = container.points[0].y;

    for (const auto& p : container.points) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }

    deepnest::CoordType width = maxX - minX;
    deepnest::CoordType height = maxY - minY;
    double area = static_cast<double>(width) * static_cast<double>(height);

    QString info = QString(
        "Container Information:\n\n"
        "Width:  %1 mm\n"
        "Height: %2 mm\n"
        "Area:   %3 mm²\n\n"
        "Bounds:\n"
        "  X: [%4, %5]\n"
        "  Y: [%6, %7]\n\n"
        "Points: %8"
    ).arg(width, 0, 'f', 2)
     .arg(height, 0, 'f', 2)
     .arg(area, 0, 'f', 2)
     .arg(minX, 0, 'f', 2)
     .arg(maxX, 0, 'f', 2)
     .arg(minY, 0, 'f', 2)
     .arg(maxY, 0, 'f', 2)
     .arg(container.points.size());

    QMessageBox::information(this, "Container Info", info);
}

void TestApplication::importSVG() {
    QString filepath = QFileDialog::getOpenFileName(
        this,
        "Import SVG File (Append Mode)",
        "",
        "SVG Files (*.svg);;All Files (*)"
    );

    if (filepath.isEmpty()) {
        return;
    }

    log(QString("Importing SVG (append mode): %1").arg(filepath));

    // Save current parts count
    size_t previousCount = parts_.size();

    try {
        // Load SVG WITHOUT clearing existing parts
        SVGLoader::LoadResult result = SVGLoader::loadFile(filepath);

        if (!result.success()) {
            log("Error importing SVG: " + result.errorMessage);
            QMessageBox::critical(this, "Import Error",
                                "Failed to import SVG file:\n" + result.errorMessage);
            return;
        }

        // Append loaded shapes as parts
        for (const auto& shape : result.shapes) {
            QPainterPath shapePath = shape.path;
            if (!shape.transform.isIdentity()) {
                shapePath = shape.transform.map(shapePath);
            }

            deepnest::Polygon partPoly = deepnest::QtBoostConverter::fromQPainterPath(
                shapePath, static_cast<int>(parts_.size())
            );

            if (partPoly.isValid()) {
                parts_.push_back(partPoly);
            }
        }

        // Handle container
        if (result.container) {
            if (sheets_.empty()) {
                // If we don't have a container, use the loaded one
                QPainterPath containerPath = result.container->path;
                if (!result.container->transform.isIdentity()) {
                    containerPath = result.container->transform.map(containerPath);
                }

                deepnest::Polygon containerPoly = deepnest::QtBoostConverter::fromQPainterPath(
                    containerPath, -1
                );
                sheets_.push_back(containerPoly);
                log("Container imported from SVG");
            } else {
                // Ask user what to do
                QMessageBox::StandardButton reply = QMessageBox::question(
                    this,
                    "Container Conflict",
                    "Imported SVG has a container. Replace existing container?",
                    QMessageBox::Yes | QMessageBox::No
                );

                if (reply == QMessageBox::Yes) {
                    QPainterPath containerPath = result.container->path;
                    if (!result.container->transform.isIdentity()) {
                        containerPath = result.container->transform.map(containerPath);
                    }

                    deepnest::Polygon containerPoly = deepnest::QtBoostConverter::fromQPainterPath(
                        containerPath, -1
                    );
                    sheets_[0] = containerPoly;
                    log("Container replaced with imported one");
                } else {
                    log("Kept existing container");
                }
            }
        }

        size_t importedCount = parts_.size() - previousCount;
        log(QString("Imported %1 parts (total: %2)")
            .arg(static_cast<int>(importedCount))
            .arg(static_cast<int>(parts_.size())));

        // Update visualization
        clearScene();
        for (const auto& part : parts_) {
            drawPolygon(part, Qt::blue, 0.3);
        }
        if (!sheets_.empty()) {
            drawPolygon(sheets_[0], Qt::black, 0.0);
        }
        view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);

    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Import Error",
                            QString("Failed to import SVG:\n%1").arg(e.what()));
        log(QString("Import failed: %1").arg(e.what()));
    }
}

void TestApplication::toggleShapeIds() {
    showShapeIds_ = !showShapeIds_;
    log(QString("Shape IDs: %1").arg(showShapeIds_ ? "ON" : "OFF"));
    
    // Refresh visualization if we have a result
    std::lock_guard<std::mutex> lock(resultMutex_);
    if (lastResult_) {
        updateVisualization(*lastResult_);
    }
}

void TestApplication::prevSheet() {
    if (currentSheetIndex_ > 0) {
        currentSheetIndex_--;
        std::lock_guard<std::mutex> lock(resultMutex_);
        if (lastResult_) {
            updateVisualization(*lastResult_);
        }
    }
}

void TestApplication::nextSheet() {
    std::lock_guard<std::mutex> lock(resultMutex_);
    if (lastResult_ && currentSheetIndex_ < static_cast<int>(lastResult_->placements.size()) - 1) {
        currentSheetIndex_++;
        updateVisualization(*lastResult_);
    }
}

bool TestApplication::testPolygons(int count) {
    reset();

    std::cout << "Generating " << count << " random polygon types..." << std::endl;

    std::random_device rd;
    std::mt19937 gen(rd());

    // Use reasonable defaults for polygon generation
    double minRadius = 5.0;
    double maxRadius = 20.0;
    int minSides = 3;
    int maxSides = 8;
    int quantityPerType = 3;  // 3 copies of each polygon type

    std::uniform_real_distribution<> sizeDist(minRadius, maxRadius);
    std::uniform_int_distribution<> sidesDist(minSides, maxSides);

    // Create random polygons
    for (int i = 0; i < count; ++i) {
        int sides = sidesDist(gen);
        double radius = sizeDist(gen);

        std::vector<deepnest::Point> points;
        for (int j = 0; j < sides; ++j) {
            double angle = 2.0 * M_PI * j / sides;
            double x = radius * std::cos(angle);
            double y = radius * std::sin(angle);
            points.push_back(deepnest::Point(x + radius, y + radius));
        }

        deepnest::Polygon poly(points, i);  // Assign id
        parts_.push_back(poly);  // Save for visualization
        solver_->addPart(poly, quantityPerType, QString("Poly_%1").arg(i).toStdString());

        std::cout << "  Generated polygon " << (i+1) << "/" << count
                  << ": " << sides << " sides, radius " << radius << std::endl;
    }

    // Create sheet that should fit all polygons
    // Calculate total area needed and add 50% margin
    double totalPartsArea = 0.0;
    for (const auto& part : parts_) {
        totalPartsArea += std::abs(part.area()) * quantityPerType;
    }

    // Square sheet with 50% margin
    double sheetDimension = std::sqrt(totalPartsArea * 1.5);

    std::cout << "  Total parts area: " << totalPartsArea << std::endl;
    std::cout << "  Creating sheet: " << sheetDimension << " x " << sheetDimension << std::endl;

    // Create sheet - counter-clockwise
    std::vector<deepnest::Point> sheetPoints;
    sheetPoints.push_back(deepnest::Point(0, 0));
    sheetPoints.push_back(deepnest::Point(0, sheetDimension));
    sheetPoints.push_back(deepnest::Point(sheetDimension, sheetDimension));
    sheetPoints.push_back(deepnest::Point(sheetDimension, 0));

    deepnest::Polygon sheet(sheetPoints, 0);

    // Validate sheet
    if (!sheet.isValid()) {
        std::cerr << "ERROR: Sheet is invalid!" << std::endl;
        return false;
    }
    if (!sheet.isCounterClockwise()) {
        sheet.reverse();
    }

    sheets_.push_back(sheet);
    solver_->addSheet(sheet, 1, "Sheet");  // Only 1 sheet - all parts should fit!

    std::cout << "✓ Generated " << count << " polygon types ("
              << (count * quantityPerType) << " total parts)" << std::endl;

    return true;
}

int TestApplication::getSheetsUsed() const {
    std::lock_guard<std::mutex> lock(resultMutex_);
    if (!lastResult_) {
        return 0;
    }

    // Count sheets that have at least one part placed
    // lastResult_->placements is vector<vector<Placement>>
    int usedSheets = 0;
    for (const auto& sheetPlacements : lastResult_->placements) {
        if (!sheetPlacements.empty()) {
            usedSheets++;
        }
    }

    return usedSheets;
}

