#include <Button.h>
#include <DS3232RTC.h>    //http://github.com/JChristensen/DS3232RTC
#include <TimeLib.h>         //http://www.arduino.cc/playground/Code/Time  
#include <Wire.h>         //http://arduino.cc/en/Reference/Wire (included with Arduino IDE)
#include <Encoder.h>
#include "LedControl.h"   //https://github.com/wayoda/LedControl

#define RotaryAPIN    2
#define RotaryBPIN    3
Button encButton(4);



boolean debug = false;            // extra debug info?




LedControl lc=LedControl(12,11,10,1);
int sensorPin = A0; // select the input pin for ldr
int sensorValue = 0; // variable to store the value coming from the sensor

Encoder myEnc(RotaryAPIN, RotaryBPIN);
long oldPosition  = -999;
int menuResult = 0;
boolean alarmHourSet = false;
boolean timeHourSet = false;

boolean alarmOn = false;
long alaHours;
long alaMins;
long timeHours;
long timeMins;


long snoozeMillis = 0;          // timekeeper for snoozing checking
long snoozeTime = 10000;        // time between snoozes

time_t prevDisplay = 0;
float timeLapse = 0;
float R;
int interval;
int brightness;
int earlier = 1800; // seconds to start the alarm earlier (NB, when testing, set the alarm at LEAST this much in the future)
const int pwmIntervals = earlier/10; //STEPS
int steps = (earlier/pwmIntervals)*1000;  // milliseconds between steps



//Setup the output PWM pin
const int outputPin = 5;

void setup() {
  Serial.begin(9600);
  
  pinMode(outputPin, OUTPUT);
  pinMode(RotaryAPIN, INPUT_PULLUP);
  pinMode(RotaryBPIN, INPUT_PULLUP);  
  encButton.begin();
  
  // Calculate the R variable (only needs to be done once at setup)
  R = (pwmIntervals * log10(2))/(log10(255));

  setSyncProvider(RTC.get);   // the function to get the time from the RTC
  setSyncInterval(60);
  if(timeStatus() != timeSet) 
      Serial.println("Unable to sync with the RTC");
  else
      Serial.println("RTC has set the system time"); 

  lc.setScanLimit(0, 4); //only using 4 digits, remember to set the MAX7219 Resistor to the correct value! (ISET)
  lc.shutdown(0,false);
  /* Set the brightness to a medium values */
  lc.setIntensity(0,8);
  /* and clear the display */
  lc.clearDisplay(0);

}

void loop() {

  tmElements_t tm;
  time_t t;
  
  sensorValue = constrain(analogRead(sensorPin), 500, 1000);
  int LDR = map(sensorValue, 500 , 1000, 1 , 15);
  if (debug) {
    Serial.print("SensorValue: ");
    Serial.println(sensorValue);     
    Serial.print("LDR: ");
    Serial.println(LDR); 
  }
  lc.setIntensity(0,LDR);
  
  static enum { TIME, MENU, SETTIME, SETALARM, SHOWALARM, ALARM, SNOOZE } state = TIME;
  switch (state) {
      
    case TIME: {

        if ( RTC.alarm(ALARM_1)&&alarmOn){
          if(debug) Serial.println("ALARM DETECTED");
          state = ALARM;
        }
        

        if (encButton.released()){
          myEnc.write(0);
          state = MENU;
        }

        if (now() != prevDisplay) { //update the display only if the time has changed
            prevDisplay = now();
            DisplayDigits(hour(),minute());
            Serial.println();
        }
        break;
      }


    case MENU: {
       
        menuResult = readEncoder(3);

       switch (menuResult) {
          case 0:
            lc.setChar(0,0,'a',false);
            lc.setChar(0,1,'l',false);
            lc.setChar(0,2,'a',false);
            lc.setRow(0,3,B00000101);
            break;
          case 1:    
            lc.setRow(0,0,B00000000);
            lc.setRow(0,1,B00000101);
            lc.setRow(0,2,B01001111);
            lc.setRow(0,3,B00001111);
            break;
          case 2:    
            lc.setRow(0,0,B00001111);
            lc.setRow(0,1,B01011011);
            lc.setRow(0,2,B01001111);
            lc.setRow(0,3,B00001111);
            break;
          }
              



        if (encButton.released()){
          
          switch (menuResult) {
            case 0:
              if (debug)  Serial.println("SET ALARM");
              alarmHourSet = false;
              alaHours = 0;
              alaMins = 0;
              state = SETALARM;
              break;
            case 1:    
              if (debug)  Serial.println("TIME & ALARM OFF");
              alarmOn = false;
              state = TIME;
              break;
            case 2:    
              if (debug) Serial.println("SET TIME");
              timeHourSet = false;
              timeHours = 0;
              timeMins = 0;
              state = SETTIME;
              break;
            }
        }  
      break;
      }
      
    case SETALARM: {
         long tempEnc = 0;

        if (!alarmHourSet){
          tempEnc = readEncoder(24);

           //leftmost digits - hours
           lc.setDigit(0,0,(tempEnc / 10),false);
           lc.setDigit(0,1,(tempEnc % 10),false);
           lc.setRow(0,2,B00000000);
           lc.setRow(0,3,B00000000);

          
          if (encButton.released()){
            alaHours = tempEnc;
            myEnc.write(0);
            alarmHourSet = true;
            myEnc.write(0);
          } 
        }
        
        if (alarmHourSet){
          tempEnc = readEncoder(60);

           //rightmost digits - mins
           lc.setDigit(0,2,(tempEnc / 10),false);
           lc.setDigit(0,3,(tempEnc % 10),false);

         
          if (encButton.released()){
            alaMins = tempEnc;

             long alaMinsSet = 0;
             long alaHourSet = 0;
            
            //FIX TO START WAKING UP BEFORE ALARM TIME
            if (alaMins < (earlier/60))
            {
              alaMinsSet = alaMins + (earlier/60);
              alaHourSet = alaHours -1;
            } 
            if (alaMins >= (earlier/60))
            {
              alaMinsSet = alaMins - (earlier/60);
              alaHourSet = alaHours;
            }
            
            RTC.setAlarm(ALM1_MATCH_HOURS, 0, alaMinsSet, alaHourSet, 0);
            alarmOn = true;
            state = SHOWALARM;  
          }
        }
        break;
      }

    case SHOWALARM: {

        lc.setIntensity(0,0); 
        delay(500);
        lc.setIntensity(0,8); 
        DisplayDigits(alaHours,alaMins);
        delay(2000);
        state = TIME;
        break;
      }


    case SETTIME: {
        long tempEnc = 0;
        
        if (!timeHourSet){
          
          tempEnc = readEncoder(24);
           
           //leftmost digits - hours
           lc.setDigit(0,0,(tempEnc / 10),false);
           lc.setDigit(0,1,(tempEnc % 10),false);
           lc.setRow(0,2,B00000000);
           lc.setRow(0,3,B00000000);
           
          if (encButton.released()){
            timeHours = tempEnc;
            timeHourSet = true;
            myEnc.write(0);
          } 
        }
        
        if (timeHourSet){
          tempEnc = readEncoder(60);

            //rightmost digits - mins
           lc.setDigit(0,2,(tempEnc / 10),false);
           lc.setDigit(0,3,(tempEnc % 10),false);
          
          if (encButton.released()){
            timeMins = tempEnc;
       
            tm.Year = 46; //since 1970, this means 2016
            tm.Month = 12;
            tm.Day = 3;
            tm.Hour = timeHours;
            tm.Minute = timeMins;
            tm.Second = 0;
            t = makeTime(tm);
            RTC.set(t);        //use the time_t value to ensure correct weekday is set
            setTime(t);        
            lc.setIntensity(0,0);
            delay(500);
            lc.setIntensity(0,8);            
            if (debug){ 

                        
              //TimeDateDisplay();
              DisplayDigits(hour(),minute());
            }
            state = TIME;  
          }
        }
        break;
      }

    case ALARM: {

        
        if ((millis() - timeLapse) > steps) {
          wakeUp();
        }

        DisplayDigits(hour(),minute());

       if (encButton.released()){
        brightness = 0;
        analogWrite(outputPin, brightness);
        alarmOn = false;
        state = TIME;
       }
    break;
    }

    //NOT USED should I even implement this?
   case SNOOZE: {
 
       // User ihas indicated is still awake
       if (millis() - snoozeMillis> snoozeTime){
         state = ALARM;
       }
      break;
    }

    
  }
  delay(10); 
}


void DisplayDigits(long value1, long value2)
{


  
   //leftmost digits - hours
   lc.setDigit(0,0,(value1 / 10),false);
   lc.setDigit(0,1,(value1 % 10),false);
   
   //right most digits / minutes)
   lc.setDigit(0,2,(value2 / 10),false);
   lc.setDigit(0,3,(value2 % 10),alarmOn);
}


long readEncoder(long maxOptions){
  //because one 'blip' is 4 
  maxOptions = maxOptions * 4;
  
  long newPosition = myEnc.read();
  
  if (newPosition > maxOptions) { newPosition = 0; myEnc.write(0);}
  if (newPosition < 0) { newPosition = maxOptions; myEnc.write(maxOptions);}
  if (newPosition != oldPosition) {
    oldPosition = newPosition;
  }
  
  return oldPosition/4;
  
}

void wakeUp(){
        if (interval < pwmIntervals){
          interval=interval+1;
        }
        
        brightness = pow (2, (interval / R)) - 1;
        // Set the LED output to the calculated brightness
        analogWrite(outputPin, brightness);
        timeLapse = millis();
}
