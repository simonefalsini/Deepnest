##############################################
# DeepNest C++ - Complete Build
##############################################
#
# This is the main project file that builds:
# - deepnest library (core nesting engine)
# - TestApplication (Qt GUI test application)
# - StepVerificationTests (automated test suite)
#
# Usage:
#   qmake deepnest-all.pro
#   make
#
# This will build all three targets in the correct order.
##############################################

TEMPLATE = subdirs
CONFIG += ordered

# Build order:
# 1. Library first (required by tests)
# 2. Test applications (depend on library)

SUBDIRS = \
    library \
    test_app \
    consolidated_tests \
    polygon_extractor

# Library configuration
library.file = deepnest.pro

# Test application configuration
test_app.file = tests/TestApplication.pro
test_app.depends = library

# Consolidated tests configuration
consolidated_tests.file = tests/DeepnestLibTests.pro
consolidated_tests.depends = library

# Polygon extractor utility
polygon_extractor.file = tests/PolygonExtractor.pro
polygon_extractor.depends = library
