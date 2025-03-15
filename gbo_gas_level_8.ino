#include <GyverINA.h>             // Подключаемые библиотеки для корректной работы модуля
#include <GyverOLED.h>            // Все эти библиотеки должны быть установлены в программе ArduinoIDE в разделе "Управление библиотеками"
#include <GyverButton.h>
#include <EEManager.h>
#include <GyverDS18.h>
#include <Arduino.h>
#include <GyverDS3231.h>

//______________________________________________ПИНЫ________________________________________________________________________________
GButton up(3, HIGH_PULL);         // Кнопки UP, DOWN, OK
GButton down(4, HIGH_PULL);       // Цифра в скобках означает номер ЦИФРОВОГО пина на плате ардуино (D2, D3, D4)
GButton ok(2, HIGH_PULL);

uint8_t sensorPin = 3;            // АНАЛОГОВЫЙ пин для расчета напряжения датчика (цифра соответствует № АНАЛОГОВОГО пина. Например: 3 - это пин А3)
uint8_t batteryPin = 0;           // АНАЛОГОВЫЙ пин для расчета напряжения АКБ (цифра соответствует № АНАЛОГОВОГО пина. Например: 0 - это пин А0)
GyverDS18Single ds(5);            // ЦИФРОВОЙ пин датчика температуры (в данном случае D5)
//__________________________________________________________________________________________________________________________________

#define ITEMS 29                  // Общее кол-во пунктов меню на дисплее
#define END_LINE_X 63             // Конечные координаты стрелки указателя уровня газа
#define END_LINE_Y 46             // -//-
#define START_MENU_PARAM 9        // С какого пикселя по оси X писать названия параметров в меню
#define START_MENU_VALUE 68       // С какого пикселя по оси X писать значения параметров в меню
//__________________________________________________________________________________________________________________________________

GyverDS3231 rtc;                  // Модуль часов реального времени
INA226 ina;                       // Модуль, который точно замеряет напряжение (INA226)

// _________________________________ВЫБИРАЕМ ДИСПЛЕЙ (раскомментировать нужный вид дисплея)_________________________________________

GyverOLED<SSD1306_128x64, OLED_BUFFER> oled;       // oled дисплей SSD1306 (бывают и 0,96", и 1,3")
//GyverOLED<SSH1106_128x64> oled;                  // oled дисплей SH1106 - только программный буфер (обычно 1,3")

//__________________________________________________________________________________________________________________________________
//                                                                              $$
//                $$  $$     $$$$      $$$$     $$$$$$    $$$$$      $$$$     $$  $$    $$  $$    $$  $$
//                $$  $$    $$  $$    $$  $$      $$      $$  $$    $$  $$    $$  $$    $$ $$     $$  $$
//                $$$$$$    $$$$$$    $$          $$      $$$$$$    $$  $$    $$ $$$    $$$$      $$ $$$
//                $$  $$    $$  $$    $$  $$      $$      $$        $$  $$    $$$ $$    $$ $$     $$$ $$
//                $$  $$    $$  $$     $$$$       $$      $$         $$$$     $$  $$    $$  $$    $$  $$
//
//
//                   !!! ВО ВСЕХ ДРОБНЫХ ЧИСЛОВЫХ ЗНАЧЕНИЯХ ИСПОЛЬЗУЙТЕ ТОЧКУ(.)  А НЕ ЗАПЯТУЮ(,) !!!
//
#define R1 47000.0                // Сопротивление резистора 1  // Рассчитывайте резисторы с запасом, даже чтобы при большем (чем обычно) напряжении акб
#define R2 23300.0                // Сопротивление резистора 2  // на аналоговый пин не пошло напряжение более 5 вольт!!!
                                                                // К примеру я выбрал макс. напряжение акб = 15 вольт.
                                                                // Рассчитать можно здесь: https://cxem.net/calc/divider_calc.php
#define DISPLAYS 4                // Количество всех видов дисплеев (разновидностей отображения информации на дисплее)
byte myDisplays[] = {1, 2, 3, 4}; // Ваши избранные дисплеи. Прописать через запятую номера тех дисплеев, которые хотите видеть на устройстве. Остальные будут игнорироваться. Максимальное кол-во указано в предыдущей строке.
const char myName[] = "ДРУЖИЩЕ";  // Ваше имя, которое будет отображаться во время заставки при включении (писать в "кавычках"), но
                                  // !!! НЕ БОЛЕЕ 10-и СИМВОЛОВ, ИНАЧЕ НЕ ПОМЕСТИТСЯ НА ДИСПЛЕЕ !!! Если имя не помещается - увидите вместо него восклицательный(!) знак
bool lang = 0;                    // Язык вашего имени на заставке. 1 - латиница. 0 - кириллица. !!! ВАЖНО для правильного центрирования на дисплее !!!

//_______________________________ОСНОВНЫЕ НАСТРОЙКИ, КОТОРЫЕ ДУБЛИРУЮТСЯ В МЕНЮ УСТРОЙСТВА__________________________________________
struct Data {
  float a = 0.870;            // Минимальное напряжение на датчике - пустой бак (0.000 - 12.000 с шагом 0.001)
  float b = 4.730;            // Максимальное напряжение на датчике - полный бак (0.000 - 12.000 с шагом 0.001)
  float c = 34.0;             // Объем газового баллона в литрах (0 - 150 с шагом 0.5)
  float d = 10.5;             // Средний расход газа на 100км (для расчета запаса хода) (0 - 25 с шагом 0.1)
  uint16_t e = 5000;          // Частота опроса датчика напряжения в миллисекундах (5000 = один опрос в 5 сек) (0 - 65535)
  byte f = 10;                // Яркость дисплея (0 - мин, 255 - макс). При значении 0 дисплей выключен!
  bool g = 1;                 // Заставка при включении (1 - показывать, 0 - не показывать)
  bool h = 1;                 // Тип датчика газа. 1 - трехконтактный (измерение напряжения). 0 - двухконтактный (измерение сопротивления)
  byte i = 1;                 // Вид отображения информации на дисплее. По умолчанию - 1. Меняется с самого устройства с помощью кнопки DOWN.
                              // Если в избранных нет дисплея с номером 1, тогда сразу поставить здесь порядковый номер первого вашего избранного дисплея (см. переменную myDisplays[])
  bool j = 1;                 // Флаг отображения оставшегося километража (1 - вкл, 0- выкл)
  bool k = 1;                 // Флаг отображения напряжения акб (1 - вкл, 0- выкл)
  bool l = 1;                 // Флаг отображения температуры (1 - вкл, 0- выкл)
  bool m = 1;                 // Флаг отображения времени (1 - вкл, 0- выкл)
  bool n = 1;                 // Флаг отображения даты (1 - вкл, 0- выкл)
  byte o = 2;                 // Часовой пояс (не совсем разобрался с этой опцией, по-моему не особо нужна для корректного отображения времени)
  bool p = 1;                 // Флаг отображения графического уровня газа (1 - вкл, 0- выкл)
  uint8_t q = 16;             // Размер (ширина) графического уровня газа (1 - 29 с шагом 1)
  bool r = 1;                 // Флаг отображения верхней горизонтальной линии (1 - вкл, 0- выкл)
  bool s = 1;                 // Флаг отображения нижней горизонтальной линии (1 - вкл, 0- выкл)
  
  uint16_t t = 2025;          // Установка текущего года
  uint8_t u = 1;              // Установка текущего месяца
  uint8_t v = 1;              // Установка текущего дня меяца
  uint8_t w = 1;              // Установка текущего времени (часы)
  uint8_t x = 1;              // Установка текущего времени (минуты)
  bool y = 0;                 // Записать новое время в память устройства (0 - не записывать, 1 - записать в память)
  float vref = 4.96;          // точное напряжение на пине 5V
  bool rect = 1;              // Тип заливки прямоугольников на одном из дисплеев устройства (1 - заливка, 0 - обводка)
  bool rectOnOff = 1;         // Только для дисплея с белыми прямоугольниками! (1 - оставлять прямоугольники при отключении параметра, 0 - убирать прямоугольники вместе с параметрами)

  bool z = 0;                 // СБРОС ДО ЗАВОДСКИХ НАСТРОЕК. НИ В КОЕМ СЛУЧАЕ НЕ МЕНЯТЬ ЗДЕСЬ!!! ТОЛЬКО ЧЕРЕЗ МЕНЮ УСТРОЙСТВА!!!
};

//__________________________________________________________________________________________________________________________________

Data data;                    // переменная структуры меню, с которой мы работаем в программе
EEManager memory(data);       // передаём нашу переменную (фактически её адрес)

//________________________________________________________________________________________________________
                              // Переменные для хранения точки отсчета таймера
uint32_t timing = 0;          // Основной дисплей          
uint32_t timing2 = 0;         // Основная задержка опроса датчика уровня газа (для основного массива значений)
uint32_t timing3 = 0;         // Мигание иконки газовой колонки
uint32_t timing4 = 0;         // Задержка для массива вольтажа АКБ
uint32_t timing5 = 0;         // Второстепенная задержка опроса датчика уровня газа (для предварительного массива значений)

bool flag = true;             // Флаг выбора в меню
bool flagMenu = true;         // Флаг меню/инфо
bool flagIcon = false;        // Флаг иконки газовой колонки

//____________________________МАССИВ ДАННЫХ ЗНАЧЕНИЙ ВОЛЬТАЖА ДАТЧИКА УРОВНЯ ГАЗА И ВОЛЬТАЖА АКБ__________
const uint8_t numReadings = 20;             // задаем размер основного массива датчика уровня газа
const uint8_t numReadBatt = 20;             // задаем размер массива вольтажа АКБ
uint16_t readings[numReadings];             // основной массив для хранения значений, полученных с INA226
uint16_t readBatt[numReadBatt];             // массив для хранения значений вольтажа АКБ
uint8_t readIndex = 0;                      // индекс последнего значения датчика уровня газа в основном массиве
uint8_t readIndexBatt = 0;                  // индекс последнего значения вольтажа АКБ
uint32_t total = 0;                         // сумма значений вольтажа с датчика уровня газа в основном массиве
uint16_t totalBatt = 0;                     // сумма значений вольтажа АКБ
const uint8_t firstNumReadings = 30;        // задаем размер предварительного массива датчика уровня газа
uint16_t firstReadings[firstNumReadings];   // предварительный массив для хранения значений полученных с INA226
uint8_t firstReadIndex = 0;                 // индекс последнего значения датчика уровня газа для предварительного массива
uint32_t firstTotal = 0;                    // сумма значений вольтажа с датчика уровня газа для предварительного массива
uint16_t average = 0;                       // среднее предварительное значение вольтажа с датчика уровня газа
//________________________________________________________________________________________________________
float startLineX = 0;         // Координаты стрелки (стрелочный указатель уровня на дисплее)
float startLineY = 0;         // -//-
int8_t levelLenght = 0;       // Процент заполненности бака газом
uint8_t startName = 0;        // Центровка на дисплее (заставка)
float Vout = 0;               // Переменная для хранения значения напряжения датчика уровня газа в средней точке делителя
float tempSensor;             // Переменная для хранения температуры
float battVoltage = 0;        // Переменная для хранения вольтажа АКБ
uint16_t battVoltDelay = 50;  // частота опроса напряжения АКБ в миллисекундах (50 = один опрос за 0,05 сек) (0 - 65535)
//________________________________________________________________________________________________________
String weekday;               // Имена дней недели на кириллице
byte counter;                 // Счетчик, необходимый для отображения нужного избранного дисплея



void setup() {

  setStampZone(data.o);       // часовой пояс
  Wire.begin();
  rtc.begin();

  if (rtc.isReset()) {        // Если был сброс питания RTC, время некорректное
    rtc.setBuildTime();       // установить время последней компиляции прошивки
  }

  memory.begin(0, 'w');       // Сменить здесь букву 'w' на любую другую, если после первой прошивки Arduino не будут сохраняться изменения в меню устройства
                              // После смены буквы нужно заново прошить Arduino

  up.setDebounce(100);
  down.setDebounce(100);
  ok.setDebounce(100);
  up.setClickTimeout(250);
  down.setClickTimeout(250);

  ds.requestTemp();         // первый запрос на измерение температуры

  oled.init();
  oled.setContrast(data.f);

    if (data.g == true) {
    viewScreensaver();
  }

  // Для повышения помехозащищенности INA226 имеет возможность настроить время выборки напряжения и тока
  // INA226 будет захватывать "кусок" сигнала выбранной продолжительности, что повысит точность на шумном сигнале
  // По умолчанию выборка занимает 1100 мкс, но может быть изменена методом .setSampleTime(канал, время);
  // Варианты времени выборки см. в таблице (файл INA226.h)

  ina.setSampleTime(INA226_VBUS, INA226_CONV_2116US);   // Время выборки напряжения
  ina.setSampleTime(INA226_VSHUNT, INA226_CONV_8244US); // Время выборки тока

  // Так же имеется возможность использовать встроенное усреднение выборок
  // Усреднение применяется и для напряжения и для тока и пропорционально увеличивает время оцифровки
  // Рекомендуется на шумной нагрузке, устанавливается методом .setAveraging(кол-во усреднений) (см. таблицу в INA226.h)

  ina.setAveraging(INA226_AVG_X4); // 4х кратное усреднение

  //________________________________________________________________________________________________________
  for (byte i = 0; i < numReadings; i++) {                    // заполняем основной массив значениями вольтажа с датчика уровня газа первый раз
    if (data.h == 1) readings[i] = ina.getVoltage() * 1000;   // если трехконтактный датчик
    else readings[i] = getVout() * 1000;                      // если двухконтактный датчик
    total = total + readings[i];
  }

  for (byte i = 0; i < sizeof(myDisplays) - 1; i++) {         // узнаем текущий номер избранного дисплея (элемент массива)
    if (data.i == myDisplays[i]) counter = i; break;
  }
  //________________________________________________________________________________________________________
}

void loop () {

  average = firstAverageVoltage();        // постоянно получаем среднее значение вольтажа с датчика уровня газа

  memory.tick();
  ok.tick();
  down.tick();
  up.tick();

  if (flagMenu) {                         // смена флага меняет вид: информация / меню   
    flag = true;
    display(); 
  }         
  else menu();

  if (ok.isHolded()) {                    // удержание кнопки ОК обновляет настройки в памяти
    flagMenu = !flagMenu;
    if(data.z) memory.reset();
    else {
      if (data.y) {
        rtc.setTime(data.t, data.u, data.v, data.w, data.x, 0);   // записать дату / время в память устройства
        data.y = 0;
      }
    memory.update();
    }
  }

  if (flagMenu && down.isClick()) {       // нажатие кнопки DOWN на дисплее с информацией переключает вид дисплея
      if (data.i == myDisplays[sizeof(myDisplays) - 1]) {
        data.i = myDisplays[0];
        counter = 0;
      } else data.i = myDisplays[counter += 1];
  }

  if (flagMenu && up.isClick()) {         // нажатие кнопки UP на дисплее с информацией переключает яркость дисплея (+25)
    if(data.f + 25 > 255) {
      data.f = 0;
      oled.setContrast(data.f);
    } else {
      data.f += 25;
      oled.setContrast(data.f);
    }
  }
}


void display() {

  oled.clear();
  float literInBalloonOled = getLiterInBalloon(averageVoltage(), 3);
  //float literInBalloonOled = 25.5;   // Раскомментировать, чтобы потестировать уровень газа в баке без подключения датчика (только не забудьте закомментировать предыдущую строку)
  //____________________________________________________________________________________________________________________
  oled.setScale(1);

    if (data.k) {                         // напряжение акб
      if (data.i == 3) {
        if (data.rectOnOff) {
          whiteRectangle(88, 51, 127, 61, data.rect);
          if (data.rect) oled.textMode(1);
        }
        oled.setCursorXY(90, 53);
        oled.print(averageBatteryVoltage(), 1);
        oled.setCursorXY(121, 53);
      } else {
        oled.setCursorXY(98, 56);
        oled.print(averageBatteryVoltage(), 1);
        oled.setCursorXY(123, 56);
      }
      oled.print(F("V"));
      oled.textMode(0);
    } else {
      if (data.i == 3) {
        if (data.rectOnOff) whiteRectangle(88, 51, 127, 61, data.rect);
      }
    }
  //____________________________________________________________________________________________________________________
    if (data.l) {
      if (data.i == 3) {
        if (data.rectOnOff) {
          whiteRectangle(88, 34, 127, 44, data.rect);
          if (data.rect) oled.textMode(1);
        }
        oled.setCursorXY(90, 36);         // температура
        oled.print(getTempDS(), 1);
        oled.setCursorXY(121, 36);
      } else {
        oled.setCursorXY(0, 56);
        oled.print(getTempDS(), 1);
          if (getTempDS() >= 100) {
            oled.setCursorXY(31, 56);
          } else {
          oled.setCursorXY(25, 56);
        }
      }
      oled.print(F("C"));
      oled.textMode(0);
    } else {
      if (data.i == 3) {
        if (data.rectOnOff) whiteRectangle(88, 34, 127, 44, data.rect);
      }
    }
  //____________________________________________________________________________________________________________________
    Datime dt = rtc;
    
    switch (dt.weekDay) {                 // дни недели
      case 1: weekday = "ПН"; break;
      case 2: weekday = "ВТ"; break;
      case 3: weekday = "СР"; break;
      case 4: weekday = "ЧТ"; break;
      case 5: weekday = "ПТ"; break;
      case 6: weekday = "СБ"; break;
      case 7: weekday = "ВС"; break;
    }
    
    if (data.n) {
      if (data.i == 3) {                  // дата
        if (data.rectOnOff) {
          whiteRectangle(19, 0, 83, 10, data.rect);
          whiteRectangle(0, 0, 14, 10, data.rect);
          if (data.rect) oled.textMode(1);
        }
        oled.setCursorXY(2, 2);
        oled.print(weekday);
        oled.setCursorXY(22, 2);
      } else {
        oled.setCursorXY(0, 0);
        oled.print(weekday);
        oled.setCursorXY(24, 0);
      }
      oled.print(rtc.dateToString());
      oled.textMode(0);
    } else {
      if (data.i == 3) {
        if (data.rectOnOff) {
          whiteRectangle(19, 0, 83, 10, data.rect);
          whiteRectangle(0, 0, 14, 10, data.rect);
        }
      }
    }


    if (data.m) {
      if (data.i == 3) {                  // время
        if (data.rectOnOff) {
          whiteRectangle(88, 0, 127, 10, data.rect);
          if (data.rect) oled.textMode(1);
        }
        oled.setCursorXY(97, 2);
      } else oled.setCursorXY(99, 0);
      
      oled.print(rtc.timeToString());
      oled.textMode(0);
    } else {
      if (data.i == 3) {
        if (data.rectOnOff) whiteRectangle(88, 0, 127, 10, data.rect);
      }
    }
  //____________________________________________________________________________________________________________________
    if (data.p) {
      switch (data.i) {                   // уровень газа горизонтальный / вертикальный
        case 1: gasLevelRect (2, 0, 13, 126, 49, 35.0, 124, 48, literInBalloonOled, 0); break;
        case 2: gasLevelRect (1, 0, 48, 127, 52, 126.0, 0, 0, literInBalloonOled, 0); break;
        case 3: gasLevelRect (1, 0, 51, 83, 61, 82.0, 0, 0, literInBalloonOled, 0); break;
      }
    }
  //____________________________________________________________________________________________________________________
    oled.setScale(3);                     // значение литража
    switch (data.i) {
      case 1:
        if (literInBalloonOled < 10) oled.setCursorXY(27, 21);
        else oled.setCursorXY(20, 21);
        break;
      case 2:
        if (literInBalloonOled < 10) oled.setCursorXY(38, 19);
        else oled.setCursorXY(30, 19);
        break;
      case 3:
        oled.setScale(3);
        oled.setCursorXY(8, 21); break;
      case 4:
        oled.setScale(1);
        oled.setCursorXY(17, 42); break;
    }   

    oled.print(literInBalloonOled, 1);
  //____________________________________________________________________________________________________________________
  if (literInBalloonOled <= 2) flagIcon = true;
  else if (literInBalloonOled > 2 && literInBalloonOled < 5) {
    if (millis() - timing3 >= 1500) {
      timing3 = millis();                 // иконка газовой колонки (когда газ заканчивается)
      flagIcon = !flagIcon;
    }
  }
  else flagIcon = false;

  if (flagIcon) {
    if (data.i == 3) gasStationIcon(73, 15);
    else gasStationIcon(1, 14);
  }
  //____________________________________________________________________________________________________________________
  int kilometer = literInBalloonOled / data.d * 100;
  oled.setScale(1);

    if (data.j) {                       // значение остатка в километрах
      if (data.i == 3) {
        if (data.rectOnOff) {
          whiteRectangle(88, 17, 127, 27, data.rect);
          if (data.rect) oled.textMode(1);
        }
        oled.setCursorXY(90, 19);
        oled.print(kilometer); 
        oled.setCursorXY(115, 19);
      } else {
        oled.setCursorXY(50, 56);
        oled.print(kilometer); 
        oled.setCursorXY(69, 56);
      }
      oled.print(F("KM"));
      oled.textMode(0);
    } else {
      if (data.i == 3) {
        if (data.rectOnOff) whiteRectangle(88, 17, 127, 27, data.rect);
      }
    }
  //____________________________________________________________________________________________________________________
  if (data.i == 4) {
    levelLenght = (100 / data.c) * literInBalloonOled;
    float arrow = 30.0;
    if (levelLenght > 30 and levelLenght < 70 or levelLenght < 7) arrow = 24.0;
    startLineX = END_LINE_X + sin(radians(90.0 - (1.8 * (100 - levelLenght)))) * arrow;    // Стрелочный указатель уровня газа
    startLineY = END_LINE_Y - cos(radians(90.0 - (1.8 * (100 - levelLenght)))) * arrow;
    oled.line(round(startLineX), round(startLineY), END_LINE_X, END_LINE_Y, OLED_FILL);

    dotsIndicator(63, 46, 2);     // начало стрелки
    dotsIndicator(116, 46, 2);    // 100%
    dotsIndicator(11, 46, 2);     // 0%
    dotsIndicator(63, 15, 2);     // 50%

    dotsIndicator(94, 15, 1);     // 75%
    dotsIndicator(32, 15, 1);     // 25%

    dotsIndicator(100, 19, 0);    // 80%
    dotsIndicator(106, 24, 0);    // 85%
    dotsIndicator(111, 31, 0);    // 90%
    dotsIndicator(114, 38, 0);    // 95%
    dotsIndicator(12, 38, 0);     // 5%
    dotsIndicator(15, 31, 0);     // 10%
    dotsIndicator(20, 24, 0);     // 15%
    dotsIndicator(26, 19, 0);     // 20%

    dotsIndicator(69, 15, 0);     // 55%
    dotsIndicator(75, 15, 0);     // 60%
    dotsIndicator(82, 15, 0);     // 65%
    dotsIndicator(88, 15, 0);     // 70%

    dotsIndicator(38, 15, 0);     // 30%
    dotsIndicator(44, 15, 0);     // 35%
    dotsIndicator(51, 15, 0);     // 40%
    dotsIndicator(57, 15, 0);     // 45%
  }
  //____________________________________________________________________________________________________________________
  if (data.r) {
    if (data.i == 1 or data.i == 2 or data.i == 4) oled.line(0, 10, 128, 10);              // горизонтальные линии
  }

  if (data.s) {
    if (data.i == 1 or data.i == 2 or data.i == 4) oled.line(0, 52, 128, 52);
  }
  //____________________________________________________________________________________________________________________
  
  if (millis() - timing >= 100) {         // обновлять дисплей каждые n миллисекунд
    timing = millis();
    oled.update();
  }
}


//_________________Расчет объема газа в баллоне и округление его значения до 0.5__________________

  float getLiterInBalloon(float a, int b) {
    float literInBalloon;
    float literInBalloonX10 = ((data.c / 100) * ((100 / (data.b - data.a)) * (a - data.a))) * 10;
    int literInBalloonX10NoFloat = literInBalloonX10;

    int t = literInBalloonX10NoFloat % 10;

    if (t < 3) t = 0;
    if (t > 7) t = 10;
    if (t >= 3 && t <= 7) t = 5;
    
    literInBalloonX10NoFloat = literInBalloonX10NoFloat / 10 * 10 + t;
    literInBalloon = literInBalloonX10NoFloat / 10.0;
    if (literInBalloon > data.c) literInBalloon = data.c;
    else if (literInBalloon < 0.5) literInBalloon = 0.0;
    return literInBalloon;
  }


  void viewScreensaver() {      // Заставка

    startName = middleLine(63, strlen(myName));

    oled.setCursor(48, 0);
    oled.setScale(1);
    oled.print(F("Hello!"));
    oled.update();
    delay(200);

    oled.setScale(2);

    if (startName < 0) {
      oled.setCursorXY(62, 13);
      oled.print(F("!"));
    }
    else {
      oled.setCursorXY(startName, 13);
      oled.print(myName);
    }

    oled.update();
    delay(800);

    oled.setCursorXY(14, 38);
    oled.setScale(1);
    oled.print(F("GBO GAS in LITERS"));
    oled.rect(5, 34, 122, 48, OLED_STROKE);
    oled.update();
    delay(1000);

    oled.rect(5, 55, 122, 63, OLED_FILL);
    oled.setCursorXY(23, 56);
    oled.textMode(1); 
    oled.print(F("by Kalistratus"));
    oled.textMode(0); 

    oled.update();
    delay(1500);
  }


  void menu() {                                                 // МЕНЮ

  static int8_t pointer = 0;                                    // Переменная указатель

  if (up.isClick() || up.isHold()) {                            // Если кнопку UP нажали или удерживают...
    if (flag) {                                                 // и если флаг установлен, то...
      pointer = constrain(pointer - 1, 0, ITEMS - 1);           // двигаем указатель в пределах дисплея,
    } else {                                                    // иначе изменяем параметры...
        switch(pointer) {
          case 0: data.a = constrain(data.a + 0.001, 0, 12.000); break;
          case 1: data.b = constrain(data.b + 0.001, 0, 12.000); break;
          case 2: data.c = constrain(data.c + 0.5, 0, 150.0); break;
          case 3: data.d = constrain(data.d + 0.1, 0, 25.0); break;
          case 4: data.e += 50; break;
          case 5: data.f += 1; break;
          case 6: data.g += 1; break;
          case 7: data.h += 1; break;
          case 8:
            if (data.i == myDisplays[sizeof(myDisplays) - 1]) {
              data.i = myDisplays[0];
              counter = 0;
            } else data.i = myDisplays[counter += 1];
            break;
          case 9: data.j += 1; break;
          case 10: data.k += 1; break;
          case 11: data.l += 1; break;
          case 12: data.m += 1; break;
          case 13: data.n += 1; break;
          case 14: data.o += 1; break;
          case 15: data.p += 1; break;
          case 16: data.q = constrain(data.q + 1, 1, 29); break;
          case 17: data.r += 1; break;
          case 18: data.s += 1; break;
          case 19: data.t = constrain(data.t + 1, 2025, 2100); break;
          case 20: data.u = constrain(data.u + 1, 1, 12); break;
          case 21: data.v = constrain(data.v + 1, 1, 31); break;
          case 22: data.w = constrain(data.w + 1, 0, 23); break;
          case 23: data.x = constrain(data.x + 1, 0, 59); break;
          case 24: data.y += 1; break;
          case 25: data.vref = constrain(data.vref + 0.01, 4.00, 6.00); break;
          case 26: data.rectOnOff += 1; break;
          case 27: data.rect += 1; break;
          case 28: data.z += 1; break;
        }
    }
  }

  if (up.isDouble()) {                                          // При двойном клике на кнопку UP некоторые параметры можно увеличивать с бОльшим шагом
    if (!flag) {
        switch(pointer) {
          case 0: data.a = constrain(data.a + 0.1, 0, 12.000); break;
          case 1: data.b = constrain(data.b + 0.1, 0, 12.000); break;
          case 2: data.c = constrain(data.c + 10, 0, 150.0); break;
          case 3: data.d = constrain(data.d + 1, 0, 25.0); break;
          case 4: data.e += 500; break;
          case 5: data.f += 10; break;
        }
    }
  }

  if (up.isTriple()) {                                          // При тройном клике на кнопку UP некоторые параметры можно увеличивать с ещё бОльшим шагом
    if (!flag) {
        switch(pointer) {
          case 0: data.a = constrain(data.a + 1, 0, 12.000); break;
          case 1: data.b = constrain(data.b + 1, 0, 12.000); break;
          case 2: data.c = constrain(data.c + 50, 0, 150.0); break;
          case 3: data.d = constrain(data.d + 5, 0, 25.0); break;
          case 4: data.e += 1000; break;
          case 5: data.f += 50; break;
        }
    }
  }

  if (down.isClick() || down.isHold()) {                        // Кнопкой DOWN уменьшаем значения параметров
    if (flag) {
      pointer = constrain(pointer + 1, 0, ITEMS - 1);
    } else {
        switch(pointer) {
          case 0: data.a = constrain(data.a - 0.001, 0, 12.000); break;
          case 1: data.b = constrain(data.b - 0.001, 0, 12.000); break;
          case 2: data.c = constrain(data.c - 0.5, 0, 150.0); break;
          case 3: data.d = constrain(data.d - 0.1, 0, 25.0); break;
          case 4: data.e -= 50; break;
          case 5: data.f -= 1; break;
          case 6: data.g -= 1; break;
          case 7: data.h -= 1; break;
          case 8:
            if (data.i == myDisplays[0]) {
              data.i = myDisplays[sizeof(myDisplays) - 1];
              counter = sizeof(myDisplays) - 1;
            } else data.i = myDisplays[counter -= 1];
            break;
          case 9: data.j -= 1; break;
          case 10: data.k -= 1; break;
          case 11: data.l -= 1; break;
          case 12: data.m -= 1; break;
          case 13: data.n -= 1; break;
          case 14: data.o -= 1; break;
          case 15: data.p -= 1; break;
          case 16: data.q = constrain(data.q - 1, 1, 29); break;
          case 17: data.r -= 1; break;
          case 18: data.s -= 1; break;
          case 19: data.t = constrain(data.t - 1, 2025, 2100); break;
          case 20: data.u = constrain(data.u - 1, 1, 12); break;
          case 21: data.v = constrain(data.v - 1, 1, 31); break;
          case 22: data.w = constrain(data.w - 1, 0, 23); break;
          case 23: data.x = constrain(data.x - 1, 0, 59); break;
          case 24: data.y -= 1; break;
          case 25: data.vref = constrain(data.vref - 0.01, 4.00, 6.00); break;
          case 26: data.rectOnOff -= 1; break;
          case 27: data.rect -= 1; break;
          case 28: data.z -= 1; break;

        }
    }
  }

  if (down.isDouble()) {                                        // При двойном клике на кнопку DOWN некоторые параметры можно уменьшать с бОльшим шагом
    if (!flag) {
        switch(pointer) {
          case 0: data.a = constrain(data.a - 0.1, 0, 12.000); break;
          case 1: data.b = constrain(data.b - 0.1, 0, 12.000); break;
          case 2: data.c = constrain(data.c - 10, 0, 150.0); break;
          case 3: data.d = constrain(data.d - 1, 0, 25.0); break;
          case 4: data.e -= 500; break;
          case 5: data.f -= 10; break;
        }
    }
  }

  if (down.isTriple()) {                                        // При тройном клике на кнопку DOWN некоторые параметры можно уменьшать с ещё бОльшим шагом
    if (!flag) {
        switch(pointer) {
          case 0: data.a = constrain(data.a - 1, 0, 12.000); break;
          case 1: data.b = constrain(data.b - 1, 0, 12.000); break;
          case 2: data.c = constrain(data.c - 50, 0, 150.0); break;
          case 3: data.d = constrain(data.d - 5, 0, 25.0); break;
          case 4: data.e -= 1000; break;
          case 5: data.f -= 50; break;
        }
    }
  }

  if (flagMenu == false && ok.isClick()) {    // Нажатие на ОК - переключение: параметр/значение параметра
    flag = !flag;                             // Инвертируем флаг
  }

//___________________________________МЕНЮ_______________________________________
  oled.clear();
  oled.setScale(1);
  oled.setCursor(START_MENU_PARAM, 0);

  if (pointer < 8) {                          // Первая страница меню
    oled.print(F("Min V"));
    oled.setCursor(START_MENU_PARAM, 1); 
    oled.print(F("Max V"));
    oled.setCursor(START_MENU_PARAM, 2); 
    oled.print(F("Balloon"));
    oled.setCursor(START_MENU_PARAM, 3); 
    oled.print(F("Consumpt"));
    oled.setCursor(START_MENU_PARAM, 4); 
    oled.print(F("Disp upd"));
    oled.setCursor(START_MENU_PARAM, 5); 
    oled.print(F("Bright"));
    oled.setCursor(START_MENU_PARAM, 6); 
    oled.print(F("ScrnSave"));
    oled.setCursor(START_MENU_PARAM, 7); 
    oled.print(F("Sensor"));
  } else if (pointer < 16) {                  // Вторая страница меню
    oled.print(F("DispType"));
    oled.setCursor(START_MENU_PARAM, 1); 
    oled.print(F("KM"));
    oled.setCursor(START_MENU_PARAM, 2); 
    oled.print(F("BattVolt"));
    oled.setCursor(START_MENU_PARAM, 3); 
    oled.print(F("Temp"));
    oled.setCursor(START_MENU_PARAM, 4); 
    oled.print(F("Time"));
    oled.setCursor(START_MENU_PARAM, 5); 
    oled.print(F("Date"));
    oled.setCursor(START_MENU_PARAM, 6); 
    oled.print(F("TimeZone"));
    oled.setCursor(START_MENU_PARAM, 7);
    oled.print(F("GasLevel"));
  } else if (pointer < 24) {                  // Третья страница меню
    oled.print(F("GasWidth"));
    oled.setCursor(START_MENU_PARAM, 1);
    oled.print(F("Top line"));
    oled.setCursor(START_MENU_PARAM, 2); 
    oled.print(F("Bot line"));
    oled.setCursor(START_MENU_PARAM, 3);
    oled.print(F("Year"));
    oled.setCursor(START_MENU_PARAM, 4); 
    oled.print(F("Month"));
    oled.setCursor(START_MENU_PARAM, 5);
    oled.print(F("Day"));
    oled.setCursor(START_MENU_PARAM, 6); 
    oled.print(F("Hours"));
    oled.setCursor(START_MENU_PARAM, 7); 
    oled.print(F("Minutes"));
  } else {                                   // Четвертая страница меню
    oled.print(F("Set Time"));
    oled.setCursor(START_MENU_PARAM, 1); 
    oled.print(F("5V Pin"));
    oled.setCursor(START_MENU_PARAM, 2); 
    oled.print(F("RectOnOf"));
    oled.setCursor(START_MENU_PARAM, 3);
    oled.print(F("RectType"));
    oled.setCursor(START_MENU_PARAM, 4);
    oled.print(F("Reset"));
  }


  if (pointer < 8) {
    oled.setCursor(START_MENU_VALUE, 0);
    oled.print(data.a, 3);
    oled.setCursor(START_MENU_VALUE, 1);
    oled.print(data.b, 3);
    oled.setCursor(START_MENU_VALUE, 2);
    oled.print(data.c, 1);
    oled.setCursor(START_MENU_VALUE, 3);
    oled.print(data.d, 1);
    oled.setCursor(START_MENU_VALUE, 4);
    oled.print(data.e);
    oled.setCursor(START_MENU_VALUE, 5);
    oled.print(data.f);
    oled.setCursor(START_MENU_VALUE, 6);
    oled.print(data.g);
    oled.setCursor(START_MENU_VALUE, 7);
    oled.print(data.h);
  } else if (pointer < 16) {
    oled.setCursor(START_MENU_VALUE, 0);
    oled.print(data.i);
    oled.setCursor(START_MENU_VALUE, 1);
    oled.print(data.j);
    oled.setCursor(START_MENU_VALUE, 2);
    oled.print(data.k);
    oled.setCursor(START_MENU_VALUE, 3);
    oled.print(data.l);
    oled.setCursor(START_MENU_VALUE, 4);
    oled.print(data.m);
    oled.setCursor(START_MENU_VALUE, 5);
    oled.print(data.n);
    oled.setCursor(START_MENU_VALUE, 6);
    oled.print(data.o);
    oled.setCursor(START_MENU_VALUE, 7);
    oled.print(data.p);
  } else if (pointer < 24) {
    oled.setCursor(START_MENU_VALUE, 0);
    oled.print(data.q);
    oled.setCursor(START_MENU_VALUE, 1);
    oled.print(data.r);
    oled.setCursor(START_MENU_VALUE, 2);
    oled.print(data.s);
    oled.setCursor(START_MENU_VALUE, 3);
    oled.print(data.t);
    oled.setCursor(START_MENU_VALUE, 4);
    oled.print(data.u);
    oled.setCursor(START_MENU_VALUE, 5);
    oled.print(data.v);
    oled.setCursor(START_MENU_VALUE, 6);
    oled.print(data.w);
    oled.setCursor(START_MENU_VALUE, 7);
    oled.print(data.x);
  } else {
    oled.setCursor(START_MENU_VALUE, 0);
    oled.print(data.y);
    oled.setCursor(START_MENU_VALUE, 1);
    oled.print(data.vref);
    oled.setCursor(START_MENU_VALUE, 2);
    oled.print(data.rectOnOff);
    oled.setCursor(START_MENU_VALUE, 3);
    oled.print(data.rect);
    oled.setCursor(START_MENU_VALUE, 4);
    oled.print(data.z);
  }


  oled.rect(83, 55, 116, 63, OLED_FILL);
  oled.setCursor(85, 7);
  oled.textMode(1);
  if (data.h == 1) oled.print(ina.getVoltage(), 3);
  else oled.print(getVout(), 3);
  oled.textMode(0);

  printPointer(pointer);                      // Вывод указателя
  oled.update();                              // Выводим кадр на дисплей
}


void printPointer(uint8_t pointer) {          // Навигация по меню

  switch (pointer) {
    case 8 ... 15: pointer = pointer - 8; break;
    case 16 ... 23: pointer = pointer - 16; break;
    case 24 ... 31: pointer = pointer - 24; break;
  }

  if (flag) {                                 // Если флаг установлен
    oled.setCursor(0, pointer);               // Указываем на параметр
    oled.print(F(">"));
  } else {                                    // Иначе
    oled.setCursor(124, pointer);             // Указываем на значение параметра
    oled.print(F("<"));
  }
}


uint16_t firstAverageVoltage() {                                        // Предварительное усреднение показаний с датчика уровня газа
                                                                        // Это быстрый опрос датчика для более точного поиска среднего значения вольтажа
  if (millis() - timing5 >= 100) {                                      // Считывание нового значения с датчика каждые n миллисекунд
    timing5 = millis();

    firstTotal = firstTotal - firstReadings[firstReadIndex];            // вычитаем последнее значение массива из суммы
    if (data.h == 1) firstReadings[firstReadIndex] = ina.getVoltage() * 1000;   // считываем значение с INA226
    else firstReadings[firstReadIndex] = getVout() * 1000;              //... или получаем напряжение с резистора

    firstTotal = firstTotal + firstReadings[firstReadIndex];            // добавляем значение к сумме
    firstReadIndex += 1;                                                // переставляем индекс на следующую позицию

    if (firstReadIndex >= firstNumReadings) firstReadIndex = 0;         // проверяем если мы выскочили индексом за пределы массива (если да, то индекс на ноль)

    uint16_t middle = firstTotal / firstNumReadings;                    // считаем среднее значение из n измерений
    return middle;
  }
}


float averageVoltage() {                                                // Усреднение (уже второе) предварительных (усредненных) показаний с датчика уровня газа
                                                                        // Это медленный опрос - для плавного изменения значений литража на дисплее
  if (millis() - timing2 >= data.e) {                                   // задержка опроса датчика
    timing2 = millis();

    total = total - readings[readIndex];                                // вычитаем последнее значение массива из суммы
    readings[readIndex] = average;                                      // получаем среднее значение напряжения из предварительного массива значений

    total = total + readings[readIndex];                                // добавляем значение к сумме
    readIndex += 1;                                                     // переставляем индекс на следующую позицию

    if (readIndex >= numReadings) readIndex = 0;                        // проверяем если мы выскочили индексом за пределы массива (если да, то индекс на ноль)

    float middle = total / numReadings / 1000.0;                        // считаем среднее значение из n измерений
    return middle;
  }
}


float averageBatteryVoltage() {                                         // Усреднение показаний вольтажа АКБ

  if (millis() - timing4 >= battVoltDelay) {                            // задержка опроса
    timing4 = millis();

    totalBatt = totalBatt - readBatt[readIndexBatt];                    // вычитаем последнее значение массива из суммы
    readBatt[readIndexBatt] = getBattVoltage() * 100;                   // считываем значение вольтажа

    totalBatt = totalBatt + readBatt[readIndexBatt];                    // добавляем значение к сумме
    readIndexBatt += 1;                                                 // переставляем индекс на следующую позицию

    if (readIndexBatt >= numReadBatt) readIndexBatt = 0;                // проверяем если мы выскочили индексом за пределы массива (если да, то индекс на ноль)

    float middle = totalBatt / numReadBatt / 100.0;                     // считаем среднее значение из n измерений
    return middle;
  }
}


float getVout() {                                                       // Перевод значений с двухконтактного датчика в вольтаж
  Vout = (data.vref / 1024.0) * analogRead(sensorPin);                  // Вычисляем напряжение в средней точке делителя
  return Vout;
}


float getBattVoltage() {                                                          // Получение вольтажа АКБ
  battVoltage = analogRead(batteryPin) * data.vref * ((R1 + R2) / R2) / 1024.0;
  return battVoltage;
}


int middleLine (uint8_t lenght, uint8_t word) {                         // Поиск начальных координат для центровки имени на дисплее
  if (!lang) word /= 2;
  startName = lenght - (((word * 10) + (word - 1) * 2) / 2);
  return startName;
}


void gasStationIcon(byte x, byte y) {                                   // Иконка газовой колонки
  oled.rect(x, y, x + 8, y + 6, OLED_STROKE);
  oled.rect(x, y + 7, x + 8, y + 12, OLED_FILL);
  oled.rect(x - 1, y + 13, x + 9, y + 14, OLED_FILL);
}


float getTempDS() {                                                     // Получение температуры
    if (!ds.isWaiting()) ds.requestTemp();
    if (ds.ready()) {
        if (ds.readTemp()) tempSensor = ds.getTemp();
        else tempSensor = 0;
    }
    return tempSensor;
}


void whiteRectangle(byte a, byte b, byte x, byte y, bool t) {           // Белые прямоугольники для одного из экранов
  if (t == 1) oled.rect(a, b, x, y, OLED_FILL);                         // byte t - тип заливки (1 - заливка, 0 - обводка)
  else oled.rect(a, b, x, y, OLED_STROKE);
}


void dotsIndicator(byte a, byte b, byte c) {                            // Точки для стрелочного индикатора
  oled.circle(a, b, c, OLED_FILL);
}


void gasLevelRect (byte t, byte a, byte b, byte c, byte d, float w, byte x, byte y, float z, byte o) {        // Шаблон графического уровня газа
  levelLenght = (w / 100.0) * ((z / data.c) * 100);                                                           // byte t - тип уровня (1 - горизонтальный, 2 - вертикальный)
  if (t == 1) {                                                                                               // byte a,b,c,d - координаты прямоугольника уровня газа
    oled.rect(a, b, c, d, OLED_STROKE);                                                                       // float w - длина ИЗМЕНЯЕМОЙ части уровня в пикселях (длина от 0% до 100%)
    oled.rect(a + 1, b + 1, levelLenght + o, d - 1, OLED_FILL);                                               // byte x - положение ЛЕВОЙ стороны вертикального уровня по оси X при минимальной ширине
  } else {                                                                                                    // byte y - положение при НУЛЕВОМ УРОВНЕ газа в баке вертикального уровня по оси Y
    oled.rect(x - data.q, b, c, d, OLED_STROKE);                                                              // float z - literInBalloonOled (текущее значение газа в литрах)
    oled.rect(x - data.q + 1, y - levelLenght, c, d, OLED_FILL);                                              // byte o - смещение слева направо в пикселях для горизонтального уровня (если уровень начинается не с координаты X = 0),
  }                                                                                                           // т.е. если уровень начинается с координаты X = 10, то byte o = 11 (10 + 1, т.к. отсчет ведется с нуля);
}