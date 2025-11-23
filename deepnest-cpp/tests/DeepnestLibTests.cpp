#include <iostream>
#include <string>
#include <vector>
#include "TestRunners.h"

int main(int argc, char* argv[]) {
    std::cout << "============================================================" << std::endl;
    std::cout << "   DeepNestLib Consolidated Test Suite" << std::endl;
    std::cout << "============================================================" << std::endl;

    bool runAll = true;
    bool runFitness = false;
    bool runGA = false;
    bool runJS = false;
    bool runSteps = false;

    if (argc > 1) {
        runAll = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--fitness") runFitness = true;
            else if (arg == "--ga") runGA = true;
            else if (arg == "--js") runJS = true;
            else if (arg == "--steps") runSteps = true;
            else if (arg == "--all") runAll = true;
            else {
                std::cout << "Unknown argument: " << arg << std::endl;
                std::cout << "Usage: DeepnestLibTests [--fitness] [--ga] [--js] [--steps] [--all]" << std::endl;
                return 1;
            }
        }
    }

    if (runAll) {
        runFitness = true;
        runGA = true;
        runJS = true;
        runSteps = true;
    }

    int totalFailures = 0;

    if (runFitness) {
        std::cout << "\nRunning Fitness Tests..." << std::endl;
        if (FitnessTests::runTests() != 0) totalFailures++;
    }

    if (runGA) {
        std::cout << "\nRunning Genetic Algorithm Tests..." << std::endl;
        if (GATests::runTests() != 0) totalFailures++;
    }

    if (runJS) {
        std::cout << "\nRunning JS Comparison Tests..." << std::endl;
        if (JSComparisonTests::runTests() != 0) totalFailures++;
    }

    if (runSteps) {
        std::cout << "\nRunning Step Verification Tests..." << std::endl;
        if (StepVerificationTests::runTests() != 0) totalFailures++;
    }

    std::cout << "\n============================================================" << std::endl;
    if (totalFailures == 0) {
        std::cout << "ALL TEST SUITES PASSED" << std::endl;
        return 0;
    } else {
        std::cout << totalFailures << " TEST SUITE(S) FAILED" << std::endl;
        return 1;
    }
}
