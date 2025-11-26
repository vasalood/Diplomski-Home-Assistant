#include <Arduino_MKRIoTCarrier.h>
#include <ArduinoMqttClient.h>
#include <WiFiNINA.h>
#include <ArduinoJson.h>
#include <math.h>
#include "visuals.h"
#include "pitches.h"
#include "secrets.h"

enum AlarmState {
  ARMED,
  AUTH_COUNTDOWN,
  INTRUDER,
  NORMAL
};

MKRIoTCarrier carrier;
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);
AlarmState alarmState = ARMED;

// IMU
float Gx, Gy, Gz;
const float GYRO_THRESHOLD = 50.0;

// Auth countdown
const int AUTH_WINDOW_SECONDS = 30;
unsigned long authStartTime = 0;
int lastDisplayedSeconds = -1;
unsigned long lastBeepTime = 0;

// PIN logika (4 cifre, 1–5 = touch dugmići)
const int PIN_LENGTH = 4;
const int CORRECT_PIN[PIN_LENGTH] = {1, 2, 3, 4};

int enteredPin[PIN_LENGTH];
int enteredLength = 0;

// BROJ POGREŠNIH POKUŠAJA
const int MAX_FAILED_ATTEMPTS = 3;
int failedAttempts = 0;

// Boje za LED
uint32_t colorRed   = 0;
uint32_t colorGreen = 0;
uint32_t colorBlue  = 0;
uint32_t colorBlack = 0;

// ------------------------ Melodije ------------------------

// success melodija (kad PIN uspe)
int successMelody[] = {
  NOTE_E6, NOTE_G6, NOTE_C7, NOTE_G6
};
int successDurations[] = {
  8, 8, 8, 8
};
const int SUCCESS_MELODY_LEN = sizeof(successMelody) / sizeof(successMelody[0]);

// error melodija (pogrešan PIN)
int errorMelody[] = {
  NOTE_G5, NOTE_E5
};
int errorDurations[] = {
  4, 4
};
const int ERROR_MELODY_LEN = sizeof(errorMelody) / sizeof(errorMelody[0]);

// helper za puštanje melodija
void playMelody(const int *melody, const int *durations, int length) {
  for (int i = 0; i < length; i++) {
    int noteDuration = 1000 / durations[i];

    if (melody[i] != 0) {
      carrier.Buzzer.sound(melody[i]);
    }
    delay(noteDuration);

    int pauseBetweenNotes = noteDuration * 0.30;
    delay(pauseBetweenNotes);

    carrier.Buzzer.noSound();
  }
}

// kratak beep za odbrojavanje
void playShortCountdownBeep() {
  int noteDuration = 1000 / 16;   // šesnaestinka
  carrier.Buzzer.sound(NOTE_A6);
  delay(noteDuration);
  int pauseBetweenNotes = noteDuration * 0.30;
  delay(pauseBetweenNotes);
  carrier.Buzzer.noSound();
}

// ------------------------ Helperi ------------------------

bool detectMovement() {
  carrier.IMUmodule.readGyroscope(Gx, Gy, Gz);

  if (fabs(Gx) > GYRO_THRESHOLD ||
      fabs(Gy) > GYRO_THRESHOLD ||
      fabs(Gz) > GYRO_THRESHOLD) {

    Serial.print("Gyroscope:\tX: ");
    Serial.print(Gx);
    Serial.print("\tY: ");
    Serial.print(Gy);
    Serial.print("\tZ: ");
    Serial.println(Gz);
    Serial.println("IMU: POMERANJE DETEKTOVANO");
    Serial.println("------------------------");
    return true;
  }

  return false;
}

void drawArmedScreen() {
  carrier.display.fillScreen(ST77XX_BLACK);
  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.setTextSize(3);
  carrier.display.setCursor(60, 90);
  carrier.display.print("ARMED");

  // NEMA više plavih LED – ARMED = mrak
  carrier.leds.fill(colorBlack, 0, 5);
  carrier.leds.show();
}

void drawNormalScreen() {
  carrier.display.fillScreen(ST77XX_BLACK);
  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.setTextSize(3);
  carrier.display.setCursor(50, 90);
  carrier.display.print("NORMAL");

  // 3 puta zablinka zeleno pa se ugasi
  for (int i = 0; i < 3; i++) {
    carrier.leds.fill(colorGreen, 0, 5);
    carrier.leds.show();
    delay(250);
    carrier.leds.fill(colorBlack, 0, 5);
    carrier.leds.show();
    delay(250);
  }
}

void drawIntruderScreen() {
  carrier.display.fillScreen(ST77XX_BLACK);
  carrier.display.setTextColor(ST77XX_RED);
  carrier.display.setTextSize(3);
  carrier.display.setCursor(30, 90);
  carrier.display.print("INTRUDER!");
}

// pozovi kad prelazimo iz ARMED u AUTH_COUNTDOWN
void startAuthCountdown() {
  alarmState = AUTH_COUNTDOWN;
  authStartTime = millis();
  lastDisplayedSeconds = -1;
  lastBeepTime = 0;
  enteredLength = 0;
  failedAttempts = 0;   // RESETujemo broj grešaka na početku novog countdowna

  Serial.println("Prelaz u AUTH_COUNTDOWN");

  // LED stalno crvena tokom countdown-a
  carrier.leds.fill(colorRed, 0, 5);
  carrier.leds.show();

  carrier.display.fillScreen(ST77XX_BLACK);
}

// dodavanje cifre u PIN
void addDigit(int d) {
  // ako je već uneo 4 cifre, ignorišemo dok se ne resetuje
  if (enteredLength >= PIN_LENGTH) return;

  enteredPin[enteredLength] = d;
  enteredLength++;

  Serial.print("Unet broj: ");
  Serial.print(d);
  Serial.print("  (duzina: ");
  Serial.print(enteredLength);
  Serial.println(")");

  // OVDE NAMERNO NE PROVERAVAMO PIN dok ne bude svih 4 cifre

  if (enteredLength == PIN_LENGTH) {
    bool ok = true;
    for (int i = 0; i < PIN_LENGTH; i++) {
      if (enteredPin[i] != CORRECT_PIN[i]) {
        ok = false;
        break;
      }
    }

    if (ok) {
      Serial.println("PIN tacan -> NORMAL stanje");
      alarmState = NORMAL;
      carrier.Buzzer.noSound();
      playMelody(successMelody, successDurations, SUCCESS_MELODY_LEN);
      drawNormalScreen();
    } else {
      Serial.println("PIN pogresan!");
      failedAttempts++;

      // kratka error melodija
      playMelody(errorMelody, errorDurations, ERROR_MELODY_LEN);

      if (failedAttempts >= MAX_FAILED_ATTEMPTS) {
        Serial.println("Previse pogresnih pokusaja -> INTRUDER!");
        alarmState = INTRUDER;
        drawIntruderScreen();
        // dalje sirena ide u updateIntruderAlarm()
      }

      // u svakom slucaju, reset uvecanog PIN-a
      enteredLength = 0;
    }
  }
}

// glavna logika za AUTH_COUNTDOWN
void updateAuthCountdown() {
  unsigned long now = millis();

  // izračunaj preostalo vreme
  int elapsedSeconds = (now - authStartTime) / 1000;
  int remaining = AUTH_WINDOW_SECONDS - elapsedSeconds;

  // Ako je vec preslo u INTRUDER zbog 3 greske, ne radimo vise nista ovde
  if (alarmState == INTRUDER) {
    return;
  }

  if (remaining <= 0 && alarmState == AUTH_COUNTDOWN) {
    // vreme isteklo, nema dobre šifre -> INTRUDER
    alarmState = INTRUDER;
    Serial.println("Isteklo vreme -> INTRUDER");
    drawIntruderScreen();
    return;
  }

  // beep na svake ~1s
  if (now - lastBeepTime >= 1000) {
    lastBeepTime = now;
    playShortCountdownBeep();
  }

  // osveži prikaz samo kad se promeni sekunda
  if (remaining != lastDisplayedSeconds) {
    lastDisplayedSeconds = remaining;

    carrier.display.fillScreen(ST77XX_BLACK);

    // countdown broj
    carrier.display.setTextColor(ST77XX_WHITE);
    carrier.display.setTextSize(4);
    carrier.display.setCursor(100, 60);
    carrier.display.print(remaining);

    // prikaz PIN-a (zvezdice)
    carrier.display.setTextSize(2);
    carrier.display.setCursor(50, 140);
    carrier.display.print("PIN: ");
    for (int i = 0; i < enteredLength; i++) {
      carrier.display.print('*');
    }
  }

  // čitanje touch dugmića za unos PIN-a
  carrier.Buttons.update();

  if (carrier.Buttons.onTouchDown(TOUCH0)) {
    addDigit(1);
    delay(200);
  }
  if (carrier.Buttons.onTouchDown(TOUCH1)) {
    addDigit(2);
    delay(200);
  }
  if (carrier.Buttons.onTouchDown(TOUCH2)) {
    addDigit(3);
    delay(200);
  }
  if (carrier.Buttons.onTouchDown(TOUCH3)) {
    addDigit(4);
    delay(200);
  }
  if (carrier.Buttons.onTouchDown(TOUCH4)) {
    addDigit(5);
    delay(200);
  }
}

// jednostavan "panic" alarm za INTRUDER stanje
void updateIntruderAlarm() {
  static bool ledsOn = false;
  static unsigned long lastToggle = 0;
  unsigned long now = millis();

  if (now - lastToggle >= 300) {
    lastToggle = now;
    ledsOn = !ledsOn;

    if (ledsOn) {
      carrier.leds.fill(colorRed, 0, 5);
      carrier.leds.show();
      carrier.Buzzer.sound(NOTE_C7);
    } else {
      carrier.leds.fill(colorBlack, 0, 5);
      carrier.leds.show();
      carrier.Buzzer.noSound();
    }
  }
}

// ------------------------ setup & loop ------------------------

void setup() {
  Serial.begin(9600);
  delay(1000);

  if (!carrier.begin()) {
    Serial.println("Carrier not connected, check connections");
    while (1);
  }

  colorRed   = carrier.leds.Color(255, 0, 0);
  colorGreen = carrier.leds.Color(0, 255, 0);
  colorBlue  = carrier.leds.Color(0, 0, 255);
  colorBlack = carrier.leds.Color(0, 0, 0);

  CARRIER_CASE = false;    // ako ga ubaciš u kutiju, stavi true

  carrier.display.setRotation(0);
  delay(1000);

  // WiFi
  Serial.print("Povezivanje na WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi povezan!");
  Serial.print("IP adresa: ");
  Serial.println(WiFi.localIP());

  // MQTT
  Serial.print("Povezivanje na MQTT broker: ");
  Serial.print(MQTT_BROKER);
  Serial.print(":");
  Serial.println(MQTT_PORT);

  if (!mqttClient.connect(MQTT_BROKER, MQTT_PORT)) {
    Serial.print("MQTT konekcija neuspesna! Code: ");
    Serial.println(mqttClient.connectError());
    while (1);
  }
  Serial.println("MQTT povezan!");
  mqttClient.setId(SENSOR_NAME);

  // start u ARMED stanju
  drawArmedScreen();
}

void loop() {
  switch (alarmState) {
    case ARMED:
      // samo gledamo IMU – ako ima pomeranja, kreće countdown
      if (detectMovement()) {
        startAuthCountdown();
      }
      delay(100); // mali delay da ne spamujemo
      break;

    case AUTH_COUNTDOWN:
      updateAuthCountdown();
      break;

    case NORMAL:
      // za sada ništa posebno, samo stoji u NORMAL
      delay(200);
      break;

    case INTRUDER:
      updateIntruderAlarm();
      break;
  }
}