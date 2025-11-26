/**
 * @file PolygonHierarchyTest.cpp
 * @brief Test program for PolygonHierarchy::buildTree functionality
 *
 * This test verifies that the buildTree() function correctly constructs
 * parent-child relationships between polygons based on containment.
 */

#include "../include/deepnest/geometry/PolygonHierarchy.h"
#include "../include/deepnest/core/Polygon.h"
#include <iostream>
#include <vector>

using namespace deepnest;

void printPolygonTree(const Polygon& poly, int indent = 0) {
    std::string indentStr(indent * 2, ' ');
    std::cout << indentStr << "Polygon ID=" << poly.id 
              << ", Points=" << poly.points.size()
              << ", Children=" << poly.children.size() << std::endl;
    
    for (const auto& child : poly.children) {
        printPolygonTree(child, indent + 1);
    }
}

int main() {
    std::cout << "=== PolygonHierarchy::buildTree Test ===" << std::endl;
    
    // Test 1: Simple case - outer polygon with one hole
    {
        std::cout << "\nTest 1: Outer polygon with one hole" << std::endl;
        std::vector<Polygon> polygons;
        
        // Outer polygon (square 0,0 to 100,100)
        Polygon outer;
        outer.points = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
        polygons.push_back(outer);
        
        // Inner polygon (hole, square 20,20 to 80,80)
        Polygon inner;
        inner.points = {{20, 20}, {80, 20}, {80, 80}, {20, 80}};
        polygons.push_back(inner);
        
        // Build tree
        std::vector<Polygon> tree = PolygonHierarchy::buildTree(polygons);
        
        std::cout << "Result: " << tree.size() << " top-level polygon(s)" << std::endl;
        for (const auto& poly : tree) {
            printPolygonTree(poly);
        }
        
        // Verify
        if (tree.size() == 1 && tree[0].children.size() == 1) {
            std::cout << "✅ Test 1 PASSED" << std::endl;
        } else {
            std::cout << "❌ Test 1 FAILED" << std::endl;
        }
    }
    
    // Test 2: Multiple top-level polygons
    {
        std::cout << "\nTest 2: Multiple top-level polygons" << std::endl;
        std::vector<Polygon> polygons;
        
        // First polygon
        Polygon poly1;
        poly1.points = {{0, 0}, {50, 0}, {50, 50}, {0, 50}};
        polygons.push_back(poly1);
        
        // Second polygon (separate, not contained)
        Polygon poly2;
        poly2.points = {{100, 0}, {150, 0}, {150, 50}, {100, 50}};
        polygons.push_back(poly2);
        
        // Build tree
        std::vector<Polygon> tree = PolygonHierarchy::buildTree(polygons);
        
        std::cout << "Result: " << tree.size() << " top-level polygon(s)" << std::endl;
        for (const auto& poly : tree) {
            printPolygonTree(poly);
        }
        
        // Verify
        if (tree.size() == 2) {
            std::cout << "✅ Test 2 PASSED" << std::endl;
        } else {
            std::cout << "❌ Test 2 FAILED" << std::endl;
        }
    }
    
    // Test 3: Nested hierarchy (polygon with hole, hole has another polygon inside)
    {
        std::cout << "\nTest 3: Nested hierarchy" << std::endl;
        std::vector<Polygon> polygons;
        
        // Outer polygon (0,0 to 100,100)
        Polygon outer;
        outer.points = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
        polygons.push_back(outer);
        
        // Middle polygon (hole, 10,10 to 90,90)
        Polygon middle;
        middle.points = {{10, 10}, {90, 10}, {90, 90}, {10, 90}};
        polygons.push_back(middle);
        
        // Inner polygon (inside middle, 30,30 to 70,70)
        Polygon inner;
        inner.points = {{30, 30}, {70, 30}, {70, 70}, {30, 70}};
        polygons.push_back(inner);
        
        // Build tree
        std::vector<Polygon> tree = PolygonHierarchy::buildTree(polygons);
        
        std::cout << "Result: " << tree.size() << " top-level polygon(s)" << std::endl;
        for (const auto& poly : tree) {
            printPolygonTree(poly);
        }
        
        // Verify: should have 1 top-level with 1 child, and that child has 1 child
        if (tree.size() == 1 && 
            tree[0].children.size() == 1 && 
            tree[0].children[0].children.size() == 1) {
            std::cout << "✅ Test 3 PASSED" << std::endl;
        } else {
            std::cout << "❌ Test 3 FAILED" << std::endl;
        }
    }
    
    // Test 4: ID assignment
    {
        std::cout << "\nTest 4: ID assignment" << std::endl;
        std::vector<Polygon> polygons;
        
        // Create 3 top-level polygons
        for (int i = 0; i < 3; ++i) {
            Polygon poly;
            poly.points = {
                {i * 100.0, 0}, 
                {i * 100.0 + 50, 0}, 
                {i * 100.0 + 50, 50}, 
                {i * 100.0, 50}
            };
            polygons.push_back(poly);
        }
        
        // Build tree with starting ID = 10
        std::vector<Polygon> tree = PolygonHierarchy::buildTree(polygons, 10);
        
        std::cout << "Result IDs: ";
        for (const auto& poly : tree) {
            std::cout << poly.id << " ";
        }
        std::cout << std::endl;
        
        // Verify IDs are 10, 11, 12
        if (tree.size() == 3 && 
            tree[0].id == 10 && 
            tree[1].id == 11 && 
            tree[2].id == 12) {
            std::cout << "✅ Test 4 PASSED" << std::endl;
        } else {
            std::cout << "❌ Test 4 FAILED" << std::endl;
        }
    }
    
    std::cout << "\n=== All Tests Complete ===" << std::endl;
    
    return 0;
}
