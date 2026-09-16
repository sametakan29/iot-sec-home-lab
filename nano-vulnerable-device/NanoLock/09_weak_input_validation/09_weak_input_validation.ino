#include <EEPROM.h>
#include <string.h>
#include <stdlib.h>

/*
  NanoLock - Stage 09: Weak Input Validation

  BİLEREK güvensiz eğitim sürümü.

  Yeni problem:
  - LOGIN sırasında PIN string olarak tam eşleştirilmiyor.
  - atoi() kullanılıyor.
  - Girdinin tam olarak 4 rakam olduğu doğrulanmıyor.
  - "1234abc", "001234", "+1234" gibi malformed inputlar
    geçerli PIN 1234 ile aynı sayıya dönüşebiliyor.
*/

const byte LOCK_LED = LED_BUILTIN;

bool authenticated = false;
bool lockOpen = false;
bool debugMode = false;

unsigned long failedLoginAttempts = 0;
unsigned long successfulLogins = 0;
unsigned long sessionStartedAt = 0;

const int EEPROM_MAGIC_ADDRESS = 0;
const int EEPROM_PIN_ADDRESS = 1;

const byte EEPROM_MAGIC = 0xA5;

const char DEFAULT_PIN[] = "1234";
char adminPin[5];

const char REMOTE_TOKEN[] = "481516";
unsigned long remoteOpenCount = 0;

const byte COMMAND_BUFFER_SIZE = 40;
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
  Serial.println(F("=== NanoLock v0.9 ==="));
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
    Serial.println(F("  FACTORY_RESET"));
    Serial.println(F("  REMOTE_OPEN <TOKEN>"));
    Serial.println(F("  REMOTE_STATS"));

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
  // LOGIN - BILEREK HATALI
  // ------------------------------------------------

  if (strncmp(command, "LOGIN ", 6) == 0) {

    char* enteredPin = command + 6;

    if (debugMode) {
      Serial.print(F("[DEBUG] Raw login input: "));
      Serial.println(enteredPin);
    }

    /*
      BILEREK GUVENSIZ:

      atoi("1234abc") -> 1234
      atoi("001234")  -> 1234
      atoi("+1234")   -> 1234

      Yani input formatını doğrulamadan integer'a
      çevirip karşılaştırıyoruz.
    */

    int enteredPinNumber = atoi(enteredPin);
    int storedPinNumber = atoi(adminPin);

    if (debugMode) {

      Serial.print(F("[DEBUG] Parsed PIN value: "));
      Serial.println(enteredPinNumber);
    }

    if (enteredPinNumber == storedPinNumber) {

      authenticated = true;
      successfulLogins++;

      sessionStartedAt = millis();

      Serial.println(F("LOGIN OK"));

    } else {

      failedLoginAttempts++;

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

    openLock();

    Serial.println(F("LOCK OPENED"));

    return;
  }


  // ------------------------------------------------
  // REMOTE OPEN
  // ------------------------------------------------

  if (strncmp(command, "REMOTE_OPEN ", 12) == 0) {

    char* receivedToken = command + 12;

    if (strcmp(receivedToken, REMOTE_TOKEN) != 0) {

      Serial.println(F("REMOTE AUTH FAILED"));
      return;
    }

    remoteOpenCount++;

    openLock();

    Serial.println(F("REMOTE AUTH OK"));
    Serial.println(F("REMOTE LOCK OPENED"));

    return;
  }


  // ------------------------------------------------
  // REMOTE STATS
  // ------------------------------------------------

  if (strcmp(command, "REMOTE_STATS") == 0) {

    Serial.println(F("=== REMOTE PROTOCOL ==="));

    Serial.print(F("ACCEPTED_REMOTE_OPEN: "));
    Serial.println(remoteOpenCount);

    Serial.println(F("TOKEN_TYPE: STATIC"));
    Serial.println(F("NONCE: NONE"));
    Serial.println(F("SEQUENCE_NUMBER: NONE"));
    Serial.println(F("TIMESTAMP: NONE"));
    Serial.println(F("REPLAY_PROTECTION: DISABLED"));

    Serial.println(F("======================="));

    return;
  }


  // ------------------------------------------------
  // HIDDEN SERVICE OPEN
  // ------------------------------------------------

  if (strcmp(command, "SERVICE_OPEN") == 0) {

    openLock();

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

    closeLock();

    Serial.println(F("LOCK CLOSED"));

    return;
  }


  // ------------------------------------------------
  // LOGOUT
  // ------------------------------------------------

  if (strcmp(command, "LOGOUT") == 0) {

    authenticated = false;
    sessionStartedAt = 0;

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

    // Burada validation hâlâ düzgün.
    if (!isValidPin(newPin)) {

      Serial.println(F("INVALID PIN"));
      Serial.println(F("PIN must be exactly 4 digits."));

      return;
    }

    strcpy(adminPin, newPin);

    savePinToEEPROM();

    Serial.println(F("PIN CHANGED"));

    return;
  }


  // ------------------------------------------------
  // FACTORY RESET
  // ------------------------------------------------

  if (strcmp(command, "FACTORY_RESET") == 0) {

    strcpy(adminPin, DEFAULT_PIN);

    savePinToEEPROM();

    authenticated = false;
    sessionStartedAt = 0;

    closeLock();

    Serial.println(F("FACTORY RESET COMPLETE"));
    Serial.println(F("DEFAULT PIN RESTORED"));
    Serial.println(F("DEVICE PIN: 1234"));

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

      unsigned long age =
        (millis() - sessionStartedAt) / 1000;

      Serial.print(F("SESSION_AGE_SECONDS: "));
      Serial.println(age);

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

    Serial.println(F("=================="));

    return;
  }


  // ------------------------------------------------
  // DEBUG
  // ------------------------------------------------

  if (strcmp(command, "DEBUG ON") == 0) {

    debugMode = true;

    Serial.println(F("DEBUG ENABLED"));

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
    Serial.println(F("FIRMWARE: 0.9-weak-input-validation"));

    Serial.print(F("ADMIN_PIN: "));
    Serial.println(adminPin);

    Serial.println(F("PIN_STORAGE: EEPROM PLAINTEXT"));
    Serial.println(F("AUTH_RATE_LIMIT: DISABLED"));
    Serial.println(F("SESSION_TIMEOUT: DISABLED"));
    Serial.println(F("FACTORY_RESET_AUTH: DISABLED"));
    Serial.println(F("LOGIN_VALIDATION: atoi()"));

    Serial.print(F("REMOTE_TOKEN: "));
    Serial.println(REMOTE_TOKEN);

    Serial.println(F("REMOTE_REPLAY_PROTECTION: DISABLED"));

    Serial.println(F("====================="));

    return;
  }


  Serial.println(F("UNKNOWN COMMAND"));
}


// ==================================================
// LOCK
// ==================================================

void openLock() {

  lockOpen = true;
  digitalWrite(LOCK_LED, HIGH);
}


void closeLock() {

  lockOpen = false;
  digitalWrite(LOCK_LED, LOW);
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
// STRICT PIN VALIDATION
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