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
