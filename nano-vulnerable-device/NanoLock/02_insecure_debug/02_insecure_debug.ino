/*
  NanoLock - Stage 02: Insecure Debug & Information Disclosure

  Bu sürüm BİLEREK güvensiz tasarlanmıştır.

  Yeni kötü tasarım kararları:
    1. DEBUG modu authentication gerektirmiyor.
    2. SHOW_CONFIG authentication gerektirmiyor.
    3. SHOW_CONFIG admin PIN'i açıkça gösteriyor.
    4. Debug modu kullanıcının gönderdiği komutları logluyor.
    5. Login sırasında girilen PIN debug çıktısında görünüyor.

  SADECE eğitim/lab amacıyla.
*/

const int LOCK_LED = LED_BUILTIN;

bool authenticated = false;
bool lockOpen = false;
bool debugMode = false;

// Bilerek firmware içine gömülü credential
const String ADMIN_PIN = "1234";

const String DEVICE_NAME = "NanoLock";
const String FIRMWARE_VERSION = "0.2-insecure";

void setup() {
  pinMode(LOCK_LED, OUTPUT);

  digitalWrite(LOCK_LED, LOW);

  Serial.begin(9600);

  delay(500);

  Serial.println();
  Serial.println("=== NanoLock v0.2 ===");
  Serial.println("System ready.");
  Serial.println("Type HELP to see commands.");
  Serial.println();
}

void loop() {
  if (Serial.available() > 0) {

    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.length() == 0) {
      return;
    }

    // Bilerek kötü:
    // Debug açıkken kullanıcının girdiği her şeyi logluyor.
    if (debugMode) {
      Serial.print("[DEBUG] Received command: ");
      Serial.println(command);
    }

    handleCommand(command);
  }
}

void handleCommand(String command) {

  // -----------------------
  // HELP
  // -----------------------

  if (command == "HELP") {
    Serial.println("Available commands:");
    Serial.println("  HELP");
    Serial.println("  STATUS");
    Serial.println("  LOGIN <PIN>");
    Serial.println("  OPEN");
    Serial.println("  CLOSE");
    Serial.println("  LOGOUT");
    Serial.println("  DEBUG ON");
    Serial.println("  DEBUG OFF");
    Serial.println("  SHOW_CONFIG");
    return;
  }

  // -----------------------
  // STATUS
  // -----------------------

  if (command == "STATUS") {

    Serial.print("LOCK: ");
    Serial.println(lockOpen ? "OPEN" : "CLOSED");

    Serial.print("AUTH: ");
    Serial.println(authenticated ? "YES" : "NO");

    Serial.print("DEBUG: ");
    Serial.println(debugMode ? "ON" : "OFF");

    return;
  }

  // -----------------------
  // LOGIN
  // -----------------------

  if (command.startsWith("LOGIN ")) {

    String enteredPin = command.substring(6);
    enteredPin.trim();

    // Bilerek kötü:
    // Girilen PIN debug loguna yazılıyor.
    if (debugMode) {
      Serial.print("[DEBUG] Login attempt with PIN: ");
      Serial.println(enteredPin);
    }

    if (enteredPin == ADMIN_PIN) {
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

  // -----------------------
  // OPEN
  // -----------------------

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

  // -----------------------
  // CLOSE
  // -----------------------

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

  // -----------------------
  // LOGOUT
  // -----------------------

  if (command == "LOGOUT") {

    authenticated = false;

    if (debugMode) {
      Serial.println("[DEBUG] User session cleared.");
    }

    Serial.println("LOGGED OUT");

    return;
  }

  // -----------------------
  // DEBUG ON
  // -----------------------

  // Bilerek kötü:
  // Authentication gerektirmiyor.
  if (command == "DEBUG ON") {

    debugMode = true;

    Serial.println("DEBUG ENABLED");
    Serial.println("[DEBUG] Verbose diagnostic mode active.");

    return;
  }

  // -----------------------
  // DEBUG OFF
  // -----------------------

  if (command == "DEBUG OFF") {

    Serial.println("[DEBUG] Disabling diagnostic mode.");

    debugMode = false;

    Serial.println("DEBUG DISABLED");

    return;
  }

  // -----------------------
  // SHOW_CONFIG
  // -----------------------

  // Bilerek ÇOK kötü:
  // Authentication gerektirmiyor
  // ve hassas bilgiler gösteriyor.
  if (command == "SHOW_CONFIG") {

    Serial.println("=== DEVICE CONFIG ===");

    Serial.print("DEVICE: ");
    Serial.println(DEVICE_NAME);

    Serial.print("FIRMWARE: ");
    Serial.println(FIRMWARE_VERSION);

    Serial.print("ADMIN_PIN: ");
    Serial.println(ADMIN_PIN);

    Serial.print("LOCK_STATE: ");
    Serial.println(lockOpen ? "OPEN" : "CLOSED");

    Serial.print("AUTH_STATE: ");
    Serial.println(authenticated ? "AUTHENTICATED" : "NOT_AUTHENTICATED");

    Serial.print("DEBUG_MODE: ");
    Serial.println(debugMode ? "ENABLED" : "DISABLED");

    Serial.println("=====================");

    return;
  }

  // -----------------------
  // UNKNOWN COMMAND
  // -----------------------

  if (debugMode) {
    Serial.print("[DEBUG] Unknown command received: ");
    Serial.println(command);
  }

  Serial.println("UNKNOWN COMMAND");
}