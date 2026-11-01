#pragma once
#include "ControllerState.h"

namespace esp32controller {
class IGameController {
public:
    virtual ~IGameController() = default;
    virtual bool begin() = 0;
    virtual ControllerChanges update(uint32_t nowMs) = 0;
    virtual const ControllerState& state() const = 0;
    virtual const ControllerCapabilities& capabilities() const = 0;
    virtual const ControllerIdentity& identity() const = 0;
};
}  // namespace esp32controller
