#include "TestApplication.h"
#include "SVGLoader.h"
#include "../include/deepnest/converters/QtBoostConverter.h"

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
#include <random>

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
    , lastResult_(nullptr)
    , maxGenerations_(100)
    , stepTimerInterval_(50)  // 50ms between steps
{
    setWindowTitle("DeepNest C++ Test Application");
    resize(1400, 900);

    // Register custom types for Qt signal/slot system
    qRegisterMetaType<deepnest::NestResult>("deepnest::NestResult");
    qRegisterMetaType<deepnest::NestProgress>("deepnest::NestProgress");

    // Initialize UI
    initUI();

    // Create solver
    solver_ = std::make_unique<deepnest::DeepNestSolver>();

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
}

TestApplication::~TestApplication() {
    if (running_) {
        solver_->stop();
    }
    delete lastResult_;
}

void TestApplication::initUI() {
    // Create central widget with graphics view
    view_ = new QGraphicsView(this);
    scene_ = new QGraphicsScene(this);
    view_->setScene(scene_);
    view_->setRenderHint(QPainter::Antialiasing);
    view_->setDragMode(QGraphicsView::ScrollHandDrag);
    setCentralWidget(view_);

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
}

void TestApplication::createToolbar() {
    QToolBar* toolbar = addToolBar("Main");
    toolbar->setMovable(false);

    QPushButton* loadBtn = new QPushButton("Load SVG", toolbar);
    connect(loadBtn, &QPushButton::clicked, this, &TestApplication::loadSVG);
    toolbar->addWidget(loadBtn);

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
        solver_->addSheet(sheetPoly, 1, result.container->id.isEmpty() ? "Sheet" : result.container->id.toStdString());
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
        QMessageBox::warning(this, "No Sheet",
                           "No sheet/container found in SVG.\n"
                           "Please add a sheet manually or mark a shape as 'container' in the SVG.");
    }

    // Visualize loaded shapes
    clearScene();
    for (const auto& shape : result.shapes) {
        QPainterPath shapePath = shape.path;
        if (!shape.transform.isIdentity()) {
            shapePath = shape.transform.map(shapePath);
        }
        scene_->addPath(shapePath, QPen(Qt::blue, 2), QBrush(QColor(100, 100, 255, 80)));
    }

    if (result.container) {
        QPainterPath containerPath = result.container->path;
        if (!result.container->transform.isIdentity()) {
            containerPath = result.container->transform.map(containerPath);
        }
        scene_->addPath(containerPath, QPen(Qt::green, 3), QBrush(QColor(100, 255, 100, 40)));
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
        return;
    }

    log("Stopping nesting...");
    stepTimer_->stop();
    solver_->stop();
    running_ = false;
    statusLabel_->setText("Stopped");
    log("Nesting stopped");
}

void TestApplication::reset() {
    if (running_) {
        stopNesting();
    }

    solver_->clear();
    parts_.clear();
    sheets_.clear();
    clearScene();
    currentGeneration_ = 0;
    bestFitness_ = std::numeric_limits<double>::max();
    delete lastResult_;
    lastResult_ = nullptr;

    updateStatus();
    log("Reset complete");
}

void TestApplication::testRandomRectangles() {
    reset();

    log("Generating random rectangles test...");

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> widthDist(50.0, 150.0);
    std::uniform_real_distribution<> heightDist(30.0, 100.0);

    // Create 10 random rectangles
    for (int i = 0; i < 10; ++i) {
        double w = widthDist(gen);
        double h = heightDist(gen);

        std::vector<deepnest::Point> points;
        points.push_back(deepnest::Point(0, 0));
        points.push_back(deepnest::Point(w, 0));
        points.push_back(deepnest::Point(w, h));
        points.push_back(deepnest::Point(0, h));

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
        solver_->addPart(rect, 2, QString("Rect_%1").arg(i).toStdString());
    }

    // Create sheet (500x400)
    std::vector<deepnest::Point> sheetPoints;
    sheetPoints.push_back(deepnest::Point(0, 0));
    sheetPoints.push_back(deepnest::Point(500, 0));
    sheetPoints.push_back(deepnest::Point(500, 400));
    sheetPoints.push_back(deepnest::Point(0, 400));

    deepnest::Polygon sheet(sheetPoints, 0);  // Assign id

    // Validate sheet
    if (!sheet.isValid()) {
        log("ERROR: Sheet is invalid!");
    }
    if (!sheet.isCounterClockwise()) {
        log("WARNING: Sheet is clockwise, reversing");
        sheet.reverse();
    }

    log(QString("Created sheet: 500x400, area=%1, CCW=%2")
        .arg(std::abs(sheet.area()), 0, 'f', 1)
        .arg(sheet.isCounterClockwise() ? "yes" : "no"));

    sheets_.push_back(sheet);  // Save for visualization
    solver_->addSheet(sheet, 3, "Sheet_500x400");

    log(QString("Created %1 part types (20 total parts) and 1 sheet type (3 sheets)")
        .arg(solver_->getPartCount()));

    // Visualize the parts on scene
    clearScene();
    double xOffset = 50;
    for (size_t i = 0; i < parts_.size(); ++i) {
        drawPolygon(parts_[i], QColor(100, 150, 200), 0.3);
        xOffset += 150;  // Space between parts
    }

    log("Ready to start nesting - click 'Start' button");
}

void TestApplication::testRandomPolygons() {
    reset();

    log("Generating random polygons test...");

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> sizeDist(30.0, 80.0);
    std::uniform_int_distribution<> sidesDist(5, 8);

    // Create random polygons
    for (int i = 0; i < 8; ++i) {
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
        solver_->addPart(poly, 2, QString("Poly_%1").arg(i).toStdString());
    }

    // Create sheet
    std::vector<deepnest::Point> sheetPoints;
    sheetPoints.push_back(deepnest::Point(0, 0));
    sheetPoints.push_back(deepnest::Point(600, 0));
    sheetPoints.push_back(deepnest::Point(600, 400));
    sheetPoints.push_back(deepnest::Point(0, 400));

    deepnest::Polygon sheet(sheetPoints, 0);  // Assign id
    sheets_.push_back(sheet);  // Save for visualization
    solver_->addSheet(sheet, 2, "Sheet_600x400");

    log(QString("Created %1 part types (16 total parts) and 1 sheet type (2 sheets)")
        .arg(solver_->getPartCount()));
    log("Ready to start nesting - click 'Start' button");
}

void TestApplication::openConfigDialog() {
    // For now, use simple dialogs for each parameter
    // A full QDialog-based config widget would be better

    bool ok;
    double spacing = QInputDialog::getDouble(this, "Spacing",
                                             "Enter spacing between parts:",
                                             solver_->getConfig().spacing,
                                             0, 100, 1, &ok);
    if (ok) solver_->setSpacing(spacing);

    int rotations = QInputDialog::getInt(this, "Rotations",
                                        "Enter number of rotations (0=none, 4=90° increments):",
                                        solver_->getConfig().rotations,
                                        0, 16, 1, &ok);
    if (ok) solver_->setRotations(rotations);

    int popSize = QInputDialog::getInt(this, "Population Size",
                                      "Enter GA population size:",
                                      solver_->getConfig().populationSize,
                                      2, 100, 1, &ok);
    if (ok) solver_->setPopulationSize(popSize);

    log("Configuration updated");
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
    log(QString("New best result! Fitness: %1 at generation %2")
        .arg(result.fitness, 0, 'f', 2)
        .arg(result.generation));

    // Store copy of result
    delete lastResult_;
    lastResult_ = new deepnest::NestResult(result);

    // Update visualization
    updateVisualization(result);
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
    if (!lastResult_) {
        QMessageBox::warning(this, "No Result", "No nesting result available to export");
        return;
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
    // Count total placements to see if we have anything to show
    int totalPlacements = 0;
    for (const auto& sheetPlacements : result.placements) {
        totalPlacements += sheetPlacements.size();
    }

    // Only update visualization if we have placements to show
    if (totalPlacements == 0) {
        log(QString("Result has no placements yet, keeping current view"));
        return;
    }

    log(QString("Visualizing %1 placed parts on %2 sheets")
        .arg(totalPlacements)
        .arg(result.placements.size()));

    clearScene();

    // Draw each sheet with placements
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> colorDist(0, 255);

    for (size_t sheetIdx = 0; sheetIdx < result.placements.size(); ++sheetIdx) {
        const auto& sheetPlacements = result.placements[sheetIdx];

        // Draw each placed part with random color
        for (const auto& placement : sheetPlacements) {
            // Use source ID to find the original polygon type
            // source contains the ID of the original part before quantity expansion
            int sourceId = (placement.source >= 0) ? placement.source : placement.id;

            if (sourceId >= 0 && sourceId < static_cast<int>(parts_.size())) {
                deepnest::Polygon part = parts_[sourceId];

                // Apply rotation and translation
                deepnest::Polygon transformed = part.rotate(placement.rotation);
                transformed = transformed.translate(placement.position.x, placement.position.y);

                QColor color(colorDist(gen), colorDist(gen), colorDist(gen));
                drawPolygon(transformed, color, 0.5);
            }
        }

        // Add offset for next sheet
        // (In a real implementation, we'd position sheets properly)
    }

    // Fit view to scene
    view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
}

void TestApplication::drawPolygon(const deepnest::Polygon& polygon, const QColor& color, double fillOpacity) {
    QPainterPath path = polygon.toQPainterPath();

    QColor fillColor = color;
    fillColor.setAlphaF(fillOpacity);

    scene_->addPath(path, QPen(color, 2), QBrush(fillColor));

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
