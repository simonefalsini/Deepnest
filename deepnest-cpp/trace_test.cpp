#include <iostream>
#include <cmath>

struct Point {
    double x, y;
    Point(double x=0, double y=0) : x(x), y(y) {}
};

struct BBox {
    double x, y, width, height;
};

// Simulate rotation around origin
Point rotate(Point p, double degrees) {
    double angle = degrees * M_PI / 180.0;
    double x1 = p.x * cos(angle) - p.y * sin(angle);
    double y1 = p.x * sin(angle) + p.y * cos(angle);
    return Point(x1, y1);
}

int main() {
    // Rectangle 128.2 x 83.1 at origin
    Point pts[4] = {
        Point(0, 0),
        Point(128.2, 0),
        Point(128.2, 83.1),
        Point(0, 83.1)
    };
    
    std::cout << "=== ORIGINAL RECTANGLE ===" << std::endl;
    std::cout << "Dimensions: 128.2 x 83.1" << std::endl;
    for (int i = 0; i < 4; i++) {
        std::cout << "Point[" << i << "]: (" << pts[i].x << ", " << pts[i].y << ")" << std::endl;
    }
    
    // Rotate 90 degrees
    std::cout << "\n=== AFTER 90° ROTATION ===" << std::endl;
    Point rotated[4];
    for (int i = 0; i < 4; i++) {
        rotated[i] = rotate(pts[i], 90.0);
        std::cout << "Point[" << i << "]: (" << rotated[i].x << ", " << rotated[i].y << ")" << std::endl;
    }
    
    // Calculate bounding box
    double minX = rotated[0].x, maxX = rotated[0].x;
    double minY = rotated[0].y, maxY = rotated[0].y;
    for (int i = 1; i < 4; i++) {
        if (rotated[i].x < minX) minX = rotated[i].x;
        if (rotated[i].x > maxX) maxX = rotated[i].x;
        if (rotated[i].y < minY) minY = rotated[i].y;
        if (rotated[i].y > maxY) maxY = rotated[i].y;
    }
    
    std::cout << "\nBounding box:" << std::endl;
    std::cout << "  min: (" << minX << ", " << minY << ")" << std::endl;
    std::cout << "  max: (" << maxX << ", " << maxY << ")" << std::endl;
    std::cout << "  size: " << (maxX - minX) << " x " << (maxY - minY) << std::endl;
    
    // Normalize to origin (C++ approach)
    std::cout << "\n=== AFTER NORMALIZATION (C++ approach) ===" << std::endl;
    Point normalized[4];
    for (int i = 0; i < 4; i++) {
        normalized[i].x = rotated[i].x - minX;
        normalized[i].y = rotated[i].y - minY;
        std::cout << "Point[" << i << "]: (" << normalized[i].x << ", " << normalized[i].y << ")" << std::endl;
    }
    std::cout << "points[0] = (" << normalized[0].x << ", " << normalized[0].y << ")" << std::endl;
    
    // What JavaScript does (no normalization)
    std::cout << "\n=== JAVASCRIPT APPROACH (no normalization) ===" << std::endl;
    std::cout << "points[0] = (" << rotated[0].x << ", " << rotated[0].y << ")" << std::endl;
    
    std::cout << "\n=== THE BUG ===" << std::endl;
    std::cout << "If innerNFP returns point (0, 0) for top-left of sheet:" << std::endl;
    std::cout << "  C++ calculates: (0, 0) - points[0] = (0, 0) - (" << normalized[0].x << ", " << normalized[0].y << ") = (" << -normalized[0].x << ", " << -normalized[0].y << ")" << std::endl;
    std::cout << "  This is WRONG! Should be (0, 0)" << std::endl;
    std::cout << "\nThe issue: After normalization, points[0] is NOT at origin!" << std::endl;
    std::cout << "  Bounding box min is at origin, but points[0] is at (" << normalized[0].x << ", " << normalized[0].y << ")" << std::endl;
    
    return 0;
}
