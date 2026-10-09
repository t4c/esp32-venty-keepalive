#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLE2902.h>

// ============================================================================
// CONFIGURATION
// ============================================================================
// Set to true to automatically disable haptic vibration on attach (avoids buzz on bounce)
// Set to false to keep device vibration untouched
const bool DISABLE_VIBRATION = true;

// Threshold in seconds (1-119). Watchdog resets timer when it falls below this.
const uint16_t KEEP_ALIVE_THRESHOLD_SECONDS = 30;

static BLEUUID serviceUUID("00000000-5354-4f52-5a26-4249434b454c");
static BLEUUID commCharUUID("00000001-5354-4f52-5a26-4249434b454c");

static boolean doConnect = false;
static boolean connected = false;
static BLEAdvertisedDevice* myDevice = nullptr;
static BLERemoteCharacteristic* pCommChar = nullptr;
static BLEClient* pClient = nullptr;

unsigned long lastPollTime = 0;
const unsigned long pollInterval = 1000;
static unsigned long lastKeepAliveTime = 0;
static unsigned long lastStatusPrintTime = 0;

static volatile float currentTargetTemp = 180.0;
static volatile float currentActualTemp = 0.0;
static float baseTargetTemp = 180.0;
static volatile uint16_t currentTimer = 120;
static volatile uint8_t currentBattery = 0;
static volatile uint8_t currentHeaterMode = 0;
static volatile bool triggerKeepAlive = false;
static bool sequenceInProgress = false;

static uint8_t statusRequestFrame[20] = {0x01, 0x00};

void printBanner() {
  Serial.println();
  Serial.println("⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣀⣾⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀");
  Serial.println("⠀⠀⠀⠀⠀⠀⠀⣠⡶⣶⠶⢦⣄⠀⠀⠀⠀⠀⠀⠠⣽⣿⣿⡶⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡴⠶⠶⢶⣄⠀⠀⠀⠀⠀⠀⠀");
  Serial.println("⠀⠀⠀⠀⠀⠀⣸⡏⠀⣤⠀⠀⢻⡆⠀⠀⠀⠀⠀⠀⠀⠻⠉⠉⠁⠀⠀⠀⠀⠠⡀⠀⠀⠀⠀⢠⣿⠁⠀⣦⠀⢹⣷⠀⠀⠀⠀⠀⠀");
  Serial.println("⠀⠀⠀⠀⠀⠀⢿⡆⠀⢧⡀⣀⡾⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢰⣷⠀⢀⡀⠀⠀⢿⣄⠀⣸⠀⢀⣿⠀⠀⠀⠀⠀⠀");
  Serial.println("⠀⠀⠀⠀⠀⠀⠘⢿⣄⠀⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⡀⠀⠀⠀⠀⠀⠘⢷⣤⣿⣶⣟⣁⠀⠀⠀⠈⠉⠀⣠⡾⠃⠀⠀⠀⠀⠀⠀");
  Serial.println("⠀⠀⣠⣤⣴⣶⣤⣄⠙⠷⣄⠀⠀⠀⠀⠀⡄⠀⠀⢠⡼⠁⠀⠀⠀⠀⠀⠀⠒⢻⢿⡿⠉⠉⠀⠀⠀⠀⢠⡶⠋⣡⣤⣶⣶⣤⣄⠀⠀");
  Serial.println("⢠⣾⠟⠁⠀⠀⠈⠹⣷⡄⠈⢷⠀⠀⠀⠀⣿⣄⣴⣿⠃⠀⠀⠀⠀⢠⡆⠀⠀⠀⠀⠁⠀⠀⠀⠀⠀⡼⠋⢠⣾⠟⠉⠀⠀⠈⠙⢷⡄");
  Serial.println("⢸⡇⠀⠀⠀⠀⡀⠀⠘⣿⡄⠀⠧⠈⠙⠷⣿⣿⣿⣧⣤⣤⠄⠀⠀⡞⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡾⠅⢠⣿⠋⠀⢀⠀⠀⠀⠀⢸⣿");
  Serial.println("⠘⣷⡄⠀⠀⣤⠇⠀⠀⣿⡇⠀⠀⠀⠀⠈⢩⠟⣿⢿⣯⠀⠀⠀⣴⣿⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠁⠀⢸⣿⠀⠀⠘⣆⠀⠀⢀⣼⠏");
  Serial.println("⠀⠀⠉⠉⠉⠁⠀⠀⠀⣿⡇⠀⣷⠀⠀⠀⠈⠀⠁⠀⠀⠀⠀⢸⣿⣿⣧⠀⠀⠀⠀⠀⠀⠀⠀⠀⢷⠀⢸⣿⠀⠀⠀⠈⠉⠙⠋⠁⠀");
  Serial.println("⠀⠀⠀⠀⠀⠀⠀⠀⢀⣿⠃⢀⡇⠀⠀⣀⠀⠀⠀⠀⠀⠀⢠⣿⣿⣿⣿⡀⠀⠀⠀⠀⠀⠀⠀⠀⢸⡄⠘⣿⡄⠀⠀⠀⠀⠀⠀⠀⠀");
  Serial.println("⠀⠀⠀⠀⠀⠀⠀⢀⣾⡟⠀⣼⠃⠀⠀⠙⣶⣤⣀⠀⠀⠀⢸⣿⣿⣿⣿⠀⠀⠀⠀⠀⢀⣀⡴⠂⠈⣷⡀⠻⣿⡀⠀⠀⠀⠀⠀⠀⠀");
  Serial.println("⠀⠀⠀⠀⠀⠀⢀⣾⠟⠀⣼⠏⠀⠀⠀⠀⠸⣿⣿⣿⣦⣄⠀⣿⣿⣿⣿⠀⢀⣠⣶⣿⣿⡿⠀⠀⠀⠹⣷⡀⠹⣷⡄⠀⠀⠀⠀⠀⠀");
  Serial.println("⠀⠀⠀⠀⠀⣠⡾⠋⢠⡾⠋⠀⠀⠀⠀⠀⠀⠘⢿⣿⣿⣿⣷⣿⣿⣿⣯⣾⣿⣿⣿⣿⠟⠀⠀⠀⠀⠀⠘⢷⣄⠙⢿⣄⠀⠀⠀⠀⠀");
  Serial.println("⠀⠀⠀⠀⣰⠟⠁⣰⠟⠁⠀⠀⠀⠀⠀⠀⠀⠀⣀⣙⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠛⠁⠀⠀⠀⠀⠀⠀⠀⠀⠻⣆⠈⠻⣆⠀⠀⠀⠀");
  Serial.println("⠀⠀⣠⠄⡟⠀⡼⠃⠀⠀⠀⠀⠀⠀⢀⡴⢾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣦⣄⡀⠀⠀⠀⠀⠀⠀⠈⠧⠀⢹⠂⣄⠀⠀");
  Serial.println("⠀⠀⢿⡄⠙⢦⡅⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⢩⣿⣿⠟⣿⠻⣿⣷⡍⠉⠉⠉⠀⠈⠉⠀⠀⠀⠀⠀⠀⠀⢀⡴⠋⣠⡿⠀⠀");
  Serial.println("⠀⠀⠀⠙⠳⠶⠁⢠⣴⣶⣶⣶⡦⢄⡀⠀⠀⠀⠀⠀⠈⠀⠀⠀⣿⠀⠀⠈⠳⠀⠀⠀⠀⠀⣀⠴⢒⣾⣿⣿⣿⣶⡄⠠⠶⠛⠁⠀⠀");
  Serial.println("⠀⠀⠀⠀⠀⠀⢰⣿⣿⣿⣿⣿⣿⠆⠉⠳⣤⣀⠀⠀⠀⠀⠀⠀⠻⠂⠀⠀⠀⠀⢀⣠⡴⠛⠁⢀⣻⣿⣿⣿⣿⣿⣧⠀⠀⠀⠀⠀⠀");
  Serial.println("⠀⠀⠀⠀⠀⠀⢸⣿⣿⣿⣿⣿⣿⠟⠀⠀⠈⠙⢿⡲⢦⣄⡀⠀⠀⠀⢀⣠⠴⢚⡽⠋⠀⠀⠀⠈⠻⣿⣿⣿⣿⣿⠇⠀⠀⠀⠀⠀⠀");
  Serial.println("⠀⠀⠀⠀⠀⠀⠀⠈⠻⢿⣿⡿⠅⠀⠀⠀⠀⠀⠀⠈⠀⠀⠉⣓⣤⠞⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣺⡿⠿⠋⠁⠀⠀⠀⠀⠀⠀⠀");
  Serial.println("⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⠓⠲⢤⣤⣤⡄⢀⣠⣤⣶⠾⠛⠁⠀⠀⠀⠀⠀⢠⠤⢤⣤⠶⠒⠋⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀");
  Serial.println("⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢈⡵⠾⠛⠋⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⣴⡾⠻⣅⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀");
  Serial.println("⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⠞⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣠⡤⠖⠛⠁⠀⠀⠈⠛⠦⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀");
  Serial.println("⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣴⠟⠁⠀⠀⠀⠀⠀⢀⣠⣤⡶⠖⠋⠹⢷⣦⣤⡀⠀⠀⠀⠀⠀⠀⠉⠳⣦⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀");
  Serial.println("⠀⠀⠀⠀⠀⠀⠀⢀⡴⠛⠋⠀⠀⣀⣀⣠⠤⠶⠒⠉⠉⠀⠀⠀⠀⠀⠀⠀⠉⠉⠒⠶⠦⢤⣀⣀⡀⠀⠈⠉⠳⣆⠀⠀⠀⠀⠀⠀⠀");
  Serial.println("⠀⠀⠀⠀⠀⠀⠀⠈⠻⠷⠒⠛⠋⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⠛⠛⠒⠿⠛⠀⠀⠀⠀⠀⠀⠀");
  Serial.println("⠀⠀⠀⠀⠀⠀⠀⣀⢀⣀⣀⣀⣀⣀⣀⢀⣀⣀⣀⡀⠀⠀⣀⣀⢀⠀⠀⠀⠀⠤⢀⠄⣀⠠⡄⣠⠠⢄⣠⢠⢄⠠⡠⡄⠀⠀⠀⠀⠀");
  Serial.println("⠀⠀⠀⠀⠀⠀⠂⠃⠛⠛⡓⠃⠓⠚⠘⡛⠛⢚⠓⠊⠓⠘⠒⠓⠙⠀⠀⠀⠀⠒⠀⠋⠓⠛⠊⠉⠙⠋⠙⠀⠋⠓⠁⠀⠀⠀⠀⠀⠀");
  Serial.println();
  Serial.println("+--------------------------------------------------+");
  Serial.println("|          VENTY KEEP ALIVE MOD by t4c             |");
  Serial.println("|              written for dRonin                  |");
  Serial.println("+--------------------------------------------------+");
  Serial.println("[SYSTEM] Starting watchdog daemon & initializing BLE stack...");
  Serial.println();
}

void sendHeaterMode(uint8_t mode) {
  if (pCommChar == nullptr || !pCommChar->canWrite()) return;

  uint8_t frame[20] = {0};
  frame[0] = 0x01; // Command.STATUS
  frame[1] = 0x20; // StatusWriteMask.HEATER (1 << 5)
  frame[11] = mode;

  pCommChar->writeValue(frame, 20, false);
}

void disableVibration() {
  if (pCommChar == nullptr || !pCommChar->canWrite()) return;

  uint8_t frame[7] = {0};
  frame[0] = 0x06; // Command.BRIGHTNESS_VIBRATION
  frame[1] = 0x08; // BrightnessVibrationWriteMask.VIBRATION (1 << 3)
  frame[5] = 0x00; // 0 = disabled

  pCommChar->writeValue(frame, 7, false);
  Serial.println("[CONFIG] Haptic vibration disabled on Venty.");
}

void executeAdaptiveReset() {
  sequenceInProgress = true;
  uint8_t activeMode = currentHeaterMode;

  Serial.printf("[WATCHDOG] Timer reached threshold (%ds) -> Executing keep-alive bounce (Mode: %d)...\n", currentTimer, activeMode);

  if (activeMode == 1) {
    // Normal heating mode: trigger Boost -> brief 0 -> return to 1
    sendHeaterMode(2);
    delay(300);
    sendHeaterMode(0);
    delay(150);
    sendHeaterMode(1);
    Serial.printf("[OK] Timer extended to 120s | Normal heating preserved (Target: %.1f C)\n", baseTargetTemp);
  } else if (activeMode == 2) {
    // Boost mode: unlock boost (0) -> heating on (1) -> re-engage boost (2)
    sendHeaterMode(0);
    delay(150);
    sendHeaterMode(1);
    delay(200);
    sendHeaterMode(2);
    Serial.println("[OK] Timer extended to 120s | Boost (Mode 2) locked and preserved");
  } else if (activeMode == 3) {
    // Superboost mode: unlock (0) -> heating on (1) -> re-engage superboost (3)
    sendHeaterMode(0);
    delay(150);
    sendHeaterMode(1);
    delay(200);
    sendHeaterMode(3);
    Serial.println("[OK] Timer extended to 120s | Superboost (Mode 3) locked and preserved");
  }

  delay(250);
  sequenceInProgress = false;
}

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
  
  if (length < 15) return;

  if (pData[0] == 0x01) {
    uint16_t curTempRaw = pData[2] | (pData[3] << 8);
    uint16_t tgtTempRaw = pData[4] | (pData[5] << 8);
    
    currentActualTemp = (curTempRaw == 0x8000) ? 0.0 : (curTempRaw / 10.0);
    currentTargetTemp = tgtTempRaw / 10.0;
    currentBattery    = pData[8];
    currentTimer      = pData[9] + pData[10];
    currentHeaterMode = pData[11];

    if (!sequenceInProgress && currentHeaterMode == 1) {
      baseTargetTemp = currentTargetTemp;
    }

    // Trigger keep-alive sequence if timer drops to or below threshold
    if (currentTimer <= KEEP_ALIVE_THRESHOLD_SECONDS && currentHeaterMode > 0 && !sequenceInProgress) {
      if (millis() - lastKeepAliveTime > 15000) {
        lastKeepAliveTime = millis();
        triggerKeepAlive = true;
      }
    }
  }
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) override {
    connected = true;
    Serial.println("[BLE] Connection established.");
  }

  void onDisconnect(BLEClient* pclient) override {
    connected = false;
    pCommChar = nullptr;
    Serial.println("[BLE] Connection lost! Scanning for device...");
  }
};

bool connectToServer() {
  Serial.print("[BLE] Connecting to target MAC: ");
  Serial.println(myDevice->getAddress().toString().c_str());

  if (pClient == nullptr) {
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback());
  }

  if (!pClient->connect(myDevice)) {
    Serial.println("[BLE] Connection attempt failed.");
    return false;
  }

  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    pClient->disconnect();
    return false;
  }

  pCommChar = pRemoteService->getCharacteristic(commCharUUID);
  if (pCommChar == nullptr) {
    pClient->disconnect();
    return false;
  }

  if (pCommChar->canNotify()) {
    pCommChar->registerForNotify(notifyCallback);
    
    BLERemoteDescriptor* p2902Desc = pCommChar->getDescriptor(BLEUUID((uint16_t)0x2902));
    if (p2902Desc != nullptr) {
      uint8_t val[] = {0x01, 0x00};
      p2902Desc->writeValue(val, 2, true);
    }
  }

  delay(200);
  if (DISABLE_VIBRATION) {
    disableVibration();
  }

  Serial.println("[STATUS] Venty successfully attached. Watchdog running.");
  return true;
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    if (advertisedDevice.haveName()) {
      String devName = advertisedDevice.getName().c_str();
      if (devName.startsWith("S&B VY")) {
        Serial.printf("[SCAN] Located candidate: %s\n", devName.c_str());
        BLEDevice::getScan()->stop();
        
        if (myDevice != nullptr) {
          delete myDevice;
        }
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnect = true;
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);

  printBanner();

  BLEDevice::init("ESP32-Venty-Daemon");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
}

void loop() {
  if (doConnect) {
    doConnect = false;
    connectToServer();
  }

  if (connected && pCommChar != nullptr) {
    if (triggerKeepAlive) {
      triggerKeepAlive = false;
      executeAdaptiveReset();
    }

    // Background polling interval
    if (!sequenceInProgress && (millis() - lastPollTime > pollInterval)) {
      lastPollTime = millis();
      if (pCommChar->canWrite()) {
        pCommChar->writeValue(statusRequestFrame, 20, false);
      }
    }

    // Telemetry output every 40 seconds
    if (millis() - lastStatusPrintTime > 40000) {
      lastStatusPrintTime = millis();

      const char* heaterStr = "STANDBY";
      if (currentHeaterMode == 1) heaterStr = "HEATING";
      else if (currentHeaterMode == 2) heaterStr = "BOOST";
      else if (currentHeaterMode == 3) heaterStr = "SUPERBOOST";

      Serial.printf("[UPTIME %05lus] Actual: %.1f C | Target: %.1f C | Batt: %d%% | Shutoff: %3ds | Mode: %s\n",
                    millis() / 1000, currentActualTemp, currentTargetTemp, currentBattery, currentTimer, heaterStr);
    }
  } else if (!connected && !doConnect) {
    static unsigned long lastScanTime = 0;
    if (millis() - lastScanTime > 4000) {
      lastScanTime = millis();
      Serial.println("[SCAN] Listening for S&B Venty BLE advertisements...");
      BLEDevice::getScan()->start(3, false);
    }
  }

  delay(50);
}
