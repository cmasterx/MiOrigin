#include "common.h"
#include <Arduino.h>
#include <assert.h>

SPIClass *vspi;
SPIClass *hspi;

void Common_Init()
{
    // todo clean this up
    Serial.println("Running test code");
    vspi = new SPIClass(VSPI);
    hspi = new SPIClass(HSPI);

    pinMode(HSPI_MOSI, OUTPUT);
    pinMode(HSPI_MISO, INPUT_PULLDOWN);
    
    vspi->begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI, LCD_CS);
    hspi->begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, SD_CS);

    pinMode(TCH_CS, OUTPUT);
    digitalWrite(TCH_CS, HIGH);
    
    digitalWrite(TCH_CS, LOW);
    digitalWrite(HSPI_MOSI, HIGH);
    delay(200);
    digitalWrite(HSPI_MOSI, LOW);
    delay(200);
    digitalWrite(HSPI_MOSI, HIGH);
    delay(200);
    digitalWrite(HSPI_MOSI, LOW);
    delay(200);
    digitalWrite(TCH_CS, HIGH);
    
    
    
    digitalWrite(TCH_CS, LOW);
    hspi->beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    uint8_t buff[] = { 0xFF, 0x01, 0x02, 0x13, 0x44, 0xFF};
    hspi->transferBytes(buff, buff, sizeof(buff));
    digitalWrite(TCH_CS, HIGH);
    hspi->endTransaction();

    Serial.println("DONE!");
    // for (;;);
}

size_t readUntilEnd(char *buffer, size_t bufferSize, Stream &stream)
{
    // block until first byte is read
    Serial.print("@ Stage 1");
    while (!Serial.available()) {
        vTaskDelay(100 / portTICK_RATE_MS);
    }

    
    size_t bytesRead = 0;

    Serial.print("@ Stage 2");
    // read minimum two bytes
    while (bytesRead < 2) {
        if (Serial.available()) {
            bytesRead += Serial.readBytesUntil('\n', buffer, bufferSize - bytesRead);
        }
        delay(10);
    }

    Serial.print("@ Stage 3");
    while (bytesRead < 2 || *((uint16_t *) (buffer + bytesRead - 2)) != LINE_TERMINATION) {
        if (Serial.available()) {
            bytesRead += Serial.readBytesUntil('\n', buffer + bytesRead, bufferSize - bytesRead);
            Serial.println();
            Common_HEX_print(buffer);
        }
        delay(10);
    }

    Serial.print("@ Stage 4");
    *((uint16_t *) (buffer + bytesRead - 2)) = 0;

    return bytesRead - 2;
}

void Common_USBC_Handler(void *params)
{
    const TickType_t waitDelay = 500 / portTICK_PERIOD_MS;
    
    while (true) {
        if (Serial.available()) {
            char buffer[512] ;
            readUntilEnd(buffer, sizeof(buffer), Serial);
            
            // process if there's data in the buffer
            if (strlen(buffer)) {

                if (!strcmp(buffer, "!update-firmware"));
                // Serial.readBytes
                // todo
                #ifdef OTA0
                
                #endif
            }
        }
        else {
            vTaskDelay(waitDelay);
        }
    }
}

void Common_HEX_print(char *cstr, Stream &stream)
{
    while (*cstr != 0) {
        stream.print(*cstr, HEX);
        cstr++;
    }
}
