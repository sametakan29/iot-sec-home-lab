#include <EEPROM.h>
#include <string.h>

/*
  NanoLock - Stage 05: Broken Authorization
  MEMORY OPTIMIZED VERSION

  Bilerek bulunan güvenlik problemleri:
  - Unauthenticated DEBUG
  - Sensitive debug logging
  - Unauthenticated SHOW_CONFIG
  - Plaintext PIN disclosure
  - Plaintext EEPROM PIN storage
  - Unlimited login attempts
  - No rate limit / lockout
  - Hidden SERVICE_OPEN authorization bypass

  SRAM optimizasyonları:
  - Arduino String kullanılmıyor.
  - Sabit Serial metinleri F() ile Flash'tan okunuyor.
*/

const byte LOCK_LED = LED_BUILTIN;

bool authenticated = false;
bool lockOpen = false;
bool debugMode = false;

unsigned long failedLoginAttempts = 0;
unsigned long successfulLogins = 0;


// ==================================================
// EEPROM CONFIG
// ==================================================

const int EEPROM_MAGIC_ADDRESS = 0;
const int EEPROM_PIN_ADDRESS = 1;

const byte EEPROM_MAGIC = 0xA5;

const char DEFAULT_PIN[] = "1234";

char adminPin[5];


// ==================================================
// SERIAL COMMAND BUFFER
// ==================================================

const byte COMMAND_BUFFER_SIZE = 32;
char commandBuffer[COMMAND_BUFFER_SIZE];


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
  Serial.println(F("=== NanoLock v0.5 ==="));
  Serial.println(F("System ready."));
  Serial.println(F("Type HELP to see commands."));
  Serial.println();
}


// ==================================================
// LOOP
// ==================================================

void loop() {

  if (Serial.available() > 0) {

    size_t length =
      Serial.readBytesUntil(
        '\n',
        commandBuffer,
        COMMAND_BUFFER_SIZE - 1
      );

    commandBuffer[length] = '\0';

    cleanCommand(commandBuffer);

    if (commandBuffer[0] == '\0') {
      return;
    }

    if (debugMode) {

      Serial.print(F("[DEBUG] Received command: "));
      Serial.println(commandBuffer);
    }

    handleCommand(commandBuffer);
  }
}


// ==================================================
// COMMAND HANDLER
// ==================================================

void handleCommand(char* command) {

  // ------------------------------------------------
  // HELP
  // ------------------------------------------------

  if (strcmp(command, "HELP") == 0) {

    Serial.println(F("Available commands:"));
    Serial.println(F("  HELP"));
    Serial.println(F("  STATUS"));
    Serial.println(F("  LOGIN <PIN>"));
    Serial.println(F("  OPEN"));
    Serial.println(F("  CLOSE"));
    Serial.println(F("  LOGOUT"));
    Serial.println(F("  CHANGE_PIN <NEW_PIN>"));
    Serial.println(F("  DEBUG ON"));
    Serial.println(F("  DEBUG OFF"));
    Serial.println(F("  SHOW_CONFIG"));
    Serial.println(F("  AUTH_STATS"));

    // SERVICE_OPEN bilerek gizli.

    return;
  }


  // ------------------------------------------------
  // STATUS
  // ------------------------------------------------

  if (strcmp(command, "STATUS") == 0) {

    Serial.print(F("LOCK: "));
    Serial.println(lockOpen ? F("OPEN") : F("CLOSED"));

    Serial.print(F("AUTH: "));
    Serial.println(authenticated ? F("YES") : F("NO"));

    Serial.print(F("DEBUG: "));
    Serial.println(debugMode ? F("ON") : F("OFF"));

    return;
  }


  // ------------------------------------------------
  // LOGIN
  // ------------------------------------------------

  if (strncmp(command, "LOGIN ", 6) == 0) {

    char* enteredPin = command + 6;

    if (debugMode) {

      Serial.print(F("[DEBUG] Login attempt with PIN: "));
      Serial.println(enteredPin);
    }

    if (strcmp(enteredPin, adminPin) == 0) {

      authenticated = true;
      successfulLogins++;

      if (debugMode) {
        Serial.println(F("[DEBUG] Authentication successful."));
      }

      Serial.println(F("LOGIN OK"));

    } else {

      failedLoginAttempts++;

      if (debugMode) {

        Serial.println(F("[DEBUG] Authentication failed."));

        Serial.print(F("[DEBUG] Failed attempts: "));
        Serial.println(failedLoginAttempts);
      }

      Serial.println(F("LOGIN FAILED"));
    }

    return;
  }


  // ------------------------------------------------
  // NORMAL OPEN
  // ------------------------------------------------

  if (strcmp(command, "OPEN") == 0) {

    if (!authenticated) {

      if (debugMode) {
        Serial.println(
          F("[DEBUG] OPEN rejected: user not authenticated.")
        );
      }

      Serial.println(F("AUTH REQUIRED"));
      return;
    }

    lockOpen = true;

    digitalWrite(LOCK_LED, HIGH);

    if (debugMode) {
      Serial.println(F("[DEBUG] Lock state changed to OPEN."));
    }

    Serial.println(F("LOCK OPENED"));

    return;
  }


  // ------------------------------------------------
  // HIDDEN SERVICE OPEN
  // ------------------------------------------------

  /*
    BİLEREK GÜVENSİZ:

    Servis teknisyeni için bırakıldığı varsayılan
    gizli komut.

    Authentication / authorization kontrolü YOK.
  */

  if (strcmp(command, "SERVICE_OPEN") == 0) {

    lockOpen = true;

    digitalWrite(LOCK_LED, HIGH);

    if (debugMode) {
      Serial.println(F("[DEBUG] Service override activated."));
    }

    Serial.println(F("SERVICE OVERRIDE"));
    Serial.println(F("LOCK OPENED"));

    return;
  }


  // ------------------------------------------------
  // CLOSE
  // ------------------------------------------------

  if (strcmp(command, "CLOSE") == 0) {

    if (!authenticated) {

      if (debugMode) {
        Serial.println(
          F("[DEBUG] CLOSE rejected: user not authenticated.")
        );
      }

      Serial.println(F("AUTH REQUIRED"));
      return;
    }

    lockOpen = false;

    digitalWrite(LOCK_LED, LOW);

    if (debugMode) {
      Serial.println(F("[DEBUG] Lock state changed to CLOSED."));
    }

    Serial.println(F("LOCK CLOSED"));

    return;
  }


  // ------------------------------------------------
  // LOGOUT
  // ------------------------------------------------

  if (strcmp(command, "LOGOUT") == 0) {

    authenticated = false;

    if (debugMode) {
      Serial.println(F("[DEBUG] User session cleared."));
    }

    Serial.println(F("LOGGED OUT"));

    return;
  }


  // ------------------------------------------------
  // CHANGE PIN
  // ------------------------------------------------

  if (strncmp(command, "CHANGE_PIN ", 11) == 0) {

    if (!authenticated) {

      Serial.println(F("AUTH REQUIRED"));
      return;
    }

    char* newPin = command + 11;

    if (!isValidPin(newPin)) {

      Serial.println(F("INVALID PIN"));
      Serial.println(F("PIN must contain exactly 4 digits."));

      return;
    }

    strcpy(adminPin, newPin);

    savePinToEEPROM();

    if (debugMode) {

      Serial.print(F("[DEBUG] New PIN saved to EEPROM: "));
      Serial.println(adminPin);
    }

    Serial.println(F("PIN CHANGED"));

    return;
  }


  // ------------------------------------------------
  // AUTH STATS
  // ------------------------------------------------

  if (strcmp(command, "AUTH_STATS") == 0) {

    Serial.println(F("=== AUTH STATS ==="));

    Serial.print(F("FAILED_ATTEMPTS: "));
    Serial.println(failedLoginAttempts);

    Serial.print(F("SUCCESSFUL_LOGINS: "));
    Serial.println(successfulLogins);

    Serial.println(F("RATE_LIMIT: NONE"));
    Serial.println(F("LOCKOUT: NONE"));
    Serial.println(F("COOLDOWN: NONE"));

    Serial.println(F("=================="));

    return;
  }


  // ------------------------------------------------
  // DEBUG ON
  // ------------------------------------------------

  if (strcmp(command, "DEBUG ON") == 0) {

    debugMode = true;

    Serial.println(F("DEBUG ENABLED"));
    Serial.println(F("[DEBUG] Verbose diagnostic mode active."));

    return;
  }


  // ------------------------------------------------
  // DEBUG OFF
  // ------------------------------------------------

  if (strcmp(command, "DEBUG OFF") == 0) {

    Serial.println(F("[DEBUG] Disabling diagnostic mode."));

    debugMode = false;

    Serial.println(F("DEBUG DISABLED"));

    return;
  }


  // ------------------------------------------------
  // SHOW CONFIG
  // ------------------------------------------------

  if (strcmp(command, "SHOW_CONFIG") == 0) {

    Serial.println(F("=== DEVICE CONFIG ==="));

    Serial.println(F("DEVICE: NanoLock"));
    Serial.println(F("FIRMWARE: 0.5-broken-authz"));

    Serial.print(F("ADMIN_PIN: "));
    Serial.println(adminPin);

    Serial.println(F("PIN_STORAGE: EEPROM PLAINTEXT"));
    Serial.println(F("AUTH_RATE_LIMIT: DISABLED"));
    Serial.println(F("AUTH_LOCKOUT: DISABLED"));

    Serial.print(F("LOCK_STATE: "));
    Serial.println(lockOpen ? F("OPEN") : F("CLOSED"));

    Serial.print(F("AUTH_STATE: "));
    Serial.println(
      authenticated ?
      F("AUTHENTICATED") :
      F("NOT_AUTHENTICATED")
    );

    Serial.print(F("DEBUG_MODE: "));
    Serial.println(
      debugMode ?
      F("ENABLED") :
      F("DISABLED")
    );

    Serial.println(F("====================="));

    return;
  }


  // ------------------------------------------------
  // UNKNOWN
  // ------------------------------------------------

  if (debugMode) {

    Serial.print(F("[DEBUG] Unknown command received: "));
    Serial.println(command);
  }

  Serial.println(F("UNKNOWN COMMAND"));
}


// ==================================================
// COMMAND CLEANUP
// ==================================================

void cleanCommand(char* text) {

  size_t len = strlen(text);

  // Sondaki CR / space / tab karakterlerini temizle.
  while (
    len > 0 &&
    (
      text[len - 1] == '\r' ||
      text[len - 1] == ' ' ||
      text[len - 1] == '\t'
    )
  ) {

    text[len - 1] = '\0';
    len--;
  }
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

  for (byte i = 0; i < 4; i++) {

    adminPin[i] =
      EEPROM.read(
        EEPROM_PIN_ADDRESS + i
      );
  }

  adminPin[4] = '\0';

  if (!isValidPin(adminPin)) {

    strcpy(adminPin, DEFAULT_PIN);

    savePinToEEPROM();
  }
}


void savePinToEEPROM() {

  for (byte i = 0; i < 4; i++) {

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

bool isValidPin(const char* pin) {

  if (strlen(pin) != 4) {
    return false;
  }

  for (byte i = 0; i < 4; i++) {

    if (
      pin[i] < '0' ||
      pin[i] > '9'
    ) {
      return false;
    }
  }

  return true;
}