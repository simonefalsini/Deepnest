#ifndef TEST_APPLICATION_H
#define TEST_APPLICATION_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QTimer>
#include <QLabel>
#include <QWheelEvent>
#include <QProgressBar>
#include <QTextEdit>
#include <memory>
#include <mutex>

#include "../include/deepnest/DeepNestSolver.h"
#include "../include/deepnest/DebugConfig.h"
#include "ConfigDialog.h"
#include "ContainerDialog.h"

/**
 * @brief Graphics view with mouse wheel zoom support
 */
class ZoomableGraphicsView : public QGraphicsView {
    Q_OBJECT
public:
    explicit ZoomableGraphicsView(QWidget* parent = nullptr)
        : QGraphicsView(parent), zoomFactor_(1.0) {}

protected:
    void wheelEvent(QWheelEvent* event) override {
        // Zoom in/out with mouse wheel
        const double scaleFactor = 1.15;
        
        if (event->angleDelta().y() > 0) {
            // Zoom in
            scale(scaleFactor, scaleFactor);
            zoomFactor_ *= scaleFactor;
        } else {
            // Zoom out
            scale(1.0 / scaleFactor, 1.0 / scaleFactor);
            zoomFactor_ /= scaleFactor;
        }
        
        event->accept();
    }

private:
    double zoomFactor_;
};

/**
 * @brief Test application for DeepNest C++ library
 *
 * This application provides a graphical interface to test the nesting engine.
 * It allows users to:
 * - Load SVG files with parts
 * - Generate random test shapes
 * - Configure nesting parameters
 * - Visualize nesting results in real-time
 * - Monitor progress and statistics
 */
class TestApplication : public QMainWindow {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     *
     * @param parent Parent widget (default: nullptr)
     */
    explicit TestApplication(QWidget* parent = nullptr);

    /**
     * @brief Destructor
     */
    ~TestApplication();

    /**
     * @brief Load SVG file from path (for command-line/automated testing)
     *
     * @param filepath Path to SVG file
     * @return true if loaded successfully, false otherwise
     */
    bool loadSVGFromPath(const QString& filepath);

    /**
     * @brief Run automatic nesting test
     *
     * Runs nesting for a specified number of generations and returns
     * the best fitness achieved.
     *
     * @param generations Number of generations to run
     * @return Best fitness value achieved
     */
    double runAutomaticTest(int generations = 100);

    /**
     * @brief Get current best fitness
     *
     * @return Current best fitness value
     */
    double getBestFitness() const { return bestFitness_; }

    /**
     * @brief Check if nesting is running
     *
     * @return true if running, false otherwise
     */
    bool isRunning() const { return running_; }

private slots:
    /**
     * @brief Load SVG file with parts and optionally sheet
     *
     * Opens a file dialog to select an SVG file, parses it,
     * and adds the shapes to the nesting problem.
     */
    void loadSVG();

    /**
     * @brief Start the nesting process
     *
     * Validates the configuration, starts the solver,
     * and begins the nesting optimization.
     */
    void startNesting();

    /**
     * @brief Stop the nesting process
     *
     * Gracefully stops the nesting engine.
     */
    void stopNesting();

    /**
     * @brief Reset the application
     *
     * Clears all parts, sheets, and results.
     */
    void reset();

    /**
     * @brief Test with random rectangles
     *
     * Generates a set of random rectangles and a sheet,
     * then starts nesting. Useful for quick testing.
     */
    void testRandomRectangles();

    /**
     * @brief Test with random polygons
     *
     * Generates a set of random polygons and a sheet,
     * then starts nesting.
     */
    void testRandomPolygons();

    /**
     * @brief Open configuration dialog
     *
     * Allows user to modify nesting parameters such as:
     * - Spacing
     * - Rotations
     * - Population size
     * - Mutation rate
     * - Threads
     * - Placement type
     */
    void openConfigDialog();

    /**
     * @brief Progress callback from nesting engine
     *
     * Called periodically during nesting to report progress.
     *
     * @param progress Current progress information
     */
    void onProgress(const deepnest::NestProgress& progress);

    /**
     * @brief Result callback from nesting engine
     *
     * Called when a better result is found.
     *
     * @param result New best result
     */
    void onResult(const deepnest::NestResult& result);

    /**
     * @brief Timer callback for stepping the nesting engine
     *
     * Called periodically by timer to advance the nesting process.
     */
    void onStepTimer();

    /**
     * @brief Export current best result to SVG
     *
     * Saves the current best nesting result to an SVG file.
     */
    void exportSVG();

    /**
     * @brief Generate random polygons
     *
     * Opens dialog to ask for number of polygons, then generates
     * random polygons and adds them to the current parts list.
     */
    void generatePolygons();

    /**
     * @brief Set container size
     *
     * Opens dialog to set or modify container dimensions.
     */
    void setContainerSize();

    /**
     * @brief View container information
     *
     * Shows current container dimensions and area.
     */
    void viewContainerInfo();

    /**
     * @brief Import SVG in append mode
     *
     * Imports shapes from SVG file without clearing existing parts.
     * Useful for combining multiple SVG files.
     */
    void importSVG();

    /**
     * @brief Toggle shape ID labels visibility
     *
     * Shows/hides ID numbers on shapes in visualization.
     */
    void toggleShapeIds();

    /**
     * @brief Show previous sheet
     */
    void prevSheet();

    /**
     * @brief Show next sheet
     */
    void nextSheet();

private:
    /**
     * @brief Initialize the user interface
     *
     * Creates all widgets, layouts, menus, and toolbars.
     */
    void initUI();

    /**
     * @brief Create the menu bar
     */
    void createMenus();

    /**
     * @brief Create the toolbar
     */
    void createToolbar();

    /**
     * @brief Create the status bar with progress indicators
     */
    void createStatusBar();

    /**
     * @brief Update the graphics scene with current result
     *
     * Draws the current best nesting result on the scene.
     *
     * @param result Nesting result to visualize
     */
    void updateVisualization(const deepnest::NestResult& result);

    /**
     * @brief Draw a polygon on the scene
     *
     * @param polygon Polygon to draw
     * @param color Color to use
     * @param fillOpacity Fill opacity (0.0-1.0)
     * @param showBorder Whether to draw border (default: false)
     */
    void drawPolygon(const deepnest::Polygon& polygon, const QColor& color, double fillOpacity = 0.3, bool showBorder = false);

    /**
     * @brief Clear the graphics scene
     */
    void clearScene();

    /**
     * @brief Update status bar with current state
     */
    void updateStatus();

    /**
     * @brief Log a message to the log widget
     *
     * @param message Message to log
     */
    void log(const QString& message);

    // UI Components
    QGraphicsScene* scene_;          ///< Graphics scene for visualization
    QGraphicsView* view_;            ///< Graphics view
    QTextEdit* logWidget_;           ///< Log output widget
    QLabel* statusLabel_;            ///< Status label in status bar
    QProgressBar* progressBar_;      ///< Progress bar in status bar
    QLabel* generationLabel_;        ///< Current generation label
    QLabel* fitnessLabel_;           ///< Current best fitness label

    // Nesting components
    std::unique_ptr<deepnest::DeepNestSolver> solver_;  ///< DeepNest solver
    QTimer* stepTimer_;                                  ///< Timer for stepping nesting engine

    // State
    bool running_;                   ///< True if nesting is running
    int currentGeneration_;          ///< Current generation number
    double bestFitness_;             ///< Best fitness value found

    // CRITICAL FIX: Use smart pointer instead of raw pointer to prevent memory corruption
    // Thread-safe access protected by resultMutex_
    std::unique_ptr<deepnest::NestResult> lastResult_;  ///< Last result (owned, thread-safe)
    std::mutex resultMutex_;         ///< Protects lastResult_ from concurrent access

    // Parts tracking for visualization
    std::vector<deepnest::Polygon> parts_;  ///< Loaded parts (indexed by id)
    std::vector<deepnest::Polygon> sheets_; ///< Loaded sheets

    // Configuration
    ConfigDialog::Config config_;    ///< Current configuration
    int maxGenerations_;             ///< Maximum generations to run
    int stepTimerInterval_;          ///< Timer interval in milliseconds
    bool showShapeIds_;              ///< Show/hide shape ID labels
    int currentSheetIndex_;          ///< Current sheet being visualized

    // Navigation controls
    QPushButton* prevSheetBtn_;      ///< Button to go to previous sheet
    QPushButton* nextSheetBtn_;      ///< Button to go to next sheet
    QLabel* sheetInfoLabel_;         ///< Label showing "Sheet X of Y"
};

#endif // TEST_APPLICATION_H
