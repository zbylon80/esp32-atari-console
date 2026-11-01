#include "XboxController.h"

namespace esp32controller {
XboxController::XboxController(IControllerBackend& backend, const ControllerConfig& config)
    : backend_(backend), config_(config) {}

bool XboxController::begin() {
    if (started_) return true;
    if (!validConfig(config_)) {
        state_.status = ConnectionStatus::Error;
        state_.error = ControllerError::InvalidConfiguration;
        return false;
    }
    if (!backend_.begin()) {
        state_.status = ConnectionStatus::Error;
        state_.error = ControllerError::BackendUnavailable;
        return false;
    }
    started_ = true;
    state_.error = ControllerError::None;
    state_.status = ConnectionStatus::Starting;
    return true;
}
void XboxController::neutralize() {
    state_.held = 0;
    state_.pressed = 0;
    state_.analog = AnalogState{};
    state_.inputValid = false;
    eventAnalog_ = AnalogState{};
}
ControllerChanges XboxController::update(uint32_t nowMs) {
    ControllerChanges changes;
    state_.pressed = state_.released = 0;
    if (!started_) return changes;
    const auto previousStatus = state_.status;
    const BackendFrame& frame = backend_.poll();
    if (!frame.connected) {
        if (state_.connected) {
            const auto held = state_.held;
            neutralize();
            state_.released = held;
            state_.connected = false;
            changes.connectionChanged = changes.inputChanged = true;
        }
        state_.status = !frame.ready ? ConnectionStatus::Starting :
            everConnected_ ? ConnectionStatus::Disconnected : ConnectionStatus::Waiting;
    } else {
        if (!state_.connected || state_.session != frame.session) {
            const auto held = state_.held;
            neutralize();
            state_.released = held;
            state_.connected = true;
            state_.session = frame.session;
            identity_ = frame.identity;
            capabilities_ = frame.capabilities;
            everConnected_ = true;
            unsupportedSeen_ = false;
            changes.connectionChanged = true;
            changes.inputChanged = true;
        }
        if (frame.unsupportedReport) {
            const auto held = state_.held;
            neutralize();
            state_.released |= held;
            changes.unsupportedInput = !unsupportedSeen_;
            changes.inputChanged = changes.inputChanged || held != 0;
            unsupportedSeen_ = true;
        } else if (frame.hasReport) {
            const auto buttons = static_cast<ButtonMask>(frame.input.buttons & kAllButtons);
            state_.pressed |= static_cast<ButtonMask>(buttons & ~state_.held);
            state_.released |= static_cast<ButtonMask>(state_.held & ~buttons);
            state_.held = buttons;
            const Stick left = normalizeStick(frame.input.lx, frame.input.ly, config_.leftStickDeadZone, config_.invertLeftY);
            const Stick right = normalizeStick(frame.input.rx, frame.input.ry, config_.rightStickDeadZone, config_.invertRightY);
            state_.analog.lx = left.x; state_.analog.ly = left.y;
            state_.analog.rx = right.x; state_.analog.ry = right.y;
            state_.analog.lt = normalizeTrigger(frame.input.lt, config_.leftTriggerDeadZone);
            state_.analog.rt = normalizeTrigger(frame.input.rt, config_.rightTriggerDeadZone);
            const bool firstReport = !state_.inputValid;
            const bool analogEvent = analogChanged(state_.analog, eventAnalog_, config_.analogEventThreshold);
            if (firstReport || analogEvent) eventAnalog_ = state_.analog;
            changes.inputChanged = changes.inputChanged || firstReport || analogEvent || state_.pressed || state_.released;
            state_.inputValid = true;
            state_.lastReportMs = nowMs;
            ++state_.reportSequence;
            changes.reportReceived = true;
        }
        state_.status = state_.inputValid ? ConnectionStatus::Active : ConnectionStatus::ConnectedAwaitingInput;
    }
    changes.statusChanged = state_.status != previousStatus;
    if (changes.connectionChanged || changes.statusChanged || changes.reportReceived || changes.unsupportedInput)
        ++state_.sequence;
    return changes;
}
}  // namespace esp32controller
