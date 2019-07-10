#include <Wire.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <SPI.h>
#include <SD.h>

#include <RTCTimer.h>
#include <Sodaq_DS3231.h>
#include <Sodaq_PcInt_PCINT0.h>

#include <SoftwareSerial_PCINT12.h>
const int SonarExcite = 10;
SoftwareSerial sonarSerial(11, -1);        

boolean stringComplete = false;

#define READ_DELAY 1

//RTC Timer
RTCTimer timer;

String dataRec = "";
int currentminute;
long currentepochtime = 0;
float boardtemp = 0.0;

int batteryPin = A6;    // select the input pin for the potentiometer
int batterysenseValue = 0;  // variable to store the value coming from the sensor
float batteryvoltage;

int range_mm;

//RTC Interrupt pin
#define RTC_PIN A7
#define RTC_INT_PERIOD EveryMinute

#define SD_SS_PIN 12

//The data log file
#define FILE_NAME "SonicLog.txt"

//Data header
#define LOGGERNAME "Ultrasonic Maxbotix Sensor Datalogger"
#define DATA_HEADER "DateTime,Loggertime,BoardTemp,Battery_V,SonarRange_mm"
     

void setup() 
{
  //Initialise the serial connection
  Serial.begin(57600);
  sonarSerial.begin(9600);
  rtc.begin();
  delay(100);
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(SonarExcite, OUTPUT);
  digitalWrite(SonarExcite, LOW);  //pin 10 is the power pin for the ultrasonic sensor

  greenred4flash();   //blink the LEDs to show the board is on

  setupLogFile();

  //Setup timer events
  setupTimer();
  
  //Setup sleep mode
  setupSleep();
  
  Serial.println("Power On, running: ultrasonic_logger_example_1.ino");

}

void loop() 
{   
  //Update the timer 
  timer.update();
  
  if(currentminute % 2 == 0)    //if the time ends in an even number, a sample will be taken. Can be changed to other intervals
     {   
          digitalWrite(8, HIGH);  
          dataRec = createDataRecord();
    
          delay(500);    
          digitalWrite(SonarExcite, HIGH);
          delay(1000);

          range_mm = SonarRead();
          
          digitalWrite(SonarExcite, LOW);          
                     
          stringComplete = false; 
 
          //Save the data record to the log file
          logData(dataRec);
 
          //Echo the data to the serial connection
          Serial.println();
          Serial.print("Data Record: ");
          Serial.println(dataRec);      
   
   
          String dataRec = "";   
     
          digitalWrite(8, LOW);
          delay(500);
          
     }
      
  systemSleep();
}

void showTime(uint32_t ts)
{
  //Retrieve and display the current date/time
  String dateTime = getDateTime();
  //Serial.println(dateTime);
}

void setupTimer()
{
  
  //Schedule the wakeup every minute
  timer.every(READ_DELAY, showTime);
  
  //Instruct the RTCTimer how to get the current time reading
  timer.setNowCallback(getNow);


}

void wakeISR()
{
  //Leave this blank
}

void setupSleep()
{
  pinMode(RTC_PIN, INPUT_PULLUP);
  PcInt::attachInterrupt(RTC_PIN, wakeISR);

  //Setup the RTC in interrupt mode
  rtc.enableInterrupts(RTC_INT_PERIOD);
  
  //Set the sleep mode
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
}

void systemSleep()
{  
  //Wait until the serial ports have finished transmitting
  Serial.flush();
  Serial1.flush();
  
  //The next timed interrupt will not be sent until this is cleared
  rtc.clearINTStatus();
    
  //Disable ADC
  ADCSRA &= ~_BV(ADEN);
  
  //Sleep time
  noInterrupts();
  sleep_enable();
  interrupts();
  sleep_cpu();
  sleep_disable();
 
  //Enbale ADC
  ADCSRA |= _BV(ADEN);
  
}

String getDateTime()
{
  String dateTimeStr;
  
  //Create a DateTime object from the current time
  DateTime dt(rtc.makeDateTime(rtc.now().getEpoch()));

  currentepochtime = (dt.get());    //Unix time in seconds 

  currentminute = (dt.minute());
  //Convert it to a String
  dt.addToString(dateTimeStr); 
  return dateTimeStr;  
}

uint32_t getNow()
{
  currentepochtime = rtc.now().getEpoch();
  return currentepochtime;
}

void greenred4flash()
{
  for (int i=1; i <= 4; i++){
  digitalWrite(8, HIGH);   
  digitalWrite(9, LOW);
  delay(50);
  digitalWrite(8, LOW);
  digitalWrite(9, HIGH);
  delay(50);
  }
  digitalWrite(9, LOW);
}

void setupLogFile()
{
  //Initialise the SD card
  if (!SD.begin(SD_SS_PIN))
  {
    Serial.println("Error: SD card failed to initialise or is missing.");
    //Hang
  //  while (true); 
  }
  
  //Check if the file already exists
  bool oldFile = SD.exists(FILE_NAME);  
  
  //Open the file in write mode
  File logFile = SD.open(FILE_NAME, FILE_WRITE);
  
  //Add header information if the file did not already exist
  if (!oldFile)
  {
    logFile.println(LOGGERNAME);
    logFile.println(DATA_HEADER);
  }
  
  //Close the file to save it
  logFile.close();  
}

void logData(String rec)
{
  //Re-open the file
  File logFile = SD.open(FILE_NAME, FILE_WRITE);
  
  //Write the CSV data
  logFile.println(rec);
  
  //Close the file to save it
  logFile.close();  
}

String createDataRecord()
{
    //Create a String type data record in csv format
    //TimeDate, Loggertime,Temp_DS, Diff1, Diff2, boardtemp
    String data = getDateTime();
    data += ",";  
  
    rtc.convertTemperature();          //convert current temperature into registers
    boardtemp = rtc.getTemperature(); //Read temperature sensor value
    
    batterysenseValue = analogRead(batteryPin);
    batteryvoltage = (3.3/1023.) * 1.47 * batterysenseValue;
    
    data += currentepochtime;
    data += ",";

    addFloatToString(data, boardtemp, 3, 1);    //float   
    data += ",";  
    addFloatToString(data, batteryvoltage, 4, 2);
  
    return data;
}


static void addFloatToString(String & str, float val, char width, unsigned char precision)
{
  char buffer[10];
  dtostrf(val, width, precision, buffer);
  str += buffer;
}


int SonarRead() 
{

  int result;
  char inData[5];                                          //char array to read data into
  int index = 0;

  while (sonarSerial.read() != -1) {}
   
  while (stringComplete == false) {

      if (sonarSerial.available())
    {
      char rByte = sonarSerial.read();                     //read serial input for "R" to mark start of data
      if(rByte == 'R')
      {
        //Serial.println("rByte set");
        while (index < 4)                                  //read next three character for range from sensor
        {
          if (sonarSerial.available())
          {
            inData[index] = sonarSerial.read(); 
            //Serial.println(inData[index]);               //Debug line

            index++;                                       // Increment where to write next
          }  
        }
        inData[index] = 0x00;                              //add a padding byte at end for atoi() function
      }

      rByte = 0;                                           //reset the rByte ready for next reading

      index = 0;                                           // Reset index ready for next reading
      stringComplete = true;                               // Set completion of read to true
      result = atoi(inData);                               // Changes string data into an integer for use
    }
  }

      dataRec += ",";    
      dataRec += result;  

  return result;
}
