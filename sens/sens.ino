#include <Adafruit_BMP085.h>
#include <RFM69.h>    
#include <OneWire.h>


#define CJKIT_VERSION 2
#include <CJKit.h>

#define DALLASPIN 4

OneWire ds(DALLASPIN);


#define NODEID           2
#define NETWORKID        100
#define GATEWAYID        1   //as a rule of thumb the gateway ID should always be 1
#define FREQUENCY        RF69_433MHZ  //match the RFM69 version! Others: RF69_433MHZ, RF69_868MHZ

#define INIT_RETRIES 10

const uint32_t RADIO_FREQUENCY = 433000000; // Hz

RFM69 radio;
void init_radio_failsafe();

void init_temperature_failsafe();
bool init_temperature();
float get_temperature();

Adafruit_BMP085 bmp;
void init_pressure_failsafe();
float get_pressure();

/*
  Transmission data. This struct is and has to be the same on both sides of the communication.
  If they aren't the same, code will fallback to printing the data as a string so it can be recovered later
*/
typedef struct transData {
  float temperature;
  float pressure;
  unsigned long time;
} transData;

// Initialize our Data Structure. we write over it every time to avoid mallocing memory repeatedly
transData TransmissionData;


/*
  Inspired By:

  HOW TO RESET AN ARDUINO USING CODE: SOFTWARE RESET
  By: TheGeekPub.com

  Not tested, should crash and then reset the board.
  Would just wire a pin to the reset pin but wasn't allowed.
*/
void(* resetFunc) (void) = 0; 



void setup() {
  Serial.begin(9600);


  pinMode(5, OUTPUT);           // set pin to input

 

  init_pressure_failsafe();
  init_temperature_failsafe();
  init_radio_failsafe();
  
}

void loop() {
  
  /*ss
    Load all of our data into the struct,
    Don't allocate/free everytime for performace 
  */
  TransmissionData.temperature = get_temperature();
  TransmissionData.pressure = get_pressure();
  TransmissionData.time = millis();

  /*
    Print out data for debug purposes
  */
  Serial.println("Current Data is:");
  Serial.println(TransmissionData.temperature);
  Serial.println(TransmissionData.pressure);
  Serial.println(TransmissionData.time);

  /*
    Send all of our struct
  */
  Serial.print("SENDING...");
  radio.send(GATEWAYID, (const void *)&TransmissionData, sizeof(TransmissionData));
  digitalWrite(5, HIGH);
  delay(10);
  digitalWrite(5, LOW);
  Serial.println("SENT!");
  delay(40);

}

/* BMP180 (Pressure) */

void init_pressure_failsafe(){
  /*
    Pressure sensor never failed but added safeguard just in case it doesn't initialize correctly

    Do this just to make sure it initializes. if it doesnt correctly, we get low percision readouts
  */
  bool sucess_pressure = false;
  for(int i = 0; i < INIT_RETRIES; i++){
    sucess_pressure = bmp.begin(BMP085_ULTRAHIGHRES);
    if(sucess_pressure){
      Serial.print("BMP180 initialized succesfully on attempt ");
      Serial.println(i);
      break;
    }
    delay(50);
  }
}

float get_pressure(){
  return bmp.readPressure();
}

/* 
  DS18B20 (Temperature) 

  Inspired By:
  https://github.com/nettigo/DS18B20
  And 
  https://github.com/microentropie/DS18B20
*/

/*
  We communicate with the DS18B20 through commands, mainly writing/reading to/from the Scratchpad

  The scratch pad is an 9 byte long data structure located inside the sensor.
  The data structure contains the information as follows.
  Bytes:
   0.   TEMPERATURE LSB
   1.   TEMPERATURE MSB
   2.   TH/USER BYTE 1
   3.   TL/USER BYTE 2
   4.   CONFIG
   5-7. RESERVED
   8.   CRC

*/


float get_temperature(){
  int16_t temp;
    
  ds.reset();
  ds.write(0xCC); // skip ROM command
  ds.write(0xBE); // read scratchpad

  // Read bytes 0 and 1 of the scrachpad into temp. the value read is temperature * 16
  ds.read_bytes((uint8_t*) &temp, sizeof(temp)); 

  ds.reset(); 
  ds.write(0xCC); // skip ROM command
  ds.write(0x44, 1); // Start getting temperature
    
  return (temp * 0.0625); // Same as dividing by 16 - gets the accurate temperature data
}

bool init_temperature()
{
  byte data[12];
  byte addr[8];


  // Read ROM:
  if (!ds.search(addr))
  {
    ds.reset_search();
    delay(250);
    return false; // no more addresses
  }
  if (OneWire::crc8(addr, 7) != addr[7]) return false; // read ERROR: invalid data readin

  // first byte is the device type
  if (addr[0] != 0x28) return false; // this is not DS18B20

  
  /*
    Update Resolution
  */
  ds.reset();
  ds.select(addr);
  ds.write(0xBE);         // Read Scratchpad
  for (byte i = 0; i < 9; i++) // we need 9 bytes
    data[i] = ds.read();
  
  
  /*
    Byte index 4 of the scratchpad is where the configuration is and bits 6 and 5 of said byte set the resolution. 
    This byte should always be 0xx11111 where xx sets the resolution.
    resolution can be from 9 to 12 bits:
    - 00 is 9
    - 01 is 10
    - 10 is 11
    - 11 is 12

    bitwise OR with byte 4 and 0x60 (0b01100000) sets the resolution bits to 11, getting the 0.0625 resolution

    Even tho we now have a percision of 0.06, datasheet only grants us an accuracy of +-0.50
  */
  data[4] |= 0x60;


  /*
    According to the Datasheet this command always requires writing starting from byte index 2, so we do just that.
  */
  ds.reset();
  ds.select(addr);
  ds.write(0x4E); // Write Scratchpad Command
  ds.write(data[2]);
  ds.write(data[3]);
  ds.write(data[4]);


  // Read temperature
  ds.reset();
  ds.write(0xCC); // skip ROM command
  ds.write(0x44, 1); // start conversion, assuming 5V connected to Vcc pin

  return true;
}


void init_temperature_failsafe(){
  /*
    Temperature sensor sometimes doesnt like to work (when it's pushed around and doen't make contact)
    (It only happened once since the start of this project but just in case)

    Do this just to make sure it initializes. if it doesnt correctly, we get low percision readouts
  */
  bool sucess_temperature = false;
  for(int i = 0; i < INIT_RETRIES; i++){
    sucess_temperature = init_temperature();
    if(sucess_temperature){
      Serial.print("DS18B20 initialized succesfully on attempt ");
      Serial.println(i);
      break;
    }
    delay(50);
  }
}

/* RF69 (Communication) */

void init_radio_failsafe(){
 

  /*  
    Set Chip-Select pin for the radio.
    The RF69 uses the SPI interface for communication so each chip requires its own chip-select
  */
  radio.setCS(10);

  /*
    Set the Interrupt pin for the radio - Connected to DIO0 (Digital IO 0).
    This pin is programmed as an interrupt for recieving data by the RFM69 library

    All other interrupts are programatically disabled so we only need to set this one
  */
  radio.setIrq(3);

  /*
    Initializes radio and returns 1 if it is successful
  */
  bool sucess = radio.initialize(FREQUENCY,NODEID,NETWORKID);
  radio.setHighPower();

  Serial.print("RADIO INITIALIZE: ");
  Serial.println(sucess);
}









