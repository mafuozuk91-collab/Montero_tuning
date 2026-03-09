#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27,20,4);

#include <GTMacro.h>   //Библиотеки таймеров
#include <GTimer.h>
#include <GTimerCbT.h>
#include <GTimerT.h>
#include <uTimerT.h>
GTimer<millis> tmr1; // таймер подсветки заднего экрана

#include <PWMrelay.h> //Библиотека ШИМ реле


#include <EncButton.h> // Библиотека енкодеров
EncButton RR(48, 50, 52);
EncButton FR(49, 51, 53);


#include <Buzzer.h> // библиотека пищалок
Buzzer buzzer(22);

#include <EEPROM.h> // библиотека памяти

int cursorRR;
int tmp; 
int mode;
int vent;
int tmpRR;
int ventRR;
int sthtrRR;
int stht;
int ventAC;
int AC;
uint8_t VAR_mode;

void setup() {// вводим переменные и достаем из памяти
EEPROM.get(1, tmp); 
EEPROM.get(5, mode);
EEPROM.get(10, vent);
EEPROM.get(15, tmpRR);
EEPROM.get(20, ventRR);
EEPROM.get(25, VAR_mode);
EEPROM.get(30, AC);
EEPROM.get(35, ventAC);
  tmr1.setMode(GTMode::Timeout);
  tmr1.setTime(5000);
  tmr1.start();
cursorRR = 0;
sthtrRR = 0;

stht = 0;
lcd.clear();
  // выводим трехстрочное меню
  lcd.backlight();
  lcd.setCursor(1,1);
  lcd.print(" TEMP:"); lcd.print(tmpRR); lcd.setCursor(10,1); lcd.print(" STHEAT");
  lcd.setCursor(1,2);
  lcd.print(" VENT:"); lcd.print(ventRR); lcd.setCursor(13,2); lcd.print("OFF");

Serial.begin(9600);} // НУЖНО ЕЩЕ ДОБАВИТЬ ОБМЕН С ПЕРЕДНИМ КЛИМАТОМ!!!!

void loop() {
  FR.tick(); //опрос заднего энкодера
  RR.tick(); // опрос переднего энкодера
  static GTimer<millis> tmr2(500, true); // таймер для показа MIN/MAX
  tmr2.setMode(GTMode::Timeout);  
  static GTimer<millis> tmr3(500, true); // задержка для начала работы серво
  tmr3.setMode(GTMode::Timeout);  


  // задний климат-контроль
if (RR.hold(600)) {VAR_mode==1?2:1; EEPROM.put (25, VAR_mode);} //переключение между управляемыми климатами ПОДОГРЕВ/КОНДИЦИОНЕР
  //ПОДОГРЕВ !добавить отключение кондиционера и моторчика
  if (VAR_mode=1) {
    if (tmr1) {lcd.noBacklight(); cursorRR = 0;// по истечении 5 секунд отключаем подсветку и убираем курсор
    lcd.setCursor(1,1);
    lcd.print(" Temp:"); lcd.print(tmpRR); lcd.setCursor(10,1); lcd.print(" StHeat");
    lcd.setCursor(1,2);
    lcd.print(" Vent:"); lcd.print(ventRR);} 
  if (RR.hasClicks()) {
        cursorRR ++; tmr1.start(); lcd.backlight(); buzzer.sound(NOTE_B6, 120);
       if (cursorRR > 3) cursorRR = 1;  
    }
  // организуем выбор и регулировку в трехстрочном меню
  switch (cursorRR) {
    case 1: 
      lcd.setCursor(1,1); lcd.print(">Temp:"); lcd.print(tmpRR);
        if (RR.left()) { tmr3.start();
          if (tmpRR < 18) 
          {tmr2.start();lcd.setCursor(1,1); lcd.print(">TEMP:MIN");
          buzzer.sound(NOTE_B6, 80); buzzer.sound(0, 80); buzzer.sound(NOTE_E4, 80); tmpRR=18;}
          if (tmr2) {lcd.setCursor(1,1); lcd.print(">Temp:"); lcd.print(tmpRR);}}
           else {tmpRR - 1; buzzer.sound(NOTE_B6, 120); tmr3.start();} 
        if (RR.right()) { tmr3.start();
          if (tmpRR > 32) 
          {tmr2.start();lcd.setCursor(1,1); lcd.print(">TEMP:MAX");
          buzzer.sound(NOTE_B6, 80); buzzer.sound(0, 80); buzzer.sound(NOTE_E4, 80); tmpRR=32; }
          if (tmr2) {lcd.setCursor(1,1); lcd.print(">Temp:"); lcd.print(tmpRR);}}
           else {tmpRR + 1; buzzer.sound(NOTE_B6, 120); tmr3.start();} 
      if (tmr3) {EEPROM.put(15, tmpRR);} // не забыть код управления сервой или реле
      break;
    case 2:
      lcd.setCursor(1,2); lcd.print(">Vent:"); if (ventRR = 0) lcd.print("OFF"); else lcd.print(ventRR);
        if (RR.left()) { tmr3.start();
          if (ventRR < 0) 
          {tmr2.start();lcd.setCursor(1,2); lcd.print(">VENT:OFF");
          buzzer.sound(NOTE_B6, 80); buzzer.sound(0, 80); buzzer.sound(NOTE_E4, 80); ventRR=0;}
          if (tmr2) {lcd.setCursor(1,1); lcd.print(">Vent:"); lcd.print(ventRR);}}
           else {ventRR - 1; buzzer.sound(NOTE_B6, 120); tmr3.start();} 
        if (RR.right()) { tmr3.start();
          if (ventRR > 8) 
          {tmr2.start();lcd.setCursor(1,2); lcd.print(">VENT:MAX");
          buzzer.sound(NOTE_B6, 80); buzzer.sound(0, 80); buzzer.sound(NOTE_E4, 80); ventRR=8;}
          if (tmr2) {lcd.setCursor(1,1); lcd.print(">Vent:"); lcd.print(ventRR);}}
           else {ventRR + 1; buzzer.sound(NOTE_B6, 120); tmr3.start();} 
      if (tmr3) {EEPROM.put(20, ventRR);} // не забыть код управления сервой или реле
      break;
    case 3:
      lcd.setCursor(10,1); lcd.print(">STHEAT"); lcd.setCursor(13,2);
          if (RR.left()) { if (stht < 0) {buzzer.sound(NOTE_B6, 80); buzzer.sound(0, 80); buzzer.sound(NOTE_E4, 80); stht = 0;}}
            else {buzzer.sound(NOTE_B6, 120); tmr3.start(); stht --;}
          if (RR.right()) { if (stht > 2) {buzzer.sound(NOTE_B6, 80); buzzer.sound(0, 80); buzzer.sound(NOTE_E4, 80); stht = 2;}}
            else {buzzer.sound(NOTE_B6, 120); tmr3.start(); stht ++;}
        if (stht=0) lcd.print("OFF");
        if (stht=1) lcd.print("LOW");
        if (stht=2) lcd.print(" HI");
        // НУЖНО РАЗОБРАТЬСЯ С ТАЙМЕРОМ И ПОДЕЛЮЧАЕМЫМИ ПИНАМИ"
       

      break;
   }

  }
  //КОНДИЦИОНЕР !добавить отключение печки, подогрева и моторчика
  if (VAR_mode=2) {
    if (tmr1) {lcd.noBacklight(); cursorRR = 0;// по истечении 5 секунд отключаем подсветку и убираем курсор
    lcd.setCursor(1,1);
    lcd.print(" A/C:"); if (AC=1) {lcd.print("OFF");} else {lcd.print("ON");}
    lcd.setCursor(1,2);
    lcd.print(" Vent:"); lcd.print(ventAC);} 
  if (RR.hasClicks()) {
        cursorRR==1?2:1; tmr1.start(); lcd.backlight(); buzzer.sound(NOTE_B6, 120); 
    } 
  switch (cursorRR) {
    case 1:
    lcd.setCursor(1,1); lcd.print(">A/C:"); if (AC=1) {lcd.print("OFF");} else {lcd.print("ON");}
        if (RR.left()) { tmr3.start();
          if (AC < 1) 
          {tmr2.start(); buzzer.sound(NOTE_B6, 80); buzzer.sound(0, 80); buzzer.sound(NOTE_E4, 80); AC=1; }
          if (tmr2) {lcd.setCursor(1,1); lcd.print(">A/C:OFF");}}
           else {AC - 1; buzzer.sound(NOTE_B6, 120); tmr3.start();} 
        if (RR.right()) { tmr3.start();
          if (AC > 2) 
          {tmr2.start(); buzzer.sound(NOTE_B6, 80); buzzer.sound(0, 80); buzzer.sound(NOTE_E4, 80); AC=2;}
          if (tmr2) {lcd.setCursor(1,1); lcd.print(">A/C:ON");}}
           else {AC + 1; buzzer.sound(NOTE_B6, 120); tmr3.start();} 
      if (tmr3) {EEPROM.put(30, AC);} // не забыть код управления сервой или реле
      break;
    case 2:
      lcd.setCursor(1,2); lcd.print(">Vent:"); if (ventAC = 0) lcd.print("OFF"); else lcd.print(ventRR);
        if (RR.left()) { tmr3.start();
          if (ventAC < 0) 
          {tmr2.start();lcd.setCursor(1,2); lcd.print(">VENT:OFF");
          buzzer.sound(NOTE_B6, 80); buzzer.sound(0, 80); buzzer.sound(NOTE_E4, 80); ventAC=0;}
          if (tmr2) {lcd.setCursor(1,1); lcd.print(">Vent:"); lcd.print(ventAC);}}
           else {ventAC - 1; buzzer.sound(NOTE_B6, 120); tmr3.start();} 
        if (RR.right()) { tmr3.start();
          if (ventAC > 8) 
          {tmr2.start();lcd.setCursor(1,2); lcd.print(">VENT:MAX");
          buzzer.sound(NOTE_B6, 80); buzzer.sound(0, 80); buzzer.sound(NOTE_E4, 80); ventAC=8;}
          if (tmr2) {lcd.setCursor(1,1); lcd.print(">Vent:"); lcd.print(ventAC);}}
           else {ventAC + 1; buzzer.sound(NOTE_B6, 120); tmr3.start();} 
      if (tmr3) {EEPROM.put(35, ventAC);} // не забыть код управления сервой или реле    
      break;    
  }

  }
}
// void updateMenu() {
  // Сначала стираем все возможные позиции курсора (очистка пробелами)
 // lcd.setCursor(0, 0); lcd.print(" "); // Перед Temp
 //lcd.setCursor(9, 0); lcd.print(" "); // Перед StHeat
 //lcd.setCursor(0, 1); lcd.print(" "); // Перед Vent

  // Ставим стрелку в зависимости от выбора
 // switch (cursorRR) {
 //   case 1: lcd.setCursor(0, 0); lcd.print(">"); break; // Указывает на Temp
 //   case 2: lcd.setCursor(0, 1); lcd.print(">"); break; // Указывает на Vent
 //   case 3: lcd.setCursor(9, 0); lcd.print(">"); break; // Указывает на StHeat
  //}

  // Обновляем только ЗНАЧЕНИЯ (статику не трогаем)
//  lcd.setCursor(6, 0); lcd.print(tmpRR); lcd.print(" "); 
//  lcd.setCursor(6, 1); lcd.print(ventRR); lcd.print(" ");
  
//  lcd.setCursor(13, 1); // Место под статус подогрева
//  if (stht == 0) lcd.print("OFF");
//  else if (stht == 1) lcd.print("LOW");
//  else lcd.print(" HI");
//}
    

  

