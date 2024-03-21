#include <Wire.h>             // enable I2C
#include <Adafruit_MCP4725.h> // MCP4725 library
Adafruit_MCP4725 dac;
//
#define ENCODER_PIN 3 // interrupt pin
#define RESOLUTION 128 // encoder wheel stroke times 2
#define dt_read 1000 // encoder read every msec
//
unsigned long gulCount = 0;
unsigned long gulStart_Timer = 0;
unsigned short gusChange = 0;
unsigned long gulStart_Read_Timer = 0;
float gsRPM = 0.0;
float gsRPS = 0.0;
float rpm2radps = 0.1047197551; // convert RPM to rad/sec
//
float vout;
uint32_t vout_bit; // ADC 12-bit = 4095
//
char title[50];

void setup() {
  Serial.begin(9600);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN), COUNT, CHANGE);
  dac.begin(0x60); // MCP4725 I2C address
  sprintf(title, "Wheel Speed (RPM),Wheel Speed (rad/s),Vout (V)");
  Serial.println(title);
}

void loop(){
if ((millis() - gulStart_Read_Timer) >= dt_read) // read data from encoder every dt_read msec
  {
    gsRPM = usRead_RPM();
    Serial.print(gsRPM); // RPM
    Serial.print(",");
    gsRPS = (gsRPM)*rpm2radps;
    Serial.print(gsRPS); // rad/s
    vout_bit = map(gsRPS*1000,0,70000,0,4095); // convert rad/sec to 12bit
    dac.setVoltage(vout_bit, false); // MCP4725 output
    Serial.print(",");
    vout = map(data_out,0,4095,0,5000); // convert 12bit to 0-5V
    Serial.print(vout/1000);
    Serial.println("");
    gulStart_Read_Timer = millis();
  }
}

float usRead_RPM()
{
  float ulRPM = 0;
  unsigned long ulTimeDif = 0;

  detachInterrupt(digitalPinToInterrupt(ENCODER_PIN));

  ulTimeDif = millis() - gulStart_Timer;
  ulRPM = 60000 * gulCount;
  ulRPM = ulRPM / ulTimeDif;
  ulRPM = ulRPM / RESOLUTION;

  gulCount = 0;
  gulStart_Timer = millis();
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN), COUNT, CHANGE);

  return ulRPM;
}

void COUNT(void)
{
  gulCount++;
  gusChange = 1;
}