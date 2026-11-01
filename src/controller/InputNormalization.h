#pragma once

#include "ControllerState.h"

namespace esp32controller {
struct ControllerConfig {
    float leftStickDeadZone = 0.12f;
    float rightStickDeadZone = 0.12f;
    float leftTriggerDeadZone = 0.02f;
    float rightTriggerDeadZone = 0.02f;
    float analogEventThreshold = 0.01f;
    bool invertLeftY = true;
    bool invertRightY = true;
};
struct Stick { float x = 0, y = 0; };

bool validConfig(const ControllerConfig& config);
float normalizeAxis(int32_t value);
float normalizeTrigger(int32_t value, float deadZone);
Stick normalizeStick(int32_t x, int32_t y, float deadZone, bool invertY);
bool analogChanged(const AnalogState& current, const AnalogState& baseline, float delta);
}  // namespace esp32controller
