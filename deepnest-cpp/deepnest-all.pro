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
    verification_tests

# Library configuration
library.file = deepnest.pro

# Test application configuration
test_app.file = tests/TestApplication.pro
test_app.depends = library

# Verification tests configuration
verification_tests.file = tests/StepVerificationTests.pro
verification_tests.depends = library
