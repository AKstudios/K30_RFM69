#include <RFM69.h>  //  https://github.com/LowPowerLab/RFM69
#include <SPI.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <Wire.h>

// define node parameters
char node[] = "200";
#define NODEID       200 // same sa above - must be unique for each node on same network (range up to 254, 255 is used for broadcast)
#define GATEWAYID     1
#define NETWORKID     101
#define FREQUENCY     RF69_915MHZ //Match this with the version of your Moteino! (others: RF69_433MHZ, RF69_868MHZ)
#define ENCRYPTKEY    "Tt-Mh=SQ#dn#JY3_" //has to be same 16 characters/bytes on all nodes, not more not less!
#define IS_RFM69HW    //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define LED             9


// define objects
RFM69 radio;

// global variables
char dataPacket[50];
int CO2ppm = 0;
int _CO2ppm;

// ISR ****************************************************************
ISR(WDT_vect)  // Interrupt service routine for WatchDog Timer
{
  wdt_disable();  // disable watchdog
}


// setup ****************************************************************
void setup()
{
  pinMode(10, OUTPUT);
  Serial.begin(115200); // open serial at 115200 bps
  Serial.println("Setup");
  Wire.begin();  // initialize I2C using Wire.h library

  radio.initialize(FREQUENCY,NODEID,NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //uncomment only for RFM69HW!
#endif
  radio.encrypt(ENCRYPTKEY);
  
  pinMode(LED, OUTPUT);  // pin 9 controls LED
  delay(1);
}


// sleep ****************************************************************
void sleep()
{
  Serial.flush(); // empty the send buffer, before continue with; going to sleep
  
  radio.sleep();
  
  cli();          // stop interrupts
  MCUSR = 0;
  WDTCSR  = (1<<WDCE | 1<<WDE);     // watchdog change enable
  WDTCSR  = 1<<WDIE | (1<<WDP3) | (0<<WDP2) | (0<<WDP1) | (0<<WDP0); // set  prescaler to 4 second
  sei();  // enable global interrupts

  byte _ADCSRA = ADCSRA;  // save ADC state
  ADCSRA = 0; // turn off ADC

  asm("wdr");
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);  // set sleep mode to power down       
  //PORTD |= (1<<PORTD4); //Activate pullup on pin 4
  //PCICR |= (1<<PCIE2);
  //PCMSK2 |= (1<<PCINT20);
  cli();
  sleep_enable();  
  //sleep_bod_disable();  // turn off BOD
  sei();       
  sleep_cpu();    // goodnight!
    
  sleep_disable();   
  sei();  

  ADCSRA = _ADCSRA; // restore ADC state (enable ADC)
  delay(1);
}

// loop ****************************************************************
void loop()
{
  sleep();
  
  CO2ppm = GetCO2(0x68); // default address for K-30 CO2 sensor is 0x68
  
  // account for dropped values
  if(CO2ppm > 0)
    _CO2ppm = CO2ppm;
  if(CO2ppm <=0)
    CO2ppm = _CO2ppm;

   char _c[5];
  
  // convert all flaoting point and integer variables into character arrays
  //dtostrf(nodeID, 2, 0, _i);
  dtostrf(CO2ppm, 1, 0, _c);  // this function converts float into char array. 3 is minimum width, 2 is decimal precision
  delay(1);
  
  dataPacket[0] = 0;  // first value of dataPacket should be a 0
  
  // create datapacket by combining all character arrays into a large character array
  strcat(dataPacket, "i:");
  strcat(dataPacket, node);
  strcat(dataPacket, ",c:");
  strcat(dataPacket, _c);
  delay(1);
  
  Serial.println(dataPacket);
  Serial.println(strlen(dataPacket));
  delay(10);

  // send datapacket
  radio.sendWithRetry(GATEWAYID, dataPacket, strlen(dataPacket), 5, 100);  // send data, retry 5 times with delay of 100ms between each retry
  dataPacket[0] = (char)0; // clearing first byte of char array clears the array

  // blink LED to indicate wireless data is sent (not necessarily received)
  digitalWrite(LED, HIGH);
  delay(10);
  digitalWrite(LED, LOW);
}

// Get CO2 concentration ****************************************************************
int GetCO2(int address)
{
  byte recieved[4] = {0,0,0,0}; // create an array to store bytes received from sensor
  
  Wire.beginTransmission(address);
  Wire.write(0x22);
  Wire.write(0x00);
  Wire.write(0x08);
  Wire.write(0x2A);
  Wire.endTransmission();
  delay(20); // give delay to ensure transmission is complete
  
  Wire.requestFrom(address,4);
  delay(10);
  
  byte i=0;
  while(Wire.available())
  {
    recieved[i] = Wire.read();
    i++;
  }
  
  byte checkSum = recieved[0] + recieved[1] + recieved[2];
  CO2ppm = (recieved[1] << 8) + recieved[2];
  
  if(checkSum == recieved[3])
    return CO2ppm;
  else
    return -1;
}
