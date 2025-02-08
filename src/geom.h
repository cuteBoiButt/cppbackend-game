#pragma once

#include <compare>
#include <cstddef>
#include <cmath>
#include <functional>

namespace geom {

using Dimension = int;
using Coord = Dimension;

using DimensionD = double;
using CoordD = DimensionD;

struct Point {
    Coord x, y;
    bool operator==(const Point& rhs) const noexcept {
        return x == rhs.x && y == rhs.y;
    }
};

struct PointHasher {
    size_t operator()(const geom::Point& point) const {
        return std::hash<geom::Coord>()(point.x) ^ (std::hash<geom::Coord>()(point.y) << 1);
    }
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

struct Vec2D {
    Vec2D() = default;
    Vec2D(double x, double y)
        : x(x)
        , y(y) {
    }

    Vec2D& operator*=(double scale) {
        x *= scale;
        y *= scale;
        return *this;
    }

    auto operator<=>(const Vec2D&) const = default;

    double x = 0;
    double y = 0;
};

inline Vec2D operator*(Vec2D lhs, double rhs) {
    return lhs *= rhs;
}

inline Vec2D operator*(double lhs, Vec2D rhs) {
    return rhs *= lhs;
}

struct Point2D {
    Point2D() = default;
    Point2D(double x, double y)
        : x(x)
        , y(y) {
    }

    Point2D& operator+=(const Vec2D& rhs) {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    auto operator<=>(const Point2D&) const = default;

    double x = 0;
    double y = 0;
};

inline Point2D operator+(Point2D lhs, const Vec2D& rhs) {
    return lhs += rhs;
}

inline Point2D operator+(const Vec2D& lhs, Point2D rhs) {
    return rhs += lhs;
}

}  // namespace geom
