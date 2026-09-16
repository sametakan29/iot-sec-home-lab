#include <EEPROM.h>

/*
  NanoLock - Stage 04: Unlimited Login Attempts

  BİLEREK güvensiz eğitim sürümü.

  Önceki açıklar:
    - Unauthenticated DEBUG
    - Sensitive debug logging
    - Unauthenticated SHOW_CONFIG
    - Plaintext PIN disclosure
    - Plaintext EEPROM credential storage

  Yeni güvenlik problemi:
    - LOGIN denemelerinde hiçbir rate limit yok.
    - Yanlış deneme sınırı yok.
    - Lockout yok.
    - Cooldown/delay yok.
    - 4 haneli PIN sınırsız şekilde denenebilir.
*/

const int LOCK_LED = LED_BUILTIN;

bool authenticated = false;
bool lockOpen = false;
bool debugMode = false;

// Bu sayaç yalnızca problemi görünür hale getirmek için.
// DİKKAT: Belirli sayıda denemeden sonra hiçbir şey yapmıyoruz.
unsigned long failedLoginAttempts = 0;
unsigned long successfulLogins = 0;

const int EEPROM_MAGIC_ADDRESS = 0;
const int EEPROM_PIN_ADDRESS = 1;

const byte EEPROM_MAGIC = 0xA5;

const char DEFAULT_PIN[] = "1234";

char adminPin[5];

const String DEVICE_NAME = "NanoLock";
const String FIRMWARE_VERSION = "0.4-unlimited-attempts";


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
  Serial.println("=== NanoLock v0.4 ===");
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
    Serial.println("  AUTH_STATS");

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

    if (debugMode) {

      Serial.print("[DEBUG] Login attempt with PIN: ");
      Serial.println(enteredPin);
    }

    if (enteredPin == String(adminPin)) {

      authenticated = true;
      successfulLogins++;

      if (debugMode) {
        Serial.println("[DEBUG] Authentication successful.");
      }

      Serial.println("LOGIN OK");

    } else {

      failedLoginAttempts++;

      if (debugMode) {

        Serial.println("[DEBUG] Authentication failed.");

        Serial.print("[DEBUG] Failed attempts: ");
        Serial.println(failedLoginAttempts);

        // Bilerek burada:
        // delay yok
        // lockout yok
        // cooldown yok
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
        Serial.println(
          "[DEBUG] OPEN rejected: user not authenticated."
        );
      }

      Serial.println("AUTH REQUIRED");
      return;
    }

    lockOpen = true;

    digitalWrite(LOCK_LED, HIGH);

    if (debugMode) {
      Serial.println(
        "[DEBUG] Lock state changed to OPEN."
      );
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
        Serial.println(
          "[DEBUG] CLOSE rejected: user not authenticated."
        );
      }

      Serial.println("AUTH REQUIRED");
      return;
    }

    lockOpen = false;

    digitalWrite(LOCK_LED, LOW);

    if (debugMode) {
      Serial.println(
        "[DEBUG] Lock state changed to CLOSED."
      );
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
      Serial.println(
        "[DEBUG] User session cleared."
      );
    }

    Serial.println("LOGGED OUT");

    return;
  }


  // ------------------------------------------------
  // CHANGE PIN
  // ------------------------------------------------

  if (command.startsWith("CHANGE_PIN ")) {

    if (!authenticated) {

      Serial.println("AUTH REQUIRED");
      return;
    }

    String newPin = command.substring(11);
    newPin.trim();

    if (!isValidPin(newPin)) {

      Serial.println("INVALID PIN");
      Serial.println(
        "PIN must contain exactly 4 digits."
      );

      return;
    }

    newPin.toCharArray(
      adminPin,
      sizeof(adminPin)
    );

    // Bilerek plaintext EEPROM
    savePinToEEPROM();

    if (debugMode) {

      Serial.print(
        "[DEBUG] New PIN saved to EEPROM: "
      );

      Serial.println(adminPin);
    }

    Serial.println("PIN CHANGED");

    return;
  }


  // ------------------------------------------------
  // AUTH STATS
  // ------------------------------------------------

  // Bilerek authentication gerektirmiyor.
  // Esas amacı rate-limit olmadığını
  // deney sırasında görünür hale getirmek.

  if (command == "AUTH_STATS") {

    Serial.println("=== AUTH STATS ===");

    Serial.print("FAILED_ATTEMPTS: ");
    Serial.println(failedLoginAttempts);

    Serial.print("SUCCESSFUL_LOGINS: ");
    Serial.println(successfulLogins);

    Serial.println("RATE_LIMIT: NONE");
    Serial.println("LOCKOUT: NONE");
    Serial.println("COOLDOWN: NONE");

    Serial.println("==================");

    return;
  }


  // ------------------------------------------------
  // DEBUG ON
  // ------------------------------------------------

  if (command == "DEBUG ON") {

    debugMode = true;

    Serial.println("DEBUG ENABLED");
    Serial.println(
      "[DEBUG] Verbose diagnostic mode active."
    );

    return;
  }


  // ------------------------------------------------
  // DEBUG OFF
  // ------------------------------------------------

  if (command == "DEBUG OFF") {

    Serial.println(
      "[DEBUG] Disabling diagnostic mode."
    );

    debugMode = false;

    Serial.println("DEBUG DISABLED");

    return;
  }


  // ------------------------------------------------
  // SHOW CONFIG
  // ------------------------------------------------

  if (command == "SHOW_CONFIG") {

    Serial.println("=== DEVICE CONFIG ===");

    Serial.print("DEVICE: ");
    Serial.println(DEVICE_NAME);

    Serial.print("FIRMWARE: ");
    Serial.println(FIRMWARE_VERSION);

    Serial.print("ADMIN_PIN: ");
    Serial.println(adminPin);

    Serial.println(
      "PIN_STORAGE: EEPROM PLAINTEXT"
    );

    Serial.println(
      "AUTH_RATE_LIMIT: DISABLED"
    );

    Serial.println(
      "AUTH_LOCKOUT: DISABLED"
    );

    Serial.print("LOCK_STATE: ");
    Serial.println(
      lockOpen ? "OPEN" : "CLOSED"
    );

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
  // UNKNOWN
  // ------------------------------------------------

  if (debugMode) {

    Serial.print(
      "[DEBUG] Unknown command received: "
    );

    Serial.println(command);
  }

  Serial.println("UNKNOWN COMMAND");
}


// ==================================================
// EEPROM
// ==================================================

void loadOrInitializePin() {

  byte magic =
    EEPROM.read(EEPROM_MAGIC_ADDRESS);

  if (magic != EEPROM_MAGIC) {

    strcpy(adminPin, DEFAULT_PIN);

    savePinToEEPROM();

    EEPROM.update(
      EEPROM_MAGIC_ADDRESS,
      EEPROM_MAGIC
    );

    return;
  }

  for (int i = 0; i < 4; i++) {

    adminPin[i] =
      EEPROM.read(
        EEPROM_PIN_ADDRESS + i
      );
  }

  adminPin[4] = '\0';

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