#include <Wire.h>             // I2C pin A4 (SDA) data, A5 (SCL)clock
#include <Adafruit_MCP4725.h> // MCP4725 library
Adafruit_MCP4725 dac;
float vout;
uint32_t vout_bit; // ADC 12-bit = 4095
//
int decimalPrecision = 2;                   // decimal places for all values shown in LED Display & Serial Monitor
int currentAnalogInputPin = A0;             // Which pin to measure Current Value (A0 is reserved for LCD Display Shield Button function)
int calibrationPin = A1;                    // Which pin to calibrate offset middle value
float manualOffset = 0.40;                  // Key in value to manually offset the initial value
float mVperAmpValue = 20.83;                 // If using "Hall-Effect" Current Transformer, key in value using this formula: mVperAmp = maximum voltage range (in milli volt) / current rating of CT
float supplyVoltage = 5000;                 // Analog input pin maximum supply voltage, Arduino Uno or Mega is 5000mV while Arduino Nano or Node MCU is 3300mV
float offsetSampleRead = 0;                 /* to read the value of a sample for offset purpose later */
float currentSampleRead  = 0;               /* to read the value of a sample including currentOffset1 value*/
float currentLastSample  = 0;               /* to count time for each sample. Technically 1 milli second 1 sample is taken */
float currentSampleSum   = 0;               /* accumulation of sample readings */
float currentSampleCount = 0;               /* to count number of sample. */
float currentMean ;                         /* to calculate the average value from all samples, in analog values*/ 
float RMSCurrentMean ;                      /* square roof of currentMean, in analog values */   
float FinalRMSCurrent ;                     /* the final RMS current reading*/

void setup() {
  Serial.begin(9600);// begin serial communication
  dac.begin(0x60);// I2C Address MCP4725
  Serial.println("Current (A),Vout (V)");
}

void loop()                                                                                                   /*codes to run again and again */
{                                      
  if(millis() >= currentLastSample + 0.1)                                                               /* every 0.2 milli second taking 1 reading */
    { 
      currentSampleRead = analogRead(currentAnalogInputPin)-analogRead(calibrationPin);                  /* read the sample value including offset value[ A1-A2]*/
      currentSampleSum = currentSampleSum + sq(currentSampleRead) ;                                      /* accumulate total analog values for each sample readings [ 0 - square(A1-A2)] */
      currentSampleCount = currentSampleCount + 1;                                                       /* to count and move on to the next following count */  
      currentLastSample = millis();                                                                      /* to reset the time again so that next cycle can start again*/ 
      }
        
  if(currentSampleCount == 1000)                                                                        /* after 5000 count or 1000 milli seconds (1 second), do this following codes*/
    { 
      currentMean = currentSampleSum/currentSampleCount;                                                /* average accumulated analog values*/
      RMSCurrentMean = sqrt(currentMean);                                                               /* square root of the average value*/
      FinalRMSCurrent = (((RMSCurrentMean /1023) *supplyVoltage) /mVperAmpValue)- manualOffset;         /* calculate the final RMS current*/
      if(FinalRMSCurrent <= (625/mVperAmpValue/100)) FinalRMSCurrent =0;                                /* if the current detected is less than or up to 1%, set current value to 0A*/
      Serial.print(FinalRMSCurrent,decimalPrecision);
      Serial.print(",");
      vout_bit = map(FinalRMSCurrent*1000,0,45000,0,4095); // convert rad/sec to 12bit
      dac.setVoltage(vout_bit, false); // MCP4725 output
      vout = map(vout_bit,0,4095,0,5000); // convert 12bit to 0-5V
      Serial.println(vout/1000);
      currentSampleSum =0;                                                                              /* to reset accumulate sample values for the next cycle */
      currentSampleCount=0;                                                                             /* to reset number of sample for the next cycle */
      }
}