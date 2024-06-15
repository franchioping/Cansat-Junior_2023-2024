#include <RFM69.h>    

#define NODEID      1
#define NETWORKID   100 // Change to a less common value on the day    
#define GATEWAYID   1   // As a rule of thumb the gateway ID should always be 1. Doesn't do anything here but kept for symmetry

// Both have to be the same
#define FREQUENCY     RF69_433MHZ


// Listen in on packets even if they weren't meant for us
bool spy = true;

RFM69 radio;

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
transData theData;

/*
  Inspired By:

  HOW TO RESET AN ARDUINO USING CODE: SOFTWARE RESET
  By: TheGeekPub.com
  More Arduino Tutorials: https://www.thegeekpub.com/arduino-tutorials/


  Not tested, should crash and then reset the board.
  Would just wire a pin to the reset pin but wasn't allowed.
*/
void(* resetFunc) (void) = 0; 


void setup() {
  // Initialze Serial
  Serial.begin(9600);

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

  /*
    Check if radio was initialized correctly. If not, try to reset the arduino
  */
  if(sucess == 0){
    Serial.println("Couln't initialize radio, hard reseting...");
    delay(200);  
    resetFunc();
  
  }

}

void loop() {

  /*
    RFM69::receiveDone() returns true if we have data ready to be read
    data is located at radio.DATA with lenght radio.DATALEN


    All the data recieved is just printed out to the serial. Would love to keep it on an SD card but wasn't allowed
  */
  if(radio.receiveDone()){

    /* ===== Pretty printing ===== */
    Serial.print('[');Serial.print(radio.SENDERID, DEC);Serial.print("] ");

    // RSSI is the signal strenght, signals if there is a good connection - Higher is Better
    Serial.print(" [RX_RSSI:");Serial.print(radio.readRSSI());Serial.print("]");

    /*
      We can detect all packets in the network, even if they aren't addressed to us
      Do this by default, 
    */
    if (spy) Serial.print("to [");Serial.print(radio.TARGETID, DEC);Serial.print("] ");

    /*
      If the data we receive isn't the size we expect, warn us about it
    */
    if (radio.DATALEN != sizeof(transData)){
      Serial.print("Invalid packet received, lenght not matching expected struct!");

      /*
        Even if the payload isnt what we expect, print it as a string just in case
        if something goes wrong this is here so we still have SOME data

        Allocates a new chunk of memory with the data lenght + 1 so we can append a NULL at the end and treat it as a string
      */
      char* data_string = (char*) malloc(radio.DATALEN+1);
      if(data_string != NULL){
        memcpy(data_string, radio, radio.DATALEN);
        data_string[radio.DATALEN] = '\0';

        Serial.println(data_string);
        free(data_string);
      }else{
        Serial.print("Failed to malloc string, not printing the packet");
      }
    }
    else{
      /*
        Assume radio.DATA actually contains our struct and not something else of the same lenght.
        Could be fixed by adding magic digits at the beggining/end but because of struct alignment that would
        be really inefficient in terms of data size and for performance reasons we want our structs to be aligned
      */

      /*
        Printing like this SHOULD allow us to graph the data later.
        uptime is included to help with data analasys
      */
      theData = *(transData*) radio.DATA;
      Serial.print(" temperature=");
      Serial.print(theData.temperature);
      Serial.print(" pressure=");
      Serial.print(theData.pressure);
      Serial.print(" time=");
      Serial.print(theData.time);
    }
    Serial.println(".");
  }
}
