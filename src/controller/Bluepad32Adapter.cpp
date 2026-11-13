#include "Bluepad32Adapter.h"
#include <Bluepad32.h>
#include <bt/uni_bt_setup.h>

namespace esp32controller {
Bluepad32Adapter* Bluepad32Adapter::instance_ = nullptr;
Bluepad32Adapter::~Bluepad32Adapter() {
    if (instance_ == this) instance_ = nullptr;
}
bool Bluepad32Adapter::begin() {
    if (started_) return instance_ == this;
    if (instance_ && instance_ != this) return false;
    instance_ = this;
    BP32.setup(&Bluepad32Adapter::onConnected, &Bluepad32Adapter::onDisconnected);
    // Keep platform discovery defaults. Never delete bonds or queue startup commands.
    started_ = true;
    return true;
}
const char* Bluepad32Adapter::version() const { return BP32.firmwareVersion(); }
void Bluepad32Adapter::onConnected(Controller* ctl) {
    if (!instance_) return;
    auto& self = *instance_;
    if (self.controller_) { ++self.ignoredConnections_; return; }
    // Classification must wait for a fresh report (Gate 2 callback-fix1).
    self.controller_ = ctl;
    self.frame_.connected = true;
    self.frame_.input = RawControllerInput{};
    ++self.frame_.session;
    const auto properties = ctl->getProperties();
    self.frame_.identity = ControllerIdentity{};
    ctl->getModelName().toCharArray(self.frame_.identity.modelName, sizeof(self.frame_.identity.modelName));
    self.frame_.identity.vendorId = properties.vendor_id;
    self.frame_.identity.productId = properties.product_id;
    self.frame_.identity.backendModel = ctl->getModel();
    self.frame_.capabilities = ControllerCapabilities{};
    if (ctl->getModel() == Controller::CONTROLLER_TYPE_XBoxOneController && properties.vendor_id == 0x045e) {
        auto& caps = self.frame_.capabilities;
        caps.knownButtons = caps.availableButtons = kAllButtons;
        caps.knownAxes = caps.availableAxes = kAllAxes;
        caps.source = CapabilitySource::DocumentedXboxProfile;
    }
}
void Bluepad32Adapter::onDisconnected(Controller* ctl) {
    if (!instance_ || instance_->controller_ != ctl) return;
    instance_->controller_ = nullptr;
    instance_->frame_.connected = false;
    instance_->frame_.hasReport = false;
    instance_->frame_.input = RawControllerInput{};
}
const BackendFrame& Bluepad32Adapter::poll() {
    frame_.hasReport = frame_.unsupportedReport = false;
    if (!started_) return frame_;
    const bool updated = BP32.update();
    frame_.ready = uni_bt_setup_is_ready();
    if (updated && controller_ && controller_->isConnected() && controller_->hasData()) {
        if (!controller_->isGamepad()) {
            frame_.unsupportedReport = true;
        } else {
            frame_.hasReport = true;
            auto& raw = frame_.input;
            raw = RawControllerInput{};
            if (controller_->a()) raw.buttons |= mask(Button::A);
            if (controller_->b()) raw.buttons |= mask(Button::B);
            if (controller_->x()) raw.buttons |= mask(Button::X);
            if (controller_->y()) raw.buttons |= mask(Button::Y);
            if (controller_->dpad() & DPAD_UP) raw.buttons |= mask(Button::Up);
            if (controller_->dpad() & DPAD_DOWN) raw.buttons |= mask(Button::Down);
            if (controller_->dpad() & DPAD_LEFT) raw.buttons |= mask(Button::Left);
            if (controller_->dpad() & DPAD_RIGHT) raw.buttons |= mask(Button::Right);
            if (controller_->l1()) raw.buttons |= mask(Button::LB);
            if (controller_->r1()) raw.buttons |= mask(Button::RB);
            if (controller_->miscStart()) raw.buttons |= mask(Button::Menu);
            if (controller_->miscSelect()) raw.buttons |= mask(Button::View);
            if (controller_->miscCapture()) raw.buttons |= mask(Button::Share);
            if (controller_->thumbL()) raw.buttons |= mask(Button::L3);
            if (controller_->thumbR()) raw.buttons |= mask(Button::R3);
            raw.lx = controller_->axisX(); raw.ly = controller_->axisY();
            raw.rx = controller_->axisRX(); raw.ry = controller_->axisRY();
            raw.lt = controller_->brake(); raw.rt = controller_->throttle();
        }
    }
    return frame_;
}
}  // namespace esp32controller
