#include "../../include/deepnest/geometry/PolygonHierarchy.h"
#include "../../include/deepnest/geometry/GeometryUtil.h"
#include <algorithm>
#include <iostream>

namespace deepnest {

int PolygonHierarchy::buildTreeRecursive(std::vector<Polygon>& list, int id) {
    // JavaScript: var parents = [];
    std::vector<Polygon*> parents;
    
    // JavaScript: for(i=0; i<list.length; i++)
    // Identify which polygons are children and which are parents
    for (size_t i = 0; i < list.size(); ++i) {
        Polygon& p = list[i];
        bool isChild = false;
        
        // JavaScript: for(j=0; j<list.length; j++)
        // Check if p is inside any other polygon
        for (size_t j = 0; j < list.size(); ++j) {
            if (i == j) continue;
            
            // JavaScript: if(GeometryUtil.pointInPolygon(p[0], list[j]) === true)
            // Check if first point of p is inside list[j]
            if (!p.points.empty() && !list[j].points.empty()) {
                auto result = GeometryUtil::pointInPolygon(p.points[0], list[j].points);
                
                // Only consider definite containment (not on edge)
                if (result.has_value() && result.value() == true) {
                    // JavaScript: if(!list[j].children) { list[j].children = []; }
                    //             list[j].children.push(p);
                    //             p.parent = list[j];
                    //             ischild = true;
                    list[j].children.push_back(p);
                    isChild = true;
                    break;
                }
            }
        }
        
        // JavaScript: if(!ischild) { parents.push(p); }
        if (!isChild) {
            parents.push_back(&p);
        }
    }
    
    // JavaScript: for(i=0; i<list.length; i++)
    //             if(parents.indexOf(list[i]) < 0) {
    //               list.splice(i, 1);
    //               i--;
    //             }
    // Remove children from main list (keep only parents)
    list.erase(
        std::remove_if(list.begin(), list.end(),
            [&parents](const Polygon& poly) {
                // Check if this polygon is NOT in parents list
                return std::find_if(parents.begin(), parents.end(),
                    [&poly](const Polygon* p) {
                        return p->points == poly.points; // Compare by points
                    }) == parents.end();
            }),
        list.end()
    );
    
    // JavaScript: for(i=0; i<parents.length; i++)
    //             parents[i].id = id;
    //             id++;
    // Assign IDs to parent polygons
    for (Polygon* parent : parents) {
        // Find the polygon in the list and assign ID
        for (Polygon& poly : list) {
            if (poly.points == parent->points) {
                poly.id = id++;
                break;
            }
        }
    }
    
    // JavaScript: for(i=0; i<parents.length; i++)
    //             if(parents[i].children) {
    //               id = toTree(parents[i].children, id);
    //             }
    // Recursively process children
    for (Polygon& poly : list) {
        if (!poly.children.empty()) {
            id = buildTreeRecursive(poly.children, id);
        }
    }
    
    // JavaScript: return id;
    return id;
}

std::vector<Polygon> PolygonHierarchy::buildTree(
    std::vector<Polygon>& polygons,
    int idStart
) {
    if (polygons.empty()) {
        return {};
    }
    
    // Call recursive function
    buildTreeRecursive(polygons, idStart);
    
    // polygons now contains only top-level polygons with children nested
    return polygons;
}

} // namespace deepnest
