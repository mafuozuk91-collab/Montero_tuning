#include <Wire.h>
#include <GyverOLED.h>
#include <MPU6050_light.h>
#include <EEPROM.h>

#define BUZZER_PIN 9
#define BTN_SET 2
#define BTN_UP 3
#define BTN_DOWN 4

// ВКЛЮЧАЕМ БУФЕР (занимает 1024 байта ОЗУ)
GyverOLED<SSD1306_128x64, OLED_BUFFER> oled;
MPU6050 mpu(Wire);

struct Config {
  float offX, offY, offZ;
  int maxRoll;
  int maxPitch;
} cfg;

enum Mode { DRIVE, SET_ROLL, SET_PITCH } currentMode = DRIVE;

unsigned long timer = 0, btnTimer = 0;
float alpha = 0.85; 
float filteredRoll = 0, filteredPitch = 0;
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

  Wire.begin();
  Wire.setClock(800000);
  oled.init();
  mpu.begin();
  loadConfig();
}

void loop() {
  mpu.update();
  
  // Упрощенная фильтрация для скорости
  filteredRoll = alpha * (filteredRoll + mpu.getGyroX() * 0.01) + (1.0 - alpha) * mpu.getAngleX();
  filteredPitch = alpha * (filteredPitch + mpu.getGyroY() * 0.01) + (1.0 - alpha) * mpu.getAngleY();

  handleButtons();

  if (millis() - timer > 15) { 
    timer = millis();
     oled.clear();

    if (currentMode == DRIVE) {
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
      
    } else {
      drawMenu();
    }
    oled.update();
  }
}


void drawHorizon(int x0, int y0, int r, float angle, int limit) {
  // Рисуем основной круг (границу)
  oled.circle(x0, y0, r, OLED_STROKE); 
  
  // Переводим в радианы один раз
  float rad = (-angle) * 0.01745; // Быстрый аналог PI / 180
  float c = cos(rad);
  float s = sin(rad);

  // Вычисляем координаты концов линии горизонта
  int dx = r * c;
  int dy = r * s;

  // Рисуем "жирную" линию горизонта (3 линии со смещением в 1 пиксель)
  // Это намного быстрее, чем цикл закрашивания
  oled.line(x0 - dx, y0 - dy, x0 + dx, y0 + dy);
  oled.line(x0 - dx, y0 - dy + 1, x0 + dx, y0 + dy + 1);
  oled.line(x0 - dx, y0 - dy - 1, x0 + dx, y0 + dy - 1);

  // Мигающая индикация превышения (без тяжелых вычислений)
  if (abs((int)angle) >= limit && (millis() & 256)) { 
    oled.circle(x0, y0, r - 3, OLED_STROKE);
  }
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

void handleButtons() {
  static bool lastBtnSet = true;
  static bool isHolding = false;
  bool btnSet = digitalRead(BTN_SET);

  // Логика зажатия (для калибровки)
  if (!btnSet) { // Кнопка нажата
    if (!isHolding) {
      btnTimer = millis();
      isHolding = true;
    }
    
    // Если держим больше 2 сек и мы в режиме DRIVE
    if (isHolding && (millis() - btnTimer > 2000) && currentMode == DRIVE) {
      oled.clear(); 
      oled.setScale(1); 
      oled.setCursorXY(20, 30); 
      oled.print("ОПУСТИТЕ ДЛЯ КАЛИБ..."); // Подсказка
      oled.update();
      
      while(!digitalRead(BTN_SET)); // Ждем отпускания для чистоты замера
      
      oled.clear();
      oled.setCursorXY(20, 30);
      oled.print("КАЛИБРОВКА...");
      oled.update();
      
      delay(500); // Даем руке убраться, чтобы не трясти датчик
      mpu.calcOffsets(); 
      cfg.offX = mpu.getAccXoffset();
      cfg.offY = mpu.getAccYoffset();
      cfg.offZ = mpu.getAccZoffset();

      configChanged = true;
      isHolding = false;
      return; 
    }
  }

  // Логика обычного клика (при отпускании)
  if (btnSet && lastBtnSet == false) {
    unsigned long hold = millis() - btnTimer;
    if (hold < 2000 && hold > 50) {
      if (currentMode == DRIVE) currentMode = SET_ROLL;
      else if (currentMode == SET_ROLL) currentMode = SET_PITCH;
      else { currentMode = DRIVE; configChanged = true; saveConfig(); }
    }
    isHolding = false;
  }
  lastBtnSet = btnSet;

  // Кнопки UP/DOWN
  if (currentMode != DRIVE) {
    if (!digitalRead(BTN_UP)) { 
      if (currentMode == SET_ROLL) cfg.maxRoll++; 
      else cfg.maxPitch++; 
      delay(100); 
    }
    if (!digitalRead(BTN_DOWN)) { 
      if (currentMode == SET_ROLL) cfg.maxRoll--; 
      else cfg.maxPitch--; 
      delay(100); 
    }
  }
}


void saveConfig() {
  if (configChanged) { EEPROM.put(0, cfg); configChanged = false; }
}

void loadConfig() {
  EEPROM.get(0, cfg);
  if (cfg.maxRoll < 5 || cfg.maxRoll > 60) cfg.maxRoll = 30;
  if (cfg.maxPitch < 5 || cfg.maxPitch > 60) cfg.maxPitch = 25;
  mpu.setAccOffsets(cfg.offX, cfg.offY, cfg.offZ);
}

void handleAlarm(float r, float p) {
  if (abs(r) >= cfg.maxRoll || abs(p) >= cfg.maxPitch) digitalWrite(BUZZER_PIN, (millis() % 200 < 100));
  else digitalWrite(BUZZER_PIN, LOW);
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

