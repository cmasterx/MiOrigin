#include "touchscreen.h"

#include <Arduino.h>
#include <XPT2046_Touchscreen.h>
#include "common.h"

void Driver::touchscreen_init()
{
    Touchscreen_cfg.state = Driver::TOUCHSCREEN_NONE;
    Touchscreen_cfg.onPress = nullptr;
    Touchscreen_cfg.onRelease = nullptr;
    Touchscreen_cfg.busyInterruptHandler = nullptr;
    Touchscreen_cfg.point.x = 0;
    Touchscreen_cfg.point.y = 0;
    Touchscreen_cfg.point.z = 0;
}

void Driver::touchscreen_begin(SPIClass &spi, uint8_t rotation, bool enableInterrupts, uint8_t interruptPin)
{
    ts.begin(spi);
    if (rotation) ts.setRotation(rotation);

    if (enableInterrupts) {
        Touchscreen_cfg.interruptPin = interruptPin;
        Touchscreen_cfg.GPIO_firstGroup = interruptPin < 32;
        pinMode(interruptPin, INPUT_PULLDOWN);
        attachInterrupt(digitalPinToInterrupt(interruptPin), touchscreenISR, HIGH);
    }
    else {
        Touchscreen_cfg.interruptPin = -1;
    }
    
}

void Driver::touchscreen_get_raw_points(const uint16_t **x, const uint16_t **y, const uint8_t **z)
{
    *x = &Driver::Touchscreen_cfg.point.x;
    *y = &Driver::Touchscreen_cfg.point.y;
    *z = &Driver::Touchscreen_cfg.point.z;
}

bool Driver::touchscreen_busy_check_interrupt(bool enable)
{
    if (Touchscreen_cfg.interruptPin >= 0) return false;

    if (enable && Touchscreen_cfg.interruptPin < 0 && !Touchscreen_cfg.busyInterruptHandler) {

        xTaskCreate(
            Driver::busyInterruptFunction,
            "ts-interhnd",
            5 * 1024,
            nullptr,
            1,
            &Touchscreen_cfg.busyInterruptHandler
        );
        return true;
    }
    else if (!enable && Touchscreen_cfg.busyInterruptHandler) {
        vTaskDelete(Touchscreen_cfg.busyInterruptHandler);
        Touchscreen_cfg.busyInterruptHandler = nullptr;
        return true;
    }

    return false;
}

void Driver::touchscreen_register_on_press(TouchscreenFunctionBehavior func)
{
    Touchscreen_cfg.onPress = func;
}

void Driver::touchscreen_register_on_release(TouchscreenFunctionBehavior func)
{
    Touchscreen_cfg.onRelease = func;
}

void Driver::busyInterruptFunction(void *args)
{
    // TODO REDESIGN THIS FUNCTION SO THAT ON HOLD IS ALWAYS SENT!
    bool touched = false;
    
    while (true) {
        bool currentState = ts.touched();
        
        #ifdef DRIVER_TS_ENABLE_DEBUG_PRINT

        if (currentState) {
            
            Serial.println("-> I am so touched!");
        }
        else{
            Serial.println("-> Nothing. I feel nothing at all...");
        }

        #endif

        Driver::ts.readData(&Touchscreen_cfg.point.x,
                            &Touchscreen_cfg.point.y,
                            &Touchscreen_cfg.point.z
                            );

        if (currentState) {
            if (Touchscreen_cfg.onPress) {
                touched = true;
                Touchscreen_cfg.onPress();
            }
        }
        else {
            if (touched) {
                touched = false;
                if (Touchscreen_cfg.onRelease) Touchscreen_cfg.onRelease();
            }
        }

        // if (currentState != touched) {
            
        //     touched = currentState;
        //     if (touched) {

        //         // update values
        //         Driver::ts.readData(&Touchscreen_cfg.point.x,
        //                             &Touchscreen_cfg.point.y,
        //                             &Touchscreen_cfg.point.z
        //                             );

        //         // call event handler
        //         if (!Touchscreen_cfg.onPress) {
        //             #ifdef DRIVER_TS_ENABLE_DEBUG_PRINT
        //             Serial.print("There is no press handler!");
        //             #endif
                    
        //             vTaskDelay(1000 / portTICK_PERIOD_MS);
        //             continue;
        //         }
                
        //         #ifdef DRIVER_TS_ENABLE_DEBUG_PRINT
        //         Serial.print("There is a press handler!");
        //         #endif
        //         Touchscreen_cfg.onPress();
        //     }
        //     else {
        //         if (!Touchscreen_cfg.onRelease) {
        //             #ifdef DRIVER_TS_ENABLE_DEBUG_PRINT
        //             Serial.print("There is no release handler!");
        //             #endif
                    
        //             vTaskDelay(1000 / portTICK_PERIOD_MS);
        //             continue;
        //         }

        //         #ifdef DRIVER_TS_ENABLE_DEBUG_PRINT
        //         Serial.print("There is a release handler!");
        //         #endif
        //         Touchscreen_cfg.onRelease();
        //     }
        // }

        vTaskDelay(DRIVER_TS_CHECK_INTERVAL / portTICK_PERIOD_MS);
    }
}

namespace Driver
{
    XPT2046_Touchscreen ts(TCH_CS);
}
