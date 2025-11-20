/**
 * @file main.cpp
 * @brief Main entry point for DeepNest C++ Test Application
 *
 * This application provides a graphical interface for testing the DeepNest
 * nesting library. It allows users to:
 * - Load SVG files with parts
 * - Generate random test shapes
 * - Configure nesting parameters
 * - Visualize results in real-time
 * - Monitor progress and export results
 *
 * Command-line usage:
 *   TestApplication                         - GUI mode
 *   TestApplication --test <file.svg>       - Automatic test mode
 *   TestApplication --benchmark <file.svg> [--generations N] [--reference-fitness F]
 */

#include "TestApplication.h"
#include <QApplication>
#include <QStyleFactory>
#include <QCommandLineParser>
#include <iostream>
#include <iomanip>

void printUsage() {
    std::cout << "\nUsage:" << std::endl;
    std::cout << "  TestApplication                              - Start GUI mode" << std::endl;
    std::cout << "  TestApplication --test <file.svg>            - Run automatic test" << std::endl;
    std::cout << "  TestApplication --benchmark <file.svg> [options]" << std::endl;
    std::cout << "\nBenchmark Options:" << std::endl;
    std::cout << "  --generations N         - Number of generations to run (default: 100)" << std::endl;
    std::cout << "  --reference-fitness F   - Reference fitness to compare against" << std::endl;
    std::cout << "\nExample:" << std::endl;
    std::cout << "  TestApplication --benchmark test.svg --generations 200 --reference-fitness 512.5" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char *argv[]) {
    // Create Qt application
    QApplication app(argc, argv);

    // Set application metadata
    QApplication::setApplicationName("DeepNest Test");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("DeepNest");

    // Set a modern style if available
    QStringList styles = QStyleFactory::keys();
    if (styles.contains("Fusion")) {
        QApplication::setStyle("Fusion");
    }

    // Parse command-line arguments
    QCommandLineParser parser;
    parser.setApplicationDescription("DeepNest C++ Test Application");
    parser.addHelpOption();
    parser.addVersionOption();

    // Add options
    QCommandLineOption testOption(QStringList() << "t" << "test",
                                  "Run automatic test with SVG file",
                                  "svg-file");
    parser.addOption(testOption);

    QCommandLineOption benchmarkOption(QStringList() << "b" << "benchmark",
                                       "Run benchmark test with SVG file",
                                       "svg-file");
    parser.addOption(benchmarkOption);

    QCommandLineOption generationsOption(QStringList() << "g" << "generations",
                                         "Number of generations to run (default: 100)",
                                         "count",
                                         "100");
    parser.addOption(generationsOption);

    QCommandLineOption referenceFitnessOption(QStringList() << "r" << "reference-fitness",
                                              "Reference fitness value to compare against",
                                              "fitness");
    parser.addOption(referenceFitnessOption);

    parser.process(app);

    // Check for test/benchmark mode
    bool testMode = parser.isSet(testOption) || parser.isSet(benchmarkOption);
    QString svgFile;
    int generations = parser.value(generationsOption).toInt();
    bool hasReferenceFitness = parser.isSet(referenceFitnessOption);
    double referenceFitness = hasReferenceFitness ? parser.value(referenceFitnessOption).toDouble() : 0.0;

    if (parser.isSet(testOption)) {
        svgFile = parser.value(testOption);
    } else if (parser.isSet(benchmarkOption)) {
        svgFile = parser.value(benchmarkOption);
    }

    if (testMode) {
        // AUTOMATIC TEST MODE - No GUI
        std::cout << "========================================" << std::endl;
        std::cout << "  DeepNest C++ Automatic Test" << std::endl;
        std::cout << "  Version 1.0.0" << std::endl;
        std::cout << "========================================" << std::endl;

        // Create window but don't show it
        TestApplication window;

        // Load SVG file
        if (!window.loadSVGFromPath(svgFile)) {
            std::cerr << "\nFailed to load SVG file. Exiting." << std::endl;
            return 1;
        }

        // Run automatic test
        double finalFitness = window.runAutomaticTest(generations);

        // Compare with reference if provided
        if (hasReferenceFitness) {
            std::cout << "\n========================================" << std::endl;
            std::cout << "  COMPARISON WITH REFERENCE" << std::endl;
            std::cout << "========================================" << std::endl;
            std::cout << "Reference fitness: " << std::fixed << std::setprecision(4) << referenceFitness << std::endl;
            std::cout << "Achieved fitness:  " << std::fixed << std::setprecision(4) << finalFitness << std::endl;

            double difference = finalFitness - referenceFitness;
            double percentDiff = (referenceFitness != 0.0) ? (difference / referenceFitness * 100.0) : 0.0;

            std::cout << "Difference:        " << std::fixed << std::setprecision(4) << difference;
            if (difference < 0) {
                std::cout << " (BETTER by " << std::setprecision(2) << std::abs(percentDiff) << "%)" << std::endl;
            } else if (difference > 0) {
                std::cout << " (WORSE by " << std::setprecision(2) << percentDiff << "%)" << std::endl;
            } else {
                std::cout << " (EQUAL)" << std::endl;
            }

            std::cout << "========================================" << std::endl;

            // Return exit code based on comparison
            // 0 = success (better or equal), 1 = worse
            return (difference <= 0) ? 0 : 1;
        }

        return 0;
    }

    // NORMAL GUI MODE
    std::cout << "========================================" << std::endl;
    std::cout << "  DeepNest C++ Test Application" << std::endl;
    std::cout << "  Version 1.0.0" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  - Load SVG: Load parts from SVG file (Step 23)" << std::endl;
    std::cout << "  - Test Rectangles: Generate random rectangles" << std::endl;
    std::cout << "  - Test Polygons: Generate random polygons" << std::endl;
    std::cout << "  - Config: Configure nesting parameters" << std::endl;
    std::cout << "  - Start (F5): Start nesting optimization" << std::endl;
    std::cout << "  - Stop (Esc): Stop nesting" << std::endl;
    std::cout << "  - Reset: Clear all parts and results" << std::endl;
    std::cout << "\nFor automatic testing, use --help to see command-line options" << std::endl;
    std::cout << std::endl;

    // Create and show main window
    TestApplication window;
    window.show();

    // Run application event loop
    int result = app.exec();

    std::cout << std::endl;
    std::cout << "Application closed. Thank you for using DeepNest!" << std::endl;

    return result;
}
