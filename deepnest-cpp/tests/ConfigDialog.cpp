#include "ConfigDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QGroupBox>

ConfigDialog::ConfigDialog(const Config& currentConfig, QWidget* parent)
    : QDialog(parent)
    , config_(currentConfig)
{
    setWindowTitle("Configuration");
    setMinimumWidth(500);
    setupUI();
    loadConfig(currentConfig);
}

void ConfigDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Create tab widget
    tabWidget_ = new QTabWidget(this);

    createNestingTab();
    createGenerationTab();
    createAdvancedTab();

    mainLayout->addWidget(tabWidget_);

    // Button box
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults,
        this
    );

    connect(buttonBox, &QDialogButtonBox::accepted, this, &ConfigDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &ConfigDialog::onReject);
    connect(buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked,
            this, &ConfigDialog::onRestoreDefaults);

    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
}

void ConfigDialog::createNestingTab() {
    QWidget* nestingWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(nestingWidget);

    // === Algorithm Parameters Group ===
    QGroupBox* algoGroup = new QGroupBox("Algorithm Parameters");
    QFormLayout* algoForm = new QFormLayout();

    spacingSpinBox_ = new QDoubleSpinBox();
    spacingSpinBox_->setRange(0.0, 100.0);
    spacingSpinBox_->setDecimals(1);
    spacingSpinBox_->setSuffix(" units");
    spacingSpinBox_->setToolTip("Minimum spacing between parts");
    algoForm->addRow("Spacing:", spacingSpinBox_);

    curveToleranceSpinBox_ = new QDoubleSpinBox();
    curveToleranceSpinBox_->setRange(0.01, 10.0);
    curveToleranceSpinBox_->setDecimals(2);
    curveToleranceSpinBox_->setToolTip("Tolerance for curve approximation (lower = more precise)");
    algoForm->addRow("Curve Tolerance:", curveToleranceSpinBox_);

    rotationsSpinBox_ = new QSpinBox();
    rotationsSpinBox_->setRange(0, 16);
    rotationsSpinBox_->setToolTip("Number of rotation steps (0=no rotation, 4=90° increments, 8=45° increments)");
    algoForm->addRow("Rotations:", rotationsSpinBox_);

    placementTypeCombo_ = new QComboBox();
    placementTypeCombo_->addItems({"gravity", "boundingbox", "convexhull"});
    placementTypeCombo_->setToolTip("Placement strategy: gravity=compact down, boundingbox=minimize bbox, convexhull=minimize convex hull");
    algoForm->addRow("Placement Type:", placementTypeCombo_);

    algoGroup->setLayout(algoForm);
    layout->addWidget(algoGroup);

    // === Genetic Algorithm Group ===
    QGroupBox* gaGroup = new QGroupBox("Genetic Algorithm");
    QFormLayout* gaForm = new QFormLayout();

    populationSizeSpinBox_ = new QSpinBox();
    populationSizeSpinBox_->setRange(2, 100);
    populationSizeSpinBox_->setToolTip("Size of GA population (larger = more diversity, slower)");
    gaForm->addRow("Population Size:", populationSizeSpinBox_);

    mutationRateSpinBox_ = new QDoubleSpinBox();
    mutationRateSpinBox_->setRange(0.0, 1.0);
    mutationRateSpinBox_->setDecimals(2);
    mutationRateSpinBox_->setSingleStep(0.05);
    mutationRateSpinBox_->setToolTip("Mutation probability (0.0-1.0, typically 0.5)");
    gaForm->addRow("Mutation Rate:", mutationRateSpinBox_);

    threadsSpinBox_ = new QSpinBox();
    threadsSpinBox_->setRange(1, 32);
    threadsSpinBox_->setToolTip("Number of parallel worker threads");
    gaForm->addRow("Threads:", threadsSpinBox_);

    gaGroup->setLayout(gaForm);
    layout->addWidget(gaGroup);

    // === Line Merge Group ===
    QGroupBox* mergeGroup = new QGroupBox("Line Merge Detection");
    QFormLayout* mergeForm = new QFormLayout();

    mergeLinesCheckBox_ = new QCheckBox();
    mergeLinesCheckBox_->setToolTip("Enable detection of aligned edges for cutting optimization");
    mergeForm->addRow("Enable Line Merge:", mergeLinesCheckBox_);

    timeRatioSpinBox_ = new QDoubleSpinBox();
    timeRatioSpinBox_->setRange(0.0, 2.0);
    timeRatioSpinBox_->setDecimals(2);
    timeRatioSpinBox_->setSingleStep(0.1);
    timeRatioSpinBox_->setToolTip("Time ratio for line merge bonus (higher = more weight on aligned edges)");
    mergeForm->addRow("Time Ratio:", timeRatioSpinBox_);

    mergeGroup->setLayout(mergeForm);
    layout->addWidget(mergeGroup);

    layout->addStretch();

    tabWidget_->addTab(nestingWidget, "Nesting");
}

void ConfigDialog::createGenerationTab() {
    QWidget* genWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(genWidget);

    // === Test Generation Group ===
    QGroupBox* testGroup = new QGroupBox("Test Generation");
    QFormLayout* testForm = new QFormLayout();

    numPartTypesSpinBox_ = new QSpinBox();
    numPartTypesSpinBox_->setRange(1, 100);
    numPartTypesSpinBox_->setToolTip("Number of different part types to generate");
    testForm->addRow("Number of Part Types:", numPartTypesSpinBox_);

    quantityPerPartSpinBox_ = new QSpinBox();
    quantityPerPartSpinBox_->setRange(1, 20);
    quantityPerPartSpinBox_->setToolTip("Number of copies of each part type");
    testForm->addRow("Quantity per Part:", quantityPerPartSpinBox_);

    numSheetTypesSpinBox_ = new QSpinBox();
    numSheetTypesSpinBox_->setRange(1, 10);
    numSheetTypesSpinBox_->setToolTip("Number of different sheet types");
    testForm->addRow("Number of Sheet Types:", numSheetTypesSpinBox_);

    quantityPerSheetSpinBox_ = new QSpinBox();
    quantityPerSheetSpinBox_->setRange(1, 20);
    quantityPerSheetSpinBox_->setToolTip("Number of copies of each sheet type");
    testForm->addRow("Quantity per Sheet:", quantityPerSheetSpinBox_);

    testGroup->setLayout(testForm);
    layout->addWidget(testGroup);

    // === Part Dimensions Group ===
    QGroupBox* partDimGroup = new QGroupBox("Part Dimensions");
    QFormLayout* partDimForm = new QFormLayout();

    minPartWidthSpinBox_ = new QDoubleSpinBox();
    minPartWidthSpinBox_->setRange(1.0, 1000.0);
    minPartWidthSpinBox_->setDecimals(1);
    minPartWidthSpinBox_->setSuffix(" units");
    minPartWidthSpinBox_->setToolTip("Minimum width for generated parts");
    partDimForm->addRow("Min Part Width:", minPartWidthSpinBox_);

    maxPartWidthSpinBox_ = new QDoubleSpinBox();
    maxPartWidthSpinBox_->setRange(1.0, 1000.0);
    maxPartWidthSpinBox_->setDecimals(1);
    maxPartWidthSpinBox_->setSuffix(" units");
    maxPartWidthSpinBox_->setToolTip("Maximum width for generated parts");
    partDimForm->addRow("Max Part Width:", maxPartWidthSpinBox_);

    minPartHeightSpinBox_ = new QDoubleSpinBox();
    minPartHeightSpinBox_->setRange(1.0, 1000.0);
    minPartHeightSpinBox_->setDecimals(1);
    minPartHeightSpinBox_->setSuffix(" units");
    minPartHeightSpinBox_->setToolTip("Minimum height for generated parts");
    partDimForm->addRow("Min Part Height:", minPartHeightSpinBox_);

    maxPartHeightSpinBox_ = new QDoubleSpinBox();
    maxPartHeightSpinBox_->setRange(1.0, 1000.0);
    maxPartHeightSpinBox_->setDecimals(1);
    maxPartHeightSpinBox_->setSuffix(" units");
    maxPartHeightSpinBox_->setToolTip("Maximum height for generated parts");
    partDimForm->addRow("Max Part Height:", maxPartHeightSpinBox_);

    minPolySidesSpinBox_ = new QSpinBox();
    minPolySidesSpinBox_->setRange(3, 20);
    minPolySidesSpinBox_->setToolTip("Minimum number of sides for random polygons");
    partDimForm->addRow("Min Polygon Sides:", minPolySidesSpinBox_);

    maxPolySidesSpinBox_ = new QSpinBox();
    maxPolySidesSpinBox_->setRange(3, 20);
    maxPolySidesSpinBox_->setToolTip("Maximum number of sides for random polygons");
    partDimForm->addRow("Max Polygon Sides:", maxPolySidesSpinBox_);

    partDimGroup->setLayout(partDimForm);
    layout->addWidget(partDimGroup);

    // === Sheet Dimensions Group ===
    QGroupBox* sheetDimGroup = new QGroupBox("Sheet Dimensions");
    QFormLayout* sheetDimForm = new QFormLayout();

    sheetWidthSpinBox_ = new QDoubleSpinBox();
    sheetWidthSpinBox_->setRange(100.0, 10000.0);
    sheetWidthSpinBox_->setDecimals(1);
    sheetWidthSpinBox_->setSuffix(" units");
    sheetWidthSpinBox_->setToolTip("Width of generated sheets");
    sheetDimForm->addRow("Sheet Width:", sheetWidthSpinBox_);

    sheetHeightSpinBox_ = new QDoubleSpinBox();
    sheetHeightSpinBox_->setRange(100.0, 10000.0);
    sheetHeightSpinBox_->setDecimals(1);
    sheetHeightSpinBox_->setSuffix(" units");
    sheetHeightSpinBox_->setToolTip("Height of generated sheets");
    sheetDimForm->addRow("Sheet Height:", sheetHeightSpinBox_);

    sheetDimGroup->setLayout(sheetDimForm);
    layout->addWidget(sheetDimGroup);

    layout->addStretch();

    tabWidget_->addTab(genWidget, "Generation");
}

void ConfigDialog::createAdvancedTab() {
    QWidget* advWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(advWidget);

    // === Runtime Parameters Group ===
    QGroupBox* runtimeGroup = new QGroupBox("Runtime Parameters");
    QFormLayout* runtimeForm = new QFormLayout();

    maxGenerationsSpinBox_ = new QSpinBox();
    maxGenerationsSpinBox_->setRange(0, 10000);
    maxGenerationsSpinBox_->setSpecialValueText("Unlimited");
    maxGenerationsSpinBox_->setToolTip("Maximum generations to run (0 = unlimited)");
    runtimeForm->addRow("Max Generations:", maxGenerationsSpinBox_);

    runtimeGroup->setLayout(runtimeForm);
    layout->addWidget(runtimeGroup);

    layout->addStretch();

    tabWidget_->addTab(advWidget, "Advanced");
}

void ConfigDialog::loadConfig(const Config& config) {
    // Nesting tab
    spacingSpinBox_->setValue(config.spacing);
    curveToleranceSpinBox_->setValue(config.curveTolerance);
    rotationsSpinBox_->setValue(config.rotations);
    populationSizeSpinBox_->setValue(config.populationSize);
    mutationRateSpinBox_->setValue(config.mutationRate);
    threadsSpinBox_->setValue(config.threads);

    int placementIndex = placementTypeCombo_->findText(config.placementType);
    if (placementIndex >= 0) {
        placementTypeCombo_->setCurrentIndex(placementIndex);
    }

    mergeLinesCheckBox_->setChecked(config.mergeLines);
    timeRatioSpinBox_->setValue(config.timeRatio);

    // Generation tab
    numPartTypesSpinBox_->setValue(config.numPartTypes);
    quantityPerPartSpinBox_->setValue(config.quantityPerPart);
    numSheetTypesSpinBox_->setValue(config.numSheetTypes);
    quantityPerSheetSpinBox_->setValue(config.quantityPerSheet);

    minPartWidthSpinBox_->setValue(config.minPartWidth);
    maxPartWidthSpinBox_->setValue(config.maxPartWidth);
    minPartHeightSpinBox_->setValue(config.minPartHeight);
    maxPartHeightSpinBox_->setValue(config.maxPartHeight);
    minPolySidesSpinBox_->setValue(config.minPolySides);
    maxPolySidesSpinBox_->setValue(config.maxPolySides);

    sheetWidthSpinBox_->setValue(config.sheetWidth);
    sheetHeightSpinBox_->setValue(config.sheetHeight);

    // Advanced tab
    maxGenerationsSpinBox_->setValue(config.maxGenerations);
}

ConfigDialog::Config ConfigDialog::getConfig() const {
    return config_;
}

void ConfigDialog::onAccept() {
    validateAndAccept();
}

void ConfigDialog::onReject() {
    reject();
}

void ConfigDialog::onRestoreDefaults() {
    Config defaults;
    loadConfig(defaults);
}

void ConfigDialog::validateAndAccept() {
    // Validate ranges
    if (minPartWidthSpinBox_->value() > maxPartWidthSpinBox_->value()) {
        QMessageBox::warning(this, "Validation Error",
            "Min part width cannot be greater than max part width!");
        tabWidget_->setCurrentIndex(1); // Switch to Generation tab
        return;
    }

    if (minPartHeightSpinBox_->value() > maxPartHeightSpinBox_->value()) {
        QMessageBox::warning(this, "Validation Error",
            "Min part height cannot be greater than max part height!");
        tabWidget_->setCurrentIndex(1);
        return;
    }

    if (minPolySidesSpinBox_->value() > maxPolySidesSpinBox_->value()) {
        QMessageBox::warning(this, "Validation Error",
            "Min polygon sides cannot be greater than max polygon sides!");
        tabWidget_->setCurrentIndex(1);
        return;
    }

    // Update config from UI
    config_.spacing = spacingSpinBox_->value();
    config_.curveTolerance = curveToleranceSpinBox_->value();
    config_.rotations = rotationsSpinBox_->value();
    config_.populationSize = populationSizeSpinBox_->value();
    config_.mutationRate = mutationRateSpinBox_->value();
    config_.threads = threadsSpinBox_->value();
    config_.placementType = placementTypeCombo_->currentText();
    config_.mergeLines = mergeLinesCheckBox_->isChecked();
    config_.timeRatio = timeRatioSpinBox_->value();

    config_.numPartTypes = numPartTypesSpinBox_->value();
    config_.quantityPerPart = quantityPerPartSpinBox_->value();
    config_.numSheetTypes = numSheetTypesSpinBox_->value();
    config_.quantityPerSheet = quantityPerSheetSpinBox_->value();

    config_.minPartWidth = minPartWidthSpinBox_->value();
    config_.maxPartWidth = maxPartWidthSpinBox_->value();
    config_.minPartHeight = minPartHeightSpinBox_->value();
    config_.maxPartHeight = maxPartHeightSpinBox_->value();
    config_.minPolySides = minPolySidesSpinBox_->value();
    config_.maxPolySides = maxPolySidesSpinBox_->value();

    config_.sheetWidth = sheetWidthSpinBox_->value();
    config_.sheetHeight = sheetHeightSpinBox_->value();

    config_.maxGenerations = maxGenerationsSpinBox_->value();

    accept();
}
