#include <Wire.h>              // Enable I2C for UNO [pin A4(SDA), A5(SCL)]
#include <Adafruit_MCP4725.h>  // MCP4725 library
#include <Adafruit_ADS1X15.h>  // ADS1015 or ADS1115 library
#include <LCDWIKI_GUI.h>       // Core graphics library
#include <LCDWIKI_SPI.h>       // Hardware-specific library

// TFT SPI screen pins
#define MODEL ILI9488_18
#define CS 10
#define CD 9
#define RST 8
#define MOSI 11
#define MISO 12
#define SCK 13
#define LED -1  // connect to 3.3V on Arduino UNO

// color values
#define BLACK 0x0000
#define WHITE 0xFFFF
#define YELLOW 0xFFE0
#define GREEN 0xFF00

#define MAX_N 300        // number of iterations to run
#define DEF_SPEED 30.00  // default reference speed
#define DT_CTRL 1000     // control loop sampling time (millisec)
#define WAIT_TIME 20000  // waiting time for return to main menu (millisec)
#define DEF_KP 0.2148
#define DEF_KI 0.0071

// keyboard pin
#define LEFT 4
#define RIGHT 5
#define UP 2
#define DOWN 3
#define SELECT 6
#define RELAY 7

// menu and keyboard constant variables
#define OPERATION 0
#define STANDALONE 1
#define PC 2
#define DEFAULT 3
#define U_ADJUST 4
#define CUSTOM 5
#define KP_MENU 6
#define KI_MENU 7
#define DISPLAY 8
#define numLR_max 4
#define numLR_min 1
#define numLRCTRL_max 5
#define numLRCTRL_min 3

// functions prototype
void show_string(uint8_t *str, int16_t x, int16_t y, uint8_t csize, uint16_t fc, uint16_t bc, boolean mode);
void layout();
void display_value();
void num_selectfcn();
void speed_menu();
void value_adjust_instruction();
void default_speed_menu();
void ctrl_menu();
void mode_menu();
void mode_select();
void mode_clear();
void dac_adjust();
void readanalog();
void dacvout();

// DAC and ADC variables
int vout_bit;
int16_t wheel_bit;
int16_t drum_bit;
int16_t loadcell_bit;
int16_t current_bit;
float vout_mv;
float dac_ctrl;  // 0-5V
float poweramp;  // 0-24V
float wheel_vol;
float drum_vol;
float loadcell_vol;
float current_vol;
float wheel_rps;
float drum_rps;
float loadcell_Nm;
float current_A;

// menu and keyboard variables initial value
int numLR_current = 1;
int numUD_current = 1;
int menu = OPERATION;
int select_status = 0;
int mode_current = 0;
int D1 = 0;
int D2 = 0;
int D3 = 0;
int D4 = 0;
int D5 = 0;

// control variables
float ierr;
float err;
float kp;
float ki;
float ref_rps;             // rad/sec
unsigned long last_round;  // control loop timer
int n;                     // itterations counter
int time;                  // use to display time on display

// interupt variables
volatile bool trigup = false;    // flag for interrupt increase value
volatile bool trigdown = false;  // flag for interrupt decrease value

// ADC, DAC, and display initialize
Adafruit_ADS1015 adc;
Adafruit_MCP4725 dac;
LCDWIKI_SPI my_lcd(MODEL, CS, CD, MISO, MOSI, RST, SCK, LED);

void setup() {
  pinMode(LEFT, INPUT);
  pinMode(RIGHT, INPUT);
  pinMode(UP, INPUT);
  pinMode(DOWN, INPUT);
  pinMode(SELECT, INPUT);
  pinMode(RELAY, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(UP), dac_adjust, CHANGE);
  attachInterrupt(digitalPinToInterrupt(DOWN), dac_adjust, CHANGE);
  digitalWrite(RELAY, LOW);  // switch to PC mode to prevent DAC initial state output to drive motor
  adc.begin(0x48);           // ADS1015 I2C address
  dac.begin(0x60);           // MCP4725 I2C address
  dac.setVoltage(0, false);  // Set DAC voltage output to 0V
  my_lcd.Init_LCD();
  my_lcd.Set_Rotation(1);
}

void loop() {

  switch (menu) {
    case OPERATION:  // Operation mode (main menu)
      my_lcd.Fill_Screen(BLACK);
      mode_menu();
      mode_current = 0;

      while (select_status == 0) {
        mode_select();
      }

      mode_clear();
      select_status = 0;
      break;

    case STANDALONE:  // Standalone mode
      mode_menu();

      while (select_status == 0) {
        mode_select();
      }

      select_status = 0;
      break;

    case PC:                      // PC mode
      dac.setVoltage(0, false);   // stop DAC voltage output
      digitalWrite(RELAY, HIGH);  // switch relay to DAQ
      layout();

      while (digitalRead(SELECT) == LOW) {
        readanalog();
        display_value();
      }

      menu = OPERATION;
      break;

    case DEFAULT:  // Standalone Default mode
      mode_clear();
      digitalWrite(RELAY, LOW);
      default_speed_menu();
      delay(3000);
      my_lcd.Fill_Screen(BLACK);
      menu = DISPLAY;
      break;

    case U_ADJUST:
      mode_clear();
      digitalWrite(RELAY, LOW);
      dac_ctrl = 1.3;
      n = 0;
      time = 0;
      layout();
      last_round = millis();

      while (n < MAX_N) {
        if (trigup && dac_ctrl < 1.88) {
          dac_ctrl += 0.02;
          trigup = false;
        }

        if (trigdown && dac_ctrl != 1.3) {
          dac_ctrl -= 0.02;
          trigdown = false;
        }

        readanalog();  // read data from all units
        poweramp = (7.1644 * dac_ctrl) - 8.1076;
        display_value();
        time += DT_CTRL / 1000;
        dacvout();

        while ((millis() - last_round) < DT_CTRL) {}  // wait for time = dt_ctrl

        last_round = millis();  // set new time for next loop
        n++;                    // loop counter +1
      }

      dac.setVoltage(0, false);  //stop DAC voltage output
      delay(WAIT_TIME);
      menu = OPERATION;
      break;

    case CUSTOM:  // Standalone Custom mode & Speed input menu
      mode_clear();
      speed_menu();
      value_adjust_instruction();

      while (select_status == 0) {
        num_selectfcn();
      }

      my_lcd.Fill_Rectangle(0, 60, 400, 180);
      ref_rps = (D1 * 10) + (D2) + (float((D3 * 10) + D4) / 100);
      select_status = 0;
      menu = KP_MENU;
      break;

    case KP_MENU:  // kp input menu
      D1 = 0;
      D2 = 0;
      D3 = 2;
      D4 = 1;
      D5 = 5;

      ctrl_menu();

      while (select_status == 0) {
        num_selectfcn();
      }

      my_lcd.Fill_Rectangle(0, 60, 400, 180);
      kp = (float((D3 * 100) + (D4 * 10) + D5) / 1000);
      select_status = 0;
      menu = KI_MENU;
      break;

    case KI_MENU:  // ki input menu
      D1 = 0;
      D2 = 0;
      D3 = 0;
      D4 = 0;
      D5 = 7;

      ctrl_menu();

      while (select_status == 0) {
        num_selectfcn();
      }

      my_lcd.Fill_Screen(BLACK);
      ki = (float((D3 * 100) + (D4 * 10) + D5) / 1000);
      select_status = 0;
      menu = DISPLAY;
      break;

    case DISPLAY:  // Display value from sensor node (loop)
      ierr = 0.0;
      time = 0;
      n = 0;
      digitalWrite(RELAY, LOW);
      layout();
      last_round = millis();

      while (n < MAX_N) {
        readanalog();  //read data from all slaves
        display_value();

        err = ref_rps - wheel_rps;            // error calculation
        ierr += DT_CTRL * err / 1000;         // integrated Error calculation
        poweramp = (kp * err) + (ki * ierr);  // control signal 0-24V

        if (poweramp > 12) {
          poweramp = 12;
        }

        if (poweramp < 0) {
          poweramp = 0;
        }

        dac_ctrl = (0.1313 * poweramp) + 1.065;  // convert control signal to 0-5V

        if (dac_ctrl > 2.6) {
          dac_ctrl = 2.6;
        }

        if (dac_ctrl < 0) {
          dac_ctrl = 0;
        }

        dacvout();

        while ((millis() - last_round) < DT_CTRL) {}  // wait for time = dt_ctrl
        last_round = millis();                        // set new time for next loop
        time += DT_CTRL / 1000;
        n++;  // loop counter +1
      }

      dac.setVoltage(0, false);  //stop DAC voltage output
      delay(WAIT_TIME);
      menu = OPERATION;
      break;
  }
}


void show_string(uint8_t *str, int16_t x, int16_t y, uint8_t csize, uint16_t fc, uint16_t bc, boolean mode) {
  my_lcd.Set_Text_Mode(mode);
  my_lcd.Set_Text_Size(csize);
  my_lcd.Set_Text_colour(fc);
  my_lcd.Set_Text_Back_colour(bc);
  my_lcd.Print_String(str, x, y);
}

void layout() {
  if (menu != PC) {
    show_string("Time", 0, 30, 3, WHITE, BLACK, 0);
    show_string("Secs", 140, 30, 3, WHITE, BLACK, 0);
  }

  switch (menu) {
    case DISPLAY:
      show_string("kp", 235, 30, 3, WHITE, BLACK, 0);
      show_string("ki", 350, 30, 3, WHITE, BLACK, 0);
      my_lcd.Set_Text_Size(2);
      my_lcd.Print_Number_Float(kp, 2, 285, 37, '.', 2, ' ');
      my_lcd.Print_Number_Float(ki, 3, 400, 37, '.', 3, ' ');
      my_lcd.Set_Text_colour(GREEN);
      my_lcd.Print_Number_Float(ref_rps, 2, 225, 87, '.', 2, ' ');
      show_string("Ref/Act SPD", 0, 80, 3, WHITE, BLACK, 0);
      show_string("/", 295, 87, 2, WHITE, BLACK, 0);
      break;

    case U_ADJUST:
      show_string("use UP/DOWN button", 250, 10, 2, YELLOW, BLACK, 0);
      show_string("to adjust Vin", 250, 40, 2, YELLOW, BLACK, 0);
      break;

    case PC:
      my_lcd.Fill_Rectangle(70, 30, 430, 80);
      show_string("Connect to DAQ via I/O port", 80, 10, 2, YELLOW, BLACK, 0);
      show_string("press SELECT button to exit", 80, 40, 2, YELLOW, BLACK, 0);
      break;
  }

  show_string("Act SPD", 0, 80, 3, WHITE, BLACK, 0);
  show_string("rad/s", 385, 80, 3, WHITE, BLACK, 0);
  show_string("Motor Vin", 0, 130, 3, WHITE, BLACK, 0);
  show_string("V", 385, 130, 3, WHITE, BLACK, 0);
  show_string("Drum speed", 0, 180, 3, WHITE, BLACK, 0);
  show_string("rad/s", 385, 180, 3, WHITE, BLACK, 0);
  show_string("Torque", 0, 230, 3, WHITE, BLACK, 0);
  show_string("Nm", 385, 230, 3, WHITE, BLACK, 0);
  show_string("Current", 0, 280, 3, WHITE, BLACK, 0);
  show_string("A", 385, 280, 3, WHITE, BLACK, 0);
}

void display_value() {
  my_lcd.Set_Text_Size(2);

  switch (menu) {
    case PC:
      my_lcd.Print_Number_Float(wheel_rps, 1, 270, 87, '.', 1, ' ');
      break;

    case U_ADJUST:
      my_lcd.Print_Number_Int(time, 90, 37, 1, ' ', 10);
      my_lcd.Print_Number_Float(wheel_rps, 1, 270, 87, '.', 1, ' ');
      break;

    default:
      my_lcd.Print_Number_Int(time, 90, 37, 1, ' ', 10);
      my_lcd.Print_Number_Float(wheel_rps, 1, 320, 87, '.', 1, ' ');
      break;
  }

  my_lcd.Print_Number_Float(poweramp, 2, 270, 137, '.', 1, ' ');
  my_lcd.Print_Number_Float(drum_rps, 1, 270, 187, '.', 1, ' ');
  my_lcd.Print_Number_Float(loadcell_Nm, 1, 270, 237, '.', 1, ' ');
  my_lcd.Print_Number_Float(current_A, 1, 270, 287, '.', 1, ' ');
}

void num_selectfcn() {

  if (menu != CUSTOM && numLR_current <= 2) numLR_current = 3;

  switch (menu) {  // arrow left and right function: act as digit selector
    case CUSTOM:
      if (digitalRead(LEFT) == HIGH && numLR_current != numLR_min) {
        numLR_current--;
      }

      if (digitalRead(RIGHT) == HIGH && numLR_current != numLR_max) {
        numLR_current++;
      }
      break;

    default:
      if (digitalRead(LEFT) == HIGH && numLR_current != numLRCTRL_min) {
        numLR_current--;
      }

      if (digitalRead(RIGHT) == HIGH && numLR_current != numLRCTRL_max) {
        numLR_current++;
      }
      break;
  }

  if (digitalRead(UP) == HIGH || digitalRead(DOWN) == HIGH) {  // arrow up and down function : act as digit value adjust
    switch (numLR_current) {
      case 1:
        if (D1 != 5 && digitalRead(UP) == HIGH) {
          D1++;
        }

        if (D1 != 0 && digitalRead(DOWN) == HIGH) {
          D1--;
        }

        if (D1 == 5) {
          D2 = 0;
          D3 = 0;
          D4 = 0;
          my_lcd.Print_Number_Int(D2, 40, 130, 1, ' ', 10);
          my_lcd.Print_Number_Int(D3, 80, 130, 1, ' ', 10);
          my_lcd.Print_Number_Int(D4, 100, 130, 1, ' ', 10);
        }

        my_lcd.Print_Number_Int(D1, 20, 130, 1, ' ', 10);
        break;

      case 2:
        if (D1 != 5 && D2 != 9 && digitalRead(UP) == HIGH) {
          D2++;
        }

        if (D2 != 0 && digitalRead(DOWN) == HIGH) {
          D2--;
        }

        my_lcd.Print_Number_Int(D2, 40, 130, 1, ' ', 10);
        break;

      case 3:
        switch (menu) {
          default:
            if (D3 != 9 && digitalRead(UP) == HIGH) {
              D3++;
            }
            break;

          case CUSTOM:
            if (D1 != 5 && D3 != 9 && digitalRead(UP) == HIGH) {
              D3++;
            }
            break;

          case KP_MENU:
            if (D3 != 5 && digitalRead(UP) == HIGH) {
              D3++;
            }

            if (D3 == 5) {
              D4 = 0;
              D5 = 0;
              my_lcd.Print_Number_Int(D4, 100, 130, 1, ' ', 10);
              my_lcd.Print_Number_Int(D5, 120, 130, 1, ' ', 10);
            }

            break;
        }

        if (D3 != 0 && digitalRead(DOWN) == HIGH) {
          D3--;
        }

        my_lcd.Print_Number_Int(D3, 80, 130, 1, ' ', 10);
        break;

      case 4:
        switch (menu) {
          default:
            if (D4 != 9 && digitalRead(UP) == HIGH) {
              D4++;
            }
            break;

          case CUSTOM:
            if (D1 != 5 && D4 != 9 && digitalRead(UP) == HIGH) {
              D4++;
            }
            break;

          case KP_MENU:
            if (D3 != 5 && D4 != 9 && digitalRead(UP) == HIGH) {
              D4++;
            }
            break;
        }

        if (D4 != 0 && digitalRead(DOWN) == HIGH) {
          D4--;
        }

        my_lcd.Print_Number_Int(D4, 100, 130, 1, ' ', 10);
        break;

      case 5:
        switch (menu) {
          default:
            if (D5 != 9 && digitalRead(UP) == HIGH) {
              D5++;
            }
            break;

          case CUSTOM:
            if (D1 != 5 && digitalRead(UP) == HIGH) {
              D5++;
            }
            break;

          case KP_MENU:
            if (D3 != 5 && D5 != 9 && digitalRead(UP) == HIGH) {
              D5++;
            }
            break;
        }

        if (D5 != 0 && digitalRead(DOWN) == HIGH) {
          D5--;
        }

        my_lcd.Print_Number_Int(D5, 120, 130, 1, ' ', 10);
        break;
    }
  }

  switch (numLR_current) {  // digit pointer function
    case 1:
      show_string("^", 20, 160, 3, WHITE, BLACK, 0);
      my_lcd.Fill_Rectangle(40, 150, 60, 180);
      break;

    case 2:
      show_string("^", 40, 160, 3, WHITE, BLACK, 0);
      my_lcd.Fill_Rectangle(20, 150, 35, 180);
      my_lcd.Fill_Rectangle(80, 150, 95, 180);
      break;

    case 3:
      show_string("^", 80, 160, 3, WHITE, BLACK, 0);
      my_lcd.Fill_Rectangle(40, 150, 60, 180);
      my_lcd.Fill_Rectangle(100, 150, 120, 180);
      break;

    case 4:
      show_string("^", 100, 160, 3, WHITE, BLACK, 0);
      my_lcd.Fill_Rectangle(80, 150, 95, 180);
      my_lcd.Fill_Rectangle(120, 150, 140, 180);
      break;

    case 5:
      show_string("^", 120, 160, 3, WHITE, BLACK, 0);
      my_lcd.Fill_Rectangle(100, 150, 120, 180);
      break;
  }

  if (digitalRead(SELECT) == HIGH) {
    select_status = 1;
  }
}

void speed_menu() {
  show_string("Please input", 20, 30, 3, WHITE, BLACK, 0);
  show_string("Wheel speed (rad/s)", 20, 80, 3, WHITE, BLACK, 0);
  my_lcd.Print_Number_Int(D1, 20, 130, 1, ' ', 10);
  my_lcd.Print_Number_Int(D2, 40, 130, 1, ' ', 10);
  show_string(".", 60, 130, 3, WHITE, BLACK, 0);
  my_lcd.Print_Number_Int(D3, 80, 130, 1, ' ', 10);
  my_lcd.Print_Number_Int(D4, 100, 130, 1, ' ', 10);
}

void value_adjust_instruction() {
  show_string("use LEFT or RIGHT button to select digid", 0, 200, 2, YELLOW, BLACK, 0);
  show_string("use UP or DOWN button to adjust value", 20, 235, 2, YELLOW, BLACK, 0);
  show_string("use SELECT button to confirm your input", 8, 270, 2, YELLOW, BLACK, 0);
}

void default_speed_menu() {
  ref_rps = DEF_SPEED;
  kp = DEF_KP;
  ki = DEF_KI;

  show_string("Values will be set at", 20, 30, 3, WHITE, BLACK, 0);
  show_string("Wheel speed", 20, 80, 3, WHITE, BLACK, 0);
  my_lcd.Print_Number_Float(DEF_SPEED, 0, 250, 80, '.', 0, ' ');
  show_string("(rad/s)", 340, 80, 3, WHITE, BLACK, 0);
  show_string("kp", 20, 130, 3, WHITE, BLACK, 0);
  my_lcd.Print_Number_Float(DEF_KP, 6, 80, 130, '.', 2, ' ');
  show_string("ki", 250, 130, 3, WHITE, BLACK, 0);
  my_lcd.Print_Number_Float(DEF_KI, 6, 300, 130, '.', 2, ' ');
  show_string("Operation will begin", 20, 180, 3, WHITE, BLACK, 0);
  show_string("in 3 seconds...", 20, 220, 3, WHITE, BLACK, 0);
}

void ctrl_menu() {
  switch (menu) {
    case KP_MENU:
      show_string("kp", 20, 80, 3, WHITE, BLACK, 0);
      break;

    case KI_MENU:
      show_string("ki", 20, 80, 3, WHITE, BLACK, 0);
      break;
  }

  my_lcd.Print_Number_Int(D2, 40, 130, 1, ' ', 10);
  show_string(".", 60, 130, 3, WHITE, BLACK, 0);
  my_lcd.Print_Number_Int(D3, 80, 130, 1, ' ', 10);
  my_lcd.Print_Number_Int(D4, 100, 130, 1, ' ', 10);
  my_lcd.Print_Number_Int(D5, 120, 130, 1, ' ', 10);
}

void mode_menu() {
  my_lcd.Set_Draw_color(BLACK);

  switch (menu) {
    case OPERATION:
      show_string("Operation mode", 70, 30, 4, WHITE, BLACK, 0);
      show_string("Standalone", 150, 100, 3, WHITE, BLACK, 0);
      show_string("PC with DAQ", 145, 200, 3, WHITE, BLACK, 0);
      show_string(">", 120, 100, 3, WHITE, BLACK, 0);
      break;

    case STANDALONE:
      show_string("Standalone Type", 70, 30, 4, WHITE, BLACK, 0);
      show_string("Default", 150, 100, 3, WHITE, BLACK, 0);
      show_string("U ctrl adjust", 150, 150, 3, WHITE, BLACK, 0);
      show_string("Custom", 145, 200, 3, WHITE, BLACK, 0);
      show_string(">", 120, 100, 3, WHITE, BLACK, 0);
      break;
  }
}

void mode_select() {

  switch (menu) {
    case OPERATION:
      if (digitalRead(DOWN) == HIGH && mode_current != 1) {
        show_string(">", 120, 200, 3, WHITE, BLACK, 0);
        my_lcd.Fill_Rectangle(120, 100, 140, 120);
        mode_current++;
      }

      if (digitalRead(UP) == HIGH && mode_current != 0) {
        show_string(">", 120, 100, 3, WHITE, BLACK, 0);
        my_lcd.Fill_Rectangle(120, 200, 140, 220);
        mode_current--;
      }

      break;

    case STANDALONE:
      if (digitalRead(DOWN) == HIGH && mode_current != 2) {
        mode_current++;

        switch (mode_current) {
          case 1:
            show_string(">", 120, 150, 3, WHITE, BLACK, 0);
            my_lcd.Fill_Rectangle(120, 100, 140, 120);
            break;

          case 2:
            show_string(">", 120, 200, 3, WHITE, BLACK, 0);
            my_lcd.Fill_Rectangle(120, 150, 140, 170);
            break;
        }
        delay(300);
      }

      if (digitalRead(UP) == HIGH && mode_current != 0) {
        mode_current--;

        switch (mode_current) {
          case 0:
            show_string(">", 120, 100, 3, WHITE, BLACK, 0);
            my_lcd.Fill_Rectangle(120, 150, 140, 170);
            break;

          case 1:
            show_string(">", 120, 150, 3, WHITE, BLACK, 0);
            my_lcd.Fill_Rectangle(120, 200, 140, 220);
            break;
        }

        delay(300);
      }
      break;
  }

  if (digitalRead(SELECT) == HIGH) {
    switch (menu) {
      case OPERATION:
        if (mode_current == 0) {
          menu = STANDALONE;
        }

        if (mode_current == 1) {
          menu = PC;
        }

        break;

      case STANDALONE:
        switch (mode_current) {
          case 0:
            menu = DEFAULT;
            break;

          case 1:
            menu = U_ADJUST;
            break;

          case 2:
            menu = CUSTOM;
            break;
        }
        break;
    }

    select_status = 1;
  }
}

void mode_clear() {
  if (menu == DEFAULT || menu == CUSTOM || menu == U_ADJUST) {
    my_lcd.Fill_Rectangle(70, 30, 430, 80);
    my_lcd.Fill_Rectangle(120, 150, 400, 170);
  }

  my_lcd.Fill_Rectangle(120, 100, 350, 120);
  my_lcd.Fill_Rectangle(120, 170, 350, 220);
}

void dac_adjust() {
  if (menu == U_ADJUST && digitalRead(UP) == HIGH) {
    trigup = true;
  }

  if (menu == U_ADJUST && digitalRead(DOWN) == HIGH) {
    trigdown = true;
  }
}

void readanalog() {
  wheel_bit = adc.readADC_SingleEnded(0);
  drum_bit = adc.readADC_SingleEnded(1);
  loadcell_bit = adc.readADC_SingleEnded(2);
  current_bit = adc.readADC_SingleEnded(3);

  wheel_vol = adc.computeVolts(wheel_bit);
  drum_vol = adc.computeVolts(drum_bit);
  loadcell_vol = adc.computeVolts(loadcell_bit);
  current_vol = adc.computeVolts(current_bit);

  if (wheel_vol < 0) {
    wheel_vol = 0;
  }

  if (drum_vol < 0) {
    drum_vol = 0;
  }

  if (loadcell_vol < 0) {
    loadcell_vol = 0;
  }

  if (current_vol < 0) {
    current_vol = 0;
  }

  wheel_rps = wheel_vol * 70 / 5;         // convert voltage to rad/sec
  drum_rps = drum_vol * (25 / 5);         // convert voltage to rad/sec
  loadcell_Nm = loadcell_vol * (70 / 5);  // convert voltage to Nm
  current_A = current_vol * (45 / 5);     // convert voltage to A
}

void dacvout() {
  vout_mv = dac_ctrl * 1000;                  // convert volt to millivolt
  vout_bit = map(vout_mv, 0, 5000, 0, 4095);  // convert millivolt to 12bits (0-4095)
  dac.setVoltage(vout_bit, false);            // DAC voltage output
}