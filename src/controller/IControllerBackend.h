#pragma once
#include "ControllerState.h"

namespace esp32controller {
struct RawControllerInput {
    ButtonMask buttons = 0;
    int32_t lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
};
struct BackendFrame {
    bool ready = false;
    bool connected = false;
    bool hasReport = false;
    bool unsupportedReport = false;
    uint32_t session = 0;
    RawControllerInput input;
    ControllerIdentity identity;
    ControllerCapabilities capabilities;
};
// This narrow seam allows lifecycle/normalization tests without Arduino or radio.
class IControllerBackend {
public:
    virtual ~IControllerBackend() = default;
    virtual bool begin() = 0;
    virtual const BackendFrame& poll() = 0;
};
}  // namespace esp32controller
