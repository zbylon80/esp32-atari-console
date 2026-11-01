#pragma once
#include "../controller/IGameController.h"

class Print;
namespace esp32controller {
struct DiagnosticsConfig {
    float analogDelta = 0.03f;
    uint32_t analogIntervalMs = 100;
};
class SerialDiagnostics {
public:
    explicit SerialDiagnostics(Print& sink, const DiagnosticsConfig& config = DiagnosticsConfig());
    void observe(const IGameController& controller, const ControllerChanges& changes, uint32_t nowMs);
    void snapshot(const IGameController& controller, uint32_t nowMs);
private:
    void line(uint32_t nowMs, const char* format, ...);
    void analogLine(const ControllerState& state, uint32_t nowMs);
    Print& sink_;
    DiagnosticsConfig config_;
    AnalogState loggedAnalog_;
    ConnectionStatus lastStatus_ = ConnectionStatus::NotStarted;
    uint32_t lineSequence_ = 0;
    uint32_t lastAnalogLogMs_ = 0;
    bool previouslyConnected_ = false;
    bool previouslyValid_ = false;
    bool analogLogged_ = false;
};
}  // namespace esp32controller
