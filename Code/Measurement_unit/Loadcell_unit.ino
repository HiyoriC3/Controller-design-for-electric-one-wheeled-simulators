#include <Wire.h>             // I2C pin A4 (SDA) data, A5 (SCL)clock
#include <Adafruit_MCP4725.h> // MCP4725 library
Adafruit_MCP4725 dac;
float vout;
uint32_t vout_bit; // ADC 12-bit = 4095
//
#include "HX711.h"
//
#define radius 0.0588 // radius, r (m)
#define DOUT  6// ST PIN D6
#define CLK   7// SCLK PIN D7
#define DEC_POINT  2// decimal point
#define zero_factor 8815965
#define dt_read 1000
//
float calibration_factor =215165.00; 
float offset=0;
float loadcell_data;
float get_units_kg();
float torque;
HX711 scale(DOUT, CLK);
//
void setup() 
{
  Serial.begin(9600);// begin serial communication
  scale.set_scale(calibration_factor); 
  scale.set_offset(zero_factor);   
  dac.begin(0x60);// I2C Address MCP4725
  Serial.println("Force (N),Torque (Nm),Vout (V)");
}
void loop() 
{ 
  delay(dt_read);
  //String data = String((get_units_kg()+offset)*9.80665, DEC_POINT);
  loadcell_data = (get_units_kg()+offset)*9.80665;
  if (loadcell_data < 0){
   loadcell_data = 0;
  }
  Serial.print(loadcell_data, DEC_POINT);
  Serial.print(",");
  torque = loadcell_data*radius;
  Serial.print(torque, DEC_POINT);
  vout_bit = map(torque*1000,0,70000,0,4095); // convert rad/sec to 12bit
  dac.setVoltage(vout_bit, false); // MCP4725 output
  Serial.print(",");
  vout = map(vout_bit,0,4095,0,5000); // convert 12bit to 0-5V
  Serial.println(vout/1000);
}

float get_units_kg()
{
  return(scale.get_units()*0.453592);
}
