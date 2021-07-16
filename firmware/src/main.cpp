// if true, compile factory firmware instead
#ifdef FACTORY

// these are defined in factory.cpp
extern void setup();
extern void loop();

#else

#include <Arduino.h>
#include "common.h"
#include <FS.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <SD.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "BLE_UUID.h"
#include "utils.h"

// extract arduino core props
#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

TaskHandle_t usbcHandler = nullptr;

struct {

    BLEServer *pServer;

    struct {

        BLEService *pService;
        BLECharacteristic *pDeviceName;
    } device;
    
    /**
     * @brief Puts BLE modem to sleep
     * 
     */
    void sleep()
    {
        if (pServer) {
            pServer->getAdvertising()->stop();
            ESP_ERROR_CHECK(esp_bt_sleep_enable());
        }
    }

    /**
     * @brief Wakes up BLE modem
     * 
     */
    void wakeup()
    {
        if (pServer) {
            ESP_ERROR_CHECK(esp_bt_sleep_disable());
            pServer->getAdvertising()->start();
        }
    }

    
} BLE_Props;

/**
 * @brief Parses command from a given string that starts with an exclamation mark
 * 
 * @param buffer 
 * @param buffer_length 
 * @param command 
 * @param command_length 
 * @return size_t 
 */
size_t parseCommandFromString(char *buffer, size_t buffer_length, char *command, size_t command_length)
{
    if (buffer_length && buffer[0] == '!') {
        size_t i;
        for (i = 1; i < buffer_length && i < command_length && (buffer[i] != '\0' || buffer[i] != '\r' || buffer[i] != ' '); ++i) {
            command[i - 1] = buffer[i];
        }

        return i;
    }

    return 0;
}

/**
 * @brief Handles responses to UART from USB-C
 * 
 * @param parameters 
 */
void handleUSBC(void *parameters = nullptr)
{
    const TickType_t waitDelay = 1000 / portTICK_PERIOD_MS;

    delay(200);
    Serial.flush();
    delay(200);
    
    while (true) {
        Serial.println("Loop");
        
        if (Serial.available()) {
            Serial.println("@ Init stage");
            char messageBuffer[256] = {0};
            readUntilEnd(messageBuffer, sizeof(messageBuffer), Serial);
            Serial.println("Reached");
            size_t messageLen = strlen(messageBuffer);

            if (messageLen && messageLen < 256) {
                // can perform string checks

                if (messageBuffer[0] == '/') {
                    // this is a command from MiClone. Parse this
                    // todo
                }
                else if (messageBuffer[0] == '!') {
                    // command from my helper program
                    // todo
                    char command[17] = { 0 };
                    size_t offset = 0;
                    offset = parseCommandFromString(messageBuffer, sizeof(messageBuffer), command, sizeof(command));

                    if (!strcmp(command, "sd") || !strcmp(command, "spiffs")) {

                        // FS &fs = !strcmp(command, "!sd") ? *(dynamic_cast<fs::FS *>)(&SD) : *(dynamic_cast<fs::FS *>)(&SPIFFS);
                        
                        offset = nextSubString(messageBuffer, offset, sizeof(messageBuffer), command, sizeof(command));
                        bool commandError = false;      // true when there is an invalid input to the command
                        
                        if (offset) {
                            if (!strcmp(command, "read")) {
                                // File f = SD.open()
                                
                            }
                            else if (!strcmp(command, "write")) {

                            }
                            else {
                                commandError = true;
                            }
                        }
                        else {
                            commandError = true;
                        }

                        utilsPrint("~Invalid '!file' command. Options are: ~file [write | read] [filename] ");
                    }
                    else if (!strcmp(command, "ping")) {
                        Serial.println("pong!");
                    }
                    else if (!strcmp(command, "boot-factory")) {
                        const esp_partition_t *currentParition = esp_ota_get_running_partition();
                        const esp_partition_t *factoryPartition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, nullptr);
                        esp_ota_set_boot_partition(factoryPartition);

                        const esp_partition_t *newBootParition = esp_ota_get_boot_partition();

                        if (newBootParition == factoryPartition) {
                            Serial.print("Booting to factory partition in");
                            for (int i = 3; i > 0; --i) {
                                Serial.print(" 3...");
                                delay(1000);
                            }

                            ESP.restart();
                        }
                        else if (newBootParition == currentParition) {
                            Serial.println("Error: Failed to select factory partition. Next boot will be current partition");
                        }
                        else {
                            Serial.println("Error: Partition is booting to something else! It is neither factory or firmware (ota0)");
                        }
                    }
                }
            }
            else {
                // should I do something here?
            }
        }
        else
        {
            vTaskDelay(waitDelay);
        }
    }
}

void installFactoryFirmware(void *params) {

    File f = SD.open("/factory.bin", "r");
    size_t factorySize = f.size();
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, nullptr);

    if (partition->size < factorySize) {
        Serial.println("Factory firmware is larger than partion size. Aborting flash");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_ERROR_CHECK(esp_partition_erase_range(partition, 0, partition->size));
    
    size_t bytesRead = 0;
    while (f.available()) {
        char buffer[256];
        size_t currentTransaction = f.readBytes(buffer, sizeof(buffer));
        ESP_ERROR_CHECK(esp_partition_write(partition, bytesRead, buffer, currentTransaction));
        bytesRead += currentTransaction;
    }
    
    if (bytesRead != factorySize) {
        Serial.println("Uh oh... Data flashed to factory partition does not match the filesize of the factory.bin");
        Serial.println("This is not supposed to happen");
        assert(false);
    }
    
    f.close();
    SD.mkdir("/firmware");
    SD.remove("/firmware/factory.bin");
    SD.rename("/factory.bin", "/firmware/factory.bin");
    
    Serial.println("Successfully flashed factory firmware!");
    
    vTaskDelete(NULL);
};

void setup()
{
    Serial.begin(9600);
    Serial.println("Starting ota firmware v2");
    
    Serial.println("-> Mounting SPIFFS...");
    if (!SPIFFS.begin(true)) {
        Serial.println("Error - SPIFFS failed!");
        for(;;);
    }
    else {
        Serial.println("-> SPIFFS mounted.");
    }
    
    
    // pinMode(SD_CS, OUTPUT);
    
    // digitalWrite(SD_CS, HIGH);
    
    delay(1000);
    
    Common_Init();

    // digitalWrite(TCH_CS, LOW);
    
    // hspi->beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));

    // {
    //     char message[] = "Hello world! This is my sentence!";
    //     hspi->transferBytes((uint8_t *) &message, nullptr, sizeof(message));
    // }
    
    // hspi->endTransaction();
    
    Serial.println("Initializing SD card...");
    if(!SD.begin(SD_CS, *hspi, 4000000U))
    {
        Serial.println("Error: Cannot open MicroSD card. Check if inserted an mounter corectly");
        for(;;);
    }
    else {
        Serial.println("SD card mounted!");
        File verify = SD.open("/test.txt", "a+");
        verify.println("Print");
        verify.close();
    }
    digitalWrite(TCH_CS, HIGH);

    auto rebootToFactory = []() {
        const esp_partition_t *factoryParition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, nullptr);
        // todo what happens if factoryPartition is nullptr?
        ESP_ERROR_CHECK(esp_ota_set_boot_partition(factoryParition));
        ESP.restart();
    };

    // check if required to boot to factory
    if (SPIFFS.exists("/handoff")) {
        // handoff file does not exist, boot to factory
        Serial.println("-> Handoff file in SPIFFS found. Rebooting to factory firmware...");
        rebootToFactory();
    }
    
    // check if new ota firmware exists
    if (SD.exists("/firmware.bin")) {
        File firmwareFile = SD.open("/firmware.bin", "r");
        SPIFFS.remove("/handoff");
        rebootToFactory();
        firmwareFile.close();
    }


    // enables watchdog timer
    // esp_task_wdt_init();

    // check if new factory firmware exists
    if (SD.exists("/factory.bin")) {
        xTaskCreatePinnedToCore(installFactoryFirmware,
                                "factory_install",
                                800,
                                nullptr,
                                1,
                                nullptr,
                                !ARDUINO_RUNNING_CORE);
    }
    
    // setup usb c handler
    xTaskCreatePinnedToCore(handleUSBC,
                            "usbc_handler",
                            1000,
                            nullptr,
                            1,
                            &usbcHandler,
                            !ARDUINO_RUNNING_CORE);
        
    // setup RS-232
    Serial2.begin(9600);

    // Start Bluetooth
    // BLEDevice::init("MiOrigin_000");
    // BLE_Props.pServer = BLEDevice::createServer();
    // BLE_Props.device.pService = BLE_Props.pServer->createService(SERVICE_DEVICE_INFO_UUID);
    // BLE_Props.device.pDeviceName = BLE_Props.device.pService->createCharacteristic(CHARACTERISTIC_DEVICE_NAME_UUID,
    //                                                                                BLECharacteristic::PROPERTY_READ);
    // BLE_Props.device.pDeviceName->setValue("Test");

    // BLE_Props.pServer->getAdvertising()->stop();

    
}

void loop()
{
    vTaskDelay(10000 / portTICK_RATE_MS);
    // do nothing
}

#endif