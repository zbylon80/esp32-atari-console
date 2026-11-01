#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Bluepad32's platform normally supplies app_main and starts Bluetooth before
// setup(). This service deliberately supplies its own entry point, keeping the
// same Arduino/IDF libraries while leaving that bootstrap unlinked.
extern void setup();
extern void loop();
static void serviceLoop(void*) {
    setup();for(;;)loop();
}
extern "C" void app_main() {
    initArduino();
    if(xTaskCreatePinnedToCore(serviceLoop,"updater",8192,nullptr,1,nullptr,1)!=pdPASS)abort();
}
