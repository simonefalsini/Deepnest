# Test Data Directory

This directory contains test data for comparing C++ and JavaScript implementations.

## Files

### polygon_A.json
Simple square polygon (100x100) for NFP testing.

### polygon_B.json
Rectangle polygon (50x50 with offset) for NFP testing.

### nfp_expected.json (TODO)
Expected NFP result from JavaScript implementation for comparison.

## Usage

These JSON files can be loaded by JSComparisonTests to verify that C++
implementation produces identical results to JavaScript reference implementation.

## Adding New Test Data

1. Generate polygon in JavaScript implementation
2. Export to JSON format with this structure:
```json
{
  "points": [{"x": 0, "y": 0}, ...],
  "id": 1,
  "rotation": 0,
  "children": []  // optional holes
}
```

3. Save in this directory
4. Add corresponding test in JSComparisonTests.cpp
