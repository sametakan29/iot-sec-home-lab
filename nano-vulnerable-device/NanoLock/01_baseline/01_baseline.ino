/*
  NanoLock - Stage 01: Baseline

  Basit bir seri port kontrollü kilit sistemi.

  Komutlar:
    HELP
    STATUS
    LOGIN 1234
    OPEN
    CLOSE
    LOGOUT

  Şimdilik amaç sadece çalışan temel sistemi kurmak.
  Güvenlik açıklarını sonraki aşamalarda bilinçli olarak ekleyeceğiz.
*/

const int LOCK_LED = LED_BUILTIN;

bool authenticated = false;
bool lockOpen = false;

const String ADMIN_PIN = "1234";

void setup() {
  pinMode(LOCK_LED, OUTPUT);

  // Kilit başlangıçta kapalı.
  digitalWrite(LOCK_LED, LOW);

  Serial.begin(9600);

  delay(500);

  Serial.println();
  Serial.println("=== NanoLock v1 ===");
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
    return;
  }

  // -----------------------
  // STATUS
  // -----------------------

  if (command == "STATUS") {

    Serial.print("LOCK: ");

    if (lockOpen) {
      Serial.println("OPEN");
    } else {
      Serial.println("CLOSED");
    }

    Serial.print("AUTH: ");

    if (authenticated) {
      Serial.println("YES");
    } else {
      Serial.println("NO");
    }

    return;
  }

  // -----------------------
  // LOGIN
  // -----------------------

  if (command.startsWith("LOGIN ")) {

    String enteredPin = command.substring(6);
    enteredPin.trim();

    if (enteredPin == ADMIN_PIN) {
      authenticated = true;
      Serial.println("LOGIN OK");
    } else {
      Serial.println("LOGIN FAILED");
    }

    return;
  }

  // -----------------------
  // OPEN
  // -----------------------

  if (command == "OPEN") {

    if (!authenticated) {
      Serial.println("AUTH REQUIRED");
      return;
    }

    lockOpen = true;

    digitalWrite(LOCK_LED, HIGH);

    Serial.println("LOCK OPENED");

    return;
  }

  // -----------------------
  // CLOSE
  // -----------------------

  if (command == "CLOSE") {

    if (!authenticated) {
      Serial.println("AUTH REQUIRED");
      return;
    }

    lockOpen = false;

    digitalWrite(LOCK_LED, LOW);

    Serial.println("LOCK CLOSED");

    return;
  }

  // -----------------------
  // LOGOUT
  // -----------------------

  if (command == "LOGOUT") {

    authenticated = false;

    Serial.println("LOGGED OUT");

    return;
  }

  // -----------------------
  // UNKNOWN COMMAND
  // -----------------------

  Serial.println("UNKNOWN COMMAND");
}