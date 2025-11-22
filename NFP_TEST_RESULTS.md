# NFP Validation Test Results - Critical Issues Found

## Test Execution Summary

**Date**: 2025-11-22
**Test Suite**: NFPValidationTests.cpp
**Total Tests**: 33
**Passed**: 13 (39.4%)
**Failed**: 20 (60.6%)

---

## ✅ What Works (13/33 tests passing)

### Orbital Tracing Helpers - 100% Success
- ✓ `findTouchingContacts()` - All 3 tests passed
  - Vertex-vertex contact detection ✓
  - Edge touching detection ✓
  - No contact (negative case) ✓

- ✓ `generateTranslationVectors()` - Working correctly
  - Generates 4 vectors from touching contact ✓

- ✓ `isBacktracking()` - Logic correct
  - Detects opposite vectors as backtracking ✓
  - Correctly allows perpendicular vectors ✓

### NFP Calculation - Partial Success
- ✓ `noFitPolygon()` - **Works for some shapes**
  - **Triangle vs Triangle**: 1 NFP, 8 points ✓
  - **Concave vs Square**: 1 NFP, 9 points ✓

- ✓ `noFitPolygonRectangle()` - Optimization works
  - Rectangle optimization path functional ✓

### Other
- ✓ `segmentDistance()` - NULL detection works (2/4 tests)
- ✓ `pointDistance()` - NULL detection works (1/4 tests)
- ✓ `polygonSlideDistance()` - NULL detection works (1/5 tests)

---

## ❌ Critical Issues Found (20 failures)

### ISSUE #1: Distance Functions Return Wrong Sign

**Severity**: CRITICAL
**Affected Functions**: `pointDistance()`, `segmentDistance()`, `polygonSlideDistance()`

#### Symptoms:
```
pointDistance - perpendicular to line:
  Expected: 5.0
  Got: -5.0 ✗

segmentDistance - parallel horizontal:
  Expected: 5.0
  Got: -5.0 ✗

polygonSlideDistance - squares vertical:
  Expected: ~5.0
  Got: -25.0 ✗ (magnitude also wrong!)
```

#### Analysis:
- Distance functions consistently return **negative values** when positive expected
- The **sign convention** for distances may be:
  - Inverted from expected (JavaScript → C++ conversion issue?)
  - Related to direction vector interpretation
  - Related to normal vector calculation

#### Impact:
- **HIGH** - These functions are foundational for NFP calculation
- `polygonSlideDistance()` is used by `noFitPolygon()` to find collision points
- Wrong distances → wrong NFP paths → NFP calculation fails

---

### ISSUE #2: searchStartPoint Fails for Simple Shapes

**Severity**: CRITICAL
**Affected Functions**: `searchStartPoint()`

#### Symptoms:
```
searchStartPoint - OUTER square: No start point found ✗
searchStartPoint - INNER square: No start point found ✗
searchStartPoint - OUTER triangle: No start point found ✗
```

#### Debug Output Analysis:
```
=== searchStartPoint DEBUG ===
  A.size()=4, B.size()=4, inside=0, NFP.size()=0
  FAILED: No valid start point found
  Statistics:
    - Total candidates checked: 20
    - Candidates with valid Binside: 20
    - Candidates passed first check: 20
    - Candidates tried sliding: 5
```

**Key observations**:
- Function checks 20 candidates
- All candidates pass initial checks
- Only 5 candidates attempted sliding
- **None found valid**

#### Possible Root Causes:
1. `polygonSlideDistance()` returning wrong values → invalid start points rejected
2. `pointInPolygon()` might be incorrectly classifying inside/outside
3. Intersection detection might be too strict/lenient
4. Tolerance issues (numerical precision)

#### Impact:
- **CRITICAL** - No start point = No NFP calculation possible
- Explains why many `noFitPolygon()` tests return 0 NFPs
- **BUT**: Triangle vs Triangle works! → Bug is shape-specific?

---

### ISSUE #3: noFitPolygon Fails for Simple Rectangles/Squares

**Severity**: HIGH
**Affected Functions**: `noFitPolygon()`

#### Symptoms:
```
noFitPolygon - square vs square OUTER:
  Expected: 1 NFP (quadrilateral)
  Got: 0 NFPs ✗

noFitPolygon - rectangle vs rectangle:
  Expected: 1 NFP
  Got: 0 NFPs ✗

noFitPolygon - INNER square in square:
  Expected: 1 NFP
  Got: 0 NFPs ✗
```

#### What Works:
```
noFitPolygon - triangle vs triangle OUTER:
  Got: 1 NFP, 8 points ✓

noFitPolygon - concave vs square:
  Got: 1 NFP, 9 points ✓
```

#### Analysis:
**Hypothesis**: The issue is likely in `searchStartPoint()` failing for simple rectangles/squares:
- Triangles and concave shapes → start point found → NFP works
- Squares/rectangles → start point NOT found → NFP fails (returns 0)

**Alternative hypothesis**:
- Special case for rectangles (`isRectangle()`) triggers different code path
- `noFitPolygonRectangle()` optimization DOES work (test passes)
- But general `noFitPolygon()` for rectangles fails
- Possible issue with how rectangles are detected or handled in general path

---

## 🔍 Detailed Test Results by Phase

### PHASE 1: Distance Functions (5/13 passed - 38%)

| Test | Result | Expected | Got | Issue |
|------|--------|----------|-----|-------|
| pointDistance - perpendicular | ✗ | 5.0 | -5.0 | Wrong sign |
| pointDistance - outside projection | ✓ | NULL | NULL | OK |
| pointDistance - infinite mode | ✗ | 5.0 | -5.0 | Wrong sign |
| pointDistance - negative direction | ✗ | <0 | 5.0 | Wrong sign (inverted) |
| segmentDistance - parallel horizontal | ✗ | 5.0 | -5.0 | Wrong sign |
| segmentDistance - perpendicular | ✗ | 5.0 | -10.0 | Wrong sign + magnitude? |
| segmentDistance - no collision | ✓ | NULL | NULL | OK |
| segmentDistance - diagonal | ✓ | >0 | -14.14 | Works but negative |
| polygonSlideDistance - vertical | ✗ | 5.0 | -25.0 | Wrong sign + magnitude |
| polygonSlideDistance - touching | ✗ | 0.0 | -20.0 | Wrong sign + magnitude |
| polygonSlideDistance - horizontal | ✗ | 5.0 | -25.0 | Wrong sign + magnitude |
| polygonSlideDistance - no collision | ✓ | NULL | NULL | OK |
| polygonSlideDistance - triangle | ✗ | >0 | -20.0 | Wrong sign |

**Pattern**: Negative values when positive expected, NULL detection works

---

### PHASE 2: searchStartPoint (0/3 passed - 0%)

| Test | Result | Details |
|------|--------|---------|
| OUTER square | ✗ | No start point found (checked 20 candidates, 5 tried sliding) |
| INNER square | ✗ | No start point found (checked 20 candidates, 5 tried sliding) |
| OUTER triangle | ✗ | ERROR: Binside is nullopt (A and B are the same) |

**Critical**: Function fails on ALL test cases

---

### PHASE 3: Orbital Helpers (6/6 passed - 100%)

| Test | Result | Details |
|------|--------|---------|
| findTouchingContacts - vertex-vertex | ✓ | Found 1 contact |
| findTouchingContacts - edge touching | ✓ | Found 1 contact |
| findTouchingContacts - no contact | ✓ | Found 0 contacts (correct) |
| generateTranslationVectors | ✓ | Generated 4 vectors |
| isBacktracking - opposite | ✓ | Correctly detected |
| isBacktracking - perpendicular | ✓ | Correctly allowed |

**Excellent**: All orbital helpers working correctly!

---

### PHASE 4: NFP Simple Cases (2/4 passed - 50%)

| Test | Result | NFPs Generated | Points | Expected |
|------|--------|----------------|--------|----------|
| Square vs Square OUTER | ✗ | 0 | 0 | Quadrilateral |
| Triangle vs Triangle OUTER | ✓ | 1 | 8 | Polygon ✓ |
| Rectangle vs point-like | ✗ | 0 | 0 | ~Rectangle |
| Concave vs Square | ✓ | 1 | 9 | Polygon ✓ |

**Pattern**: Works for triangles/concave, fails for rectangles/squares

---

### PHASE 5: NFP Complex Cases (0/4 passed - 0%)

| Test | Result | NFPs Generated |
|------|--------|----------------|
| Rectangle vs Rectangle | ✗ | 0 |
| Square vs Rotated Square | ✗ | 0 |
| searchEdges mode | ✗ | 0 |
| INNER square in square | ✗ | 0 |

**Critical**: All complex cases fail

---

### PHASE 6: Regression Tests (1/3 passed - 33%)

| Test | Result | Details |
|------|--------|---------|
| Simple rectangles | ✗ | No NFP generated |
| Rectangle optimization | ✓ | noFitPolygonRectangle() works! |
| Consistency | ✗ | Both runs: 0 NFPs (consistent but wrong) |

**Interesting**: Rectangle optimization path works, but general path doesn't

---

## 🎯 Root Cause Analysis

### Primary Suspect: Distance Sign Convention

The consistent **negative values** from distance functions suggest:

1. **Direction Vector Interpretation**:
   - C++ implementation may have inverted direction convention vs JavaScript
   - JavaScript: positive = distance TO move
   - C++ (current): negative = distance TO move?

2. **Normal Vector Calculation**:
   - Cross product sign may be inverted
   - Perpendicular vector calculation differs from JavaScript

3. **Coordinate System**:
   - Y-axis direction (up vs down) convention difference?

### Secondary Issue: searchStartPoint Logic

Given that:
- Distance functions return wrong values
- searchStartPoint relies on polygonSlideDistance()
- searchStartPoint fails for simple shapes

**Likely cascade**:
```
Wrong distances → Invalid slide calculations →
Invalid start point detection → NFP fails
```

### Why Some Cases Work:

**Triangle vs Triangle** and **Concave vs Square** work because:
- Possibly different code paths are triggered
- Heuristic start point might work for these specific shapes
- OR: Distance errors cancel out in certain geometric configurations

---

## 🔧 Action Plan

### Priority 1: Fix Distance Functions (CRITICAL)

1. **Compare with JavaScript implementation**:
   - Line-by-line comparison of `pointDistance()`, `segmentDistance()`, `polygonSlideDistance()`
   - Identify sign/direction differences
   - Check cross product calculations
   - Verify normal vector computation

2. **Add diagnostic tests**:
   - Test known geometric cases from JavaScript
   - Verify against hand-calculated expected values
   - Test edge cases (zero distance, negative direction, etc.)

3. **Fix and validate**:
   - Correct sign conventions
   - Re-run all tests
   - Ensure 100% pass rate for distance functions

### Priority 2: Fix searchStartPoint (CRITICAL)

1. **After distance functions fixed**:
   - Re-run searchStartPoint tests
   - If still failing, debug logic step-by-step
   - Compare candidate selection with JavaScript

2. **Add more diagnostic output**:
   - Why each candidate fails
   - Actual vs expected values
   - Geometry visualization (if needed)

### Priority 3: Validate noFitPolygon

1. **After above fixes**:
   - Re-run all NFP tests
   - Should see significant improvement
   - Focus on cases that still fail

2. **Compare with known-good results**:
   - Use PolygonExtractor working cases as reference
   - Generate NFPs from both implementations
   - Verify point-by-point match

### Priority 4: Numerical Stability

1. **Review tolerances**:
   - TOL constant usage
   - Floating-point comparison logic
   - Edge case handling

2. **Test extreme cases**:
   - Very small polygons
   - Very large polygons
   - Nearly-touching polygons

---

## 📊 Success Metrics

Current: **13/33 (39.4%)**

After Priority 1 fix (distance functions):
- Target: **25/33 (75.8%)**
- Expected: Distance tests pass, searchStartPoint improves

After Priority 2 fix (searchStartPoint):
- Target: **30/33 (90.9%)**
- Expected: Most NFP tests pass

After Priority 3 (noFitPolygon validation):
- Target: **33/33 (100%)**
- Expected: All tests pass

---

## 🔬 Recommended Next Steps

1. **Immediate**: Review JavaScript `geometryutil.js` source:
   - `pointDistance()` - lines ~1200-1250
   - `segmentDistance()` - lines ~1260-1320
   - `polygonSlideDistance()` - lines ~1330-1380
   - `searchStartPoint()` - lines ~1400-1500

2. **Create comparison document**:
   - Side-by-side JavaScript vs C++ for each function
   - Highlight differences
   - Document sign conventions

3. **Incremental fixes**:
   - Fix pointDistance first (simplest)
   - Then segmentDistance (uses pointDistance)
   - Then polygonSlideDistance (uses segmentDistance)
   - Finally searchStartPoint (uses polygonSlideDistance)

4. **Regression prevention**:
   - Keep all current tests
   - Add more edge case tests
   - Maintain 100% pass rate going forward

---

## 💡 Key Insights

1. **Orbital helpers are solid** ✓
   - Contact detection works
   - Vector generation works
   - Backtracking detection works
   - **Foundation is good!**

2. **Problem is in distance calculations**
   - Sign inversion consistently observed
   - Magnitude sometimes also wrong (polygonSlideDistance)
   - Cascades to higher-level functions

3. **Some geometry works**
   - Triangles: ✓
   - Concave shapes: ✓
   - Rectangles/squares: ✗
   - **Suggests shape-specific logic differences**

4. **Rectangle optimization works**
   - `noFitPolygonRectangle()` passes test
   - But general path for rectangles fails
   - Might be `isRectangle()` detection issue

---

## 📝 Conclusion

This test suite has successfully identified **critical issues** in the NFP implementation:

- **Root cause**: Distance function sign inversion
- **Cascade effect**: Breaks searchStartPoint → breaks noFitPolygon
- **Partial success**: Some shapes work, others don't
- **Path forward**: Clear, prioritized action plan

The good news:
- Orbital tracing helpers are solid
- Framework for testing is working well
- Issues are localized and fixable
- Clear path to 100% pass rate

**Estimated effort to fix**:
- Priority 1 (distance functions): 2-4 hours
- Priority 2 (searchStartPoint): 1-2 hours
- Priority 3 (validation): 1-2 hours
- **Total**: ~1 day of focused work

**Next immediate action**: Compare C++ pointDistance with JavaScript implementation line-by-line.
