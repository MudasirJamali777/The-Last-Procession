#pragma once
#include <raylib.h>
#include <cmath>

inline float ClampFloat(float value, float minValue, float maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

inline Vector3 Vec3Add(Vector3 a, Vector3 b) {
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

inline Vector3 Vec3Sub(Vector3 a, Vector3 b) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

inline Vector3 Vec3Scale(Vector3 v, float s) {
    return { v.x * s, v.y * s, v.z * s };
}

inline float LengthXZ(Vector3 v) {
    return std::sqrt(v.x * v.x + v.z * v.z);
}

inline Vector3 NormalizeXZ(Vector3 v) {
    float len = LengthXZ(v);
    if (len <= 0.0001f) return { 0.0f, 0.0f, 0.0f };
    return { v.x / len, 0.0f, v.z / len };
}

inline float DistanceXZ(Vector3 a, Vector3 b) {
    return LengthXZ(Vec3Sub(a, b));
}

inline Color Tint(Color c, float mul) {
    int r = (int)(c.r * mul);
    int g = (int)(c.g * mul);
    int b = (int)(c.b * mul);
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    return { (unsigned char)r, (unsigned char)g, (unsigned char)b, c.a };
}
