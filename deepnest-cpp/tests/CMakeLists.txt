# CMakeLists.txt for DeepNest C++ Test Tools
cmake_minimum_required(VERSION 3.10)

# Enable Qt MOC processing
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt5 - Required for test tools
find_package(Qt5 REQUIRED COMPONENTS Core Xml Widgets Svg)

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

# ===== FitnessTests =====
add_executable(FitnessTests
    FitnessTests.cpp
)

target_include_directories(FitnessTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Clipper2Lib/include
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(FitnessTests
    deepnest
    ${Boost_LIBRARIES}
    pthread
)

target_compile_features(FitnessTests PRIVATE cxx_std_17)

set_target_properties(FitnessTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS FitnessTests
    RUNTIME DESTINATION bin
)

message(STATUS "FitnessTests configured")

# ===== JSComparisonTests =====
add_executable(JSComparisonTests
    JSComparisonTests.cpp
)

target_include_directories(JSComparisonTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Clipper2Lib/include
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(JSComparisonTests
    deepnest
    ${Boost_LIBRARIES}
    pthread
)

target_compile_features(JSComparisonTests PRIVATE cxx_std_17)

set_target_properties(JSComparisonTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS JSComparisonTests
    RUNTIME DESTINATION bin
)

message(STATUS "JSComparisonTests configured")

# ===== StepVerificationTests =====
add_executable(StepVerificationTests
    StepVerificationTests.cpp
)

target_include_directories(StepVerificationTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Clipper2Lib/include
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(StepVerificationTests
    deepnest
    ${Boost_LIBRARIES}
    pthread
)

target_compile_features(StepVerificationTests PRIVATE cxx_std_17)

set_target_properties(StepVerificationTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS StepVerificationTests
    RUNTIME DESTINATION bin
)

message(STATUS "StepVerificationTests configured")

# ===== DiagnosticTool =====
add_executable(DiagnosticTool
    DiagnosticTool.cpp
    SVGLoader.cpp
    SVGLoader.h
)

target_include_directories(DiagnosticTool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Clipper2Lib/include
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(DiagnosticTool
    deepnest
    Qt5::Core
    Qt5::Xml
    Qt5::Widgets
    ${Boost_LIBRARIES}
    pthread
)

target_compile_features(DiagnosticTool PRIVATE cxx_std_17)

set_target_properties(DiagnosticTool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS DiagnosticTool
    RUNTIME DESTINATION bin
)

message(STATUS "DiagnosticTool configured")

# ===== TestApplication (GUI Test Tool) =====
add_executable(TestApplication
    main.cpp
    TestApplication.cpp
    TestApplication.h
    ConfigDialog.cpp
    ConfigDialog.h
    ContainerDialog.cpp
    ContainerDialog.h
    RandomShapeGenerator.cpp
    RandomShapeGenerator.h
    SVGLoader.cpp
    SVGLoader.h
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
    Qt5::Svg
    ${Boost_LIBRARIES}
    pthread
)

target_compile_features(TestApplication PRIVATE cxx_std_17)

set_target_properties(TestApplication PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS TestApplication
    RUNTIME DESTINATION bin
)

message(STATUS "TestApplication configured")
