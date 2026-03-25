#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

/* =========================
   WiFi Access Point
   ========================= */
const char *ssid = "RC car";
const char *password = "12345678";

WebSocketsServer webSocket(81);

/* =========================
   OLED Display
   ========================= */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

/* =========================
   Motor Pins
   ========================= */
// Driver 1
#define D1_A_IN1 25
#define D1_A_IN2 26
#define D1_B_IN1 27
#define D1_B_IN2 14

// Driver 2
#define D2_A_IN1 18
#define D2_A_IN2 19
#define D2_B_IN1 32
#define D2_B_IN2 33

/* =========================
   Servo Pins
   ========================= */
#define SERVO1_PIN 16
#define SERVO2_PIN 17
#define SERVO3_PIN 23

Servo servo1;
Servo servo2;
Servo servo3;

// Default centre position for all servos (degrees)
int servo1Pos = 90;
int servo2Pos = 90;
int servo3Pos = 90;

/* =========================
   Ramping
   ========================= */
#define RAMP_STEP 5
#define LOOP_DELAY 5

int targetLeft  = 0;
int targetRight = 0;
int currentLeft  = 0;
int currentRight = 0;

/* =========================
   Eye Animation Variables
   ========================= */
float eyeOffset = 0;
float eyeTarget = 0;
unsigned long lastEyeMove = 0;

bool blinking = false;
int blinkLevel = 0;
unsigned long lastBlinkTime = 0;

bool startupDone = false;
unsigned long lastStartupFrame = 0;
int startupBlocks = 0;

/* =========================
   WiFi Animation Variables
   ========================= */
int wifiClients = 0;
int wifiAnimFrame = 0;
unsigned long lastWifiCheck = 0;
unsigned long lastWifiAnim  = 0;
bool wifiPulse = false;
unsigned long wifiPulseStart = 0;

/* =========================
   Motor Control
   ========================= */
void setMotorA(int speed) {
  if (speed > 0) {
    digitalWrite(D1_A_IN1, HIGH); digitalWrite(D1_A_IN2, LOW);
    digitalWrite(D2_A_IN1, HIGH); digitalWrite(D2_A_IN2, LOW);
  } else if (speed < 0) {
    digitalWrite(D1_A_IN1, LOW);  digitalWrite(D1_A_IN2, HIGH);
    digitalWrite(D2_A_IN1, LOW);  digitalWrite(D2_A_IN2, HIGH);
  } else {
    digitalWrite(D1_A_IN1, LOW);  digitalWrite(D1_A_IN2, LOW);
    digitalWrite(D2_A_IN1, LOW);  digitalWrite(D2_A_IN2, LOW);
  }
}

void setMotorB(int speed) {
  if (speed > 0) {
    digitalWrite(D1_B_IN1, HIGH); digitalWrite(D1_B_IN2, LOW);
    digitalWrite(D2_B_IN1, HIGH); digitalWrite(D2_B_IN2, LOW);
  } else if (speed < 0) {
    digitalWrite(D1_B_IN1, LOW);  digitalWrite(D1_B_IN2, HIGH);
    digitalWrite(D2_B_IN1, LOW);  digitalWrite(D2_B_IN2, HIGH);
  } else {
    digitalWrite(D1_B_IN1, LOW);  digitalWrite(D1_B_IN2, LOW);
    digitalWrite(D2_B_IN1, LOW);  digitalWrite(D2_B_IN2, LOW);
  }
}

int ramp(int current, int target) {
  if (current < target) {
    current += RAMP_STEP;
    if (current > target) current = target;
  } else if (current > target) {
    current -= RAMP_STEP;
    if (current < target) current = target;
  }
  return current;
}

/* =========================
   WiFi Icon
   ========================= */
void drawWiFiIcon() {
  if (millis() - lastWifiCheck > 500) {
    lastWifiCheck = millis();
    int newCount = WiFi.softAPgetStationNum();
    if (newCount != wifiClients) {
      wifiPulse = true;
      wifiPulseStart = millis();
    }
    wifiClients = newCount;
  }

  if (millis() - lastWifiAnim > 150) {
    lastWifiAnim = millis();
    wifiAnimFrame++;
    if (wifiAnimFrame > 3) wifiAnimFrame = 0;
  }

  int x = 120, y = 10;

  if (wifiClients == 0) {
    if (wifiAnimFrame % 2 == 0) {
      display.drawLine(x-4, y-4, x+4, y+4, SSD1306_WHITE);
      display.drawLine(x+4, y-4, x-4, y+4, SSD1306_WHITE);
    }
  } else {
    if (wifiAnimFrame >= 1) display.drawCircle(x, y, 2, SSD1306_WHITE);
    if (wifiAnimFrame >= 2) display.drawCircle(x, y, 4, SSD1306_WHITE);
    if (wifiAnimFrame >= 3) display.drawCircle(x, y, 6, SSD1306_WHITE);
  }

  if (wifiPulse) {
    display.drawCircle(x, y, 8, SSD1306_WHITE);
    if (millis() - wifiPulseStart > 400) wifiPulse = false;
  }
}

/* =========================
   Eyes + Blink
   ========================= */
void updateEyes() {
  if (!startupDone) return;

  if (millis() - lastEyeMove > 20) {
    lastEyeMove = millis();
    if (abs(eyeOffset - eyeTarget) < 1) {
      // Clamped to ±10 to prevent eye overflow off screen edges
      eyeTarget = random(-10, 10);
    }
    eyeOffset += (eyeTarget - eyeOffset) * 0.05;
  }

  if (millis() - lastBlinkTime > random(3000, 6000)) {
    blinking = true;
    lastBlinkTime = millis();
  }

  if (blinking) {
    blinkLevel += 3;
    if (blinkLevel > 30) {
      blinkLevel = 0;
      blinking = false;
    }
  }

  display.clearDisplay();

  int eyeWidth  = 30;
  int eyeHeight = 30 - blinkLevel;
  int baseY     = 17 + blinkLevel / 2;
  int leftX     = 34 + (int)eyeOffset;
  int rightX    = 84 + (int)eyeOffset;

  display.fillRoundRect(leftX,  baseY, eyeWidth, eyeHeight, 8, SSD1306_WHITE);
  display.fillRoundRect(rightX, baseY, eyeWidth, eyeHeight, 8, SSD1306_WHITE);

  drawWiFiIcon();
  display.display();
}

/* =========================
   Startup Animation
   (millis-based — no delay)
   ========================= */
void startupAnimation() {
  if (startupDone) return;

  if (millis() - lastStartupFrame < 40) return;
  lastStartupFrame = millis();

  display.clearDisplay();

  for (int i = 0; i < startupBlocks; i++) {
    int x = random(0, SCREEN_WIDTH  - 10);
    int y = random(0, SCREEN_HEIGHT - 10);
    display.fillRect(x, y, 10, 10, SSD1306_WHITE);
  }

  display.display();
  startupBlocks++;

  if (startupBlocks > 40) {
    startupDone = true;
    display.clearDisplay();
    display.display();
  }
}

/* =========================
   WebSocket Control
   ========================= */
void webSocketEvent(uint8_t num, WStype_t type,
                    uint8_t *payload, size_t length) {

  // Zero motors on disconnect (dead-man's switch)
  if (type == WStype_DISCONNECTED) {
    targetLeft  = 0;
    targetRight = 0;
    return;
  }

  if (type != WStype_TEXT) return;

  // Increased doc size to hold servo fields
  StaticJsonDocument<300> doc;
  if (deserializeJson(doc, payload)) return;

  // --- Motors ---
  float x     = doc["x"]     | 0.0f;
  float y     = doc["y"]     | 0.0f;
  int   speed = doc["speed"] | 0;

  targetLeft  = constrain((int)((y + x) * speed), -255, 255);
  targetRight = constrain((int)((y - x) * speed), -255, 255);

  // --- Servos (0-180 degrees, default 90 if key absent) ---
  if (doc.containsKey("s1")) servo1Pos = constrain((int)doc["s1"], 0, 180);
  if (doc.containsKey("s2")) servo2Pos = constrain((int)doc["s2"], 0, 180);
  if (doc.containsKey("s3")) servo3Pos = constrain((int)doc["s3"], 0, 180);

  servo1.write(servo1Pos);
  servo2.write(servo2Pos);
  servo3.write(servo3Pos);
}

/* =========================
   Setup
   ========================= */
void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());   // Proper random seed on ESP32

  // Motor pins
  pinMode(D1_A_IN1, OUTPUT); pinMode(D1_A_IN2, OUTPUT);
  pinMode(D1_B_IN1, OUTPUT); pinMode(D1_B_IN2, OUTPUT);
  pinMode(D2_A_IN1, OUTPUT); pinMode(D2_A_IN2, OUTPUT);
  pinMode(D2_B_IN1, OUTPUT); pinMode(D2_B_IN2, OUTPUT);

  // Servos — ESP32Servo allocates its own PWM timers
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);

  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);
  servo3.setPeriodHertz(50);

  servo1.attach(SERVO1_PIN, 500, 2400);  // min/max pulse width µs for MG90
  servo2.attach(SERVO2_PIN, 500, 2400);
  servo3.attach(SERVO3_PIN, 500, 2400);

  // Centre all servos on boot
  servo1.write(90);
  servo2.write(90);
  servo3.write(90);

  // OLED
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 init failed");
    while (true);
  }

  // WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

/* =========================
   Loop
   ========================= */
void loop() {
  webSocket.loop();

  currentLeft  = ramp(currentLeft,  targetLeft);
  currentRight = ramp(currentRight, targetRight);

  setMotorA(currentLeft);
  setMotorB(currentRight);

  startupAnimation();
  updateEyes();

  delay(LOOP_DELAY);
}
