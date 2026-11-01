#include "InputNormalization.h"
#include <cmath>

namespace esp32controller {
namespace {
bool validDeadZone(float value) { return std::isfinite(value) && value >= 0 && value < 1; }
float clamp(float value, float low, float high) {
    return value < low ? low : value > high ? high : value;
}
bool changed(float current, float previous, float delta) {
    // Returning to neutral must be published even when smaller than delta.
    return (current == 0 && previous != 0) ||
        (current != previous && std::fabs(current - previous) >= delta);
}
}
bool validConfig(const ControllerConfig& c) {
    return validDeadZone(c.leftStickDeadZone) && validDeadZone(c.rightStickDeadZone) &&
        validDeadZone(c.leftTriggerDeadZone) && validDeadZone(c.rightTriggerDeadZone) &&
        std::isfinite(c.analogEventThreshold) && c.analogEventThreshold >= 0 && c.analogEventThreshold <= 1;
}
float normalizeAxis(int32_t value) {
    // Pinned Bluepad32 parser and measured controller endpoints: -512 .. 511.
    return clamp(static_cast<float>(value) / (value < 0 ? 512.0f : 511.0f), -1, 1);
}
float normalizeTrigger(int32_t value, float deadZone) {
    if (!validDeadZone(deadZone)) return 0;
    const float normalized = clamp(static_cast<float>(value) / 1023.0f, 0, 1);
    return normalized <= deadZone ? 0 : (normalized - deadZone) / (1 - deadZone);
}
Stick normalizeStick(int32_t x, int32_t y, float deadZone, bool invertY) {
    Stick result;
    if (!validDeadZone(deadZone)) return result;
    const float nx = normalizeAxis(x);
    const float ny = normalizeAxis(y) * (invertY ? -1.0f : 1.0f);
    const float radius = std::sqrt(nx * nx + ny * ny);
    if (radius <= deadZone || radius == 0) return result;
    const float magnitude = (clamp(radius, 0, 1) - deadZone) / (1 - deadZone);
    result.x = nx / radius * magnitude;
    result.y = ny / radius * magnitude;
    return result;
}
bool analogChanged(const AnalogState& a, const AnalogState& b, float delta) {
    return changed(a.lx,b.lx,delta) || changed(a.ly,b.ly,delta) ||
        changed(a.rx,b.rx,delta) || changed(a.ry,b.ry,delta) ||
        changed(a.lt,b.lt,delta) || changed(a.rt,b.rt,delta);
}
}  // namespace esp32controller
