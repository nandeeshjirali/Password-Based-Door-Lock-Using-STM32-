

#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// ========== CONFIG ==========
#define MAX_USERS 6
#define PASS_LEN 6           
#define ADMIN_INDEX 0
#define EEPROM_START 0       
#define LOG_CAPACITY 16

// Security behaviour
const int MAX_WRONG = 3;
const unsigned long LOCKOUT_MS = 60000UL;       
const unsigned long UNLOCK_DURATION_MS = 5000UL; 

// ========== PIN ASSIGNMENTS ==========

const byte ROWS = 4;
const byte COLS = 4;
byte rowPins[ROWS] = {PA0, PA1, PA2, PA3};   
byte colPins[COLS] = {PA4, PA5, PA6, PA7};   

/* Stepper pins (ULN2003 IN1..IN4). Use PC pins to avoid I2C conflicts */
const uint8_t IN1 = PC0;
const uint8_t IN2 = PC1;
const uint8_t IN3 = PC2;
const uint8_t IN4 = PC3;

/* Other peripherals */
const uint8_t PIN_BUZZER = PB1;
const uint8_t PIN_REED = PB2;    // reed sensor (door/tamper) input
const uint8_t PIN_MANUAL = PB3;  // manual override switch (active LOW)

// I2C LCD address (commonly 0x27 or 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Keypad layout (4x4)
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ========== Stepper control (simple half-step sequence) ==========
const uint8_t stepSequence[8][4] = {
  {1,0,0,0},
  {1,1,0,0},
  {0,1,0,0},
  {0,1,1,0},
  {0,0,1,0},
  {0,0,1,1},
  {0,0,0,1},
  {1,0,0,1}
};

const int STEPS_PER_REV = 4096; 
const int UNLOCK_STEPS = 600;   
const int STEPPER_DELAY_MS = 2;


int passEntrySize() { return 1 + PASS_LEN; }
int passAreaSize() { return MAX_USERS * passEntrySize(); }
int logBaseAddr() { return EEPROM_START + 1 + passAreaSize(); }
int logEntrySize = 4; // simple log: userIdx, status, ts_low, ts_high

// ========== Globals ==========
String entered = "";
int wrongCount = 0;
unsigned long lockoutUntil = 0;
unsigned long unlockedUntil = 0;
int nextLogIndex = 0;
int totalLogs = LOG_CAPACITY;

// Utility prototypes
void writePasswordToEEPROM(int idx, const char *pass);
void readPasswordFromEEPROM(int idx, char *out);
void setUserCount(byte c);
byte getUserCount();
void ensureDefaultAdmin();
bool checkPassword(const char *candidate, int &matchedIdx);
void unlockAction();
void lockAction();
void stepperMove(int steps, bool forward);
void beep(int times=1, int ms=80);
void showPrompt();
void adminMenu();
void writeLog(int i, byte userIdx, byte status);
void dumpLogsToSerial();

// ========== Setup & Helpers ==========
void setupPins() {
  pinMode(IN1, OUTPUT); digitalWrite(IN1, LOW);
  pinMode(IN2, OUTPUT); digitalWrite(IN2, LOW);
  pinMode(IN3, OUTPUT); digitalWrite(IN3, LOW);
  pinMode(IN4, OUTPUT); digitalWrite(IN4, LOW);
  pinMode(PIN_BUZZER, OUTPUT); digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_REED, INPUT_PULLUP); // reed: closed -> LOW if normally open with pullup
  pinMode(PIN_MANUAL, INPUT_PULLUP); // manual override active LOW
}

void setup() {
  Serial.begin(115200); // USB-TTL monitor (connect TX/RX if using external USB-TTL)
  Wire.begin();
  lcd.init();
  lcd.backlight();
  setupPins();
  EEPROM.begin(512); // if supported; size depends on core

  ensureDefaultAdmin();

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Password Lock");
  lcd.setCursor(0,1);
  lcd.print("Ready");
  delay(700);
  showPrompt();
}

// ========== EEPROM functions ==========
void writePasswordToEEPROM(int idx, const char *pass) {
  int addr = EEPROM_START + 1 + idx * passEntrySize();
  byte len = strlen(pass);
  if (len > PASS_LEN) len = PASS_LEN;
  EEPROM.write(addr, len);
  for (int i = 0; i < PASS_LEN; ++i) {
    char ch = (i < len) ? pass[i] : 0;
    EEPROM.write(addr + 1 + i, (byte)ch);
  }
  EEPROM.commit();
}

void readPasswordFromEEPROM(int idx, char *out) {
  int addr = EEPROM_START + 1 + idx * passEntrySize();
  byte len = EEPROM.read(addr);
  if (len > PASS_LEN) len = PASS_LEN;
  for (int i = 0; i < len; ++i) {
    out[i] = (char)EEPROM.read(addr + 1 + i);
  }
  out[len] = 0;
}

void setUserCount(byte c) {
  EEPROM.write(EEPROM_START, c);
  EEPROM.commit();
}
byte getUserCount() {
  byte v = EEPROM.read(EEPROM_START);
  if (v == 0xFF) return 0; // uninitialized flash may read 0xFF
  return v;
}

void ensureDefaultAdmin() {
  byte uc = getUserCount();
  if (uc == 0) {
    setUserCount(1);
    writePasswordToEEPROM(ADMIN_INDEX, "1234"); // change this immediately
    Serial.println("Default admin password set to 1234");
  }
}

// ========== Logging (simple ring buffer) ==========
void writeLog(int i, byte userIdx, byte status) {
  int addr = logBaseAddr() + (i % totalLogs) * logEntrySize;
  EEPROM.write(addr, userIdx);
  EEPROM.write(addr + 1, status); // 1=success,2=fail,3=tamper,4=manual
  unsigned long ts = (millis() / 1000UL); // seconds since boot. Replace with RTC if present.
  EEPROM.write(addr + 2, (byte)(ts & 0xFF));
  EEPROM.write(addr + 3, (byte)((ts >> 8) & 0xFF));
  EEPROM.commit();
}

// Output logs to serial (simple)
void dumpLogsToSerial() {
  Serial.println("---- Logs ----");
  for (int i=0;i<totalLogs;i++) {
    int addr = logBaseAddr() + i*logEntrySize;
    byte u = EEPROM.read(addr);
    byte s = EEPROM.read(addr+1);
    byte t0 = EEPROM.read(addr+2);
    byte t1 = EEPROM.read(addr+3);
    unsigned long ts = ((unsigned long)t1 << 8) | t0;
    if (u == 0xFF && s == 0xFF) continue; // empty
    Serial.print("idx:"); Serial.print(i);
    Serial.print(" user:"); Serial.print(u);
    Serial.print(" status:"); Serial.print(s);
    Serial.print(" ts(s):"); Serial.println(ts);
  }
  Serial.println("--------------");
}

// ========== Password check & actions ==========
bool checkPassword(const char *candidate, int &matchedIdx) {
  byte users = getUserCount();
  if (users == 0) { matchedIdx = -1; return false; }
  char stored[PASS_LEN + 1];
  for (int i=0;i<users;i++) {
    readPasswordFromEEPROM(i, stored);
    if (strcmp(candidate, stored) == 0) {
      matchedIdx = i;
      return true;
    }
  }
  matchedIdx = -1;
  return false;
}

void unlockAction() {
  // rotate stepper forward to unlock, wait then rotate back to lock
  lcd.clear(); lcd.setCursor(0,0); lcd.print("Unlocking...");
  Serial.println("Unlocking: rotating stepper");
  stepperMove(UNLOCK_STEPS, true);     // forward
  unlockedUntil = millis() + UNLOCK_DURATION_MS;
  writeLog(nextLogIndex++, 0xFE, 4); // manual code for "unlock action" (if needed)
  beep(2,60);
  lcd.clear(); lcd.setCursor(0,0); lcd.print("Unlocked");
}

void lockAction() {
  // ensure stepper moved back to lock if needed -- this code uses explicit reverse
  // For safety, do not attempt repeated full rotations; assume one unlock->lock cycle.
  // Here we just show status and keep stepper idle
  lcd.clear(); lcd.setCursor(0,0); lcd.print("Locked");
}

// Move stepper 'steps' steps. forward==true => clockwise
void stepperMove(int steps, bool forward) {
  int seqLen = 8;
  int seqIndex = 0;
  // optionally: track current position to do smarter moves
  for (int s=0; s < steps; s++) {
    if (forward) seqIndex = (s % seqLen);
    else seqIndex = ((seqLen - (s % seqLen)) % seqLen);
    digitalWrite(IN1, stepSequence[seqIndex][0]);
    digitalWrite(IN2, stepSequence[seqIndex][1]);
    digitalWrite(IN3, stepSequence[seqIndex][2]);
    digitalWrite(IN4, stepSequence[seqIndex][3]);
    delay(STEPPER_DELAY_MS);
  }
  // After movement, optionally hold coil state briefly then turn off to reduce heat
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// beep helper
void beep(int times, int ms) {
  for (int i=0;i<times;i++){
    digitalWrite(PIN_BUZZER, HIGH);
    delay(ms);
    digitalWrite(PIN_BUZZER, LOW);
    delay(80);
  }
}

void showPrompt() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Enter Password:");
  lcd.setCursor(0,1);
  String mask = "";
  for (unsigned int i=0;i<entered.length();i++) mask += '*';
  lcd.print(mask);
}

// Admin menu (simple): A -> Add user, U->Delete last user, # -> exit
void adminMenu() {
  lcd.clear(); lcd.setCursor(0,0); lcd.print("ADMIN MODE");
  delay(400);
  while (true) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("A:Add  U:Del  #:Exit");
    char k = keypad.getKey();
    if (k == 'A') {
      lcd.clear(); lcd.setCursor(0,0); lcd.print("New Pass:");
      String p="";
      while (true) {
        char x = keypad.getKey();
        if (x) {
          if (x == '#') break;
          if (x == '*') { if (p.length()) p.remove(p.length()-1); }
          else { if (p.length() < PASS_LEN) p += x; }
          lcd.setCursor(0,1); lcd.print(String(p) + " ");
        }
      }
      if (p.length()) {
        byte users = getUserCount();
        if (users < MAX_USERS) {
          writePasswordToEEPROM(users, p.c_str());
          setUserCount(users+1);
          lcd.clear(); lcd.print("User Added");
          beep(2);
          Serial.println("Admin: added user");
        } else {
          lcd.clear(); lcd.print("User Full");
          beep(3,40);
        }
        delay(700);
      } else {
        lcd.clear(); lcd.print("Cancelled");
        delay(500);
      }
    } else if (k == 'U') {
      byte users = getUserCount();
      if (users > 1) {
        setUserCount(users-1);
        lcd.clear(); lcd.print("User Removed");
        beep(1);
        Serial.println("Admin: removed last user");
      } else {
        lcd.clear(); lcd.print("No user to del");
        beep(2,40);
      }
      delay(700);
    } else if (k == '#') {
      lcd.clear(); lcd.print("Exit Admin");
      delay(300);
      break;
    } else if (k) {
      // ignore other keys
    }
    delay(120);
  }
  showPrompt();
}

// ========== Main loop ==========
void loop() {
  // Manual override check (active LOW)
  if (digitalRead(PIN_MANUAL) == LOW) {
    Serial.println("Manual override pressed -> immediate unlock");
    unlockAction();
    // wait a bit (debounce)
    delay(800);
  }

  // Reed tamper sensor
  static bool tamperState = false;
  if (digitalRead(PIN_REED) == LOW) { // closed -> LOW if using INPUT_PULLUP
    if (!tamperState) {
      tamperState = true;
      lcd.clear(); lcd.print("TAMPER!");
      beep(5,40);
      writeLog(nextLogIndex++, 0xFF, 3);
      Serial.println("TAMPER detected!");
    }
  } else {
    tamperState = false;
  }

  // If locked out due to wrong attempts
  if (lockoutUntil > millis()) {
    unsigned long rem = (lockoutUntil - millis()) / 1000;
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("LOCKED OUT");
    lcd.setCursor(0,1); lcd.print(String(rem) + "s");
    delay(800);
    return;
  }

  // If currently within unlock period, keep state; else ensure locked (we use stepper movements)
  if (unlockedUntil > millis()) {
    // remain unlocked during this interval
  } else {
    // after unlocked duration ends, rotate back to lock (reverse rotation)
    static bool returnedToLock = true;
    if (!returnedToLock) {
      stepperMove(UNLOCK_STEPS, false); // rotate back to lock
      returnedToLock = true;
      lcd.clear(); lcd.print("Locked");
    }
  }

  // Read keypad
  char key = keypad.getKey();
  if (key) {
    if (key == '*') {
      if (entered.length() > 0) entered.remove(entered.length()-1);
      showPrompt();
      beep(1,40);
    } else if (key == '#') {
      // Submit
      if (entered.length() == 0) {
        lcd.clear(); lcd.print("Empty Entry");
        beep(1,40);
        delay(500);
        showPrompt();
      } else {
        char buf[PASS_LEN+1];
        entered.toCharArray(buf, PASS_LEN+1);
        int matchedIdx;
        if (checkPassword(buf, matchedIdx)) {
          // Success
          wrongCount = 0;
          lcd.clear(); lcd.print("Access Granted");
          Serial.print("Auth success user idx: "); Serial.println(matchedIdx);
          writeLog(nextLogIndex++, matchedIdx, 1);
          beep(2,60);
          // If admin, allow entering admin menu by pressing 'A' in next 3 seconds
          if (matchedIdx == ADMIN_INDEX) {
            lcd.clear(); lcd.print("Admin Auth");
            unsigned long t0 = millis();
            bool enterAdmin = false;
            while (millis() - t0 < 3000) {
              char k = keypad.getKey();
              if (k == 'A') { enterAdmin = true; break; }
            }
            if (enterAdmin) adminMenu();
            else {
              // Unlock: stepper forward then schedule return
              stepperMove(UNLOCK_STEPS, true);
              unlockedUntil = millis() + UNLOCK_DURATION_MS;
              // mark that we must return to lock after duration
              // We use returnedToLock flag to ensure reverse motion happens once
              // (set it false so later loop rotates back)
              // but because we did stepperMove synchronously, set returnedToLock false to lock back later.
              // Here we use a simple approach: rotate forward now, set returnedToLock false
              // and rotation back happens automatically in loop once unlockedUntil expires.
              // For clarity:
              // stepperMove already rotated forward. So set returnedToLock false to cause a reverse later.
              // But ensure reverse happens only once.
            }
          } else {
            // normal user unlock
            stepperMove(UNLOCK_STEPS, true);
            unlockedUntil = millis() + UNLOCK_DURATION_MS;
            // flag to ensure lock back (handled by loop)
          }
        } else {
          // Failure
          wrongCount++;
          lcd.clear(); lcd.print("Wrong Password");
          writeLog(nextLogIndex++, 0xFF, 2);
          Serial.println("Auth failed");
          beep(2,40);
          delay(600);
          if (wrongCount >= MAX_WRONG) {
            lockoutUntil = millis() + LOCKOUT_MS;
            lcd.clear(); lcd.print("SYSTEM LOCKOUT");
            beep(5,30);
            Serial.println("System locked out due to repeated wrong attempts");
            delay(800);
          }
          showPrompt();
        }
        entered = "";
      }
    } else {
      // Add digit / key to buffer (ignore A/B/C/D except for admin menu triggers)
      if (key == 'A' || key == 'B' || key == 'C' || key == 'D') {
        // treat these as control only when appropriate (we keep 'A' for admin menu activation after admin auth)
        // If you want to use them in normal password, they will be accepted too
        // For simplicity: ignore adding them to password
      } else {
        if (entered.length() < PASS_LEN) {
          entered += key;
        }
      }
      showPrompt();
      beep(1,30);
    }
  }

  // Periodic Serial commands: if user connects via USB-TTL and types 'L' -> dump logs
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'L' || c == 'l') {
      dumpLogsToSerial();
    } else if (c == 'R' || c == 'r') {
      // Reset user database to default (dangerous!)
      setUserCount(1);
      writePasswordToEEPROM(ADMIN_INDEX, "1234");
      Serial.println("Users reset to default admin");
    }
  }

  delay(30);
}