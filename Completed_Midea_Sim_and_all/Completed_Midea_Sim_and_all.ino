/* Full sketch: ESP8266 (pins 1,3) + SIM800L + PIR + mmWave + COOLIX IR + SMS
   - Hardware UART (Serial) on GPIO1/GPIO3 used for SIM800L comms (Serial.begin(9600))
   - Manual console commands: "start" => manual ON, "stop" => manual OFF
   - Accepts SMS commands only from AUTHORIZED_NUMBER
   - Sends raw IR arrays + COOLIX hex codes, repeated IR_REPEAT times
*/

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

// --- Hardware pins ---
#define PIR_PIN   D7
#define RADAR_PIN D6
const uint16_t kIrLed = 15; // D8

// --- SIM / SMS ---
#define SIM_BAUD 9600
const char AUTHORIZED_NUMBER[] = "+233504009883"; // only accept commands from this number
const char ALERT_NUMBER[] = "+233504009883";      // send confirmations to this number

// --- IR raw arrays & hex codes ---
// COOLIX ON (raw)
uint16_t rawOnData[] = {
 4412,4374,556,1592,558,516,558,1594,556,1594,556,516,
 558,516,558,1594,556,516,558,518,558,1592,558,516,558,516,
 558,1594,556,1594,556,516,558,1592,558,1592,558,516,558,1592,
 558,1592,558,1592,558,1592,558,1592,558,1594,556,516,558,1592,
 558,516,558,518,558,516,558,516,560,516,558,516,558,516,558,516,
 558,516,558,518,556,518,558,516,558,518,558,516,558,1592,558,1592,
 558,1592,558,1592,556,1594,556,1594,558,1592,556,1592,558,5186,
 4412,4372,558,1594,556,518,556,1594,556,1594,556,518,556,518,556,1594,
 556,518,556,518,556,1594,556,518,556,518,556,1596,554,1596,554,520,
 552,1596,554,1596,554,520,554,1596,554,1596,554,1596,552,1598,554,1596,
 552,1598,554,520,552,1598,552,522,552,522,552,524,552,524,548,526,548,546,
 528,546,528,548,526,548,526,548,526,548,526,548,526,548,524,550,524,1626,
 522,1628,522,1628,520,1628,522,1628,522,1628,522,1628,522,1630,522
};
const uint16_t rawOnLen = sizeof(rawOnData) / sizeof(rawOnData[0]);
uint64_t onCode = 0x00B2BF00ULL;  // COOLIX B2BF00

// COOLIX OFF (raw)
uint16_t rawOffData[] = {
 4412,4376,554,1596,554,522,552,1594,556,1596,554,522,552,522,552,1596,
 554,520,554,520,556,1592,556,522,554,546,528,1596,554,1596,554,522,552,1596,
 554,522,554,1598,554,1596,554,1596,554,1594,556,520,554,1594,556,1594,554,1596,
 554,522,554,520,554,522,554,520,554,1596,554,520,554,522,554,1596,554,1596,554,1594,
 554,520,554,522,552,520,554,520,554,520,556,520,554,520,554,520,554,1596,554,1596,
 554,1596,554,1596,554,1596,554,5190,4410,4378,552,1596,554,520,554,1596,554,1596,554,520,
 554,520,554,1596,554,520,556,520,554,1596,554,520,554,522,554,1596,554,1596,554,520,554,1596,
 554,522,554,1598,552,1598,552,1598,552,1598,552,522,552,1598,552,1598,550,1600,550,524,550,524,
 552,522,552,524,550,1598,552,524,552,524,550,1600,550,1600,550,1600,550,524,550,526,548,526,548,526,
 548,546,528,546,530,546,526,548,528,1624,524,1626,524,1624,526,1626,524,1626,524
};
const uint16_t rawOffLen = sizeof(rawOffData) / sizeof(rawOffData[0]);
uint64_t offCode = 0x00B27BE0ULL;  // COOLIX B27BE0

// --- Behavior configuration ---
const uint8_t IR_REPEAT = 5;     // number of times to send each IR action
const uint16_t IR_DELAY_MS = 200; // ms delay between sends
bool lastPresence = false;

// SMS parsing state
String lastSmsSender = "";
bool expectSmsBody = false;

// IR sender
IRsend irsend(kIrLed);

// ------ Helper functions ------
void sendIRRepeated(uint64_t coolixHex, uint16_t *rawData, uint16_t rawLen) {
  for (uint8_t i = 0; i < IR_REPEAT; ++i) {
    // send high-level COOLIX code (24 bits)
    irsend.sendCOOLIX((uint32_t)coolixHex, 24);
    delay(100);
    // send raw timings as a fallback
    irsend.sendRaw(rawData, rawLen, 38);
    delay(IR_DELAY_MS);
  }
}

void sendSMSto(const char *number, const char *message) {
  // Uses hardware Serial (pins 1,3) connected to SIM800L
  Serial.print("Sending SMS to ");
  Serial.println(number);

  Serial.print("AT+CMGS=\"");
  Serial.print(number);
  Serial.println("\"");
  delay(300);
  Serial.print(message);
  delay(300);
  Serial.write(26); // CTRL+Z
  delay(3000);
  Serial.println("SMS complete (waited)");
}

// Accept only exact match (normalized) against AUTHORIZED_NUMBER
bool phoneIsAuthorized(const String &phone) {
  String p = phone;
  p.trim();
  p.replace(" ", "");
  if (p == String(AUTHORIZED_NUMBER)) return true;
  return false;
}

// Extract phone between first pair of quotes in +CMT: line
String extractSenderFromCMT(const String &line) {
  int firstQuote = line.indexOf('"');
  if (firstQuote < 0) return "";
  int secondQuote = line.indexOf('"', firstQuote + 1);
  if (secondQuote < 0) return "";
  return line.substring(firstQuote + 1, secondQuote);
}

// process SMS body (body already trimmed, uppercase)
void processSmsBody(const String &body, const String &sender) {
  Serial.print("Processing SMS body from ");
  Serial.print(sender);
  Serial.print(": ");
  Serial.println(body);

  if (!phoneIsAuthorized(sender)) {
    Serial.println("Unauthorized sender. Ignoring.");
    return;
  }

  if (body == "O" || body == "ON") {
    Serial.println("*** SMS command: ON");
    sendIRRepeated(onCode, rawOnData, rawOnLen);
    sendSMSto(sender.c_str(), "Remote ON executed");
    lastPresence = true;
  } else if (body == "F" || body == "OFF") {
    Serial.println("*** SMS command: OFF");
    sendIRRepeated(offCode, rawOffData, rawOffLen);
    sendSMSto(sender.c_str(), "Remote OFF executed");
    lastPresence = false;
  } else {
    Serial.println("SMS command not recognized.");
  }
}

// Setup SIM800L with echo off, text mode, and push new SMS
void setupSIM800L() {
  Serial.begin(SIM_BAUD); // hardware UART (pins 1 & 3)
  delay(8000);            // give module boot time
  Serial.println("ATE0"); // disable echo
  delay(300);
  Serial.println("AT");   // sanity check
  delay(300);
  Serial.println("AT+CMGF=1"); // text mode
  delay(300);
  Serial.println("AT+CNMI=2,2,0,0,0"); // push incoming SMS
  delay(300);
}

// ------ Setup & Loop ------
void setup() {
  pinMode(PIR_PIN, INPUT);
  pinMode(RADAR_PIN, INPUT);
  irsend.begin();

  // Serial used for both SIM800L and debug (pins 1 & 3),
  // so set Serial Monitor to 9600 baud before opening.
  setupSIM800L();

  Serial.println("System ready: IR + SMS control (authorized only).");
  Serial.println("Manual console commands: type 'start' (manual ON) or 'stop' (manual OFF)");
}

void loop() {
  // --- Manual console input (typing into Serial Monitor 'start' or 'stop') ---
  // Because Serial is shared with SIM800L, we only accept strict alphabetic lines >=4 chars to avoid accidental triggers.
  if (Serial.available() && !expectSmsBody) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() >= 4) {
      // ensure only letters (no +, no digits, not modem lines)
      bool lettersOnly = true;
      for (size_t i = 0; i < line.length(); ++i) {
        char ch = line.charAt(i);
        if (!isAlpha(ch)) { lettersOnly = false; break; }
      }
      if (lettersOnly) {
        String cmd = line;
        cmd.toLowerCase();
        if (cmd == "start") {
          Serial.println("Manual console: START (manual ON)");
          sendIRRepeated(onCode, rawOnData, rawOnLen);
          sendSMSto(ALERT_NUMBER, "Manual ON triggered (console)");
          lastPresence = true;
          delay(200);
        } else if (cmd == "stop") {
          Serial.println("Manual console: STOP (manual OFF)");
          sendIRRepeated(offCode, rawOffData, rawOffLen);
          sendSMSto(ALERT_NUMBER, "Manual OFF triggered (console)");
          lastPresence = false;
          delay(200);
        } else {
          Serial.print("Console: unknown command '");
          Serial.print(line);
          Serial.println("'");
        }
      } // end lettersOnly
    } // end length >=4
  }

  // --- Presence detection (auto) ---
  bool pir = digitalRead(PIR_PIN);
  bool radar = digitalRead(RADAR_PIN);
  bool pres = pir || radar;

  if (pres && !lastPresence) {
    Serial.print("Presence detected (");
    if (pir) Serial.print("PIR ");
    if (radar) Serial.print("RADAR ");
    Serial.println("). Waiting 30s to confirm...");
    delay(30000);
    if (digitalRead(PIR_PIN) || digitalRead(RADAR_PIN)) {
      Serial.println("Confirmed presence. Sending IR ON + SMS.");
      sendIRRepeated(onCode, rawOnData, rawOnLen);
      sendSMSto(ALERT_NUMBER, "Auto ON: presence detected");
      lastPresence = true;
    } else {
      Serial.println("Presence aborted.");
    }
  } else if (!pres && lastPresence) {
    Serial.println("Absence detected. Waiting 30s to confirm...");
    delay(30000);
    if (!digitalRead(PIR_PIN) && !digitalRead(RADAR_PIN)) {
      Serial.println("Confirmed absence. Sending IR OFF + SMS.");
      sendIRRepeated(offCode, rawOffData, rawOffLen);
      sendSMSto(ALERT_NUMBER, "Auto OFF: no presence detected");
      lastPresence = false;
    } else {
      Serial.println("Absence aborted.");
    }
  }

  // --- Incoming SMS parsing (CNMI pushes: +CMT: "sender"... ) ---
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
      // nothing
    } else {
      Serial.print("<< ");
      Serial.println(line);
      if (line.startsWith("+CMT:")) {
        lastSmsSender = extractSenderFromCMT(line);
        expectSmsBody = true;
        Serial.print("Expected SMS body from: ");
        Serial.println(lastSmsSender);
      } else if (expectSmsBody) {
        String body = line;
        body.trim();
        body.toUpperCase();
        processSmsBody(body, lastSmsSender);
        expectSmsBody = false;
        lastSmsSender = "";
      } else {
        // --- Handle other modem responses here ---
        if (line == "OK") {
          Serial.println("[MODEM] OK");
        } else if (line == "ERROR") {
          Serial.println("[MODEM] ERROR");
        } else if (line.startsWith("+CMGS:")) {
          Serial.print("[MODEM] Message sent, id: ");
          Serial.println(line.substring(6));
        } else if (line.startsWith("+CME ERROR:")) {
          Serial.print("[MODEM] CME ERROR: ");
          Serial.println(line.substring(11));
        } else if (line.startsWith("+CREG:") || line.startsWith("+CSQ:") || line.startsWith("+CMTI:")) {
          Serial.print("[MODEM INFO] ");
          Serial.println(line);
        } else {
          Serial.print("[MODEM RAW] ");
          Serial.println(line);
        }
      }
    }
  }

  delay(100);
}
