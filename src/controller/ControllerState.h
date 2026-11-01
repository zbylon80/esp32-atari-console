#pragma once

#include <cstdint>

namespace esp32controller {
enum class Button : uint16_t {
    A = 1u << 0, B = 1u << 1, X = 1u << 2, Y = 1u << 3,
    Up = 1u << 4, Down = 1u << 5, Left = 1u << 6, Right = 1u << 7,
    LB = 1u << 8, RB = 1u << 9, Menu = 1u << 10, View = 1u << 11,
    L3 = 1u << 12, R3 = 1u << 13
};
using ButtonMask = uint16_t;
constexpr ButtonMask mask(Button button) { return static_cast<ButtonMask>(button); }
constexpr ButtonMask kAllButtons = (1u << 14) - 1;

enum class Axis : uint8_t { LX = 1, LY = 2, RX = 4, RY = 8, LT = 16, RT = 32 };
constexpr uint8_t kAllAxes = 63;
enum class Capability { Unknown, Unavailable, Available };
enum class CapabilitySource { Unknown, DocumentedXboxProfile };

struct ControllerCapabilities {
    ButtonMask knownButtons = 0;
    ButtonMask availableButtons = 0;
    uint8_t knownAxes = 0;
    uint8_t availableAxes = 0;
    CapabilitySource source = CapabilitySource::Unknown;

    Capability button(Button value) const {
        return !(knownButtons & mask(value)) ? Capability::Unknown :
            (availableButtons & mask(value)) ? Capability::Available : Capability::Unavailable;
    }
    Capability axis(Axis value) const {
        const auto bit = static_cast<uint8_t>(value);
        return !(knownAxes & bit) ? Capability::Unknown :
            (availableAxes & bit) ? Capability::Available : Capability::Unavailable;
    }
};

struct ControllerIdentity {
    char modelName[40] = {};
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    int32_t backendModel = 0;
    // Exact hardware revision and firmware are not inferred from this metadata.
};

struct AnalogState {
    float lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
};
enum class ConnectionStatus {
    NotStarted, Starting, Waiting, ConnectedAwaitingInput, Active, Disconnected, Error
};
enum class ControllerError { None, InvalidConfiguration, BackendUnavailable };

struct ControllerState {
    bool connected = false;
    bool inputValid = false;
    ButtonMask held = 0;
    ButtonMask pressed = 0;
    ButtonMask released = 0;
    AnalogState analog;
    ConnectionStatus status = ConnectionStatus::NotStarted;
    ControllerError error = ControllerError::None;
    uint32_t session = 0;
    uint32_t sequence = 0;
    uint32_t reportSequence = 0;
    uint32_t lastReportMs = 0;

    bool isHeld(Button value) const { return (held & mask(value)) != 0; }
    bool wasPressed(Button value) const { return (pressed & mask(value)) != 0; }
    bool wasReleased(Button value) const { return (released & mask(value)) != 0; }
    uint32_t reportAgeMs(uint32_t now) const { return now - lastReportMs; }
};

struct ControllerChanges {
    bool connectionChanged = false;
    bool statusChanged = false;
    bool reportReceived = false;
    bool inputChanged = false;
    bool unsupportedInput = false;
};
}  // namespace esp32controller
