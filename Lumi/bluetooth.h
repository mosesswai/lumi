#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID            "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX  "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX  "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

#define DEVICE_NAME             "Lumi"

BLEServer *BTServer = NULL;
BLECharacteristic * notifyCharacteristic;
BLECharacteristic * inputCharacteristic;
uint8_t txValue = 0;

void initBLE(BLEServerCallbacks *serverCallback, BLECharacteristicCallbacks *inputCallback) {
  // create the BLE Device
  BLEDevice::init(DEVICE_NAME);

  // create the BLE Server
  BTServer = BLEDevice::createServer();
  BTServer->setCallbacks(serverCallback);

  // create the BLE Service
  BLEService *BTService = BTServer->createService(SERVICE_UUID);

  // create the notification characteristic
  notifyCharacteristic = BTService->createCharacteristic(
										CHARACTERISTIC_UUID_TX,
										BLECharacteristic::PROPERTY_NOTIFY |
                    BLECharacteristic::PROPERTY_INDICATE
									);
                      
  notifyCharacteristic->addDescriptor(new BLE2902());

  // create the input characteristic
  inputCharacteristic = BTService->createCharacteristic(
											 CHARACTERISTIC_UUID_RX,
											BLECharacteristic::PROPERTY_WRITE
										);

  inputCharacteristic->setCallbacks(inputCallback);

  // start the service
  BTService->start();  
}

// make Lumi discoverable to other devices
void advertiseBT(){
  BTServer->getAdvertising()->start();
}

// hide Lumi from other devices
void hideBT(){
  BTServer->getAdvertising()->stop();
}

// publish text TX channel
template <typename T>
void publishBT(const T debugText){
  notifyCharacteristic->setValue(debugText);
  notifyCharacteristic->notify();
}