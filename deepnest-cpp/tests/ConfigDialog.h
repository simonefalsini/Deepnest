#ifndef CONFIG_DIALOG_H
#define CONFIG_DIALOG_H

#include <QDialog>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QFormLayout>
#include <QGroupBox>
#include <QTabWidget>

/**
 * @brief Unified configuration dialog for all test application settings
 *
 * This dialog consolidates all configuration options in one place:
 * - DeepNest algorithm parameters (spacing, rotations, GA settings)
 * - Test generation parameters (number of parts, sheets, dimensions)
 * - Runtime settings (threads, placement strategy)
 */
class ConfigDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Configuration structure for all settings
     */
    struct Config {
        // === Nesting Algorithm Parameters ===
        double spacing;              // Minimum spacing between parts
        double curveTolerance;       // Tolerance for curve approximation
        int rotations;               // Number of rotation steps (0=no rotation, 4=90° increments)
        int populationSize;          // Genetic algorithm population size
        int mutationRate;            // Mutation rate percentage (0-100, typically 50)
        int threads;                 // Number of parallel worker threads
        QString placementType;       // "gravity", "boundingbox", or "convexhull"
        bool mergeLines;             // Enable line merge detection
        double timeRatio;            // Line merge time ratio

        // === Test Generation Parameters ===
        int numPartTypes;            // Number of different part types to generate
        int quantityPerPart;         // Number of copies of each part type
        int numSheetTypes;           // Number of different sheet types
        int quantityPerSheet;        // Number of copies of each sheet type

        // === Part Dimension Parameters ===
        double minPartWidth;         // Minimum part width
        double maxPartWidth;         // Maximum part width
        double minPartHeight;        // Minimum part height
        double maxPartHeight;        // Maximum part height
        int minPolySides;            // Minimum polygon sides (for random polygons)
        int maxPolySides;            // Maximum polygon sides (for random polygons)

        // === Sheet Dimension Parameters ===
        double sheetWidth;           // Sheet width
        double sheetHeight;          // Sheet height

        // === Runtime Parameters ===
        int maxGenerations;          // Maximum generations to run (0=unlimited)

        // Default constructor with reasonable defaults
        Config()
            : spacing(0.0)
            , curveTolerance(0.3)
            , rotations(4)
            , populationSize(10)
            , mutationRate(50)
            , threads(4)
            , placementType("gravity")
            , mergeLines(false)
            , timeRatio(0.5)
            , numPartTypes(10)
            , quantityPerPart(2)
            , numSheetTypes(1)
            , quantityPerSheet(3)
            , minPartWidth(50.0)
            , maxPartWidth(150.0)
            , minPartHeight(30.0)
            , maxPartHeight(100.0)
            , minPolySides(5)
            , maxPolySides(8)
            , sheetWidth(500.0)
            , sheetHeight(400.0)
            , maxGenerations(100)
        {}
    };

    /**
     * @brief Constructor
     * @param currentConfig Current configuration to display
     * @param parent Parent widget
     */
    explicit ConfigDialog(const Config& currentConfig, QWidget* parent = nullptr);

    /**
     * @brief Get the configuration from the dialog
     * @return Updated configuration
     */
    Config getConfig() const;

private slots:
    void onAccept();
    void onReject();
    void onRestoreDefaults();

private:
    void setupUI();
    void createNestingTab();
    void createGenerationTab();
    void createAdvancedTab();
    void loadConfig(const Config& config);
    void validateAndAccept();

    // UI Components - Nesting Tab
    QDoubleSpinBox* spacingSpinBox_;
    QDoubleSpinBox* curveToleranceSpinBox_;
    QSpinBox* rotationsSpinBox_;
    QSpinBox* populationSizeSpinBox_;
    QSpinBox* mutationRateSpinBox_;
    QSpinBox* threadsSpinBox_;
    QComboBox* placementTypeCombo_;
    QCheckBox* mergeLinesCheckBox_;
    QDoubleSpinBox* timeRatioSpinBox_;

    // UI Components - Generation Tab
    QSpinBox* numPartTypesSpinBox_;
    QSpinBox* quantityPerPartSpinBox_;
    QSpinBox* numSheetTypesSpinBox_;
    QSpinBox* quantityPerSheetSpinBox_;

    QDoubleSpinBox* minPartWidthSpinBox_;
    QDoubleSpinBox* maxPartWidthSpinBox_;
    QDoubleSpinBox* minPartHeightSpinBox_;
    QDoubleSpinBox* maxPartHeightSpinBox_;
    QSpinBox* minPolySidesSpinBox_;
    QSpinBox* maxPolySidesSpinBox_;

    QDoubleSpinBox* sheetWidthSpinBox_;
    QDoubleSpinBox* sheetHeightSpinBox_;

    // UI Components - Advanced Tab
    QSpinBox* maxGenerationsSpinBox_;

    // Layout
    QTabWidget* tabWidget_;

    // Configuration
    Config config_;
};

#endif // CONFIG_DIALOG_H
