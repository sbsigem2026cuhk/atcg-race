/*
 * E. coli Tamagotchi - iGEM Education Souvenir
 * ESP32-C3 version + 1.44" 128x128 ST7735 color TFT
 *
 * Libraries needed:
 *   - Adafruit ST7735 and ST7789 Library
 *   - Adafruit GFX Library
 *   - ESP32 board package in Arduino IDE
 *
 * IMPORTANT ESP32-C3 notes:
 *   - ESP32-C3 GPIO is 3.3V logic. Do not feed 5V into GPIO pins.
 *   - Use a 3.3V-safe TFT logic level, or add level shifting if your TFT module expects 5V logic.
 *
 * Suggested wiring (generic ESP32-C3 GPIO labels; adjust if your board labels differ):
 *   TFT: SCL->GPIO4, SDA->GPIO6, RES->GPIO5, DC->GPIO7, CS->GPIO10, BLK->3V3, VCC->3V3/VBUS if module-safe, GND->GND
 *   Buttons (other leg to GND): Feed->GPIO0, Menu->GPIO1, Up->GPIO2, Down->GPIO3
 *   Buzzer (optional): + -> GPIO8, - -> GND
 *
 * The original OLED game was drawn on a 128x64 canvas. This color build keeps the same logical
 * coordinates and scales Y positions to fill the 128x128 TFT.
 */

#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <esp_sleep.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertising.h>
#include <BLEServer.h>
#include <BLECharacteristic.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64   // logical height; Y coordinates are scaled to the 128px TFT
#define TFT_PHYSICAL_HEIGHT 128

// 1.44" ST7735 hardware SPI pins for ESP32-C3.
// The TFT module labels SCL/SDA are SPI clock/data, not I2C.
#define TFT_SCLK 4
#define TFT_MOSI 6
#define TFT_RST  5
#define TFT_DC   7
#define TFT_CS   10
// Optional: move TFT BLK from 3V3 to this GPIO if you want deep sleep to turn off backlight.
// Leave -1 when BLK is wired directly to 3V3.
#define PIN_TFT_BLK -1

#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_GREEN   0x07E0
#define COLOR_YELLOW  0xFFE0
#define COLOR_ORANGE  0xFD20
#define COLOR_BLUE    0x001F
#define COLOR_RED     0xF800

#define SSD1306_WHITE COLOR_WHITE
#define SSD1306_BLACK COLOR_BLACK
#define SSD1306_SWITCHCAPVCC 0
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON  0xAF
#define SCREEN_ADDR 0x3C

class ScaledST7735 : public Print {
public:
  ScaledST7735(int8_t cs, int8_t dc, int8_t mosi, int8_t sclk, int8_t rst)
    : tft(cs, dc, rst), mosiPin(mosi), sclkPin(sclk), csPin(cs), canvas(SCREEN_WIDTH, TFT_PHYSICAL_HEIGHT) {}

  bool begin(uint8_t, uint8_t) {
    SPI.begin(sclkPin, -1, mosiPin, csPin);
    tft.initR(INITR_144GREENTAB);
    tft.setRotation(1);
    tft.setSPISpeed(40000000);
    tft.fillScreen(COLOR_BLACK);
    canvas.fillScreen(COLOR_BLACK);
    return true;
  }

  void clearDisplay() { canvas.fillScreen(COLOR_BLACK); }

  void display() {
    tft.drawRGBBitmap(0, 0, canvas.getBuffer(), SCREEN_WIDTH, TFT_PHYSICAL_HEIGHT);
  }

  void ssd1306_command(uint8_t cmd) {
    if (cmd == SSD1306_DISPLAYOFF) {
      canvas.fillScreen(COLOR_BLACK);
      tft.fillScreen(COLOR_BLACK);
      tft.sendCommand(ST77XX_DISPOFF);
      delay(10);
      tft.sendCommand(ST77XX_SLPIN);
    } else if (cmd == SSD1306_DISPLAYON) {
      tft.sendCommand(ST77XX_SLPOUT);
      delay(120);
      tft.sendCommand(ST77XX_DISPON);
      display();
    }
  }

  void setCursor(int16_t x, int16_t y) { canvas.setCursor(x, sy(y)); }
  void setTextSize(uint8_t s) { canvas.setTextSize(s); }
  void setTextColor(uint16_t color) { canvas.setTextColor(color); }
  void setTextColor(uint16_t color, uint16_t bg) { canvas.setTextColor(color, bg); }

  size_t write(uint8_t c) override { return canvas.write(c); }

  void drawPixel(int16_t x, int16_t y, uint16_t color) { canvas.drawPixel(x, sy(y), color); }
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) { canvas.drawFastHLine(x, sy(y), w, color); }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) { canvas.drawFastVLine(x, sy(y), sh(h), color); }
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) { canvas.drawLine(x0, sy(y0), x1, sy(y1), color); }
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { canvas.drawRect(x, sy(y), w, sh(h), color); }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { canvas.fillRect(x, sy(y), w, sh(h), color); }
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) { canvas.fillRoundRect(x, sy(y), w, sh(h), r, color); }
  void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) { canvas.fillCircle(x, sy(y), r, color); }
  void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color) { canvas.drawCircle(x, sy(y), r, color); }
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) { canvas.fillTriangle(x0, sy(y0), x1, sy(y1), x2, sy(y2), color); }

private:
  Adafruit_ST7735 tft;
  int8_t mosiPin;
  int8_t sclkPin;
  int8_t csPin;
  GFXcanvas16 canvas;

  int16_t sy(int16_t y) const { return y * 2; }
  int16_t sh(int16_t h) const { return h * 2; }
};

ScaledST7735 display(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// ESP32-C3 pins (GPIO numbers)
#define PIN_BTN_FEED  0
#define PIN_BTN_MENU  1   // open menu / confirm
#define PIN_BTN_UP    2   // menu: scroll up
#define PIN_BTN_DOWN  3   // menu: scroll down
#define PIN_BUZZER    8

// ---------- Stats ----------
unsigned long colony = 1;
uint8_t atp     = 80;
uint8_t stress  = 0;
uint8_t acetate = 0;          // waste ("poop"), produced from ATP use
uint8_t atpSinceAcetate = 0;  // counts ATP units consumed toward next acetate (15 per 1)
unsigned int atpUsedForGrowth = 0; // counts ATP consumed toward next colony growth (180 per growth)

// ---------- Timing ----------
unsigned long lastDecay = 0;
const unsigned long DECAY_INTERVAL_MS = 120000;      // 2 minutes
unsigned long lastStressShrinkTime = 0;
const unsigned long STRESS_SHRINK_MS = 3600000;      // 1 hour
const unsigned int ATP_FOR_COLONY_GROWTH = 180;
unsigned long lastBlink = 0;
bool eyesOpen = true;
int frame = 0;

// ---------- Buttons ----------
bool feedPressed  = false;
bool menuPressed  = false;
bool upPressed    = false;
bool downPressed  = false;

unsigned long lastButtonTime = 0;

// ---------- Egg hatching ----------
#define EGG_FEEDS_TO_HATCH  5
bool gameStateIsPet = false;

// ---------- Full state save (16 bytes, survives power loss) ----------
#define SAVE_SIZE          16
#define SAVE_MAGIC       0xEC
#define SAVE_THROTTLE_MS   5000   // min interval between non-forced saves (flash wear)
const char* SAVE_NAMESPACE = "ecoli";
const char* SAVE_KEY = "state";
const char* MBTI_KEY = "mbti";
const char* MBTI_MASK_KEY = "mbtimask";
const char* REWARD_MASK_KEY = "rewmask";
Preferences prefs;
unsigned long lastSaveTime = 0;
// Layout: [0]=magic, [1]=hatched, [2]=atp, [3]=stress, [4]=acetate, [5]=atpSinceAcetate,
//         [6-7]=atpUsedForGrowth, [8]=petSleeping, [9]=currentScreen, [10]=eggFeedCount,
//         [11-14]=colony (4 bytes LSB first), [15]=checksum (XOR of 1..14)
uint8_t eggFeedCount = 0;
bool eggFeedPressed = false;

// ---------- MBTI genetics ----------
#define MBTI_COUNT 16
uint8_t activeMbti = 0;
uint16_t mbtiMask = 0;
uint8_t expressionCursor = 0;
uint16_t phoneRewardMask = 0;

#define PHONE_REWARD_COUNT 12

const char* phoneRewardLabel(uint8_t id) {
  switch (id) {
    case 0:  return "GFP gene";
    default: return "TBC";
  }
}

const char* mbtiLabel(uint8_t id) {
  switch (id) {
    case 0:  return "INTJ";
    case 1:  return "INTP";
    case 2:  return "ENTJ";
    case 3:  return "ENTP";
    case 4:  return "INFJ";
    case 5:  return "INFP";
    case 6:  return "ENFJ";
    case 7:  return "ENFP";
    case 8:  return "ISTJ";
    case 9:  return "ISFJ";
    case 10: return "ESTJ";
    case 11: return "ESFJ";
    case 12: return "ISTP";
    case 13: return "ISFP";
    case 14: return "ESTP";
    default: return "ESFP";
  }
}

void saveMbtiData() {
  prefs.begin(SAVE_NAMESPACE, false);
  prefs.putUChar(MBTI_KEY, activeMbti);
  prefs.putUShort(MBTI_MASK_KEY, mbtiMask);
  prefs.putUShort(REWARD_MASK_KEY, phoneRewardMask);
  prefs.end();
}

void loadMbtiData() {
  prefs.begin(SAVE_NAMESPACE, true);
  activeMbti = prefs.getUChar(MBTI_KEY, 255);
  mbtiMask = prefs.getUShort(MBTI_MASK_KEY, 0);
  phoneRewardMask = prefs.getUShort(REWARD_MASK_KEY, 0);
  prefs.end();

  if (activeMbti < MBTI_COUNT) {
    mbtiMask |= (1U << activeMbti);
  }
}

void assignRandomMbti() {
  activeMbti = (uint8_t)random(MBTI_COUNT);
  mbtiMask = (1U << activeMbti);
  expressionCursor = activeMbti;
  saveMbtiData();
}

void addMbti(uint8_t id) {
  if (id >= MBTI_COUNT) return;
  mbtiMask |= (1U << id);
  saveMbtiData();
}

void addPhoneReward(uint8_t id) {
  if (id >= PHONE_REWARD_COUNT) return;
  phoneRewardMask |= (1U << id);
  saveMbtiData();
}

// ---------- Pet sleep ----------
bool petSleeping = false;
#define WAKE_HOLD_MS  3000
#define SLEEP_SCREEN_OFF_MS  5000  // in sleep: ZZZ shown, then blank TFT if no buttons
unsigned long menuHoldStart = 0;
bool menuIsHeld = false;
bool wakeHandled = false;
bool sleepDisplayOff = false;       // true = TFT blank (stats still run)
unsigned long lastSleepIdleTime = 0;

// ---------- Screens ----------
// 0 = main pet, 1 = menu, 2 = status, 3 = reset confirm, 4 = DNA game
// 5 = transformation code input, 6 = BLE searching, 7 = paired menu
// 8 = expression list, 9 = gene transfer waiting
// 10 = phone transformation code input, 11 = phone command wait
uint8_t currentScreen = 0;

// ---------- Menu ----------
#define MENU_COUNT  9
void printMenuLabel(uint8_t id) {
  switch (id) {
    case 0: display.print(F("Status"));         break;
    case 1: display.print(F("Clean Poop"));     break;
    case 2: display.print(F("Play"));           break;
    case 3: display.print(F("Conjugagtion"));   break;
    case 4: display.print(F("Transformation")); break;
    case 5: display.print(F("Expression"));     break;
    case 6: display.print(F("Sleep"));          break;
    case 7: display.print(F("Exit"));           break;
    case 8: display.print(F("Reset"));          break;
  }
}
uint8_t menuCursor = 0;
uint8_t menuWinTop = 0;

// Keep 3-line menu window aligned with menuCursor (scroll up or down)
void adjustMenuWindow() {
  if (menuCursor < menuWinTop)
    menuWinTop = menuCursor;
  else if (menuCursor >= menuWinTop + 3)
    menuWinTop = menuCursor - 2;
}

// ---------- DNA Pair Game (screen 4) ----------
#define DNA_A 1
#define DNA_T 2
#define DNA_C 3
#define DNA_G 4
#define DNA_CELL 9
#define DNA_GX   42
#define DNA_GY   14
#define DNA_FALL_MS 500

uint8_t dnaGrid[5][5];    // [row][col], row 0 = bottom
int8_t  dnaCol, dnaRow;    // falling piece position (row 5 = above grid)
uint8_t dnaBase;           // falling piece type 1-4
uint16_t dnaScore;
unsigned long dnaLastFall;
bool dnaOver, dnaWon;

// ---------- Reset confirm (screen 3) ----------
#define RESET_HOLD_MS  3000
unsigned long resetHoldStart = 0;  // 0 = not holding any button

// ---------- BLE Transformation Pairing ----------
// First implementation: same-number BLE discovery only. Actual data transfer/battle actions are placeholders.
BLEScan* transScan = nullptr;
BLEAdvertising* transAdvertising = nullptr;
bool transBleReady = false;
bool transSearching = false;
bool transFound = false;
uint8_t transPassword = 0;
uint8_t transOptionCursor = 0;  // paired menu: 0=Horizontal Gene Transfer, 1=Battle
uint16_t transDeviceId = 0;
unsigned long lastTransScan = 0;
bool geneTransferActive = false;
bool geneTransferFound = false;
uint8_t incomingMbti = 255;
unsigned long lastGeneScan = 0;
uint16_t transPeerId = 0;

// ---------- Phone Transformation BLE server ----------
const char* PHONE_SERVICE_UUID = "6b7f1001-7a6d-4d8e-9c4b-ec0111110001";
const char* PHONE_COMMAND_UUID = "6b7f1002-7a6d-4d8e-9c4b-ec0111110001";
BLEServer* phoneServer = nullptr;
BLEService* phoneService = nullptr;
BLECharacteristic* phoneCommandChar = nullptr;
bool phoneBleReady = false;
bool phoneActive = false;
bool phoneConnected = false;
bool phoneCommandReceived = false;
uint16_t phoneConnId = 0xFFFF;
String phoneLastCommand = "";

String transDeviceName() {
  String name = "ECOLI-";
  name += transPassword;
  name += "-";
  if (transDeviceId < 0x1000) name += "0";
  if (transDeviceId < 0x0100) name += "0";
  if (transDeviceId < 0x0010) name += "0";
  name += String(transDeviceId, HEX);
  name.toUpperCase();
  return name;
}

String geneTransferDeviceName() {
  String name = "ECGT-";
  name += transPassword;
  name += "-";
  name += activeMbti;
  name += "-";
  if (transDeviceId < 0x1000) name += "0";
  if (transDeviceId < 0x0100) name += "0";
  if (transDeviceId < 0x0010) name += "0";
  name += String(transDeviceId, HEX);
  name.toUpperCase();
  return name;
}

class PhoneServerCallbacks : public BLEServerCallbacks {
public:
  void onConnect(BLEServer* pServer) {
    phoneConnected = true;
  }

  void onDisconnect(BLEServer* pServer) {
    phoneConnected = false;
    phoneConnId = 0xFFFF;
    if (phoneActive && transAdvertising) transAdvertising->start();
  }
};

class PhoneCommandCallbacks : public BLECharacteristicCallbacks {
public:
  void onWrite(BLECharacteristic* characteristic) {
    phoneLastCommand = characteristic->getValue();
    phoneCommandReceived = true;
  }
};

class TransAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
public:
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (!advertisedDevice.haveName()) return;

    auto rawName = advertisedDevice.getName();
    String foundName = String(rawName.c_str());
    foundName.toUpperCase();

    if (transSearching && !transFound) {
      String prefix = "ECOLI-";
      prefix += transPassword;
      prefix += "-";

      String selfName = transDeviceName();
      if (foundName.startsWith(prefix) && foundName != selfName) {
        transPeerId = (uint16_t)strtoul(foundName.substring(prefix.length()).c_str(), nullptr, 16);
        transFound = true;
      }
    }

    if (geneTransferActive && !geneTransferFound) {
      String prefix = "ECGT-";
      prefix += transPassword;
      prefix += "-";

      String selfName = geneTransferDeviceName();
      if (foundName.startsWith(prefix) && foundName != selfName) {
        int firstDash = foundName.indexOf('-', 5);
        int secondDash = foundName.indexOf('-', firstDash + 1);
        if (firstDash > 0 && secondDash > firstDash) {
          incomingMbti = (uint8_t)foundName.substring(firstDash + 1, secondDash).toInt();
          if (incomingMbti < MBTI_COUNT) geneTransferFound = true;
        }
      }
    }

  }
};

// ============================= Preferences save/load =============================
void saveState(bool force) {
  if (!force && (millis() - lastSaveTime < SAVE_THROTTLE_MS)) return;
  uint8_t buf[SAVE_SIZE];
  buf[0] = SAVE_MAGIC;
  buf[1] = gameStateIsPet ? 1 : 0;
  buf[2] = atp;
  buf[3] = stress;
  buf[4] = acetate;
  buf[5] = atpSinceAcetate;
  buf[6] = (uint8_t)(atpUsedForGrowth & 0xFF);
  buf[7] = (uint8_t)((atpUsedForGrowth >> 8) & 0xFF);
  buf[8] = petSleeping ? 1 : 0;
  buf[9] = currentScreen;
  buf[10] = eggFeedCount;
  unsigned long c = colony;
  buf[11] = (uint8_t)(c & 0xFF);
  buf[12] = (uint8_t)((c >> 8) & 0xFF);
  buf[13] = (uint8_t)((c >> 16) & 0xFF);
  buf[14] = (uint8_t)((c >> 24) & 0xFF);
  uint8_t sum = 0;
  for (int i = 1; i <= 14; i++) sum ^= buf[i];
  buf[15] = sum;
  prefs.begin(SAVE_NAMESPACE, false);
  prefs.putBytes(SAVE_KEY, buf, SAVE_SIZE);
  prefs.end();
  lastSaveTime = millis();
}

// Returns true if a valid saved state was loaded.
bool loadState() {
  uint8_t buf[SAVE_SIZE];
  prefs.begin(SAVE_NAMESPACE, true);
  size_t len = prefs.getBytesLength(SAVE_KEY);
  if (len != SAVE_SIZE) {
    prefs.end();
    return false;
  }
  prefs.getBytes(SAVE_KEY, buf, SAVE_SIZE);
  prefs.end();
  if (buf[0] != SAVE_MAGIC) return false;
  uint8_t sum = 0;
  for (int i = 1; i <= 14; i++) sum ^= buf[i];
  if (sum != buf[15]) return false;
  gameStateIsPet = (buf[1] != 0);
  atp = buf[2];
  stress = buf[3];
  acetate = buf[4];
  atpSinceAcetate = buf[5];
  atpUsedForGrowth = (unsigned int)buf[6] | ((unsigned int)buf[7] << 8);
  // Deep sleep resumes by restarting setup(), so always wake into the main screen.
  petSleeping = false;
  sleepDisplayOff = false;
  currentScreen = buf[9];
  if (currentScreen >= 3) currentScreen = 0;  // never restore reset-confirm or old plasmid screens
  eggFeedCount = buf[10];
  colony = (unsigned long)buf[11] | ((unsigned long)buf[12] << 8) |
           ((unsigned long)buf[13] << 16) | ((unsigned long)buf[14] << 24);
  if (colony == 0) colony = 1;
  return true;
}

// ============================= BLE transformation helpers =============================
void initTransformationBle() {
  if (transBleReady) return;
  transDeviceId = (uint16_t)(esp_random() & 0xFFFF);
  BLEDevice::init("EcoliTama");
  transScan = BLEDevice::getScan();
  transScan->setAdvertisedDeviceCallbacks(new TransAdvertisedDeviceCallbacks(), true);
  transScan->setActiveScan(true);
  transScan->setInterval(100);
  transScan->setWindow(80);
  transAdvertising = BLEDevice::getAdvertising();
  transBleReady = true;
}

void startTransformationSearch() {
  initTransformationBle();
  transFound = false;
  transSearching = true;
  lastTransScan = 0;

  BLEAdvertisementData advData;
  advData.setName(transDeviceName().c_str());
  transAdvertising->stop();
  transAdvertising->setAdvertisementData(advData);
  transAdvertising->start();
}

void stopTransformationSearch() {
  if (transScan) transScan->stop();
  if (transAdvertising) transAdvertising->stop();
  transSearching = false;
}

void runTransformationScan(unsigned long now) {
  if (!transSearching || transFound || !transScan) return;
  if (now - lastTransScan < 800) return;
  lastTransScan = now;
  transScan->start(1, false);
  transScan->clearResults();
}

void startGeneTransferWait() {
  initTransformationBle();
  geneTransferActive = true;
  geneTransferFound = false;
  incomingMbti = 255;
  lastGeneScan = 0;

  BLEAdvertisementData advData;
  advData.setName(geneTransferDeviceName().c_str());
  transAdvertising->stop();
  transAdvertising->setAdvertisementData(advData);
  transAdvertising->start();
}

void stopGeneTransferWait() {
  if (transScan) transScan->stop();
  if (transAdvertising) transAdvertising->stop();
  geneTransferActive = false;
}

void runGeneTransferScan(unsigned long now) {
  if (!geneTransferActive || geneTransferFound || !transScan) return;
  if (now - lastGeneScan < 800) return;
  lastGeneScan = now;
  transScan->start(1, false);
  transScan->clearResults();
}

String phoneDeviceName() {
  String name = "EC-PHONE-";
  name += transPassword;
  name += "-";
  if (transDeviceId < 0x1000) name += "0";
  if (transDeviceId < 0x0100) name += "0";
  if (transDeviceId < 0x0010) name += "0";
  name += String(transDeviceId, HEX);
  name.toUpperCase();
  return name;
}

void initPhoneBle() {
  initTransformationBle();
  if (phoneBleReady) return;

  phoneServer = BLEDevice::createServer();
  phoneServer->setCallbacks(new PhoneServerCallbacks());
  phoneService = phoneServer->createService(PHONE_SERVICE_UUID);
  phoneCommandChar = phoneService->createCharacteristic(
    PHONE_COMMAND_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );
  phoneCommandChar->setCallbacks(new PhoneCommandCallbacks());
  phoneCommandChar->setValue("WAIT");
  phoneService->start();
  phoneBleReady = true;
}

void startPhoneTransformationWait() {
  initPhoneBle();
  phoneActive = true;
  phoneConnected = false;
  phoneCommandReceived = false;
  phoneLastCommand = "";
  phoneConnId = 0xFFFF;

  BLEAdvertisementData advData;
  BLEAdvertisementData scanData;
  advData.setFlags(0x06);  // General discoverable, BLE-only
  advData.setCompleteServices(BLEUUID(PHONE_SERVICE_UUID));
  scanData.setName(phoneDeviceName().c_str());
  transAdvertising->stop();
  transAdvertising->setAdvertisementData(advData);
  transAdvertising->setScanResponseData(scanData);
  transAdvertising->start();
}

void stopPhoneTransformationWait() {
  if (phoneServer && phoneConnected) {
    phoneServer->disconnect(0);
  }
  if (transAdvertising) transAdvertising->stop();
  phoneActive = false;
  phoneConnected = false;
  phoneCommandReceived = false;
  phoneConnId = 0xFFFF;
}

void showCenteredMessage(const __FlashStringHelper* msg, unsigned long ms) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(14, 28);
  display.print(msg);
  display.display();
  delay(ms);
}

void drawPhoneTransformationFrame(const __FlashStringHelper* label, int tempC, uint8_t iceLevel, uint8_t pores, int plasmidX, bool plasmidInside) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("Temp: "));
  display.print(tempC);
  display.print(F(" C"));

  display.fillRoundRect(44, 25, 40, 18, 7, SSD1306_WHITE);
  display.fillCircle(56, 32, 1, SSD1306_BLACK);
  display.fillCircle(70, 32, 1, SSD1306_BLACK);
  display.drawLine(50, 43, 45, 48, SSD1306_WHITE);
  display.drawLine(78, 43, 83, 48, SSD1306_WHITE);

  for (uint8_t i = 0; i < iceLevel; i++) {
    int x = 28 + (i * 17) % 72;
    int y = 18 + (i * 11) % 31;
    display.drawLine(x - 2, y, x + 2, y, SSD1306_WHITE);
    display.drawLine(x, y - 2, x, y + 2, SSD1306_WHITE);
  }

  for (uint8_t i = 0; i < pores; i++) {
    int x = 48 + i * 7;
    int y = (i % 2 == 0) ? 26 : 41;
    display.fillCircle(x, y, 2, SSD1306_BLACK);
  }

  if (plasmidX >= 0) {
    if (plasmidInside) {
      display.drawCircle(63, 34, 5, SSD1306_BLACK);
      display.drawPixel(63, 29, SSD1306_BLACK);
    } else {
      display.drawCircle(plasmidX, 34, 5, SSD1306_WHITE);
      display.drawPixel(plasmidX, 29, SSD1306_WHITE);
    }
  }

  display.setCursor(0, 56);
  display.print(label);
  display.display();
}

void playPhoneTransformationAnimation() {
  for (uint8_t step = 0; step <= 40; step++) {
    int tempC = 36 - (36 * step) / 40;
    drawPhoneTransformationFrame(F("Ice Incubation..."), tempC, 8, 0, -1, false);
    delay(100);
  }

  for (uint8_t step = 0; step <= 42; step++) {
    drawPhoneTransformationFrame(F("Heat Shocking..."), step, 0, 0, -1, false);
    delay(45);
  }

  for (uint8_t pores = 1; pores <= 5; pores++) {
    drawPhoneTransformationFrame(F("Heat Shocking..."), 42, 0, pores, -1, false);
    delay(280);
  }

  for (int x = 114; x >= 64; x -= 4) {
    bool inside = x <= 68;
    drawPhoneTransformationFrame(F("Heat Shocking..."), 42, 0, 5, x, inside);
    delay(120);
  }

  for (int tempC = 42; tempC >= 36; tempC--) {
    drawPhoneTransformationFrame(F("Recovering..."), tempC, 0, 5, 64, true);
    delay(180);
  }

  for (int pores = 5; pores >= 0; pores--) {
    drawPhoneTransformationFrame(F("Recovering..."), 36, 0, pores, 64, true);
    delay(260);
  }
}

void playHorizontalGeneTransferAnimation() {
  // Conjugation-style HGT: two cells connect with a tube, then a gene segment moves across.
  for (uint8_t step = 0; step <= 12; step++) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("Horizontal GT"));
    display.fillRoundRect(12, 26, 34, 14, 6, SSD1306_WHITE);
    display.fillRoundRect(82, 26, 34, 14, 6, SSD1306_WHITE);
    display.fillCircle(24, 31, 1, SSD1306_BLACK);
    display.fillCircle(34, 31, 1, SSD1306_BLACK);
    display.fillCircle(94, 31, 1, SSD1306_BLACK);
    display.fillCircle(104, 31, 1, SSD1306_BLACK);
    int tubeEnd = 46 + step * 3;
    display.drawLine(46, 33, tubeEnd, 33, SSD1306_WHITE);
    display.display();
    delay(100);
  }

  for (uint8_t step = 0; step <= 24; step++) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("Gene passing"));
    display.fillRoundRect(12, 26, 34, 14, 6, SSD1306_WHITE);
    display.fillRoundRect(82, 26, 34, 14, 6, SSD1306_WHITE);
    display.drawLine(46, 33, 82, 33, SSD1306_WHITE);
    int geneX = 48 + step;
    display.drawFastHLine(geneX, 30, 10, SSD1306_WHITE);
    display.drawFastHLine(geneX, 36, 10, SSD1306_WHITE);
    display.drawLine(geneX, 30, geneX + 10, 36, SSD1306_WHITE);
    display.drawLine(geneX, 36, geneX + 10, 30, SSD1306_WHITE);
    display.display();
    delay(70);
  }
}

void playConjugationPairAnimation() {
  // Pair-up only: cells form a conjugation bridge, with no gene segment movement.
  for (uint8_t step = 0; step <= 12; step++) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("Conjugagtion"));
    display.fillRoundRect(12, 26, 34, 14, 6, SSD1306_WHITE);
    display.fillRoundRect(82, 26, 34, 14, 6, SSD1306_WHITE);
    display.fillCircle(24, 31, 1, SSD1306_BLACK);
    display.fillCircle(34, 31, 1, SSD1306_BLACK);
    display.fillCircle(94, 31, 1, SSD1306_BLACK);
    display.fillCircle(104, 31, 1, SSD1306_BLACK);
    int tubeEnd = 46 + step * 3;
    display.drawLine(46, 33, tubeEnd, 33, SSD1306_WHITE);
    display.display();
    delay(120);
  }

  delay(400);
}

void finishGeneTransfer() {
  stopGeneTransferWait();
  playHorizontalGeneTransferAnimation();
  if (incomingMbti < MBTI_COUNT) addMbti(incomingMbti);
  showCenteredMessage(F("Success"), 2000);
  currentScreen = 7;
  transOptionCursor = 0;
  feedPressed = menuPressed = upPressed = downPressed = true;
}

void handlePhoneCommand(String command) {
  command.trim();
  command.toUpperCase();

  if (command == "GENE:GFP" || command == "REWARD:GFP" || command == "GFP") {
    playPhoneTransformationAnimation();
    addPhoneReward(0);
    showCenteredMessage(F("Success"), 2000);
    stopPhoneTransformationWait();
    currentScreen = 0;
    feedPressed = menuPressed = upPressed = downPressed = true;
    return;
  }

  showCenteredMessage(F("Unknown Cmd"), 1200);
}

void showPairedThenMenu() {
  stopTransformationSearch();
  beep(400, 250);
  playConjugationPairAnimation();
  transOptionCursor = 0;
  currentScreen = 7;
  feedPressed = menuPressed = upPressed = downPressed = true;
}

void enterDeepSleep() {
  stopTransformationSearch();
  stopGeneTransferWait();
  stopPhoneTransformationWait();

  petSleeping = false;
  sleepDisplayOff = true;
  currentScreen = 0;
  saveState(true);

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(16, 24);
  display.print(F("Deep Sleep"));
  display.setCursor(8, 38);
  display.print(F("Press btn wake"));
  display.display();
  delay(700);

  display.ssd1306_command(SSD1306_DISPLAYOFF);

#if PIN_TFT_BLK >= 0
  pinMode(PIN_TFT_BLK, OUTPUT);
  digitalWrite(PIN_TFT_BLK, LOW);
#endif

  BLEDevice::deinit(true);

  uint64_t wakeMask = (1ULL << PIN_BTN_FEED) |
                      (1ULL << PIN_BTN_MENU) |
                      (1ULL << PIN_BTN_UP) |
                      (1ULL << PIN_BTN_DOWN);
  esp_deep_sleep_enable_gpio_wakeup(wakeMask, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_deep_sleep_start();
}

// ============================= setup =============================
void setup() {
  pinMode(PIN_BTN_FEED,  INPUT_PULLUP);
  pinMode(PIN_BTN_MENU,  INPUT_PULLUP);
  pinMode(PIN_BTN_UP,    INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN,  INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
#if PIN_TFT_BLK >= 0
  pinMode(PIN_TFT_BLK, OUTPUT);
  digitalWrite(PIN_TFT_BLK, HIGH);
#endif

  randomSeed(esp_random());
  loadMbtiData();

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDR)) {
    for (;;) delay(1000);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();
  lastButtonTime = millis();

  if (loadState()) {
    lastDecay = millis();
    lastStressShrinkTime = millis();
    if (gameStateIsPet && activeMbti >= MBTI_COUNT) assignRandomMbti();
  } else {
    gameStateIsPet = false;
    eggFeedCount = 0;
  }
}

// ============================= loop =============================
void loop() {
  unsigned long now = millis();

  // ===================== EGG MODE =====================
  if (!gameStateIsPet) {
    int shake = 0;
    switch ((now / 150) % 4) {
      case 0: shake = -2; break;
      case 2: shake =  2; break;
      default: shake = 0; break;
    }
    if (digitalRead(PIN_BTN_FEED) == LOW) {
      if (!eggFeedPressed) {
        eggFeedPressed = true;
        if (eggFeedCount < EGG_FEEDS_TO_HATCH) {
          eggFeedCount++;
          beep(300, 200);
        }
        if (eggFeedCount >= EGG_FEEDS_TO_HATCH) {
          gameStateIsPet = true;
          petSleeping = false;
          currentScreen = 0;
          colony = 1; atp = 80; stress = 0;
          atpSinceAcetate = 0; atpUsedForGrowth = 0; acetate = 0;
          assignRandomMbti();
          lastDecay = now;
          lastStressShrinkTime = now;
          lastButtonTime = now;
          saveState(true);
          beep(400, 300);
        }
      }
    } else {
      eggFeedPressed = false;
    }
    display.clearDisplay();
    drawEgg(shake, eggFeedCount);
    display.display();
    delay(80);
    return;
  }

  // ===================== PET SLEEPING (ZZZ, then TFT off after idle; stats still tick) =====================
  if (petSleeping) {
    bool anyWakeBtn = (digitalRead(PIN_BTN_FEED) == LOW || digitalRead(PIN_BTN_MENU) == LOW ||
                       digitalRead(PIN_BTN_UP) == LOW || digitalRead(PIN_BTN_DOWN) == LOW);
    if (anyWakeBtn) {
      if (sleepDisplayOff) {
        display.ssd1306_command(SSD1306_DISPLAYON);
        sleepDisplayOff = false;
      }
      lastSleepIdleTime = now;
    }

    // Long-press any button (3 s) to leave sleep (after screen is on)
    if (anyWakeBtn) {
      if (!menuIsHeld) {
        menuIsHeld = true;
        menuHoldStart = now;
        wakeHandled = false;
      } else if (!wakeHandled && (now - menuHoldStart >= WAKE_HOLD_MS)) {
        wakeHandled = true;
        petSleeping = false;
        sleepDisplayOff = false;
        currentScreen = 0;
        saveState(false);
        beep(400, 300);
        display.ssd1306_command(SSD1306_DISPLAYON);
      }
    } else {
      menuIsHeld = false;
      wakeHandled = false;
    }

    if (sleepDisplayOff) {
      delay(200);
    } else if (now - lastSleepIdleTime >= SLEEP_SCREEN_OFF_MS) {
      display.clearDisplay();
      display.display();
      display.ssd1306_command(SSD1306_DISPLAYOFF);
      sleepDisplayOff = true;
      delay(200);
    } else {
      display.ssd1306_command(SSD1306_DISPLAYON);
      frame = (now / 600) % 3;
      display.clearDisplay();
      drawPetSleeping();
      display.display();
      delay(120);
    }
    // Fall through — stats tick below
  }

  // ===================== AWAKE: stats tick =====================
  if (now - lastDecay >= DECAY_INTERVAL_MS) {
    lastDecay = now;

    // ATP decreases slowly: -1 every 2 minutes
    if (atp > 0) {
      atp--;
      // Track ATP \"used\" toward colony growth (180 units per growth)
      if (atpUsedForGrowth < 60000) atpUsedForGrowth++;
    }

    // Count ATP \"used\" toward acetate production (1 unit per decay tick)
    if (atpSinceAcetate < 250) atpSinceAcetate++;
    while (atpSinceAcetate >= 15 && acetate < 8) {
      atpSinceAcetate -= 15;
      acetate++;
    }

    // Stress increases slowly when ATP is low
    if (atp < 20 && stress < 100) {
      stress++;
    }
    // If acetate is maxed (8), extra stress like severe starvation
    if (acetate >= 8 && stress < 100) {
      uint8_t extra = 3;
      if (stress + extra > 100) stress = 100;
      else stress += extra;
    }
    // If plenty of ATP and low acetate, stress can slowly recover
    if (atp > 50 && acetate < 5 && stress > 0) {
      stress--;
    }

    // Colony growth: for each 180 ATP units spent, double N
    while (atpUsedForGrowth >= ATP_FOR_COLONY_GROWTH) {
      colony *= 2;
      atpUsedForGrowth -= ATP_FOR_COLONY_GROWTH;
    }

    saveState(false);
  }
  if (stress > 80 && (now - lastStressShrinkTime) >= STRESS_SHRINK_MS) {
    if (colony > 1) colony /= 2;
    lastStressShrinkTime = now;
    saveState(false);
  }

  // Death (wake TFT if sleeping so user sees egg reset)
  if (stress >= 100) {
    if (petSleeping) {
      petSleeping = false;
      sleepDisplayOff = false;
      display.ssd1306_command(SSD1306_DISPLAYON);
    }
    beep(120, 600);
    resetToEgg();
    return;
  }

  // If sleeping, stats have ticked — nothing else to do
  if (petSleeping) return;

  // Blink
  if (now - lastBlink > 2000) { lastBlink = now; eyesOpen = !eyesOpen; }
  frame = (now / 400) % 4;

  // ===================== SCREEN: MENU =====================
  if (currentScreen == 1) {
    // Feed (Btn1): back to main
    if (digitalRead(PIN_BTN_FEED) == LOW) {
      if (!feedPressed) {
        feedPressed = true;
        lastButtonTime = now;
        currentScreen = 0;
        beep(150, 80);
      }
    } else { feedPressed = false; }

    // Down (Btn4): scroll highlight down
    if (digitalRead(PIN_BTN_DOWN) == LOW) {
      if (!downPressed) {
        downPressed = true;
        lastButtonTime = now;
        menuCursor = (menuCursor + 1) % MENU_COUNT;
        adjustMenuWindow();
        beep(150, 80);
      }
    } else { downPressed = false; }

    // Up (Btn3): scroll highlight up
    if (digitalRead(PIN_BTN_UP) == LOW) {
      if (!upPressed) {
        upPressed = true;
        lastButtonTime = now;
        menuCursor = (menuCursor + MENU_COUNT - 1) % MENU_COUNT;
        adjustMenuWindow();
        beep(150, 80);
      }
    } else { upPressed = false; }

    // Menu (Btn2): select
    if (digitalRead(PIN_BTN_MENU) == LOW) {
      if (!menuPressed) {
        menuPressed = true;
        lastButtonTime = now;
        menuSelect(menuCursor);
      }
    } else { menuPressed = false; }

    display.clearDisplay();
    drawMenu();
    display.display();
    delay(80);
    return;
  }

  // ===================== SCREEN: DNA PAIR GAME =====================
  if (currentScreen == 4) {
    // Menu (Btn2): exit game
    if (digitalRead(PIN_BTN_MENU) == LOW) {
      if (!menuPressed) {
        menuPressed = true;
        lastButtonTime = now;
        currentScreen = 0;
        beep(150, 80);
      }
    } else { menuPressed = false; }

    if (!dnaOver && !dnaWon) {
      // Up (Btn3): move piece left
      if (digitalRead(PIN_BTN_UP) == LOW) {
        if (!upPressed) {
          upPressed = true;
          lastButtonTime = now;
          if (dnaCol > 0 && (dnaRow >= 5 || dnaGrid[dnaRow][dnaCol - 1] == 0))
            dnaCol--;
          beep(150, 40);
        }
      } else { upPressed = false; }

      // Down (Btn4): move piece right
      if (digitalRead(PIN_BTN_DOWN) == LOW) {
        if (!downPressed) {
          downPressed = true;
          lastButtonTime = now;
          if (dnaCol < 4 && (dnaRow >= 5 || dnaGrid[dnaRow][dnaCol + 1] == 0))
            dnaCol++;
          beep(150, 40);
        }
      } else { downPressed = false; }

      // Feed (Btn1): hard drop
      if (digitalRead(PIN_BTN_FEED) == LOW) {
        if (!feedPressed) {
          feedPressed = true;
          lastButtonTime = now;
          dnaHardDrop();
        }
      } else { feedPressed = false; }

      // Auto-drop on timer
      if (now - dnaLastFall >= DNA_FALL_MS) {
        dnaLastFall = now;
        dnaDropOne();
      }
    } else {
      // Game over or won — any non-Menu button returns to main
      if (digitalRead(PIN_BTN_FEED) == LOW || digitalRead(PIN_BTN_UP) == LOW || digitalRead(PIN_BTN_DOWN) == LOW) {
        if (!feedPressed) {
          feedPressed = true;
          lastButtonTime = now;
          currentScreen = 0;
          beep(150, 80);
        }
      } else { feedPressed = false; }
    }

    display.clearDisplay();
    drawDnaGame();
    display.display();
    delay(50);
    return;
  }

  // ===================== SCREEN: TRANSFORMATION CODE INPUT =====================
  if (currentScreen == 5) {
    // Feed (Btn1): cancel to main
    if (digitalRead(PIN_BTN_FEED) == LOW) {
      if (!feedPressed) {
        feedPressed = true;
        lastButtonTime = now;
        currentScreen = 0;
        beep(150, 80);
      }
    } else { feedPressed = false; }

    // Down (Btn4): increase password digit 0..9
    if (digitalRead(PIN_BTN_DOWN) == LOW) {
      if (!downPressed) {
        downPressed = true;
        lastButtonTime = now;
        transPassword = (transPassword + 1) % 10;
        beep(150, 50);
      }
    } else { downPressed = false; }

    // Up (Btn3): decrease password digit 0..9
    if (digitalRead(PIN_BTN_UP) == LOW) {
      if (!upPressed) {
        upPressed = true;
        lastButtonTime = now;
        transPassword = (transPassword + 9) % 10;
        beep(150, 50);
      }
    } else { upPressed = false; }

    // Menu (Btn2): begin BLE same-code search
    if (digitalRead(PIN_BTN_MENU) == LOW) {
      if (!menuPressed) {
        menuPressed = true;
        lastButtonTime = now;
        startTransformationSearch();
        currentScreen = 6;
        beep(250, 100);
      }
    } else { menuPressed = false; }

    display.clearDisplay();
    drawTransformationCodeScreen();
    display.display();
    delay(80);
    return;
  }

  // ===================== SCREEN: TRANSFORMATION SEARCHING =====================
  if (currentScreen == 6) {
    // Feed (Btn1): stop searching and return to main
    if (digitalRead(PIN_BTN_FEED) == LOW) {
      if (!feedPressed) {
        feedPressed = true;
        lastButtonTime = now;
        stopTransformationSearch();
        currentScreen = 0;
        beep(150, 80);
        delay(80);
        return;
      }
    } else { feedPressed = false; }

    display.clearDisplay();
    drawTransformationSearchScreen();
    display.display();
    runTransformationScan(now);

    if (transFound) {
      showPairedThenMenu();
      return;
    }

    delay(80);
    return;
  }

  // ===================== SCREEN: PAIRED MENU =====================
  if (currentScreen == 7) {
    // Feed (Btn1): back to main
    if (digitalRead(PIN_BTN_FEED) == LOW) {
      if (!feedPressed) {
        feedPressed = true;
        lastButtonTime = now;
        stopTransformationSearch();
        stopGeneTransferWait();
        currentScreen = 0;
        beep(150, 80);
      }
    } else { feedPressed = false; }

    // Up/Down: choose blank future action
    if (digitalRead(PIN_BTN_UP) == LOW) {
      if (!upPressed) {
        upPressed = true;
        lastButtonTime = now;
        transOptionCursor = (transOptionCursor + 1) % 2;
        beep(150, 50);
      }
    } else { upPressed = false; }

    if (digitalRead(PIN_BTN_DOWN) == LOW) {
      if (!downPressed) {
        downPressed = true;
        lastButtonTime = now;
        transOptionCursor = (transOptionCursor + 1) % 2;
        beep(150, 50);
      }
    } else { downPressed = false; }

    // Menu (Btn2): Horizontal gene transfer starts the exchange wait; Battle remains blank for now
    if (digitalRead(PIN_BTN_MENU) == LOW) {
      if (!menuPressed) {
        menuPressed = true;
        lastButtonTime = now;
        if (transOptionCursor == 0) {
          startGeneTransferWait();
          currentScreen = 9;
          beep(250, 120);
        } else {
          beep(220, 120);
        }
      }
    } else { menuPressed = false; }

    display.clearDisplay();
    drawPairedMenu();
    display.display();
    delay(80);
    return;
  }

  // ===================== SCREEN: EXPRESSION =====================
  if (currentScreen == 8) {
    if (digitalRead(PIN_BTN_FEED) == LOW) {
      if (!feedPressed) {
        feedPressed = true;
        lastButtonTime = now;
        currentScreen = 0;
        beep(150, 80);
      }
    } else { feedPressed = false; }

    if (digitalRead(PIN_BTN_UP) == LOW) {
      if (!upPressed) {
        upPressed = true;
        lastButtonTime = now;
        do {
          expressionCursor = (expressionCursor + MBTI_COUNT - 1) % MBTI_COUNT;
        } while ((mbtiMask & (1U << expressionCursor)) == 0);
        activeMbti = expressionCursor;
        saveMbtiData();
        beep(150, 50);
      }
    } else { upPressed = false; }

    if (digitalRead(PIN_BTN_DOWN) == LOW) {
      if (!downPressed) {
        downPressed = true;
        lastButtonTime = now;
        do {
          expressionCursor = (expressionCursor + 1) % MBTI_COUNT;
        } while ((mbtiMask & (1U << expressionCursor)) == 0);
        activeMbti = expressionCursor;
        saveMbtiData();
        beep(150, 50);
      }
    } else { downPressed = false; }

    if (digitalRead(PIN_BTN_MENU) == LOW) {
      if (!menuPressed) {
        menuPressed = true;
        lastButtonTime = now;
        beep(120, 50);
      }
    } else { menuPressed = false; }

    display.clearDisplay();
    drawExpressionScreen();
    display.display();
    delay(80);
    return;
  }

  // ===================== SCREEN: GENE TRANSFER WAIT =====================
  if (currentScreen == 9) {
    if (digitalRead(PIN_BTN_FEED) == LOW) {
      if (!feedPressed) {
        feedPressed = true;
        lastButtonTime = now;
        stopGeneTransferWait();
        currentScreen = 7;
        beep(150, 80);
        delay(80);
        return;
      }
    } else { feedPressed = false; }

    display.clearDisplay();
    drawGeneTransferWaitScreen();
    display.display();
    runGeneTransferScan(now);

    if (geneTransferFound) {
      finishGeneTransfer();
      return;
    }

    delay(80);
    return;
  }

  // ===================== SCREEN: PHONE TRANSFORMATION CODE INPUT =====================
  if (currentScreen == 10) {
    if (digitalRead(PIN_BTN_FEED) == LOW) {
      if (!feedPressed) {
        feedPressed = true;
        lastButtonTime = now;
        currentScreen = 0;
        beep(150, 80);
      }
    } else { feedPressed = false; }

    if (digitalRead(PIN_BTN_DOWN) == LOW) {
      if (!downPressed) {
        downPressed = true;
        lastButtonTime = now;
        transPassword = (transPassword + 1) % 10;
        beep(150, 50);
      }
    } else { downPressed = false; }

    if (digitalRead(PIN_BTN_UP) == LOW) {
      if (!upPressed) {
        upPressed = true;
        lastButtonTime = now;
        transPassword = (transPassword + 9) % 10;
        beep(150, 50);
      }
    } else { upPressed = false; }

    if (digitalRead(PIN_BTN_MENU) == LOW) {
      if (!menuPressed) {
        menuPressed = true;
        lastButtonTime = now;
        startPhoneTransformationWait();
        currentScreen = 11;
        beep(250, 100);
      }
    } else { menuPressed = false; }

    display.clearDisplay();
    drawPhoneTransformationCodeScreen();
    display.display();
    delay(80);
    return;
  }

  // ===================== SCREEN: PHONE TRANSFORMATION WAIT =====================
  if (currentScreen == 11) {
    if (digitalRead(PIN_BTN_FEED) == LOW) {
      if (!feedPressed) {
        feedPressed = true;
        lastButtonTime = now;
        stopPhoneTransformationWait();
        currentScreen = 0;
        beep(150, 80);
        delay(80);
        return;
      }
    } else { feedPressed = false; }

    display.clearDisplay();
    drawPhoneTransformationWaitScreen();
    display.display();

    if (phoneCommandReceived) {
      String command = phoneLastCommand;
      phoneCommandReceived = false;
      phoneLastCommand = "";
      handlePhoneCommand(command);
    }

    delay(120);
    return;
  }

  // ===================== SCREEN: RESET CONFIRM =====================
  if (currentScreen == 3) {
    bool anyDown = (digitalRead(PIN_BTN_FEED) == LOW || digitalRead(PIN_BTN_MENU) == LOW || digitalRead(PIN_BTN_UP) == LOW || digitalRead(PIN_BTN_DOWN) == LOW);
    if (anyDown) {
      if (resetHoldStart == 0) {
        resetHoldStart = now;
      } else if ((now - resetHoldStart) >= RESET_HOLD_MS) {
        resetHoldStart = 0;
        resetToEgg();
        beep(400, 300);
        delay(80);
        return;
      }
    } else {
      if (resetHoldStart != 0) {
        if ((now - resetHoldStart) < RESET_HOLD_MS) {
          currentScreen = 0;
          beep(150, 80);
        }
        resetHoldStart = 0;
      }
    }
    display.clearDisplay();
    drawResetConfirm();
    display.display();
    delay(80);
    return;
  }

  // ===================== SCREEN: STATUS =====================
  if (currentScreen == 2) {
    bool anyNew = false;
    if (digitalRead(PIN_BTN_FEED) == LOW) {
      if (!feedPressed) { feedPressed = true; anyNew = true; }
    } else { feedPressed = false; }
    if (digitalRead(PIN_BTN_MENU) == LOW) {
      if (!menuPressed) { menuPressed = true; anyNew = true; }
    } else { menuPressed = false; }
    if (digitalRead(PIN_BTN_UP) == LOW) {
      if (!upPressed) { upPressed = true; anyNew = true; }
    } else { upPressed = false; }
    if (digitalRead(PIN_BTN_DOWN) == LOW) {
      if (!downPressed) { downPressed = true; anyNew = true; }
    } else { downPressed = false; }

    if (anyNew) {
      lastButtonTime = now;
      currentScreen = 0;
      beep(150, 80);
    }

    display.clearDisplay();
    drawStatsScreen();
    display.display();
    delay(80);
    return;
  }

  // ===================== SCREEN: MAIN PET =====================
  // Btn1 Feed
  if (digitalRead(PIN_BTN_FEED) == LOW) {
    if (!feedPressed) {
      feedPressed = true;
      lastButtonTime = now;
      // ATP +25, capped at 100 (acetate comes only from ATP use, not from feeding)
      atp = (atp + 25 > 100) ? 100 : atp + 25;
      saveState(false);
      beep(200, 400);
    }
  } else { feedPressed = false; }

  // Btn2 Menu → open menu
  if (digitalRead(PIN_BTN_MENU) == LOW) {
    if (!menuPressed) {
      menuPressed = true;
      lastButtonTime = now;
      currentScreen = 1;
      menuCursor = 0;
      menuWinTop = 0;
      beep(150, 150);
    }
  } else { menuPressed = false; }

  // Up / Down: no action on main (still count for screen timeout)
  if (digitalRead(PIN_BTN_UP) == LOW) lastButtonTime = now;
  if (digitalRead(PIN_BTN_DOWN) == LOW) lastButtonTime = now;

  display.clearDisplay();
  drawPet();
  drawBars();
  display.display();
  delay(80);
}

// ===================== Menu select =====================
void menuSelect(uint8_t item) {
  switch (item) {
    case 0:  // Status
      currentScreen = 2;
      feedPressed = menuPressed = upPressed = downPressed = true;
      beep(200, 150);
      break;
    case 1:  // Clean Poop: remove acetate and return to main
      acetate = 0;
      atpSinceAcetate = 0;
      currentScreen = 0;
      saveState(false);
      beep(180, 120);
      break;
    case 2:  // Play — DNA Pair Game
      currentScreen = 4;
      dnaInit();
      feedPressed = menuPressed = upPressed = downPressed = true;
      beep(300, 120);
      break;
    case 3:  // Transformation pairing
      transPassword = 0;
      currentScreen = 5;
      feedPressed = menuPressed = upPressed = downPressed = true;
      beep(200, 120);
      break;
    case 4:  // Phone Transformation pairing
      transPassword = 0;
      currentScreen = 10;
      feedPressed = menuPressed = upPressed = downPressed = true;
      beep(200, 120);
      break;
    case 5:  // Expression: choose active MBTI from collected expressions
      if (activeMbti >= MBTI_COUNT) assignRandomMbti();
      expressionCursor = activeMbti;
      currentScreen = 8;
      feedPressed = menuPressed = upPressed = downPressed = true;
      beep(200, 120);
      break;
    case 6:  // Sleep — save state, power down, wake by button reset/resume
      beep(100, 300);
      enterDeepSleep();
      break;
    case 7:  // Exit
      currentScreen = 0;
      saveState(false);
      beep(150, 80);
      break;
    case 8:  // Reset: show confirm screen
      currentScreen = 3;
      resetHoldStart = 0;
      feedPressed = menuPressed = upPressed = downPressed = true;
      beep(200, 150);
      break;
  }
}

// ===================== Reset =====================
void resetToEgg() {
  colony = 1; atp = 80; stress = 0;
  atpSinceAcetate = 0; atpUsedForGrowth = 0; acetate = 0;
  gameStateIsPet = false;
  eggFeedCount = 0;
  eggFeedPressed = false;
  petSleeping = false;
  sleepDisplayOff = false;
  currentScreen = 0;
  resetHoldStart = 0;
  activeMbti = 255;
  mbtiMask = 0;
  phoneRewardMask = 0;
  saveMbtiData();
  saveState(true);
}

// ===================== DNA Pair Game =====================

bool dnaIsComp(uint8_t a, uint8_t b) {
  if (a == 0 || b == 0) return false;
  uint8_t s = a + b;
  return s == 3 || s == 7;  // A(1)+T(2)=3, C(3)+G(4)=7
}

void dnaSpawn() {
  dnaBase = random(1, 5);
  dnaCol = 2;
  dnaRow = 5;
  dnaLastFall = millis();
}

void dnaInit() {
  memset(dnaGrid, 0, sizeof(dnaGrid));
  // Bottom row: AAAAA; row above: _ A _ A _
  for (uint8_t c = 0; c < 5; c++) dnaGrid[0][c] = DNA_A;
  dnaGrid[1][1] = DNA_A;
  dnaGrid[1][3] = DNA_A;
  dnaScore = 0;
  dnaOver = dnaWon = false;
  dnaSpawn();
}

bool dnaGridEmpty() {
  for (uint8_t i = 0; i < 25; i++)
    if (((uint8_t*)dnaGrid)[i]) return false;
  return true;
}

uint8_t dnaEliminate() {
  uint32_t kill = 0;
  for (uint8_t r = 0; r < 5; r++)
    for (uint8_t c = 0; c < 5; c++) {
      if (!dnaGrid[r][c]) continue;
      if (c < 4 && dnaIsComp(dnaGrid[r][c], dnaGrid[r][c+1]))
        kill |= (1UL << (r*5+c)) | (1UL << (r*5+c+1));
      if (r < 4 && dnaIsComp(dnaGrid[r][c], dnaGrid[r+1][c]))
        kill |= (1UL << (r*5+c)) | (1UL << ((r+1)*5+c));
    }
  uint8_t n = 0;
  for (uint8_t r = 0; r < 5; r++)
    for (uint8_t c = 0; c < 5; c++)
      if (kill & (1UL << (r*5+c))) { dnaGrid[r][c] = 0; n++; }
  return n;
}

void dnaGravity() {
  for (uint8_t c = 0; c < 5; c++) {
    uint8_t w = 0;
    for (uint8_t r = 0; r < 5; r++)
      if (dnaGrid[r][c]) {
        if (w != r) { dnaGrid[w][c] = dnaGrid[r][c]; dnaGrid[r][c] = 0; }
        w++;
      }
  }
}

void dnaLand() {
  if (dnaRow >= 5) { dnaOver = true; beep(80, 300); return; }
  dnaGrid[dnaRow][dnaCol] = dnaBase;
  uint8_t e;
  while ((e = dnaEliminate()) > 0) {
    dnaScore += (uint16_t)e * 10;
    beep(350, 50);
    dnaGravity();
  }
  if (dnaGridEmpty() || dnaScore >= 1000) { dnaWon = true; beep(500, 200); return; }
  beep(150, 30);
  dnaSpawn();
}

void dnaDropOne() {
  if (dnaRow == 0) { dnaLand(); return; }
  int8_t below = dnaRow - 1;
  if (below < 5 && dnaGrid[below][dnaCol]) { dnaLand(); return; }
  dnaRow = below;
}

void dnaHardDrop() {
  while (true) {
    if (dnaRow == 0) { dnaLand(); return; }
    int8_t below = dnaRow - 1;
    if (below < 5 && dnaGrid[below][dnaCol]) { dnaLand(); return; }
    dnaRow--;
  }
}

char dnaLetter(uint8_t b) {
  if (b == DNA_A) return 'A';
  if (b == DNA_T) return 'T';
  if (b == DNA_C) return 'C';
  return 'G';
}

void drawDnaGame() {
  // Score (top-left)
  display.setCursor(0, 2);
  display.print(F("Sc:"));
  display.print(dnaScore);

  // Grid border
  display.drawRect(DNA_GX - 1, DNA_GY - 1,
                   5 * DNA_CELL + 2, 5 * DNA_CELL + 2, SSD1306_WHITE);

  // Grid contents (white letters on black)
  for (uint8_t r = 0; r < 5; r++)
    for (uint8_t c = 0; c < 5; c++)
      if (dnaGrid[r][c]) {
        display.setCursor(DNA_GX + c * DNA_CELL + 2,
                          DNA_GY + (4 - r) * DNA_CELL + 1);
        display.write(dnaLetter(dnaGrid[r][c]));
      }

  // Falling piece (inverted: white box + black letter)
  if (!dnaOver && !dnaWon) {
    int16_t px = DNA_GX + dnaCol * DNA_CELL;
    int16_t py = (dnaRow >= 5)
      ? (DNA_GY - DNA_CELL)
      : (DNA_GY + (4 - dnaRow) * DNA_CELL);
    display.fillRect(px, py, DNA_CELL, DNA_CELL, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(px + 2, py + 1);
    display.write(dnaLetter(dnaBase));
    display.setTextColor(SSD1306_WHITE);
  }

  // Controls (right side)
  display.setCursor(92, 16);
  display.print(F("^=L"));
  display.setCursor(92, 26);
  display.print(F("v=R"));
  display.setCursor(92, 38);
  display.print(F("F=go"));

  // Game over / win overlay
  if (dnaOver || dnaWon) {
    display.fillRect(6, 20, 116, 28, SSD1306_BLACK);
    display.drawRect(6, 20, 116, 28, SSD1306_WHITE);
    display.setCursor(14, 23);
    display.print(dnaOver ? F("GAME OVER!") : F("YOU WIN!"));
    display.setCursor(14, 35);
    display.print(F("Score: "));
    display.print(dnaScore);
  }
}

// ===================== Drawing =====================

// ---- Egg ----
void drawEgg(int shakeX, int crackStage) {
  int cx = SCREEN_WIDTH / 2 + shakeX;
  int cy = 36;
  int eggW = 32, eggH = 30, topN = 15;

  for (int y = 0; y < eggH; y++) {
    float t = (float)y / eggH;
    int w;
    if      (t < 0.3) w = map(t * 100, 0, 30, eggW - topN, eggW);
    else if (t < 0.7) w = eggW;
    else              w = map(t * 100, 70, 100, eggW, eggW - 4);
    display.drawFastHLine(cx - w / 2, cy - eggH / 2 + y, w, SSD1306_WHITE);
  }
  display.fillCircle(cx, cy - eggH / 2 + 4, (eggW - topN) / 2, SSD1306_WHITE);
  display.fillCircle(cx, cy + eggH / 2 - 4, eggW / 2 - 2, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(cx - 18, 2);
  display.print(F("???"));
  display.setTextSize(1);

  if (crackStage >= 1) {
    display.drawLine(cx-8,cy-8,cx+10,cy+2,SSD1306_BLACK);
    display.drawLine(cx-8,cy-7,cx+10,cy+3,SSD1306_BLACK);
  }
  if (crackStage >= 2) {
    display.drawLine(cx+8,cy+8,cx-10,cy+12,SSD1306_BLACK);
    display.drawLine(cx-15,cy-2,cx-6,cy+10,SSD1306_BLACK);
  }
  if (crackStage >= 3) {
    display.drawLine(cx-18,cy-10,cx-4,cy+6,SSD1306_BLACK);
    display.drawLine(cx+12,cy-8,cx+4,cy+8,SSD1306_BLACK);
  }
  if (crackStage >= 4) {
    display.drawLine(cx-2,cy-16,cx-2,cy+6,SSD1306_BLACK);
    display.drawLine(cx-22,cy+2,cx+20,cy-4,SSD1306_BLACK);
  }
  if (crackStage >= 5) {
    display.drawLine(cx-24,cy,cx+22,cy,SSD1306_BLACK);
    display.drawLine(cx-10,cy-22,cx-10,cy+18,SSD1306_BLACK);
    display.drawLine(cx+14,cy-14,cx+14,cy+16,SSD1306_BLACK);
    display.fillCircle(cx+20,cy-10,3,SSD1306_BLACK);
  }
}

// ---- Normal E. coli ----
void drawPet() {
  int cx = SCREEN_WIDTH / 2;
  int cy = 22;
  int rx = 22, ry = 10;

  display.fillRoundRect(cx - rx, cy - ry, rx * 2, ry * 2, 6, SSD1306_WHITE);

  display.setCursor(0, 2);
  if (activeMbti < MBTI_COUNT) display.print(mbtiLabel(activeMbti));
  display.setCursor(cx - 10, 2);
  display.print(F("E.coli"));

  if (eyesOpen) {
    display.fillCircle(cx - 6, cy - 2, 2, SSD1306_BLACK);
    display.fillCircle(cx + 6, cy - 2, 2, SSD1306_BLACK);
  }

  int wiggle = (frame % 2) ? 2 : -2;
  display.drawLine(cx - rx, cy, cx - rx - 8, cy + wiggle + 4, SSD1306_WHITE);
  display.drawLine(cx + rx, cy, cx + rx + 10, cy - wiggle, SSD1306_WHITE);

  // Draw acetate \"poop\" particles around the pet
  drawAcetate();
}

// ---- Sleeping E. coli (ZZZ) — shown until 5 s idle, then display powers down ----
void drawPetSleeping() {
  int cx = SCREEN_WIDTH / 2;
  int cy = 26;
  int rx = 22, ry = 10;

  display.fillRoundRect(cx - rx, cy - ry, rx * 2, ry * 2, 6, SSD1306_WHITE);

  display.drawLine(cx - 8, cy - 2, cx - 4, cy - 2, SSD1306_BLACK);
  display.drawLine(cx + 4, cy - 2, cx + 8, cy - 2, SSD1306_BLACK);

  display.fillTriangle(cx + 6, cy - ry, cx - 10 + 6, cy - ry, cx + 14, cy - ry - 14, SSD1306_WHITE);
  display.fillCircle(cx + 14, cy - ry - 14, 3, SSD1306_WHITE);

  int z = frame;
  display.setCursor(cx + rx + 4,  cy - 14 - z * 3);  display.print(F("z"));
  display.setCursor(cx + rx + 10, cy - 20 - z * 3);  display.print(F("z"));
  display.setCursor(cx + rx + 16, cy - 26 - z * 3);  display.print(F("Z"));

  display.setCursor(cx - 20, 2);
  display.print(F("E.coli"));

  display.setCursor(0, 56);
  display.print(F("Tap:on Hold 3s:wake"));
}

// ---- Acetate \"poop\" sprites ----
void drawAcetate() {
  if (acetate == 0) return;

  // Pre-chosen \"random\" positions for up to 8 acetate dots
  const int ax[8] = { 18, 34, 50, 66, 82, 98, 26, 74 };
  const int ay[8] = { 40, 46, 38, 44, 40, 46, 52, 52 };

  uint8_t count = (acetate > 8) ? 8 : acetate;
  for (uint8_t i = 0; i < count; i++) {
    int x = ax[i];
    int y = ay[i];
    // Small droplet: circle + base line
    display.fillCircle(x, y - 1, 2, SSD1306_WHITE);
    display.drawFastHLine(x - 2, y + 1, 5, SSD1306_WHITE);
  }
}

// ---- Menu screen ----
void drawMenu() {
  display.setCursor(0, 0);
  display.print(F("== Menu =="));

  for (uint8_t i = 0; i < 3; i++) {
    uint8_t idx = menuWinTop + i;
    if (idx >= MENU_COUNT) break;
    int y = 18 + i * 16;
    display.setCursor(0, y);
    display.print(idx == menuCursor ? F("> ") : F("  "));
    printMenuLabel(idx);
  }

  if (menuWinTop > 0) {
    display.setCursor(120, 18);
    display.write(0x18);  // up arrow
  }
  if (menuWinTop + 3 < MENU_COUNT) {
    display.setCursor(120, 50);
    display.write(0x19);  // down arrow
  }

  display.setCursor(0, 56);
  display.print(F("F=back M=OK ^=up v=dn"));
}

// ---- Transformation code input ----
void drawTransformationCodeScreen() {
  display.setCursor(0, 0);
  display.println(F("HGT Pairing"));
  display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);

  display.setCursor(0, 18);
  display.print(F("Pair code:"));

  display.setTextSize(3);
  display.setCursor(56, 28);
  display.print(transPassword);
  display.setTextSize(1);

  display.setCursor(0, 56);
  display.print(F("^/- v/+ M=go F=back"));
}

// ---- Transformation BLE searching ----
void drawTransformationSearchScreen() {
  uint8_t dots = (millis() / 400) % 4;
  display.setCursor(0, 0);
  display.println(F("Searching BLE"));
  display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);

  display.setCursor(0, 20);
  display.print(F("Code: "));
  display.print(transPassword);

  display.setCursor(0, 34);
  display.print(F("Looking"));
  for (uint8_t i = 0; i < dots; i++) display.print('.');

  display.setCursor(0, 56);
  display.print(F("Feed: cancel"));
}

// ---- Paired menu (future action placeholders) ----
void drawPairedMenu() {
  display.setCursor(0, 0);
  display.println(F("Paired Menu"));
  display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);

  display.setCursor(0, 22);
  display.print(transOptionCursor == 0 ? F("> H. Gene Transfer") : F("  H. Gene Transfer"));
  display.setCursor(0, 40);
  display.print(transOptionCursor == 1 ? F("> Battle") : F("  Battle"));

  display.setCursor(0, 56);
  display.print(F("^/v=move M=OK F=back"));
}

// ---- Expression page ----
void drawExpressionScreen() {
  display.setCursor(0, 0);
  display.println(F("Expression"));
  display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);

  uint8_t row = 0;
  for (uint8_t i = 0; i < MBTI_COUNT && row < 4; i++) {
    if ((mbtiMask & (1U << i)) == 0) continue;
    display.setCursor(0, 16 + row * 10);
    display.print(i == expressionCursor ? F("> ") : F("  "));
    display.print(mbtiLabel(i));
    row++;
  }

  for (uint8_t i = 0; i < PHONE_REWARD_COUNT && row < 4; i++) {
    if ((phoneRewardMask & (1U << i)) == 0) continue;
    display.setCursor(0, 16 + row * 10);
    display.print(F("  "));
    display.print(phoneRewardLabel(i));
    row++;
  }

  display.setCursor(0, 56);
  display.print(F("^/v auto-set F=back"));
}

// ---- Gene transfer wait ----
void drawGeneTransferWaitScreen() {
  uint8_t dots = (millis() / 400) % 4;
  display.setCursor(0, 0);
  display.println(F("Horizontal GT"));
  display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);

  display.setCursor(0, 18);
  display.print(F("Your MBTI: "));
  display.print(mbtiLabel(activeMbti));

  display.setCursor(0, 34);
  display.print(F("Waiting"));
  for (uint8_t i = 0; i < dots; i++) display.print('.');

  display.setCursor(0, 56);
  display.print(F("Feed: cancel"));
}

// ---- Phone Transformation code input ----
void drawPhoneTransformationCodeScreen() {
  display.setCursor(0, 0);
  display.println(F("Transformation"));
  display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);

  display.setCursor(0, 18);
  display.print(F("Phone code:"));

  display.setTextSize(3);
  display.setCursor(56, 28);
  display.print(transPassword);
  display.setTextSize(1);

  display.setCursor(0, 56);
  display.print(F("^/- v/+ M=go F=back"));
}

// ---- Phone Transformation wait ----
void drawPhoneTransformationWaitScreen() {
  display.setCursor(0, 0);
  display.println(F("Phone Link"));
  display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);

  if (phoneConnected) {
    display.setCursor(8, 18);
    display.print(F("CUHKSBS"));
    display.setCursor(8, 30);
    display.print(F("IGEM 2026"));
    display.setCursor(0, 44);
    display.print(F("Waiting command"));
  } else {
    uint8_t dots = (millis() / 400) % 4;
    display.setCursor(0, 18);
    display.print(F("Code: "));
    display.print(transPassword);
    display.setCursor(0, 34);
    display.print(F("Waiting phone"));
    for (uint8_t i = 0; i < dots; i++) display.print('.');
  }

  display.setCursor(0, 56);
  display.print(F("Feed: disconnect"));
}

// ---- Reset confirm screen ----
void drawResetConfirm() {
  display.setCursor(0, 0);
  display.println(F("Reset E.coli?"));
  display.println();
  display.println(F("Long press 3s"));
  display.println(F("any btn = YES"));
  display.println();
  display.println(F("Short press"));
  display.println(F("any btn = NO"));
}

// ---- Status bars on main screen ----
void drawBars() {
  display.setCursor(0, 40);
  display.print(F("N="));
  display.print(colony);

  display.setCursor(0, 52);
  display.print(F("A:"));
  drawBar(14, 52, atp);

  display.setCursor(68, 52);
  display.print(F("S:"));
  drawBar(80, 52, stress);
}

void drawBar(int x, int y, uint8_t val) {
  display.drawRect(x, y, 42, 6, SSD1306_WHITE);
  int w = map(val, 0, 100, 0, 40);
  if (w > 0) display.fillRect(x + 1, y + 1, w, 4, SSD1306_WHITE);
}

// ---- Stats text screen ----
void drawStatsScreen() {
  display.setCursor(0, 0);
  display.println(F("E.coli Tamagotchi"));
  display.println(F("--- Stats ---"));
  display.print(F("Colony N= "));
  display.println(colony);
  display.print(F("ATP:      "));
  display.println(atp);
  display.print(F("Stress:   "));
  display.println(stress);
  display.print(F("Acetate:  "));
  display.println(acetate);
  display.println();
  display.println(F("Press any btn to exit"));
}

// ---- Buzzer ----
void beep(unsigned int freqHz, unsigned int durationMs) {
#ifdef PIN_BUZZER
  unsigned long endTime = millis() + durationMs;
  unsigned long periodUs = 1000000UL / freqHz;
  while (millis() < endTime) {
    digitalWrite(PIN_BUZZER, HIGH);
    delayMicroseconds(periodUs / 2);
    digitalWrite(PIN_BUZZER, LOW);
    delayMicroseconds(periodUs / 2);
  }
#endif
}
