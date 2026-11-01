#pragma once
#include "IControllerBackend.h"

class Controller;
namespace esp32controller {
class Bluepad32Adapter final : public IControllerBackend {
public:
    Bluepad32Adapter() = default;
    ~Bluepad32Adapter() override;
    Bluepad32Adapter(const Bluepad32Adapter&) = delete;
    Bluepad32Adapter& operator=(const Bluepad32Adapter&) = delete;
    bool begin() override;
    const BackendFrame& poll() override;
    const char* version() const;
    uint32_t ignoredConnections() const { return ignoredConnections_; }
private:
    static void onConnected(Controller* ctl);
    static void onDisconnected(Controller* ctl);
    static Bluepad32Adapter* instance_;
    Controller* controller_ = nullptr;
    BackendFrame frame_;
    uint32_t ignoredConnections_ = 0;
    bool started_ = false;
};
}  // namespace esp32controller
