# DeepNest C++ Testing Documentation

## Overview

This directory contains comprehensive test suites for the DeepNest C++ implementation, organized according to the implementation phases.

## Test Suites

### FASE 4: Testing and Validation

Three specialized test suites verify correctness and JavaScript compatibility:

#### 1. FitnessTests.cpp (FASE 4.1)
**Purpose**: Unit tests for individual fitness calculation components

**What it tests**:
- Sheet area penalty calculation
- Unplaced parts penalty formula
- Minarea component (gravity, boundingbox, convexhull modes)
- Bounds width component
- Complete fitness formula integration
- Edge cases (all placed, all unplaced, etc.)

**Run**:
```bash
cd deepnest-cpp/tests
qmake FitnessTests.pro
make
./FitnessTests
```

**Expected output**:
- All tests should pass (✓)
- Fitness values should be in thousands/millions range (not ~1.0)
- Formulas should match JavaScript reference values

**Key tests**:
- Sheet Area Penalty: Verifies penalty = sheetArea (not 1.0)
- Unplaced Parts Penalty: Verifies 100M * (partArea/totalArea) formula
- Complete Fitness Formula: End-to-end formula validation

#### 2. GeneticAlgorithmTests.cpp (FASE 4.2)
**Purpose**: Integration tests for GA evolution and optimization

**What it tests**:
- Population initialization and diversity
- Initial fitness variation across individuals
- Mutation creates different individuals
- Fitness improves over generations

**Run**:
```bash
cd deepnest-cpp/tests
qmake GeneticAlgorithmTests.pro
make
./GeneticAlgorithmTests
```

#### 3. JSComparisonTests.cpp (FASE 4.3)
**Purpose**: Comparison tests between JavaScript and C++ implementations

**What it tests**:
- Fitness formula mathematical equivalence
- Polygon area calculation matches JavaScript
- Placement formulas match JavaScript

**Run**:
```bash
cd deepnest-cpp/tests
qmake JSComparisonTests.pro
make
./JSComparisonTests
```

## Running All Tests

```bash
cd deepnest-cpp/tests

# Run FASE 4 tests
for test in FitnessTests GeneticAlgorithmTests JSComparisonTests; do
    echo "Running $test..."
    qmake $test.pro
    make
    ./$test
done
```

## Test Data

See testdata/README.md for test data format and usage.

## References

- IMPLEMENTATION_PLAN.md: Complete implementation roadmap
- StepVerificationTests.cpp: Example test structure
