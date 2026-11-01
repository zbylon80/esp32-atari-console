#include "controller/XboxController.h"
#include "diagnostics/SerialDiagnostics.h"
#include "controller/Bluepad32Adapter.h"
#include <Bluepad32.h>
#include <Print.h>
#include <cmath>
#include <climits>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>

using namespace esp32controller;
FakeBluepad32 BP32;
bool fakeHostReady = true;
#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(std::string(__func__) + ": " + #condition); } while (false)
#define NEAR(a,b) CHECK(std::fabs((a) - (b)) < 0.00002f)

struct FakeBackend : IControllerBackend {
    BackendFrame next, current;
    bool available = true;
    unsigned starts = 0;
    bool begin() override { ++starts; return available; }
    const BackendFrame& poll() override {
        current = next;
        next.hasReport = next.unsupportedReport = false;
        return current;
    }
};
struct TextSink : Print {
    std::string text;
    size_t write(const uint8_t* data, size_t size) override {
        text.append(reinterpret_cast<const char*>(data), size);
        return size;
    }
    size_t count(const std::string& value) const {
        size_t found = 0, offset = 0;
        while ((offset = text.find(value, offset)) != std::string::npos) { ++found; offset += value.size(); }
        return found;
    }
};
ControllerConfig zeroZones() {
    ControllerConfig c;
    c.leftStickDeadZone = c.rightStickDeadZone = 0;
    c.leftTriggerDeadZone = c.rightTriggerDeadZone = 0;
    return c;
}
struct Rig {
    FakeBackend backend;
    XboxController controller;
    explicit Rig(const ControllerConfig& config = zeroZones()) : controller(backend, config) {
        CHECK(controller.begin());
        backend.next.ready = true;
    }
    ControllerChanges connect(uint32_t now = 1) {
        backend.next.connected = true;
        ++backend.next.session;
        return controller.update(now);
    }
    ControllerChanges report(ButtonMask buttons, uint32_t now) {
        backend.next.input.buttons = buttons;
        backend.next.hasReport = true;
        return controller.update(now);
    }
};

void initializationErrors() {
    FakeBackend backend;
    auto config = zeroZones();
    config.leftStickDeadZone = 1;
    XboxController invalid(backend, config);
    CHECK(!invalid.begin());
    CHECK(backend.starts == 0);
    CHECK(invalid.state().error == ControllerError::InvalidConfiguration);
    backend.available = false;
    XboxController unavailable(backend);
    CHECK(!unavailable.begin());
    CHECK(unavailable.state().status == ConnectionStatus::Error);
    CHECK(unavailable.state().error == ControllerError::BackendUnavailable);
    backend.available = true;
    CHECK(unavailable.begin());
    CHECK(unavailable.state().error == ControllerError::None);
    const auto starts = backend.starts;
    CHECK(unavailable.begin());
    CHECK(backend.starts == starts);
}
void connectionBeforeFirstReport() {
    Rig r;
    r.controller.update(0);
    CHECK(r.controller.state().status == ConnectionStatus::Waiting);
    auto change = r.connect();
    CHECK(change.connectionChanged);
    CHECK(r.controller.state().connected);
    CHECK(!r.controller.state().inputValid);
    CHECK(r.controller.state().status == ConnectionStatus::ConnectedAwaitingInput);
    for (uint32_t now = 2; now < 20; ++now) r.controller.update(now);
    CHECK(r.controller.state().connected);
    change = r.report(mask(Button::A), 20);
    CHECK(change.reportReceived);
    CHECK(r.controller.state().wasPressed(Button::A));
    CHECK(r.controller.state().inputValid);
}
void everyDigitalTransition() {
    Rig r;
    r.connect();
    for (unsigned i = 0; i < 14; ++i) {
        const ButtonMask bit = static_cast<ButtonMask>(1u << i);
        r.report(bit, i * 10 + 2);
        CHECK(r.controller.state().pressed == bit);
        CHECK(r.controller.state().released == 0);
        CHECK(r.controller.state().held == bit);
        // Reading does not consume edges; the next update does.
        CHECK(r.controller.state().pressed == bit);
        r.controller.update(i * 10 + 3);
        CHECK(r.controller.state().pressed == 0);
        CHECK(r.controller.state().held == bit);
        r.report(bit, i * 10 + 4);
        CHECK(r.controller.state().pressed == 0);
        r.report(0, i * 10 + 5);
        CHECK(r.controller.state().released == bit);
        CHECK(r.controller.state().held == 0);
        r.controller.update(i * 10 + 6);
        CHECK(r.controller.state().released == 0);
    }
    r.report(mask(Button::A) | mask(Button::Up), 200);
    CHECK(r.controller.state().pressed == (mask(Button::A) | mask(Button::Up)));
    r.report(mask(Button::Up) | mask(Button::B), 201);
    CHECK(r.controller.state().pressed == mask(Button::B));
    CHECK(r.controller.state().released == mask(Button::A));
    CHECK(r.controller.state().isHeld(Button::Up));
}
void disconnectAndFreshSession() {
    Rig r;
    r.connect();
    r.backend.next.input.lx = 511;
    r.backend.next.input.rt = 1023;
    r.report(mask(Button::Left) | mask(Button::A), 50);
    const auto reports = r.controller.state().reportSequence;
    r.backend.next.connected = false;
    r.backend.next.hasReport = true;  // Even a stale pending report must not survive disconnect.
    auto changes = r.controller.update(80);
    CHECK(changes.connectionChanged);
    CHECK(!r.controller.state().connected && !r.controller.state().inputValid);
    CHECK(r.controller.state().released == (mask(Button::Left) | mask(Button::A)));
    CHECK(r.controller.state().held == 0 && r.controller.state().pressed == 0);
    NEAR(r.controller.state().analog.lx, 0);
    NEAR(r.controller.state().analog.rt, 0);
    CHECK(r.controller.state().lastReportMs == 50);
    CHECK(r.controller.state().reportSequence == reports);
    r.controller.update(81);
    CHECK(r.controller.state().released == 0);
    CHECK(r.controller.state().status == ConnectionStatus::Disconnected);
    r.connect(100);  // Raw input still contains the old held direction; no fresh report.
    CHECK(r.controller.state().held == 0 && !r.controller.state().inputValid);
    r.backend.next.input = RawControllerInput{};
    r.report(mask(Button::B), 110);
    CHECK(r.controller.state().pressed == mask(Button::B));
    CHECK(r.controller.state().reportSequence == reports + 1);
}
void sessionReplacementAndWrap() {
    Rig r;
    r.connect();
    r.report(mask(Button::A), 2);
    r.backend.next.session = UINT32_MAX;
    r.controller.update(3);  // Disconnect/reconnect may be coalesced into a new backend session.
    CHECK(r.controller.state().released == mask(Button::A));
    CHECK(!r.controller.state().inputValid && r.controller.state().held == 0);
    r.report(mask(Button::A), UINT32_MAX - 2);
    CHECK(r.controller.state().reportAgeMs(2) == 5);
    r.backend.next.session = 0;
    r.controller.update(3);
    CHECK(r.controller.state().session == 0);
    CHECK(!r.controller.state().inputValid && r.controller.state().held == 0);
}
void unsupportedAndCapabilities() {
    ControllerCapabilities c;
    CHECK(c.button(Button::A) == Capability::Unknown);
    CHECK(c.axis(Axis::LT) == Capability::Unknown);
    c.knownButtons = mask(Button::A) | mask(Button::B);
    c.availableButtons = mask(Button::A);
    CHECK(c.button(Button::A) == Capability::Available);
    CHECK(c.button(Button::B) == Capability::Unavailable);
    CHECK(c.button(Button::X) == Capability::Unknown);
    c.knownAxes = kAllAxes;
    CHECK(c.axis(Axis::RT) == Capability::Unavailable);
    Rig r;
    r.backend.next.capabilities = c;
    r.connect();
    CHECK(r.controller.capabilities().button(Button::X) == Capability::Unknown);
    r.report(mask(Button::A), 2);
    r.backend.next.unsupportedReport = true;
    auto changes = r.controller.update(3);
    CHECK(changes.unsupportedInput);
    CHECK(!r.controller.state().inputValid && r.controller.state().held == 0);
    CHECK(r.controller.state().released == mask(Button::A));
    r.backend.next.unsupportedReport = true;
    CHECK(!r.controller.update(4).unsupportedInput);
}
void normalizationEndpoints() {
    NEAR(normalizeAxis(-512), -1); NEAR(normalizeAxis(511), 1); NEAR(normalizeAxis(0), 0);
    NEAR(normalizeAxis(INT32_MIN), -1); NEAR(normalizeAxis(INT32_MAX), 1);
    NEAR(normalizeAxis(-256), -0.5f);
    NEAR(normalizeAxis(255), 255.0f / 511.0f);
    NEAR(normalizeTrigger(0, 0), 0); NEAR(normalizeTrigger(1023, 0), 1);
    NEAR(normalizeTrigger(-100, 0.02f), 0); NEAR(normalizeTrigger(9999, 0.02f), 1);
    NEAR(normalizeTrigger(10, 0.02f), 0);
    NEAR(normalizeTrigger(512, 0.1f), (512.0f / 1023.0f - 0.1f) / 0.9f);
    Rig r;
    r.connect();
    r.backend.next.input.lx = -512; r.backend.next.input.ry = -512;
    r.backend.next.input.lt = 1023; r.backend.next.input.rt = 0;
    r.report(0, 2);
    NEAR(r.controller.state().analog.lx, -1); NEAR(r.controller.state().analog.ry, 1);
    NEAR(r.controller.state().analog.lt, 1); NEAR(r.controller.state().analog.rt, 0);
}
void radialDeadZonesAndInvalidValues() {
    auto stick = normalizeStick(0, 0, 0.12f, true);
    NEAR(stick.x,0); NEAR(stick.y,0);
    stick = normalizeStick(20, -20, 0.12f, true);
    NEAR(stick.x,0); NEAR(stick.y,0);
    stick = normalizeStick(-256, 0, 0.5f, false);  // Exact boundary.
    NEAR(stick.x,0);
    stick = normalizeStick(-384, 0, 0.5f, false);
    NEAR(stick.x,-0.5f);
    stick = normalizeStick(511, 511, 0.12f, true);
    NEAR(stick.x, std::sqrt(0.5f)); NEAR(stick.y, -std::sqrt(0.5f));
    NEAR(stick.x * stick.x + stick.y * stick.y, 1);
    stick = normalizeStick(511, 0, 1, true);
    NEAR(stick.x,0);
    auto c = ControllerConfig{};
    c.rightStickDeadZone = std::numeric_limits<float>::quiet_NaN(); CHECK(!validConfig(c));
    c = ControllerConfig{}; c.leftTriggerDeadZone = -0.1f; CHECK(!validConfig(c));
    c = ControllerConfig{}; c.analogEventThreshold = std::numeric_limits<float>::infinity(); CHECK(!validConfig(c));
}
void noiseAndFreshness() {
    auto c = zeroZones(); c.analogEventThreshold = 0.05f;
    Rig r(c); r.connect(); r.report(0, 2);
    for (int32_t x : {1, 5, 10}) {
        r.backend.next.input.lx = x;
        CHECK(!r.report(0, static_cast<uint32_t>(x + 10)).inputChanged);
    }
    CHECK(r.controller.state().analog.lx > 0);  // State is current even below notification threshold.
    r.backend.next.input.lx = 30;
    CHECK(r.report(0, 40).inputChanged);  // Cumulative movement exceeds the publication baseline.
    r.backend.next.input.lx = 0;
    CHECK(r.report(mask(Button::A), 41).inputChanged);
    CHECK(r.controller.state().lastReportMs == 41);
    r.controller.update(1000000);  // No invented silence timeout or held-button auto-release.
    CHECK(r.controller.state().isHeld(Button::A));
    CHECK(r.controller.state().inputValid);
    CHECK(r.controller.state().pressed == 0);
    Rig defaults(ControllerConfig{}); defaults.connect(); defaults.report(0, 2);
    defaults.backend.next.input.lx = 10; defaults.backend.next.input.ly = -11;
    defaults.backend.next.input.rx = 8; defaults.backend.next.input.ry = 7;
    CHECK(!defaults.report(0, 3).inputChanged);
}
void digitalDiagnosticsDoNotRepeat() {
    Rig r; TextSink sink; SerialDiagnostics log(sink);
    auto change = r.connect(); log.observe(r.controller, change, 1);
    change = r.report(mask(Button::A), 2); log.observe(r.controller, change, 2);
    for (uint32_t now = 3; now < 100; ++now) {
        change = r.report(mask(Button::A), now); log.observe(r.controller, change, now);
    }
    change = r.report(0, 100); log.observe(r.controller, change, 100);
    CHECK(sink.count("A pressed") == 1);
    CHECK(sink.count("A released") == 1);
    CHECK(sink.count("First valid gamepad report") == 1);
    CHECK(sink.count("LX=") == 1);  // Unchanged analog state is not logged continuously.
    change = r.report(mask(Button::B), 101); log.observe(r.controller, change, 101);
    r.backend.next.unsupportedReport = true;
    change = r.controller.update(102); log.observe(r.controller, change, 102);
    CHECK(sink.count("B released reason=invalid-input") == 1);
}
void analogDiagnosticsPendingAndNeutral() {
    Rig r; TextSink sink; DiagnosticsConfig config; config.analogDelta = 0.03f;
    SerialDiagnostics log(sink, config);
    auto change = r.connect(); log.observe(r.controller, change, 1);
    change = r.report(0, 2); log.observe(r.controller, change, 2);
    r.backend.next.input.lx = 25;
    change = r.report(0, 10); log.observe(r.controller, change, 10);
    CHECK(sink.count("LX=") == 1);
    change = r.controller.update(102); log.observe(r.controller, change, 102);
    CHECK(sink.count("LX=") == 2);  // Pending analog value logs even without another report.
    r.backend.next.input.lx = 10;
    change = r.report(0, 202); log.observe(r.controller, change, 202);
    CHECK(sink.count("LX=") == 2);  // Difference is smaller than the logger delta.
    r.backend.next.input.lx = 0;
    change = r.report(0, 203); log.observe(r.controller, change, 203);
    CHECK(sink.count("LX=") == 3);  // Return to neutral is always observable.
    r.backend.next.input.lx = 511;
    change = r.report(mask(Button::Up), 210); log.observe(r.controller, change, 210);
    r.backend.next.connected = false;
    change = r.controller.update(211); log.observe(r.controller, change, 211);
    CHECK(sink.count("Up released reason=lifecycle") == 1);
    CHECK(sink.count("held=0 analog=0 inputValid=0") == 1);
    const auto analogLines = sink.count("LX=");
    change = r.controller.update(400); log.observe(r.controller, change, 400);
    CHECK(sink.count("LX=") == analogLines);  // No delayed stale analog output after disconnect.

    // A small first sample is logged; neutral must still log below analogDelta.
    Rig small; TextSink smallSink; SerialDiagnostics smallLog(smallSink, config);
    change = small.connect(); smallLog.observe(small.controller, change, 1);
    small.backend.next.input.lx = 5;
    change = small.report(0, 2); smallLog.observe(small.controller, change, 2);
    CHECK(smallSink.count("LX=") == 1);
    small.backend.next.input.lx = 0;
    change = small.report(0, 102); smallLog.observe(small.controller, change, 102);
    CHECK(smallSink.count("LX=") == 2);
}
void actualAdapterBoundaryAndMapping() {
    BP32 = FakeBluepad32{};
    fakeHostReady = true;
    Bluepad32Adapter adapter, second;
    CHECK(adapter.begin()); CHECK(adapter.begin()); CHECK(!second.begin());
    Controller ctl;
    BP32.pendingConnect = &ctl;
    auto frame = adapter.poll();
    CHECK(frame.connected && !frame.hasReport);  // Connection BEFORE gamepad class exists.
    CHECK(frame.capabilities.source == CapabilitySource::DocumentedXboxProfile);
    CHECK(frame.identity.vendorId == 0x045e);
    CHECK(frame.session == 1);
    ctl.gamepad = true;
    struct Mapping { bool Controller::*field; Button button; };
    const Mapping mappings[] = {
        {&Controller::aDown,Button::A}, {&Controller::bDown,Button::B},
        {&Controller::xDown,Button::X}, {&Controller::yDown,Button::Y},
        {&Controller::lbDown,Button::LB}, {&Controller::rbDown,Button::RB},
        {&Controller::menuDown,Button::Menu}, {&Controller::viewDown,Button::View},
        {&Controller::l3Down,Button::L3}, {&Controller::r3Down,Button::R3}
    };
    for (const auto& mapping : mappings) {
        ctl.*(mapping.field) = true; BP32.pendingReport = true;
        frame = adapter.poll(); CHECK(frame.hasReport); CHECK(frame.input.buttons == mask(mapping.button));
        ctl.*(mapping.field) = false;
    }
    struct Direction { uint8_t bit; Button button; };
    for (const auto& d : {Direction{DPAD_UP,Button::Up}, Direction{DPAD_DOWN,Button::Down},
                         Direction{DPAD_LEFT,Button::Left}, Direction{DPAD_RIGHT,Button::Right}}) {
        ctl.dpadValue = d.bit; BP32.pendingReport = true;
        frame = adapter.poll(); CHECK(frame.input.buttons == mask(d.button));
    }
    ctl.lx=-111; ctl.ly=222; ctl.rx=-333; ctl.ry=444; ctl.lt=123; ctl.rt=987;
    BP32.pendingReport = true;
    frame = adapter.poll();
    CHECK(frame.input.lx==-111 && frame.input.ly==222 && frame.input.rx==-333 && frame.input.ry==444);
    CHECK(frame.input.lt==123 && frame.input.rt==987);
    CHECK(!adapter.poll().hasReport);  // No replay of the last report on an idle poll.
    BP32.pendingDisconnect = true;
    frame = adapter.poll(); CHECK(!frame.connected && !frame.hasReport);
    CHECK(frame.input.buttons==0 && frame.input.lt==0);
    ctl.gamepad = false; ctl.model = 0; ctl.properties.vendor_id = 0;
    BP32.pendingConnect = &ctl;
    frame = adapter.poll(); CHECK(frame.connected && !frame.hasReport && frame.session==2);
    CHECK(frame.capabilities.axis(Axis::LT)==Capability::Unknown);
    BP32.pendingReport = true;
    frame = adapter.poll(); CHECK(frame.unsupportedReport && !frame.hasReport);
    BP32.pendingDisconnect = true; adapter.poll();
}
int main() {
    struct Test { const char* name; void (*run)(); };
    const Test tests[] = {
        {"initialization errors", initializationErrors}, {"connection before first report", connectionBeforeFirstReport},
        {"all digital transitions", everyDigitalTransition}, {"disconnect and fresh session", disconnectAndFreshSession},
        {"session replacement and timestamp wrap", sessionReplacementAndWrap}, {"unsupported/capabilities", unsupportedAndCapabilities},
        {"normalization endpoints", normalizationEndpoints}, {"radial dead zones/config", radialDeadZonesAndInvalidValues},
        {"noise/freshness", noiseAndFreshness}, {"digital log suppression", digitalDiagnosticsDoNotRepeat},
        {"analog log pending/neutral/disconnect", analogDiagnosticsPendingAndNeutral},
        {"actual adapter early callback/all mappings", actualAdapterBoundaryAndMapping}
    };
    unsigned failures = 0;
    for (const auto& test : tests) {
        try { test.run(); std::printf("PASS %s\n", test.name); }
        catch (const std::exception& error) { ++failures; std::printf("FAIL %s: %s\n", test.name, error.what()); }
    }
    std::printf("%u/%u groups passed (host logic only; no radio validation)\n",
        static_cast<unsigned>(sizeof(tests) / sizeof(tests[0])) - failures,
        static_cast<unsigned>(sizeof(tests) / sizeof(tests[0])));
    return failures ? 1 : 0;
}
