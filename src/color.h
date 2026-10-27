#ifndef COLOR_H
#define COLOR_H

#include "interval.h"
#include "vec3.h"

using color = vec3;

inline double linear_to_gamma(double linear_component) {
    if (linear_component > 0)
        return std::sqrt(linear_component);

    return 0;
}

void write_color(std::ostream& out, const color& pixel_color) {
    auto r = pixel_color.x();
    auto g = pixel_color.y();
    auto b = pixel_color.z();

    // Apply a linear to gamma transform for gamma 2
    r = linear_to_gamma(r);
    g = linear_to_gamma(g);
    b = linear_to_gamma(b);

    // Translate the [0,1] component values to the byte range [0,255];
    static const interval intensity(0.000, 0.999);
    int rbyte = int(256 * intensity.clamp(r));
    int gbyte = int(256 * intensity.clamp(g));
    int bbyte = int(256 * intensity.clamp(b));

    // Write out the pixel color components.
    out << rbyte << ' ' << gbyte << ' ' << bbyte << '\n';
}

// ŠÈˆÕACESToneMappingƒwƒ‹ƒp[
inline color tone_map_aces(const color& c) {
    static const interval intensity(0.000, 0.999);
    float a = 2.51f;
    float b = 0.03f;
    float c_val = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return color(
        intensity.clamp((c.x() * (a * c.x() + b)) / (c.x() * (c_val * c.x() + d) + e)),
        intensity.clamp((c.y() * (a * c.y() + b)) / (c.y() * (c_val * c.y() + d) + e)),
        intensity.clamp((c.z() * (a * c.z() + b)) / (c.z() * (c_val * c.z() + d) + e))
    );
}
#endif
