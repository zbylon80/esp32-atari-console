#include "SerialDiagnostics.h"
#include "../controller/InputNormalization.h"
#include <Print.h>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace esp32controller {
namespace {
const char* const names[] = { "A", "B", "X", "Y", "Up", "Down", "Left", "Right", "LB", "RB", "Menu", "View", "L3", "R3" };
const char* statusName(ConnectionStatus status) {
    switch (status) {
    case ConnectionStatus::NotStarted: return "not-started";
    case ConnectionStatus::Starting: return "starting";
    case ConnectionStatus::Waiting: return "host-ready/waiting";
    case ConnectionStatus::ConnectedAwaitingInput: return "connected/awaiting-input";
    case ConnectionStatus::Active: return "active";
    case ConnectionStatus::Disconnected: return "disconnected/waiting";
    case ConnectionStatus::Error: return "error";
    }
    return "unknown";
}
}
SerialDiagnostics::SerialDiagnostics(Print& sink, const DiagnosticsConfig& config) : sink_(sink), config_(config) {
    if (!std::isfinite(config_.analogDelta) || config_.analogDelta < 0 || config_.analogDelta > 1)
        config_.analogDelta = DiagnosticsConfig{}.analogDelta;
}
void SerialDiagnostics::line(uint32_t nowMs, const char* format, ...) {
    char output[240];
    const int prefix = std::snprintf(output, sizeof(output), "[DIAG n=%lu t=%lu] ",
        static_cast<unsigned long>(++lineSequence_), static_cast<unsigned long>(nowMs));
    if (prefix < 0 || static_cast<size_t>(prefix) >= sizeof(output) - 2) return;
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(output + prefix, sizeof(output) - prefix - 1, format, arguments);
    va_end(arguments);
    const size_t length = std::strlen(output);
    output[length] = '\n';
    sink_.write(reinterpret_cast<const uint8_t*>(output), length + 1);
}
void SerialDiagnostics::analogLine(const ControllerState& state, uint32_t nowMs) {
    const auto& a = state.analog;
    line(nowMs, "[INPUT] LX=%.3f LY=%.3f RX=%.3f RY=%.3f LT=%.3f RT=%.3f report=%lu",
        static_cast<double>(a.lx), static_cast<double>(a.ly), static_cast<double>(a.rx),
        static_cast<double>(a.ry), static_cast<double>(a.lt), static_cast<double>(a.rt),
        static_cast<unsigned long>(state.reportSequence));
    loggedAnalog_ = a;
    lastAnalogLogMs_ = nowMs;
    analogLogged_ = true;
}
void SerialDiagnostics::observe(const IGameController& controller, const ControllerChanges& changes, uint32_t nowMs) {
    const auto& state = controller.state();
    if (state.status != lastStatus_) {
        line(nowMs, "[BT] status=%s error=%u", statusName(state.status), static_cast<unsigned>(state.error));
        lastStatus_ = state.status;
    }
    if (changes.connectionChanged && state.connected) {
        const auto& info = controller.identity();
        line(nowMs, "[BT] connected session=%lu model=%s VID=%04x PID=%04x",
            static_cast<unsigned long>(state.session), info.modelName, info.vendorId, info.productId);
        const auto& caps = controller.capabilities();
        line(nowMs, "[CAP] source=%s knownButtons=%04x availableButtons=%04x knownAxes=%02x availableAxes=%02x",
            caps.source == CapabilitySource::DocumentedXboxProfile ? "documented-Xbox-profile" : "unknown",
            caps.knownButtons, caps.availableButtons, caps.knownAxes, caps.availableAxes);
        analogLogged_ = false;
        previouslyValid_ = false;
    } else if (previouslyConnected_ && !state.connected) {
        line(nowMs, "[BT] disconnected; held=0 analog=0 inputValid=0; waiting for reconnection");
        loggedAnalog_ = AnalogState{};
        analogLogged_ = false;
    }
    if (changes.unsupportedInput)
        line(nowMs, "[WARN] Non-gamepad report; input invalidated (capability metadata does not override report type)");
    for (unsigned i = 0; i < 14; ++i) {
        if (state.released & (1u << i))
            line(nowMs, "[INPUT] %s released reason=%s", names[i], changes.connectionChanged ? "lifecycle" :
                !state.inputValid ? "invalid-input" : "report");
        if (state.pressed & (1u << i))
            line(nowMs, "[INPUT] %s pressed", names[i]);
    }
    if (state.inputValid && !previouslyValid_)
        line(nowMs, "[INPUT] First valid gamepad report for this session");
    if (state.inputValid && (!analogLogged_ ||
        (analogChanged(state.analog, loggedAnalog_, config_.analogDelta) && nowMs - lastAnalogLogMs_ >= config_.analogIntervalMs)))
        analogLine(state, nowMs);
    if (!state.inputValid) {
        loggedAnalog_ = AnalogState{};
        analogLogged_ = false;
    }
    previouslyConnected_ = state.connected;
    previouslyValid_ = state.inputValid;
}
void SerialDiagnostics::snapshot(const IGameController& controller, uint32_t nowMs) {
    const auto& s = controller.state();
    line(nowMs, "[STATUS] status=%s connected=%u inputValid=%u held=%04x session=%lu reports=%lu lastReportMs=%lu seq=%lu",
        statusName(s.status), s.connected, s.inputValid, s.held, static_cast<unsigned long>(s.session),
        static_cast<unsigned long>(s.reportSequence), static_cast<unsigned long>(s.lastReportMs),
        static_cast<unsigned long>(s.sequence));
}
}  // namespace esp32controller
