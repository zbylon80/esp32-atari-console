#pragma once
#include "IGameController.h"
#include "IControllerBackend.h"
#include "InputNormalization.h"

namespace esp32controller {
class XboxController final : public IGameController {
public:
    explicit XboxController(IControllerBackend& backend, const ControllerConfig& config = ControllerConfig());
    bool begin() override;
    ControllerChanges update(uint32_t nowMs) override;
    const ControllerState& state() const override { return state_; }
    const ControllerCapabilities& capabilities() const override { return capabilities_; }
    const ControllerIdentity& identity() const override { return identity_; }
private:
    void neutralize();
    IControllerBackend& backend_;
    ControllerConfig config_;
    ControllerState state_;
    ControllerIdentity identity_;
    ControllerCapabilities capabilities_;
    AnalogState eventAnalog_;
    bool started_ = false;
    bool everConnected_ = false;
    bool unsupportedSeen_ = false;
};
}  // namespace esp32controller
