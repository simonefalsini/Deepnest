# Problematic Polygons - Debug Report

## Summary

When testing with `test__SVGnest-output_clean.svg` (154 parts), the nesting process encounters:
- **16+ polygon pairs** that fail both Minkowski sum AND orbital tracing NFP calculation
- **Segmentation fault** when trying to use these failed NFP calculations

## Status

✅ **Fixed**:
- Implemented Ramer-Douglas-Peucker simplification (tolerance=2.0)
- Added command-line testing support
- Removed bounding box approximation fallback (was causing crashes)
- Added safety checks and logging in NFPCalculator
- Force single-thread mode for automatic tests

❌ **Still Crashing**:
- Application crashes when processing polygons with failed NFPs
- Crash occurs AFTER NFP calculation, likely in PlacementWorker or PolygonOperations::unionPolygons

## Problematic Polygon Pairs

From test run, these pairs consistently fail:

| Polygon A (ID) | Polygon B (ID) | A Points | B Points |
|----------------|----------------|----------|----------|
| 45             | 55             | 9        | 8        |
| 57             | 58             | 8        | 8        |
| 47             | 64             | 8        | 4        |
| 30             | 68             | 14       | 5        |
| 57             | 69             | 8        | 5        |
| 45             | 70             | 9        | 17       |
| 67             | 113            | 5        | 7        |
| 110            | 112            | 7        | 7        |
| 65             | 115            | 4        | 7        |
| 95             | 117            | 5        | 7        |
| 49             | 118            | 8        | 7        |
| 103            | 119            | 7        | 7        |
| 94             | 120            | 5        | 7        |
| 86             | 127            | 5        | 5        |
| 32             | 130            | 5        | 5        |
| 19             | 131            | 6        | 5        |
| 39             | 60             | 13       | 8        |

## Root Cause

1. **Complex geometries**: These polygon pairs have geometries that:
   - Cause Boost.Polygon Minkowski sum to fail (likely due to degenerate points or self-intersections)
   - Also cause orbital tracing algorithm to fail

2. **Empty NFP handling**: When NFP is empty, the code path leads to:
   - Line 284 in PlacementWorker.cpp: `getOuterNFP()` returns empty Polygon
   - Line 286-289: Check for empty catches it, sets error=true, breaks loop
   - BUT: Segfault occurs before or after this check, possibly in:
     - `unionPolygons()` at line 326
     - Polygon operations that don't handle empty polygons
     - Memory access in placement calculations

## Diagnostic Steps Taken

1. ✅ Tested with single thread (NOT a threading issue)
2. ✅ Added extensive logging to NFPCalculator
3. ✅ Removed bounding box fallback
4. ✅ Added safety checks in `getInnerNFP`
5. ❌ Need GDB backtrace to find exact crash location

## Next Steps

### Immediate (Debug Crash)
1. Build with debug symbols: `qmake CONFIG+=debug && make`
2. Run in GDB to get backtrace of crash
3. Add null/empty checks in:
   - `PolygonOperations::unionPolygons()`
   - `PlacementWorker::placeParts()` before union operations
   - Any Polygon method that accesses `.points[0]` without checking size

### Medium Term (Fix Geometry Issues)
1. Investigate WHY these specific pairs fail:
   - Export individual problematic polygons to SVG
   - Visualize in QGIS or similar
   - Check for self-intersections, degenerate segments

2. Improve polygon simplification:
   - Increase tolerance above 2.0 for problematic cases
   - Add topology validation before NFP calculation
   - Consider using Clipper2 for pre-processing

3. Implement robust fallback:
   - Instead of returning empty, return conservative bounding box
   - Or skip collision detection for this pair (allow potential overlap)
   - Or mark these parts as "problematic" and place them last

### Long Term
1. Port orbital tracing algorithm improvements from DeepNest Java
2. Consider alternative NFP algorithms for complex geometries
3. Add polygon validation and repair in SVG loader

## Test Commands

```bash
# Run automatic test (will crash)
cd deepnest-cpp/bin
xvfb-run -a ./TestApplication --test ../tests/testdata/test__SVGnest-output_clean.svg --generations 5

# Run with GDB (when built with debug symbols)
xvfb-run -a gdb ./TestApplication
> run --test ../tests/testdata/test__SVGnest-output_clean.svg --generations 1
> bt

# Extract problematic polygon IDs
grep "Polygon.*id=" stderr.log | sort -u
```

## Files Modified

- `deepnest-cpp/src/nfp/NFPCalculator.cpp`: Removed bounding box fallback, added logging
- `deepnest-cpp/src/geometry/GeometryUtil.cpp`: Added R-D-P simplification
- `deepnest-cpp/src/core/Polygon.cpp`: Use simplifyPolygon() with tolerance=2.0
- `deepnest-cpp/tests/TestApplication.cpp`: Added automatic testing, force single thread
- `deepnest-cpp/tests/main.cpp`: Command-line argument parsing

## References

- Original JavaScript simplify.js: Uses R-D-P with tolerance=2
- SVG file: `deepnest-cpp/tests/testdata/test__SVGnest-output_clean.svg`
- 154 parts total, ~16 pairs (~10%) fail NFP calculation
