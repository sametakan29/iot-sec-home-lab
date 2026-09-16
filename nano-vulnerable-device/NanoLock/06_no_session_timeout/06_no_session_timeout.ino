#include <EEPROM.h>
#include <string.h>

/*
  NanoLock - Stage 06: No Session Timeout

  BİLEREK güvensiz eğitim sürümü.

  Önceki problemler:
  - Unauthenticated DEBUG
  - Sensitive debug logging
  - Unauthenticated SHOW_CONFIG
  - Plaintext EEPROM PIN
  - Unlimited login attempts
  - No rate limit / lockout
  - Hidden SERVICE_OPEN authorization bypass

  Yeni problem:
  - Başarılı login sonrası session süresiz açık kalıyor.
  - Inactivity timeout YOK.
  - Session expiration YOK.
*/

const byte LOCK_LED = LED_BUILTIN;

bool authenticated = false;
bool lockOpen = false;
bool debugMode = false;

unsigned long failedLoginAttempts = 0;
unsigned long successfulLogins = 0;

// Session bilgileri
unsigned long sessionStartedAt = 0;
unsigned long lastActivityAt = 0;


// ==================================================
// EEPROM
// ==================================================

const int EEPROM_MAGIC_ADDRESS = 0;
const int EEPROM_PIN_ADDRESS = 1;

const byte EEPROM_MAGIC = 0xA5;

const char DEFAULT_PIN[] = "1234";

char adminPin[5];


// ==================================================
// SERIAL BUFFER
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
  Serial.println(F("=== NanoLock v0.6 ==="));
  Serial.println(F("System ready."));
  Serial.println(F("Type HELP to see commands."));
  Serial.println();
}


// ==================================================
// LOOP
// ==================================================

void loop() {

  if (Serial.available() > 0) {

    size_t length = Serial.readBytesUntil(
      '\n',
      commandBuffer,
      COMMAND_BUFFER_SIZE - 1
    );

    commandBuffer[length] = '\0';

    cleanCommand(commandBuffer);

    if (commandBuffer[0] == '\0') {
      return;
    }

    // Authenticated kullanıcı herhangi bir işlem yaptığında
    // aktivite zamanı güncelleniyor.
    //
    // Ancak bilerek hiçbir timeout uygulanmıyor.
    if (authenticated) {
      lastActivityAt = millis();
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
    Serial.println(F("  SESSION_INFO"));

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

      sessionStartedAt = millis();
      lastActivityAt = millis();

      if (debugMode) {
        Serial.println(F("[DEBUG] Authentication successful."));
        Serial.println(F("[DEBUG] Session created WITHOUT expiration."));
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
  // OPEN
  // ------------------------------------------------

  if (strcmp(command, "OPEN") == 0) {

    if (!authenticated) {

      Serial.println(F("AUTH REQUIRED"));
      return;
    }

    lockOpen = true;
    digitalWrite(LOCK_LED, HIGH);

    Serial.println(F("LOCK OPENED"));

    return;
  }


  // ------------------------------------------------
  // HIDDEN SERVICE OPEN
  // ------------------------------------------------

  if (strcmp(command, "SERVICE_OPEN") == 0) {

    lockOpen = true;
    digitalWrite(LOCK_LED, HIGH);

    Serial.println(F("SERVICE OVERRIDE"));
    Serial.println(F("LOCK OPENED"));

    return;
  }


  // ------------------------------------------------
  // CLOSE
  // ------------------------------------------------

  if (strcmp(command, "CLOSE") == 0) {

    if (!authenticated) {

      Serial.println(F("AUTH REQUIRED"));
      return;
    }

    lockOpen = false;
    digitalWrite(LOCK_LED, LOW);

    Serial.println(F("LOCK CLOSED"));

    return;
  }


  // ------------------------------------------------
  // LOGOUT
  // ------------------------------------------------

  if (strcmp(command, "LOGOUT") == 0) {

    authenticated = false;

    sessionStartedAt = 0;
    lastActivityAt = 0;

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
  // SESSION INFO
  // ------------------------------------------------

  if (strcmp(command, "SESSION_INFO") == 0) {

    Serial.println(F("=== SESSION INFO ==="));

    Serial.print(F("AUTHENTICATED: "));
    Serial.println(
      authenticated ?
      F("YES") :
      F("NO")
    );

    if (authenticated) {

      unsigned long now = millis();

      unsigned long sessionAge =
        (now - sessionStartedAt) / 1000;

      unsigned long idleTime =
        (now - lastActivityAt) / 1000;

      Serial.print(F("SESSION_AGE_SECONDS: "));
      Serial.println(sessionAge);

      Serial.print(F("IDLE_SECONDS: "));
      Serial.println(idleTime);

      // Buradaki asıl güvenlik problemi
      Serial.println(F("SESSION_TIMEOUT: DISABLED"));
      Serial.println(F("SESSION_EXPIRATION: NONE"));
    }

    Serial.println(F("===================="));

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
  // DEBUG
  // ------------------------------------------------

  if (strcmp(command, "DEBUG ON") == 0) {

    debugMode = true;

    Serial.println(F("DEBUG ENABLED"));
    Serial.println(F("[DEBUG] Verbose diagnostic mode active."));

    return;
  }

  if (strcmp(command, "DEBUG OFF") == 0) {

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
    Serial.println(F("FIRMWARE: 0.6-no-session-timeout"));

    Serial.print(F("ADMIN_PIN: "));
    Serial.println(adminPin);

    Serial.println(F("PIN_STORAGE: EEPROM PLAINTEXT"));
    Serial.println(F("AUTH_RATE_LIMIT: DISABLED"));
    Serial.println(F("AUTH_LOCKOUT: DISABLED"));
    Serial.println(F("SESSION_TIMEOUT: DISABLED"));

    Serial.print(F("LOCK_STATE: "));
    Serial.println(lockOpen ? F("OPEN") : F("CLOSED"));

    Serial.print(F("AUTH_STATE: "));
    Serial.println(
      authenticated ?
      F("AUTHENTICATED") :
      F("NOT_AUTHENTICATED")
    );

    Serial.println(F("====================="));

    return;
  }


  // ------------------------------------------------
  // UNKNOWN
  // ------------------------------------------------

  Serial.println(F("UNKNOWN COMMAND"));
}


// ==================================================
// CLEAN COMMAND
// ==================================================

void cleanCommand(char* text) {

  size_t len = strlen(text);

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

  byte magic = EEPROM.read(
    EEPROM_MAGIC_ADDRESS
  );

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

    adminPin[i] = EEPROM.read(
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