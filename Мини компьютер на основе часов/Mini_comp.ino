#include <Wire.h>
#include <GyverOLED.h>
#include <MPU6050_light.h>
#include <iarduino_RTC.h>                    // подключаем библиотеку для работы с RTC модулем
#include <EEPROM.h>

// ПИНЫ
#define BUZZER_PIN 9
#define BTN_SET 2
#define BTN_UP 3
#define BTN_DOWN 4
#define PIN_H A0        // Датчик высоты
#define PIN_P A1        // Датчик давления
#define VALVE_UP 5      // Реле накачки
#define VALVE_DOWN 6    // Реле сброса

GyverOLED<SSD1306_128x64, OLED_BUFFER> oled;
MPU6050 mpu(Wire);
iarduino_RTC time(RTC_DS1302, 6, 5, 4); //нужно смотреть свободный пин

struct Config {
  float offX, offY, offZ;
  int maxRoll = 30;
  int maxPitch = 25;
  int maxP = 500;       // Макс давление (условные единицы 0-1023)
  int minH = 200;       // Мин высота для перегруза
} cfg;

enum Mode { CLOCK, DATE, DRIVE, AIR, SET_ROLL, SET_PITCH } currentMode = CLOCK;

unsigned long timer = 0, btnTimer = 0;
float filteredRoll = 0, filteredPitch = 0;
int fH = 0, fP = 0; // Фильтрованные значения пневмы
bool configChanged = false;
// Таблица предвычисленных значений dx для r=28 (индекс = y + 28)
const uint8_t circle_lut[] PROGMEM = {
  0, 7, 10, 13, 15, 17, 19, 20, 21, 23, 24, 25, 25, 26, 26, 27, 
  27, 27, 27, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 
  28, 28, 28, 28, 28, 27, 27, 27, 27, 26, 26, 25, 25, 24, 23, 21, 
  20, 19, 17, 15, 13, 10, 7, 0
};


void setup() {
  pinMode(BTN_SET, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(VALVE_UP, OUTPUT);
  pinMode(VALVE_DOWN, OUTPUT);
  time.begin();
  Wire.begin();
  Wire.setClock(400000);
  oled.init();
  oled.update();


  byte status = mpu.begin();
  loadConfig();
}

void loop() {
  mpu.update();
  handleButtons();

  if (millis() - timer > 15) { 
    timer = millis();
    
    // Фильтрация данных
    filteredRoll = mpu.getAngleX();
    filteredPitch = mpu.getAngleY();
    fH = (analogRead(PIN_H) * 0.1) + (fH * 0.9);
    fP = (analogRead(PIN_P) * 0.1) + (fP * 0.9);

    oled.clear();
    switch (currentMode) {
      case CLOCK: drawClock(); break;
      case DATE: drawDate(); break;
      case DRIVE: drawDrive(); break;
      case AIR:   drawAir(); break;
      case SET_ROLL:
      case SET_PITCH: drawMenu(); break;
    }
    oled.update();
  }
}

// --- ЭКРАНЫ ---

void drawClock() {
  const char* days[] = {"Воскресенье", "Понедельник", "  Вторник  ", "   Среда   ", "  Четверг  ", "  Пятница  ", "  Суббота  "};
  oled.home(); oled.setScale(1);
  
  oled.print(days[time.weekday]); oled.setCursor(100, 0); oled.print(time.gettime(" :s"));
  
  oled.setCursor(4, 2); oled.setScale(4);
  oled.print(time.gettime("H:i")); 
  oled.setScale(1); oled.setCursor(38, 7); 
  oled.print(time.gettime("d-m-Y"));
}

void drawDate() {
  const char* days[] = {"Воскресенье", "Понедельник", "  Вторник  ", "   Среда   ", "  Четверг  ", "  Пятница  ", "  Суббота  "};
  oled.setScale(2);
  oled.setCursor(0, 2);
  oled.print(days[time.weekday]);
  oled.setCursor(0, 4);
  oled.print(time.gettime("d-m-Y   D"));

}

void drawDrive() {
      handleAlarm(filteredRoll, filteredPitch);
      
      oled.setScale(1);
      oled.setCursorXY(0, 3); 
      oled.print(F("КРЕН: ")); oled.print((int)filteredRoll); 
      
      oled.setCursorXY(64, 3); 
      oled.print(F("ТАНГАЖ: ")); oled.print((int)filteredPitch);

      // Рисуем графику (в NO_BUFFER режиме она рисуется медленнее)
      // Чтобы не мерцало, drawHorizon должен быть максимально быстрым
      drawHorizon(32, 40, 28, filteredRoll, cfg.maxRoll);
      drawCarBack(32, 40);
      
      drawHorizon(96, 40, 28, filteredPitch, cfg.maxPitch);
      drawCarSide(96, 40);
      

}

void drawAir() {
  bool overload = (fH < cfg.minH && fP >= cfg.maxP);
  
  oled.setScale(2); oled.setCursor(0, 0); oled.print("PNEUMA");
  oled.setScale(1);
  oled.setCursor(0, 25); oled.print("HEIGHT: "); oled.print(fH);
  oled.setCursor(0, 40); oled.print("PRESS:  "); oled.print(fP);

  if (overload) {
    oled.setCursor(20, 55); oled.invertText(true); oled.print(" OVERLOAD! "); oled.invertText(false);
    if (millis() % 400 < 200) digitalWrite(BUZZER_PIN, HIGH);
  } else digitalWrite(BUZZER_PIN, LOW);

  // Ручное управление в этом режиме
  if (!digitalRead(BTN_UP) && fP < cfg.maxP) digitalWrite(VALVE_UP, HIGH); 
  else digitalWrite(VALVE_UP, LOW);
  
  if (!digitalRead(BTN_DOWN)) digitalWrite(VALVE_DOWN, HIGH);
  else digitalWrite(VALVE_DOWN, LOW);
}

void drawMenu() {
  oled.setScale(1);
  oled.rect(5, 10, 122, 58, OLED_STROKE);
  oled.setCursorXY(35, 18);
  if (currentMode == SET_ROLL) {
    oled.print("МАКС КРЕН");
    oled.setScale(2); oled.setCursorXY(50, 35); oled.print(cfg.maxRoll);
  } else {
    oled.print("МАКС ТАНГАЖ");
    oled.setScale(2); oled.setCursorXY(50, 35); oled.print(cfg.maxPitch);
  }
}

// --- СЕРВИСНЫЕ ФУНКЦИИ ---

void handleButtons() {
  static bool lastBtnSet = true;
  bool btnSet = digitalRead(BTN_SET);

  // 1. Момент НАЖАТИЯ
  if (!btnSet && lastBtnSet) { 
    btnTimer = millis(); 
  }

  // 2. Момент ОТПУСКАНИЯ
  if (btnSet && !lastBtnSet) {
    unsigned long holdTime = millis() - btnTimer;

    // --- СЦЕНАРИЙ А: КАЛИБРОВКА (более 4 секунд) ---
    if (holdTime > 4000 && currentMode == DRIVE) {
      oled.clear();
      oled.setScale(1);
      oled.setCursorXY(20, 30);
      oled.print(F("КАЛИБРОВКА..."));
      oled.update();
      
      delay(1000); // Даем машине успокоиться
      mpu.calcOffsets(); 
      cfg.offX = mpu.getAccXoffset();
      cfg.offY = mpu.getAccYoffset();
      cfg.offZ = mpu.getAccZoffset();
      
      configChanged = true;
      saveConfig(); // Сразу сохраняем результат
    } 

    // --- СЦЕНАРИЙ Б: НАСТРОЙКИ (от 2 до 4 секунд) ---
    else if (holdTime > 2000) {
      if (currentMode == DRIVE) currentMode = SET_ROLL;
      else if (currentMode == SET_ROLL) currentMode = SET_PITCH;
      else { 
        currentMode = DRIVE; 
        saveConfig(); 
      }
    }

    // --- СЦЕНАРИЙ В: ОБЫЧНЫЙ КЛИК (переключение экранов) ---
    else if (holdTime > 50) { // Защита от дребезга 50мс
      if (currentMode == CLOCK) currentMode = DATE;
      else if (currentMode == DATE) currentMode = DRIVE;
      else if (currentMode == DRIVE) currentMode = AIR;
      else if (currentMode == AIR) currentMode = CLOCK;
      else if (currentMode == SET_ROLL) currentMode = SET_PITCH;
      else if (currentMode == SET_PITCH) currentMode = DRIVE;
      // Если мы в режиме настроек, обычный клик ничего не делает (или можно добавить выход)
    }
  }

  lastBtnSet = btnSet;
  if (currentMode == SET_ROLL || currentMode == SET_PITCH) {
    if (!digitalRead(BTN_UP)) { 
      if (currentMode == SET_ROLL && cfg.maxRoll < 60) cfg.maxRoll++; 
      else if (currentMode == SET_PITCH && cfg.maxPitch < 60) cfg.maxPitch++; 
      configChanged = true;
      delay(150); // Небольшая задержка, чтобы цифры не летели слишком быстро
    }
    if (!digitalRead(BTN_DOWN)) { 
      if (currentMode == SET_ROLL && cfg.maxRoll > 5) cfg.maxRoll--; 
      else if (currentMode == SET_PITCH && cfg.maxPitch > 5) cfg.maxPitch--; 
      configChanged = true;
      delay(150); 
    }
  }
}


void drawHorizon(int x0, int y0, int r, float angle, int limit) {
  oled.circle(x0, y0, r, OLED_STROKE);
  float rad = (-angle) * PI / 180.0;
  int xL = x0 - r * cos(rad); int yL = y0 - r * sin(rad);
  int xR = x0 + r * cos(rad); int yR = y0 + r * sin(rad);
  oled.line(xL, yL, xR, yR);
  if (abs(angle) >= limit && (millis() % 400 < 200)) oled.circle(x0, y0, r - 3, OLED_STROKE);
}

void handleAlarm(float r, float p) {
  if (abs(r) >= cfg.maxRoll || abs(p) >= cfg.maxPitch) digitalWrite(BUZZER_PIN, (millis() % 200 < 100));
  else if (currentMode != AIR) digitalWrite(BUZZER_PIN, LOW);
}

// Функции отрисовки машины остаются те же...
void drawCarBack(int x, int y) {
  oled.rect(x - 14, y - 10, x + 14, y + 10, OLED_CLEAR); // Маска
  oled.rect(x - 11, y + 4, x - 7, y + 8, OLED_FILL);   // Колеса
  oled.rect(x + 7, y + 4, x + 11, y + 8, OLED_FILL);
  oled.rect(x - 9, y - 2, x + 9, y + 5, OLED_STROKE); // Кузов
  oled.rect(x - 6, y - 7, x + 6, y - 2, OLED_STROKE); // Кабина
  oled.line(x - 5, y - 9, x + 5, y - 9);              // Багажник
  //oled.circle(x, y - 1, 3, OLED_FILL);      // Запаска на двери (фишка внедорожника)
}

void drawCarSide(int x, int y) {
  // Очищаем прямоугольник под машину
  oled.rect(x - 14, y - 10, x + 14, y + 10, OLED_CLEAR);

  oled.rect(x - 12, y - 2, x + 12, y + 3, OLED_STROKE); // Кузов
  oled.line(x - 6, y - 2, x - 1, y - 7);               // Стекло
  oled.line(x - 1, y - 7, x + 12, y - 7);               // крыша
  oled.line(x + 12, y - 7, x + 12, y - 2);            //пятая дверь
  oled.line(x + 2, y - 2, x + 2, y - 7);                      //срез лобового стекла
  oled.line(x - 6, y - 2, x - 4, y + 3);              //срез переднего крыла
  oled.circle(x - 7, y + 5, 3, OLED_FILL);             // Колесо 1
  oled.circle(x + 7, y + 5, 3, OLED_FILL);             // Колесо 2
  oled.line(x - 0, y - 9, x + 10, y - 9);               // Багажник
}

void saveConfig() {
  if (configChanged) { EEPROM.put(0, cfg); configChanged = false; }
}

void loadConfig() {
  EEPROM.get(0, cfg);
  if (cfg.maxRoll < 5 || cfg.maxRoll > 60) cfg.maxRoll = 30;
  if (cfg.maxPitch < 5 || cfg.maxPitch > 60) cfg.maxPitch = 18;
  mpu.setAccOffsets(cfg.offX, cfg.offY, cfg.offZ);
}
