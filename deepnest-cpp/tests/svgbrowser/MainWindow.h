#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QGraphicsScene>
#include <QMap>
#include <QColor>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QDoubleSpinBox>
#include "ZoomableGraphicsView.h"

// Forward declarations
class QGraphicsItem;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void openDirectory();
    void onFileSelectionChanged();
    void onSceneSelectionChanged();
    void runNFP();
    void onUnionToggled(bool checked);
    void updateVertexBList();
    void onImportClicked();
    void onSelectionChanged();

private:
    void setupUi();
    QColor getFileColor(const QString& filename);
    void updateResultView(const std::vector<QPainterPath>& resultPaths, const QPainterPath& polyA);

    QListWidget* fileList;
    
    // Main view
    QGraphicsScene* scene;
    ZoomableGraphicsView* view;
    
    // NFP Panel
    QComboBox* implementationCombo;
    QCheckBox* unionCheckBox;
    
    QComboBox* rotationACombo;
    QDoubleSpinBox* rotationASpin;
    
    QComboBox* rotationBCombo;
    QDoubleSpinBox* rotationBSpin;
    
    QComboBox* vertexBCombo;
    QPushButton* importButton;

    QGraphicsScene* resultScene;
    ZoomableGraphicsView* resultView;

    QString currentDir;
    QMap<QString, QColor> fileColors;
    
    // Selection tracking
    QList<QGraphicsItem*> selectionOrder;
    double currentMaxZ = 0.0;
};

#endif // MAINWINDOW_H
