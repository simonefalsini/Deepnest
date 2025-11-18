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
 */

#include "TestApplication.h"
#include <QApplication>
#include <QStyleFactory>
#include <iostream>

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

    // Print welcome message
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
