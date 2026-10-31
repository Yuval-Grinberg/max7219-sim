// Demo for the MAX7219 simulator — the same file runs unchanged on a real ESP32.
// Wiring on the ESP32: DIN -> GPIO23 (MOSI), CLK -> GPIO18 (SCK), CS -> GPIO5.
//
// The line below is only read by the simulator (it's a comment to the Arduino IDE):
// sim: --modules 4x1 --button 0=space
//
// In the simulator: press SPACE (= the BOOT button on GPIO0) to switch effects,
// and type a line of text in the terminal to change the scrolling message.

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Max72xxPanel.h>

const int pinCS = 5;
const int numberOfHorizontalDisplays = 4;
const int numberOfVerticalDisplays = 1;
const int pinButton = 0;  // BOOT button

Max72xxPanel matrix = Max72xxPanel(pinCS, numberOfHorizontalDisplays, numberOfVerticalDisplays);

String message = "Hello from the ESP32!";
int effect = 0;
const int effects = 3;

void setup() {
  Serial.begin(115200);
  pinMode(pinButton, INPUT_PULLUP);
  matrix.setIntensity(6);
  Serial.println("Demo started. Type a message and press Enter.");
}

void loop() {
  readSerialMessage();
  if (buttonPressed()) {
    effect = (effect + 1) % effects;
    Serial.printf("Effect %d\n", effect);
  }
  switch (effect) {
    case 0: scrollText(); break;
    case 1: bouncingBall(); break;
    case 2: sineWave(); break;
  }
}

// --- functions below loop(): fine in the Arduino IDE, and in the simulator too ---

bool buttonPressed() {
  static bool last = HIGH;
  bool now = digitalRead(pinButton);
  bool pressed = (last == HIGH && now == LOW);
  last = now;
  return pressed;
}

void readSerialMessage() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length()) {
      message = line;
      Serial.println("New message: " + message);
    }
  }
}

void scrollText() {
  static int offset = 0;
  const int charWidth = 6;
  int textWidth = message.length() * charWidth;
  matrix.fillScreen(LOW);
  matrix.setCursor(matrix.width() - offset, 0);
  matrix.print(message);
  matrix.write();
  offset = (offset + 1) % (textWidth + matrix.width());
  delay(40);
}

void bouncingBall() {
  static float x = 3, y = 2, dx = 0.7, dy = 0.45;
  x += dx;
  y += dy;
  if (x < 1 || x > matrix.width() - 2) dx = -dx;
  if (y < 1 || y > matrix.height() - 2) dy = -dy;
  matrix.fillScreen(LOW);
  matrix.drawRect(0, 0, matrix.width(), matrix.height(), HIGH);
  matrix.fillCircle((int)x, (int)y, 1, HIGH);
  matrix.write();
  delay(30);
}

void sineWave() {
  static float phase = 0;
  matrix.fillScreen(LOW);
  for (int x = 0; x < matrix.width(); x++) {
    int y = 3.5 + 3.5 * sin(phase + x * 0.35);
    matrix.drawPixel(x, y, HIGH);
  }
  matrix.write();
  phase += 0.2;
  delay(30);
}
