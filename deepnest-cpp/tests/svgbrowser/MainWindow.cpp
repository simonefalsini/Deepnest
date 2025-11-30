#include "MainWindow.h"
#include "../../SVGLoader.h"
#include "../../include/deepnest/nfp/MinkowskiSum.h"
#include "../../include/deepnest/nfp/Libnest2D_NFP.h"
#include "../../include/deepnest/geometry/GeometryUtil.h"
#include "../../include/deepnest/geometry/PolygonOperations.h"
#include "../../include/deepnest/core/Polygon.h"
#include <QSplitter>
#include <QToolBar>
#include <QMenuBar>
#include <QFileDialog>
#include <QDir>
#include <QGraphicsPathItem>
#include <QRandomGenerator>
#include <QVBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QMessageBox>
#include <QDebug>
#include <QDebug>

using namespace deepnest;

// Helper to convert QGraphicsItem to deepnest::Polygon
Polygon itemToPolygon(QGraphicsItem* item) {
    QGraphicsPathItem* pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item);
    if (!pathItem) return Polygon();
    
    // Get path in scene coordinates
    QPainterPath path = pathItem->path();
    path = pathItem->sceneTransform().map(path);
    
    // Convert to deepnest::Polygon
    // Note: SVGLoader already handles complex paths by breaking them down or using PolygonHierarchy
    // Here we assume simple polygons for NFP testing or take the largest subpath
    return Polygon::fromQPainterPath(path);
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
    resize(1280, 800);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi()
{
    // Central widget is a splitter
    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(mainSplitter);

    // Left side: File list
    fileList = new QListWidget(this);
    fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mainSplitter->addWidget(fileList);

    // Center: Main Graphics view
    scene = new QGraphicsScene(this);
    connect(scene, &QGraphicsScene::selectionChanged, this, &MainWindow::onSceneSelectionChanged);
    connect(scene, &QGraphicsScene::selectionChanged, this, &MainWindow::onSelectionChanged);
    connect(scene, &QGraphicsScene::selectionChanged, this, &MainWindow::updateVertexBList);
    
    view = new ZoomableGraphicsView(scene, this);
    view->setBackgroundBrush(Qt::lightGray);
    mainSplitter->addWidget(view);

    // Right side: NFP Panel
    QWidget* nfpPanel = new QWidget(this);
    QVBoxLayout* nfpLayout = new QVBoxLayout(nfpPanel);
    
    // Controls Group
    QGroupBox* controlsGroup = new QGroupBox("NFP Controls", this);
    QVBoxLayout* controlsLayout = new QVBoxLayout(controlsGroup);
    
    implementationCombo = new QComboBox(this);
    implementationCombo->addItem("scale::calculateNFP");
    implementationCombo->addItem("trunk::calculateNFP");
    implementationCombo->addItem("newmnk::calculateNFP");
    implementationCombo->addItem("GeometryUtil::noFitPolygon");
    implementationCombo->addItem("GeometryUtil::noFitPolygonRectangle");
    implementationCombo->addItem("libnest2d::nfpConvexOnly");
    implementationCombo->addItem("libnest2d::nfpSimpleSimple");
    implementationCombo->addItem("libnest2d::svgnest::noFitPolygon");
    implementationCombo->addItem("libnest2d::libnfporb::generateNFP");
    controlsLayout->addWidget(new QLabel("Implementation:"));
    controlsLayout->addWidget(implementationCombo);
    
    unionCheckBox = new QCheckBox("Union Result", this);
    controlsLayout->addWidget(unionCheckBox);
    
    // Polygon A Controls
    QGroupBox* groupA = new QGroupBox("Polygon A", this);
    QVBoxLayout* layoutA = new QVBoxLayout(groupA);
    
    QHBoxLayout* rotALayout = new QHBoxLayout();
    rotationACombo = new QComboBox(this);
    rotationACombo->addItems({"0", "90", "180", "270", "Custom"});
    rotationASpin = new QDoubleSpinBox(this);
    rotationASpin->setRange(0, 360);
    rotationASpin->setEnabled(false);
    rotALayout->addWidget(rotationACombo);
    rotALayout->addWidget(rotationASpin);
    layoutA->addLayout(rotALayout);
    controlsLayout->addWidget(groupA);

    // Polygon B Controls
    QGroupBox* groupB = new QGroupBox("Polygon B", this);
    QVBoxLayout* layoutB = new QVBoxLayout(groupB);
    
    QHBoxLayout* rotBLayout = new QHBoxLayout();
    rotationBCombo = new QComboBox(this);
    rotationBCombo->addItems({"0", "90", "180", "270", "Custom"});
    rotationBSpin = new QDoubleSpinBox(this);
    rotationBSpin->setRange(0, 360);
    rotationBSpin->setEnabled(false);
    rotBLayout->addWidget(rotationBCombo);
    rotBLayout->addWidget(rotationBSpin);
    layoutB->addLayout(rotBLayout);
    controlsLayout->addWidget(groupB);
    
    // Connect rotation combos
    auto setupRotation = [this](QComboBox* combo, QDoubleSpinBox* spin) {
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), [combo, spin](int index) {
            if (combo->itemText(index) == "Custom") {
                spin->setEnabled(true);
            } else {
                spin->setEnabled(false);
                spin->setValue(combo->itemText(index).toDouble());
            }
        });
    };
    setupRotation(rotationACombo, rotationASpin);
    setupRotation(rotationBCombo, rotationBSpin);
    
    // Vertex B Selection
    vertexBCombo = new QComboBox(this);
    controlsLayout->addWidget(new QLabel("Reference Vertex (B):"));
    controlsLayout->addWidget(vertexBCombo);
    connect(vertexBCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::runNFP);

    // Import Button
    importButton = new QPushButton("Import Result", this);
    connect(importButton, &QPushButton::clicked, this, &MainWindow::onImportClicked);
    controlsLayout->addWidget(importButton);

    // Run Button
    QPushButton* runButton = new QPushButton("Run NFP", this);
    connect(runButton, &QPushButton::clicked, this, &MainWindow::runNFP);
    controlsLayout->addWidget(runButton);
    
    nfpLayout->addWidget(controlsGroup);
    
    // Result View
    resultScene = new QGraphicsScene(this);
    resultView = new ZoomableGraphicsView(resultScene, this);
    resultView->setBackgroundBrush(Qt::white);
    nfpLayout->addWidget(new QLabel("Result:"));
    nfpLayout->addWidget(resultView);
    
    mainSplitter->addWidget(nfpPanel);

    // Set initial sizes (15% left, 55% center, 30% right)
    mainSplitter->setStretchFactor(0, 15);
    mainSplitter->setStretchFactor(1, 55);
    mainSplitter->setStretchFactor(2, 30);

    // Menu
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* openAction = fileMenu->addAction(tr("&Open Directory..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openDirectory);

    QAction* exitAction = fileMenu->addAction(tr("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    // Toolbar
    QToolBar* toolbar = addToolBar(tr("Main Toolbar"));
    toolbar->addAction(openAction);

    // Connections
    connect(fileList, &QListWidget::itemSelectionChanged, this, &MainWindow::onFileSelectionChanged);
}

void MainWindow::openDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
                                                  currentDir,
                                                  QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        currentDir = dir;
        fileList->clear();
        fileColors.clear();
        scene->clear();
        selectionOrder.clear();

        QDir directory(currentDir);
        QStringList filters;
        filters << "*.svg";
        directory.setNameFilters(filters);
        
        QStringList files = directory.entryList(QDir::Files);
        fileList->addItems(files);
    }
}

void MainWindow::onFileSelectionChanged()
{
    scene->clear();
    selectionOrder.clear();
    
    QList<QListWidgetItem*> selectedItems = fileList->selectedItems();
    
    for (QListWidgetItem* item : selectedItems) {
        QString filename = item->text();
        QString fullPath = QDir(currentDir).filePath(filename);
        
        // Load shapes from SVG
        std::vector<SVGLoader::Shape> shapes = SVGLoader::loadShapes(fullPath);
        
        QColor color = getFileColor(filename);
        // Semi-transparent fill
        color.setAlpha(128);
        
        QPen pen(Qt::black, 0); // Cosmetic pen
        QBrush brush(color);
        
        for (const auto& shape : shapes) {
            QGraphicsPathItem* pathItem = scene->addPath(shape.path);
            
            // Apply transform if present
            if (!shape.transform.isIdentity()) {
                pathItem->setTransform(shape.transform);
            }
            
            pathItem->setPen(pen);
            pathItem->setBrush(brush);
            pathItem->setFlag(QGraphicsItem::ItemIsMovable);
            pathItem->setFlag(QGraphicsItem::ItemIsSelectable);
            
            // Store filename in data for identification
            pathItem->setData(0, filename);
            
            // Add tooltip with info
            pathItem->setToolTip(QString("File: %1\nID: %2").arg(filename).arg(shape.id));
        }
    }
    
    view->fitToContent();
}

void MainWindow::onSceneSelectionChanged()
{
    QList<QGraphicsItem*> currentSelection = scene->selectedItems();
    
    // Remove items that are no longer selected
    for (int i = selectionOrder.size() - 1; i >= 0; --i) {
        if (!currentSelection.contains(selectionOrder[i])) {
            selectionOrder.removeAt(i);
        }
    }
    
    // Add new items
    for (QGraphicsItem* item : currentSelection) {
        if (!selectionOrder.contains(item)) {
            selectionOrder.append(item);
        }
    }
    
    // Visual feedback (optional)
    // We could highlight A and B differently here if desired
}

void MainWindow::onSelectionChanged() {
    QList<QGraphicsItem*> selected = scene->selectedItems();
    if (selected.isEmpty()) return;
    
    // Bring selected items to front
    for (auto item : selected) {
        currentMaxZ += 0.1;
        item->setZValue(currentMaxZ);
    }
}

void MainWindow::updateVertexBList() 
{
    if (vertexBCombo)
    {
        vertexBCombo->blockSignals(true);
        vertexBCombo->clear();
    
        if (selectionOrder.size() >= 2) {
            QGraphicsItem* itemB = selectionOrder[1];
            Polygon polyB = itemToPolygon(itemB);
        
            // Apply rotations if checked (to match what runNFP does)
            double rotB = rotationBSpin->value();
            if (rotB != 0.0) polyB = polyB.rotate(rotB);
        
            for (size_t i = 0; i < polyB.points.size(); ++i) {
                vertexBCombo->addItem(QString("Vertex %1 (%2, %3)").arg(i).arg(polyB.points[i].x).arg(polyB.points[i].y));
            }
        }
        vertexBCombo->blockSignals(false);
    }

}

void MainWindow::onUnionToggled(bool checked) {
    // Re-run NFP if we have a valid result to update the view, 
    // but for now just let the user click Run again.
    // Or we could trigger runNFP() if selection is valid.
    if (selectionOrder.size() >= 2) {
        runNFP();
    }
}

void MainWindow::onImportClicked() {
    if (resultScene->items().isEmpty()) return;
    
    for (auto item : resultScene->items()) {
        QGraphicsPathItem* pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item);
        if (pathItem && pathItem->data(0).toString() == "NFP") {
            QGraphicsPathItem* newItem = scene->addPath(pathItem->path());
            newItem->setPen(QPen(Qt::blue, 0));
            newItem->setBrush(QBrush(QColor(0, 0, 255, 100)));
            newItem->setFlag(QGraphicsItem::ItemIsMovable);
            newItem->setFlag(QGraphicsItem::ItemIsSelectable);
            newItem->setZValue(++currentMaxZ);
            newItem->setData(0, "Imported NFP");
        }
    }
}



void MainWindow::runNFP()
{
    if (selectionOrder.size() < 2) {
        QMessageBox::warning(this, "Selection Error", "Please select at least two items (A and B).");
        return;
    }
    
    QGraphicsItem* itemA = selectionOrder[0];
    QGraphicsItem* itemB = selectionOrder[1];
    
    Polygon polyA = itemToPolygon(itemA);
    Polygon polyB = itemToPolygon(itemB);
    
    // Apply rotations
    double rotA = rotationASpin->value();
    if (rotA != 0.0) {
        polyA = polyA.rotate(rotA);
    }
    
    double rotB = rotationBSpin->value();
    if (rotB != 0.0) {
        polyB = polyB.rotate(rotB);
    }
    
    // Apply Vertex B selection (rotate polygon so selected vertex is first)
    int selectedVertexIndex = vertexBCombo->currentIndex();
    if (selectedVertexIndex > 0 && selectedVertexIndex < (int)polyB.points.size()) {
        std::rotate(polyB.points.begin(), polyB.points.begin() + selectedVertexIndex, polyB.points.end());
    }
    
    std::vector<Polygon> nfpResult;
    QString impl = implementationCombo->currentText();
    
    try {
        if (impl == "scale::calculateNFP") {
            nfpResult = scale::MinkowskiSum::calculateNFP(polyA, polyB);
        } else if (impl == "trunk::calculateNFP") {
            nfpResult = trunk::MinkowskiSum::calculateNFP(polyA, polyB);
        } else if (impl == "newmnk::calculateNFP") {
            nfpResult = newmnk::calculateNFP(polyA, polyB);
        } else if (impl == "GeometryUtil::noFitPolygon") {
            auto result = GeometryUtil::noFitPolygon(polyA.points, polyB.points);
            for (const auto& pts : result) {
                nfpResult.push_back(Polygon(pts));
            }
        } else if (impl == "GeometryUtil::noFitPolygonRectangle") {
             auto result = GeometryUtil::noFitPolygonRectangle(polyA.points, polyB.points);
             for (const auto& pts : result) {
                nfpResult.push_back(Polygon(pts));
            }
        } else if (impl == "libnest2d::nfpConvexOnly") {
            nfpResult = libnest2d_port::nfpConvexOnly(polyA, polyB);
        } else if (impl == "libnest2d::nfpSimpleSimple") {
            nfpResult = libnest2d_port::nfpSimpleSimple(polyA, polyB);
        } else if (impl == "libnest2d::svgnest::noFitPolygon") {
            nfpResult = libnest2d_port::svgnest_noFitPolygon(polyA, polyB);
        } else if (impl == "libnest2d::libnfporb::generateNFP") {
            nfpResult = libnest2d_port::libnfporb_generateNFP(polyA, polyB);
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", QString("NFP Calculation failed: %1").arg(e.what()));
        return;
    }
    
    // Convert result to QPainterPaths
    std::vector<QPainterPath> resultPaths;
    if (unionCheckBox->isChecked() && !nfpResult.empty()) {
        // Union all result polygons
        // Using QPainterPath for union as it's easier for visualization
        QPainterPath united;
        for (const auto& p : nfpResult) {
            united = united.united(p.toQPainterPath());
        }
        resultPaths.push_back(united);
    } else {
        for (const auto& p : nfpResult) {
            resultPaths.push_back(p.toQPainterPath());
        }
    }
    
    // Convert polyA back to QPainterPath for visualization
    updateResultView(resultPaths, polyA.toQPainterPath());
}

void MainWindow::updateResultView(const std::vector<QPainterPath>& resultPaths, const QPainterPath& polyA)
{
    resultScene->clear();
    
    // Draw A (stationary)
    QGraphicsPathItem* itemA = resultScene->addPath(polyA);
    itemA->setBrush(QColor(200, 200, 200, 100));
    itemA->setPen(QPen(Qt::black, 0));
    itemA->setData(0, "Stationary");
    
    // Draw NFP result
    int colorIndex = 0;
    for (const auto& path : resultPaths) {
        QGraphicsPathItem* itemNFP = resultScene->addPath(path);
        
        // Generate distinct color
        int h = (colorIndex * 137) % 360; // Golden angle approximation for distribution
        QColor color = QColor::fromHsv(h, 200, 200, 100);
        
        itemNFP->setBrush(color);
        itemNFP->setPen(QPen(color.darker(), 0));
        itemNFP->setData(0, "NFP");
        
        colorIndex++;
    }
    
    resultView->fitToContent();
}

QColor MainWindow::getFileColor(const QString& filename)
{
    if (!fileColors.contains(filename)) {
        // Generate a random pastel color
        int h = QRandomGenerator::global()->bounded(360);
        int s = 100 + QRandomGenerator::global()->bounded(100); // 100-200
        int v = 200 + QRandomGenerator::global()->bounded(55);  // 200-255
        fileColors[filename] = QColor::fromHsv(h, s, v);
    }
    return fileColors[filename];
}
