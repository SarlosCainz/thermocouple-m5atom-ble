#include <M5Atom.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLECharacteristic.h>
#include <BLE2902.h>
#include "Adafruit_MCP9601.h"

#define I2C_ADDRESS (0x67)

#define SERVICE_UUID        "4FAFC201-1FB5-459E-8FCC-C5C9C331914B"
#define CHARACTERISTIC_UUID "ac8701a0-a844-4e22-91e3-66562a045ded"

struct info_t {
    float hot_junction = 0;
    bool connect = false;
    BLE2902 *ble2902 = new BLE2902();
} info;

BLEServer *bleServer = nullptr;
BLECharacteristic *bleCharacteristic = nullptr;

Adafruit_MCP9601 mcp;

void sendValue(bool notify) {
    bleCharacteristic->setValue((uint8_t*)&info.hot_junction, sizeof(float));
    if (notify) {
        bleCharacteristic->notify();
    }
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) override {
        info.connect = true;
        M5.dis.drawpix(0, 0x000080);
    };

    void onDisconnect(BLEServer *pServer) override {
        info.connect = false;
        info.ble2902->setNotifications(false);
        pServer->startAdvertising();
        M5.dis.drawpix(0, 0x00ff00);
    }
};

class MyCharacteristicCallback: public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic *pCharacteristic) override {
        sendValue(false);
    }

    void onNotify(BLECharacteristic* pCharacteristic) override {
        M5.dis.drawpix(0, 0x0000ff);
    }
};

void bleSetup() {
    BLEDevice::init(DEVICE_NAME);
    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new MyServerCallbacks());

    // Create Service
    BLEService *pService = bleServer->createService(SERVICE_UUID);

    // Create Characteristic
    bleCharacteristic = pService->createCharacteristic(
            CHARACTERISTIC_UUID,
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    bleCharacteristic->addDescriptor(info.ble2902);
    bleCharacteristic->setCallbacks(new MyCharacteristicCallback());

    pService->start();

    // Advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);

    BLEDevice::startAdvertising();
}

void setup() {
    M5.begin(true, false, true);
    delay(50);
    M5.dis.drawpix(0, 0);
    Serial.println("Setup start.");

    Wire.begin(26, 32);
    while (!mcp.begin(I2C_ADDRESS)) {
        Serial.println("MCP9601 initialize failed.");
        M5.dis.drawpix(0, 0xff0000);
        delay(1000);
    }
    mcp.setADCresolution(MCP9600_ADCRESOLUTION_18);
    mcp.setThermocoupleType(MCP9600_TYPE_K);
    mcp.setFilterCoefficient(3);
    mcp.enable(true);

    bleSetup();
    M5.dis.drawpix(0, 0x00ff00);
    Serial.println("Setup done.");
}

void loop() {
    uint8_t status = mcp.getStatus();
    if (status & MCP9601_STATUS_OPENCIRCUIT || status & MCP9601_STATUS_SHORTCIRCUIT) {
        M5.dis.drawpix(0, 0xff0000);
    } else if (info.connect){
        info.hot_junction = mcp.readThermocouple();
        if (info.ble2902->getNotifications()) {
            sendValue(true);
        }
    }

    delay(1000);
}
