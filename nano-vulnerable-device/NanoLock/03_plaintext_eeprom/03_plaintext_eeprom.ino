#include <EEPROM.h>

/*
  NanoLock - Stage 03: Plaintext EEPROM Storage

  BİLEREK güvensiz eğitim sürümü.

  Önceki açıklar korunuyor:
    - Authentication istemeyen DEBUG modu
    - Authentication istemeyen SHOW_CONFIG
    - SHOW_CONFIG PIN'i gösteriyor
    - Debug logları girilen credential'ları gösteriyor
    - Credential firmware/default config içinde bulunuyor

  Yeni açık:
    - Admin PIN EEPROM'da plaintext olarak saklanıyor.
*/

const int LOCK_LED = LED_BUILTIN;

bool authenticated = false;
bool lockOpen = false;
bool debugMode = false;

// --------------------------------------------------
// EEPROM layout
// --------------------------------------------------
//
// Address 0   -> magic byte
// Address 1-4 -> 4 digit PIN
// Address 5   -> null terminator
//

const int EEPROM_MAGIC_ADDRESS = 0;
const int EEPROM_PIN_ADDRESS = 1;

const byte EEPROM_MAGIC = 0xA5;

// Default PIN.
// İlk boot sırasında EEPROM'a plaintext yazılacak.
const char DEFAULT_PIN[] = "1234";

// Runtime'da kullanılacak PIN.
// 4 digit + '\0'
char adminPin[5];

const String DEVICE_NAME = "NanoLock";
const String FIRMWARE_VERSION = "0.3-insecure-eeprom";


// ==================================================
// SETUP
// ==================================================

void setup() {

  pinMode(LOCK_LED, OUTPUT);
  digitalWrite(LOCK_LED, LOW);

  Serial.begin(9600);

  delay(500);

  loadOrInitializePin();

  Serial.println();
  Serial.println("=== NanoLock v0.3 ===");
  Serial.println("System ready.");
  Serial.println("Type HELP to see commands.");
  Serial.println();
}


// ==================================================
// LOOP
// ==================================================

void loop() {

  if (Serial.available() > 0) {

    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.length() == 0) {
      return;
    }

    if (debugMode) {
      Serial.print("[DEBUG] Received command: ");
      Serial.println(command);
    }

    handleCommand(command);
  }
}


// ==================================================
// COMMAND HANDLER
// ==================================================

void handleCommand(String command) {

  // ------------------------------------------------
  // HELP
  // ------------------------------------------------

  if (command == "HELP") {

    Serial.println("Available commands:");
    Serial.println("  HELP");
    Serial.println("  STATUS");
    Serial.println("  LOGIN <PIN>");
    Serial.println("  OPEN");
    Serial.println("  CLOSE");
    Serial.println("  LOGOUT");
    Serial.println("  CHANGE_PIN <NEW_PIN>");
    Serial.println("  DEBUG ON");
    Serial.println("  DEBUG OFF");
    Serial.println("  SHOW_CONFIG");

    return;
  }


  // ------------------------------------------------
  // STATUS
  // ------------------------------------------------

  if (command == "STATUS") {

    Serial.print("LOCK: ");
    Serial.println(lockOpen ? "OPEN" : "CLOSED");

    Serial.print("AUTH: ");
    Serial.println(authenticated ? "YES" : "NO");

    Serial.print("DEBUG: ");
    Serial.println(debugMode ? "ON" : "OFF");

    return;
  }


  // ------------------------------------------------
  // LOGIN
  // ------------------------------------------------

  if (command.startsWith("LOGIN ")) {

    String enteredPin = command.substring(6);
    enteredPin.trim();

    // Bilerek kötü:
    // Girilen credential debug loguna yazılıyor.
    if (debugMode) {

      Serial.print("[DEBUG] Login attempt with PIN: ");
      Serial.println(enteredPin);
    }

    if (enteredPin == String(adminPin)) {

      authenticated = true;

      if (debugMode) {
        Serial.println("[DEBUG] Authentication successful.");
      }

      Serial.println("LOGIN OK");

    } else {

      if (debugMode) {
        Serial.println("[DEBUG] Authentication failed.");
      }

      Serial.println("LOGIN FAILED");
    }

    return;
  }


  // ------------------------------------------------
  // OPEN
  // ------------------------------------------------

  if (command == "OPEN") {

    if (!authenticated) {

      if (debugMode) {
        Serial.println("[DEBUG] OPEN rejected: user not authenticated.");
      }

      Serial.println("AUTH REQUIRED");

      return;
    }

    lockOpen = true;

    digitalWrite(LOCK_LED, HIGH);

    if (debugMode) {
      Serial.println("[DEBUG] Lock state changed to OPEN.");
    }

    Serial.println("LOCK OPENED");

    return;
  }


  // ------------------------------------------------
  // CLOSE
  // ------------------------------------------------

  if (command == "CLOSE") {

    if (!authenticated) {

      if (debugMode) {
        Serial.println("[DEBUG] CLOSE rejected: user not authenticated.");
      }

      Serial.println("AUTH REQUIRED");

      return;
    }

    lockOpen = false;

    digitalWrite(LOCK_LED, LOW);

    if (debugMode) {
      Serial.println("[DEBUG] Lock state changed to CLOSED.");
    }

    Serial.println("LOCK CLOSED");

    return;
  }


  // ------------------------------------------------
  // LOGOUT
  // ------------------------------------------------

  if (command == "LOGOUT") {

    authenticated = false;

    if (debugMode) {
      Serial.println("[DEBUG] User session cleared.");
    }

    Serial.println("LOGGED OUT");

    return;
  }


  // ------------------------------------------------
  // CHANGE PIN
  // ------------------------------------------------

  if (command.startsWith("CHANGE_PIN ")) {

    // Şimdilik authorization doğru çalışıyor.
    // Bu aşamada sadece storage hatasına odaklanıyoruz.

    if (!authenticated) {
      Serial.println("AUTH REQUIRED");
      return;
    }

    String newPin = command.substring(11);
    newPin.trim();

    if (!isValidPin(newPin)) {
      Serial.println("INVALID PIN");
      Serial.println("PIN must contain exactly 4 digits.");
      return;
    }

    // Runtime PIN güncelle
    newPin.toCharArray(adminPin, sizeof(adminPin));

    // BİLEREK GÜVENSİZ:
    // PIN EEPROM'a plaintext yazılıyor.
    savePinToEEPROM();

    if (debugMode) {

      Serial.print("[DEBUG] New PIN saved to EEPROM: ");
      Serial.println(adminPin);
    }

    Serial.println("PIN CHANGED");

    return;
  }


  // ------------------------------------------------
  // DEBUG ON
  // ------------------------------------------------

  // Bilerek kötü:
  // Authentication gerektirmiyor.

  if (command == "DEBUG ON") {

    debugMode = true;

    Serial.println("DEBUG ENABLED");
    Serial.println("[DEBUG] Verbose diagnostic mode active.");

    return;
  }


  // ------------------------------------------------
  // DEBUG OFF
  // ------------------------------------------------

  if (command == "DEBUG OFF") {

    Serial.println("[DEBUG] Disabling diagnostic mode.");

    debugMode = false;

    Serial.println("DEBUG DISABLED");

    return;
  }


  // ------------------------------------------------
  // SHOW CONFIG
  // ------------------------------------------------

  // Bilerek çok kötü:
  // Authentication gerektirmiyor.
  // PIN dahil hassas bilgiler gösteriliyor.

  if (command == "SHOW_CONFIG") {

    Serial.println("=== DEVICE CONFIG ===");

    Serial.print("DEVICE: ");
    Serial.println(DEVICE_NAME);

    Serial.print("FIRMWARE: ");
    Serial.println(FIRMWARE_VERSION);

    Serial.print("ADMIN_PIN: ");
    Serial.println(adminPin);

    Serial.println("PIN_STORAGE: EEPROM PLAINTEXT");

    Serial.print("LOCK_STATE: ");
    Serial.println(lockOpen ? "OPEN" : "CLOSED");

    Serial.print("AUTH_STATE: ");
    Serial.println(
      authenticated ?
      "AUTHENTICATED" :
      "NOT_AUTHENTICATED"
    );

    Serial.print("DEBUG_MODE: ");
    Serial.println(
      debugMode ?
      "ENABLED" :
      "DISABLED"
    );

    Serial.println("=====================");

    return;
  }


  // ------------------------------------------------
  // UNKNOWN COMMAND
  // ------------------------------------------------

  if (debugMode) {

    Serial.print("[DEBUG] Unknown command received: ");
    Serial.println(command);
  }

  Serial.println("UNKNOWN COMMAND");
}


// ==================================================
// EEPROM FUNCTIONS
// ==================================================

void loadOrInitializePin() {

  byte magic = EEPROM.read(EEPROM_MAGIC_ADDRESS);

  // EEPROM daha önce initialize edilmemiş.
  if (magic != EEPROM_MAGIC) {

    strcpy(adminPin, DEFAULT_PIN);

    savePinToEEPROM();

    EEPROM.update(
      EEPROM_MAGIC_ADDRESS,
      EEPROM_MAGIC
    );

    return;
  }

  // EEPROM'dan PIN'i oku.
  for (int i = 0; i < 4; i++) {

    adminPin[i] =
      EEPROM.read(EEPROM_PIN_ADDRESS + i);
  }

  adminPin[4] = '\0';

  // EEPROM'daki veri bozuksa default PIN'e dön.
  if (!isValidPin(String(adminPin))) {

    strcpy(adminPin, DEFAULT_PIN);

    savePinToEEPROM();
  }
}


void savePinToEEPROM() {

  for (int i = 0; i < 4; i++) {

    EEPROM.update(
      EEPROM_PIN_ADDRESS + i,
      adminPin[i]
    );
  }

  // String terminator
  EEPROM.update(
    EEPROM_PIN_ADDRESS + 4,
    '\0'
  );

  EEPROM.update(
    EEPROM_MAGIC_ADDRESS,
    EEPROM_MAGIC
  );
}


// ==================================================
// PIN VALIDATION
// ==================================================

bool isValidPin(String pin) {

  if (pin.length() != 4) {
    return false;
  }

  for (int i = 0; i < 4; i++) {

    if (!isDigit(pin[i])) {
      return false;
    }
  }

  return true;
}