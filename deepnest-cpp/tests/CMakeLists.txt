# CMakeLists.txt for DeepNest C++ Test Tools
cmake_minimum_required(VERSION 3.10)

# Find Qt5 - Required for test tools
find_package(Qt5 REQUIRED COMPONENTS Core Xml Widgets)

# ===== PolygonExtractor Test Tool =====
add_executable(PolygonExtractor
    PolygonExtractor.cpp
    SVGLoader.cpp
    SVGLoader.h
)

target_include_directories(PolygonExtractor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Clipper2Lib/include
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(PolygonExtractor
    deepnest
    Qt5::Core
    Qt5::Xml
    Qt5::Widgets
    ${Boost_LIBRARIES}
    boost_chrono
    pthread
)

# C++17 standard
target_compile_features(PolygonExtractor PRIVATE cxx_std_17)

# Set output directory
set_target_properties(PolygonExtractor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install target
install(TARGETS PolygonExtractor
    RUNTIME DESTINATION bin
)

message(STATUS "PolygonExtractor test tool configured")

# ===== GeometryValidationTests =====
add_executable(GeometryValidationTests
    GeometryValidationTests.cpp
)

target_include_directories(GeometryValidationTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Clipper2Lib/include
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(GeometryValidationTests
    deepnest
    ${Boost_LIBRARIES}
    pthread
)

# C++17 standard
target_compile_features(GeometryValidationTests PRIVATE cxx_std_17)

# Set output directory
set_target_properties(GeometryValidationTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install target
install(TARGETS GeometryValidationTests
    RUNTIME DESTINATION bin
)

message(STATUS "GeometryValidationTests configured")

# ===== NFPValidationTests =====
add_executable(NFPValidationTests
    NFPValidationTests.cpp
)

target_include_directories(NFPValidationTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Clipper2Lib/include
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(NFPValidationTests
    deepnest
    ${Boost_LIBRARIES}
    pthread
)

# C++17 standard
target_compile_features(NFPValidationTests PRIVATE cxx_std_17)

# Set output directory
set_target_properties(NFPValidationTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install target
install(TARGETS NFPValidationTests
    RUNTIME DESTINATION bin
)

message(STATUS "NFPValidationTests configured")

# ===== SearchStartPointDebug =====
add_executable(SearchStartPointDebug
    SearchStartPointDebug.cpp
)

target_include_directories(SearchStartPointDebug PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Clipper2Lib/include
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(SearchStartPointDebug
    deepnest
    ${Boost_LIBRARIES}
    pthread
)

# C++17 standard
target_compile_features(SearchStartPointDebug PRIVATE cxx_std_17)

# Set output directory
set_target_properties(SearchStartPointDebug PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

message(STATUS "SearchStartPointDebug configured")

# ===== NFPDebug =====
add_executable(NFPDebug
    NFPDebug.cpp
)

target_include_directories(NFPDebug PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Clipper2Lib/include
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(NFPDebug
    deepnest
    ${Boost_LIBRARIES}
    pthread
)

# C++17 standard
target_compile_features(NFPDebug PRIVATE cxx_std_17)

# Set output directory
set_target_properties(NFPDebug PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

message(STATUS "NFPDebug configured")

# ===== TestApplication (GUI) =====
add_executable(TestApplication
    main.cpp
    TestApplication.cpp
    SVGLoader.cpp
    ConfigDialog.cpp
    ContainerDialog.cpp
)

target_include_directories(TestApplication PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Clipper2Lib/include
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(TestApplication
    deepnest
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Xml
    ${Boost_LIBRARIES}
    pthread
)

target_compile_features(TestApplication PRIVATE cxx_std_17)

set_target_properties(TestApplication PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS TestApplication RUNTIME DESTINATION bin)

message(STATUS "TestApplication configured")

# ===== DeepnestLibTests (Suite) =====
add_executable(DeepnestLibTests
    DeepnestLibTests.cpp
    FitnessTests.cpp
    GeneticAlgorithmTests.cpp
    JSComparisonTests.cpp
    StepVerificationTests.cpp
    SVGLoader.cpp
    RandomShapeGenerator.cpp
)

target_include_directories(DeepnestLibTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Clipper2Lib/include
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(DeepnestLibTests
    deepnest
    Qt5::Core
    Qt5::Xml
    Qt5::Widgets
    ${Boost_LIBRARIES}
    pthread
)

target_compile_features(DeepnestLibTests PRIVATE cxx_std_17)

set_target_properties(DeepnestLibTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS DeepnestLibTests RUNTIME DESTINATION bin)

message(STATUS "DeepnestLibTests configured")

# ===== PolygonHierarchyTest =====
add_executable(PolygonHierarchyTest
    PolygonHierarchyTest.cpp
)

target_include_directories(PolygonHierarchyTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Clipper2Lib/include
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(PolygonHierarchyTest
    deepnest
    ${Boost_LIBRARIES}
    pthread
)

target_compile_features(PolygonHierarchyTest PRIVATE cxx_std_17)

set_target_properties(PolygonHierarchyTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS PolygonHierarchyTest RUNTIME DESTINATION bin)

message(STATUS "PolygonHierarchyTest configured")

# ===== OverlapDetectionTest =====
add_executable(OverlapDetectionTest
    OverlapDetectionTest.cpp
)

target_include_directories(OverlapDetectionTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Clipper2Lib/include
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(OverlapDetectionTest
    deepnest
    ${Boost_LIBRARIES}
    pthread
)

target_compile_features(OverlapDetectionTest PRIVATE cxx_std_17)

set_target_properties(OverlapDetectionTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS OverlapDetectionTest RUNTIME DESTINATION bin)

message(STATUS "OverlapDetectionTest configured")

# ===== MinkowskiSumComparisonTest =====
add_executable(MinkowskiSumComparisonTest
    MinkowskiSumComparisonTest.cpp
)

target_include_directories(MinkowskiSumComparisonTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Clipper2Lib/include
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(MinkowskiSumComparisonTest
    deepnest
    ${Boost_LIBRARIES}
    pthread
)

target_compile_features(MinkowskiSumComparisonTest PRIVATE cxx_std_17)

set_target_properties(MinkowskiSumComparisonTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS MinkowskiSumComparisonTest RUNTIME DESTINATION bin)

message(STATUS "MinkowskiSumComparisonTest configured")

# ===== DiagnosticTool =====
add_executable(DiagnosticTool
    DiagnosticTool.cpp
)

target_include_directories(DiagnosticTool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Clipper2Lib/include
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(DiagnosticTool
    deepnest
    ${Boost_LIBRARIES}
    pthread
)

target_compile_features(DiagnosticTool PRIVATE cxx_std_17)

set_target_properties(DiagnosticTool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS DiagnosticTool RUNTIME DESTINATION bin)

message(STATUS "DiagnosticTool configured")
