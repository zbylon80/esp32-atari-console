#pragma once
#include <cstdint>
#include <cstring>
#include <functional>

// API seam for compiling the REAL adapter on the host. This does not implement
// any Bluetooth stack, parser or radio behavior.
constexpr uint8_t DPAD_UP = 1, DPAD_DOWN = 2, DPAD_RIGHT = 4, DPAD_LEFT = 8;
struct FakeModelName {
    void toCharArray(char* buffer, size_t length) const {
        const char name[] = "XBox One";
        if (length) {
            const size_t count = length - 1 < sizeof(name) - 1 ? length - 1 : sizeof(name) - 1;
            std::memcpy(buffer, name, count); buffer[count] = '\0';
        }
    }
};
constexpr int BP32_MAX_CONTROLLERS=4;
struct FakeProperties { uint16_t vendor_id = 0x045e, product_id = 0x0b13; uint8_t btaddr[6]={}; };
class Controller {
public:
    enum { CONTROLLER_TYPE_XBoxOneController = 32 };
    bool connected = false, fresh = false, gamepad = false;
    bool aDown = false, bDown = false, xDown = false, yDown = false;
    bool lbDown = false, rbDown = false, menuDown = false, viewDown = false;
    bool l3Down = false, r3Down = false;
    uint8_t dpadValue = 0;
    int32_t lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
    int model = CONTROLLER_TYPE_XBoxOneController;
    FakeProperties properties;
    bool isConnected() const { return connected; }
    bool hasData() const { return fresh; }
    bool isGamepad() const { return gamepad; }
    FakeModelName getModelName() const { return {}; }
    FakeProperties getProperties() const { return properties; }
    int getModel() const { return model; }
    void disconnect() { connected=false; ++disconnects; }
    unsigned disconnects=0;
    uint8_t dpad() const { return dpadValue; }
    bool a() const { return aDown; } bool b() const { return bDown; }
    bool x() const { return xDown; } bool y() const { return yDown; }
    bool l1() const { return lbDown; } bool r1() const { return rbDown; }
    bool miscStart() const { return menuDown; } bool miscSelect() const { return viewDown; }
    bool thumbL() const { return l3Down; } bool thumbR() const { return r3Down; }
    int32_t axisX() const { return lx; } int32_t axisY() const { return ly; }
    int32_t axisRX() const { return rx; } int32_t axisRY() const { return ry; }
    int32_t brake() const { return lt; } int32_t throttle() const { return rt; }
};
class FakeBluepad32 {
public:
    std::function<void(Controller*)> connectCallback, disconnectCallback;
    Controller* active = nullptr;
    Controller* pendingConnect = nullptr;
    bool pendingDisconnect = false, pendingReport = false;
    bool discovery=true;
    bool requiresBleScan=false;
    void enableNewBluetoothConnections(bool enabled){discovery=enabled;}
    void setup(std::function<void(Controller*)> connect, std::function<void(Controller*)> disconnect) {
        connectCallback = connect; disconnectCallback = disconnect;
    }
    bool update() {
        // Model BLE's scan-dependent reconnect path in hub lifecycle tests.
        // This is a behavioral seam, not a substitute for a physical radio test.
        if(requiresBleScan&&!discovery){pendingReport=false;return false;}
        if (pendingDisconnect && active) {
            disconnectCallback(active); active->connected = false; active = nullptr;
        }
        pendingDisconnect = false;
        if (pendingConnect) {
            active = pendingConnect; pendingConnect = nullptr; active->connected = true;
            connectCallback(active);
        }
        if (active) active->fresh = pendingReport;
        const bool updated = active && pendingReport;
        pendingReport = false;
        return updated;
    }
    const char* firmwareVersion() const { return "host-test seam"; }
};
extern FakeBluepad32 BP32;
