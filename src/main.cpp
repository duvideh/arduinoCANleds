
//RPM Shift Lights using an MCP2515 CAN bus board.
//Adafruit NeoPixel LED Sticks, two 8-led RGBW warm-white sticks.
//NeoPixel codes is (#,#,#,#,#) = (numberOfLed,Red,Green,Blue,White) numberOfLed = 0-15, Color is in brightness 0-255
//4N25 Opto-coupler input for dimming. 12v on diode side, ground on switch.

#include <arduino.h>
//#include <mcp2515.h> old
#include <mcp_can.h> //library: coryjfowler/mcp_can
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>

#define rxPin 4
#define txPin 3

// Set up a new SoftwareSerial object
SoftwareSerial softSerial (rxPin, txPin);

int RPMno = 6000;   //base RPM for LED activation
int vehicleRPM = 0; //rpm value to be provided by CAN bus
int oilTemp = 0; //oil temperature from CAN bus
int coolant = 0; // coolant temperature from CAN bus

//struct can_frame canMsg;  //old
MCP_CAN CAN0(7); // bracketed number is number of CS pin
int ex = 0;
int why = 0;
long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];

//NeoPixels
#define PIN 6
#define NUM_LEDS 16
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, PIN, NEO_GRBW + NEO_KHZ800);
//full brightness
uint32_t r = strip.Color  (200, 0, 0, 0);
uint32_t g = strip.Color  (0, 180, 0, 0);
uint32_t b = strip.Color  (0, 0, 255, 0);
uint32_t w = strip.Color  (0, 0, 0, 120);
//dimmed
uint32_t rd = strip.Color (8, 0, 0, 0);
uint32_t gd = strip.Color (0, 7, 0, 0);
uint32_t bd = strip.Color (0, 0, 5, 0);
uint32_t wd = strip.Color (0, 0, 0, 3);
//off
uint32_t o = strip.Color   (0, 0, 0, 0);
int ledStatus = 0; //color state of leds
int previousledStatus = 0;
int space = 50; //delay for led startup sequence

//headlight circuit
#define headlights 2
#define headlightSignal 10
bool val = 1;
bool dimmerOutput = 0;


unsigned long millisStart = 0;
unsigned long millisTwo = 0;

void leds(); 
void ledsDimmed();
void dimmer();
void getMessage();
void ledStartup();

//   _____ ______ _______ _    _ _____
//  / ____|  ____|__   __| |  | |  __ \ '/'
// | (___ | |__     | |  | |  | | |__) |
//  \___ \|  __|    | |  | |  | |  ___/
//  ____) | |____   | |  | |__| | |
// |_____/|______|  |_|   \____/|_|

void setup()
{
  //initiate neopixels
  strip.begin();
  //set strip to nothing, flushes potential random colors
  strip.clear();
  strip.fill(o);
  strip.show();

  ledStartup();

  //assign I/O for headlights
  pinMode(headlights, INPUT_PULLUP); //input from radio wire
  pinMode(headlightSignal, OUTPUT); //output wire to MEGA


  //setup the CANBus module
  //mcp2515.reset();
  //mcp2515.setBitrate(CAN_500KBPS, MCP_16MHZ);
  //mcp2515.setListenOnlyMode();
  if(CAN0.begin(MCP_STDEXT, CAN_500KBPS, MCP_16MHZ) == CAN_OK) Serial.print("MCP2515 Init Okay!!\r\n");
  else Serial.print("MCP2515 Init Failed!!\r\n");
  CAN0.init_Mask(0,0,0x010F0000);                // Init first mask...
  CAN0.init_Filt(0,0,0x140);                // Init first filter...
  CAN0.init_Filt(1,0,0x360);                // Init second filter...
  
  CAN0.setMode(MCP_NORMAL);         // Change to normal mode to allow messages to be transmitted

  softSerial.begin(9600);
  Serial.begin(9600);
  
  millisStart = millis();
}


//  _      ____   ____  _____
// | |    / __ \ / __ \|  __ \ '/'
// | |   | |  | | |  | | |__) |
// | |   | |  | | |  | |  ___/
// | |___| |__| | |__| | |
// |______\____/ \____/|_|

void loop()
{
  //get CAN Message
  //if ((mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK)) {
    getMessage();
  //}
  if(millis() >= millisTwo + 1) {
    strip.clear();
    dimmer();
    if (ledStatus != previousledStatus ) {
      strip.show();
    }
   // if (dimmerOutput == 1) {
   //   digitalWrite(headlightSignal,HIGH);
   // }
   // else {
   //  digitalWrite(headlightSignal,LOW);
   // }
   previousledStatus = ledStatus;
  }

  if (millis() >= millisStart + 200) {
    softSerial.println("<");
    softSerial.println(oilTemp);
    softSerial.println(",");
    softSerial.println(coolant);
    softSerial.println(",");
    softSerial.println(vehicleRPM);
    softSerial.println(">");
    //Serial.println(vehicleRPM);
    
    millisStart = millis();
  }
  millisTwo = millis();
  //Serial.println(oilTemp);
  
}



//__      ______ _____ _____
// \ \    / / __ \_   _|  __ \ '/'
//  \ \  / / |  | || | | |  | |
//   \ \/ /| |  | || | | |  | |
//    \  / | |__| || |_| |__| |
//     \/   \____/_____|_____/
//
//            _
//  __ _  ___| |_ _ __ _ __  _ __ ___
// / _` |/ _ \ __| '__| '_ \| '_ ` _ \ '/'
//| (_| |  __/ |_| |  | |_) | | | | | |
// \__, |\___|\__|_|  | .__/|_| |_| |_|
// |___/              |_|

void getMessage (void) {           //formerly getRPM
  CAN0.readMsgBuf(&rxId, &len, rxBuf); // Read data: len = data length, buf = data byte(s)
  if (rxId == 0x140) {
    ex = rxBuf[2];
    why = rxBuf[3];
    vehicleRPM = (why & 0x3f) * 256 + ex;
  }
  if (rxId == 0x360) {
    oilTemp = (rxBuf[2]) - 40;
    coolant = (rxBuf[3]) - 40;    
  }
  
  
  
  
  // if (canMsg.can_id == 0x140) {
  //  ex = canMsg.data[2];
  //  why = canMsg.data[3];
  //  //combine the 2 digit hex for field 2 and field 3 to get decimal rpm
  //  vehicleRPM = (why & 0x3f) * 256 + ex;
  //Serial.print("RPM: ");
  //Serial.println(vehicleRPM);
  //}
  // if (canMsg.can_id == 0x360) {
  //   oilTemp = (canMsg.data[2]) - 40;
  //   coolant = (canMsg.data[3]) - 40;
  //   //Serial.println("Oil:");
  //   //Serial.println(oilTemp);
  //   //Serial.println("Coolant:");
  //   //Serial.println(coolant);
  // }
}

//     _ _
//  __| (_)_ __ ___  _ __ ___   ___ _ __
// / _` | | '_ ` _ \| '_ ` _ \ / _ \ '__|
//| (_| | | | | | | | | | | | |  __/ |
// \__,_|_|_| |_| |_|_| |_| |_|\___|_|

void dimmer (void) {

  val = digitalRead(headlights);
  if (val == HIGH) {
    leds();
    //dimmerOutput = 0;
    digitalWrite(headlightSignal,LOW);
  }
  else {
    ledsDimmed();
    //dimmerOutput = 1;
    digitalWrite(headlightSignal,HIGH);
  }
}


//  _          _
// | | ___  __| |___
// | |/ _ \/ _` / __|
// | |  __/ (_| \__ \ '/' 
// |_|\___|\__,_|___/  font = ogre

void leds(void) {

  if (vehicleRPM > 0 && vehicleRPM < (RPMno - 1000) )
  {
    strip.fill(o);
    ledStatus = 0;
  }

  if (vehicleRPM >= (RPMno - 1000) && vehicleRPM < (RPMno - 500) )
  {
    strip.fill(g, 15);
    ledStatus = 15;
  }

  if (vehicleRPM >= (RPMno - 500) && vehicleRPM < RPMno )
  {
    strip.fill(g, 14);
    ledStatus = 14;
  }

  if (vehicleRPM >= (RPMno) && vehicleRPM < (RPMno + 71))
  {
    strip.fill(b, 13);
    ledStatus = 13;
  }

  if (vehicleRPM >= (RPMno + 71) && vehicleRPM < (RPMno + 143))
  {
    strip.fill(b, 12);
    ledStatus = 12;
  }

  if (vehicleRPM >= (RPMno + 143) && vehicleRPM < (RPMno + 214))
  {
    strip.fill(b, 11);
    ledStatus = 11;
  }

  if (vehicleRPM >= (RPMno + 214) && vehicleRPM < (RPMno + 28))
  {
    strip.fill(b, 10);
    ledStatus = 10;
  }

  if (vehicleRPM >= (RPMno + 286) && vehicleRPM < (RPMno + 357))
  {
    strip.fill(b, 9);
    ledStatus = 9;
  }

  if (vehicleRPM >= (RPMno + 357) && vehicleRPM < (RPMno + 429))
  {
    strip.fill(b, 8);
    ledStatus = 8;
  }

  if (vehicleRPM >= (RPMno + 429) && vehicleRPM < (RPMno + 500))
  {
    strip.fill(b, 7);
    ledStatus = 7;
  }

  if (vehicleRPM >= (RPMno + 500) && vehicleRPM < (RPMno + 571))
  {
    strip.fill(b, 6);
    ledStatus = 6;
  }

  if (vehicleRPM >= (RPMno + 571) && vehicleRPM < (RPMno + 643))
  {
    strip.fill(r, 5);
    ledStatus = 5;
  }
  if (vehicleRPM >= (RPMno + 643) && vehicleRPM < (RPMno + 714))
  {
    strip.fill(r, 4);
    ledStatus = 4;
  }

  if (vehicleRPM >= (RPMno + 714) && vehicleRPM < (RPMno + 786))
  {
    strip.fill(r, 3);
    ledStatus = 3;
  }

  if (vehicleRPM >= (RPMno + 786) && vehicleRPM < (RPMno + 857))
  {
    strip.fill(r, 2);
    ledStatus = 2;
  }

  if (vehicleRPM >= (RPMno + 857) && vehicleRPM < (RPMno + 929))
  {
    strip.fill(r, 1);
    ledStatus = 1;
  }

  if (vehicleRPM >= (RPMno + 929) && vehicleRPM < (RPMno + 1000))
  {
    strip.fill(r);
    ledStatus = 16;
  }

  if (vehicleRPM >= (RPMno + 1000) && vehicleRPM < (RPMno + 1500))
  {
    strip.fill(w);
    ledStatus = 17;
  }

  if (vehicleRPM >= (RPMno + 1500) && vehicleRPM < (RPMno + 2000) )
  {
    strip.fill(o);
    ledStatus = 0;
  }

  if (vehicleRPM > 10000)
  {
    strip.fill(o);
    ledStatus = 0;
  }
}

//  _          _        ___ _                              _
// | | ___  __| |___   /   (_)_ __ ___  _ __ ___   ___  __| |
// | |/ _ \/ _` / __| / /\ / | '_ ` _ \| '_ ` _ \ / _ \/ _` |
// | |  __/ (_| \__ \/ /_//| | | | | | | | | | | |  __/ (_| |
// |_|\___|\__,_|___/___,' |_|_| |_| |_|_| |_| |_|\___|\__,_|

void ledsDimmed(void)  {

  if (vehicleRPM > 0 && vehicleRPM < (RPMno - 1000) )
  {
    strip.fill(o);
    ledStatus = 0;
  }

  if (vehicleRPM >= (RPMno - 1000) && vehicleRPM < (RPMno - 500) )
  {
    strip.fill(gd, 15);
    ledStatus = 15;
  }

  if (vehicleRPM >= (RPMno - 500) && vehicleRPM < RPMno )
  {
    strip.fill(gd, 14);
    ledStatus = 14;
  }

  if (vehicleRPM >= (RPMno) && vehicleRPM < (RPMno + 71))
  {
    strip.fill(bd, 13);
    ledStatus = 13;
  }

  if (vehicleRPM >= (RPMno + 71) && vehicleRPM < (RPMno + 143))
  {
    strip.fill(bd, 12);
    ledStatus = 12;
  }

  if (vehicleRPM >= (RPMno + 143) && vehicleRPM < (RPMno + 214))
  {
    strip.fill(bd, 11);
    ledStatus = 11;
  }

  if (vehicleRPM >= (RPMno + 214) && vehicleRPM < (RPMno + 28))
  {
    strip.fill(bd, 10);
    ledStatus = 10;
  }

  if (vehicleRPM >= (RPMno + 286) && vehicleRPM < (RPMno + 357))
  {
    strip.fill(bd, 9);
    ledStatus = 9;
  }

  if (vehicleRPM >= (RPMno + 357) && vehicleRPM < (RPMno + 429))
  {
    strip.fill(bd, 8);
    ledStatus = 8;
  }

  if (vehicleRPM >= (RPMno + 429) && vehicleRPM < (RPMno + 500))
  {
    strip.fill(bd, 7);
    ledStatus = 7;
  }

  if (vehicleRPM >= (RPMno + 500) && vehicleRPM < (RPMno + 571))
  {
    strip.fill(bd, 6);
    ledStatus = 6;
  }

  if (vehicleRPM >= (RPMno + 571) && vehicleRPM < (RPMno + 643))
  {
    strip.fill(rd, 5);
    ledStatus = 5;
  }
  if (vehicleRPM >= (RPMno + 643) && vehicleRPM < (RPMno + 714))
  {
    strip.fill(rd, 4);
    ledStatus = 4;
  }

  if (vehicleRPM >= (RPMno + 714) && vehicleRPM < (RPMno + 786))
  {
    strip.fill(rd, 3);
    ledStatus = 3;
  }

  if (vehicleRPM >= (RPMno + 786) && vehicleRPM < (RPMno + 857))
  {
    strip.fill(rd, 2);
    ledStatus = 2;
  }

  if (vehicleRPM >= (RPMno + 857) && vehicleRPM < (RPMno + 929))
  {
    strip.fill(rd, 1);
    ledStatus = 1;
  }

  if (vehicleRPM >= (RPMno + 929) && vehicleRPM < (RPMno + 1000))
  {
    strip.fill(rd);
    ledStatus = 16;
  }

  if (vehicleRPM >= (RPMno + 1000) && vehicleRPM < (RPMno + 1500))
  {
    strip.fill(wd);
    ledStatus = 17;
  }

  if (vehicleRPM >= (RPMno + 1500) && vehicleRPM < (RPMno + 2000) )
  {
    strip.fill(o);
    ledStatus = 0;
  }

  if (vehicleRPM > 10000)
  {
    strip.fill(o);
    ledStatus = 0;
  }
}

//  _          _      __ _             _
// | | ___  __| |___ / _\ |_ __ _ _ __| |_ _   _ _ __
// | |/ _ \/ _` / __|\ \| __/ _` | '__| __| | | | '_ \ '/'
// | |  __/ (_| \__ \_\ \ || (_| | |  | |_| |_| | |_) |
// |_|\___|\__,_|___/\__/\__\__,_|_|   \__|\__,_| .__/
//                                             |_|
void ledStartup(void)
{
  strip.fill(o);
  strip.show();
  delay(space);

  strip.fill(gd, 15);
  strip.show();
  delay(space);

  strip.fill(gd, 14);
  strip.show();
  delay(space);

  strip.fill(bd, 13);
  strip.show();
  delay(space);

  strip.fill(bd, 12);
  strip.show();
  delay(space);

  strip.fill(bd, 11);
  strip.show();
  delay(space);

  strip.fill(bd, 10);
  strip.show();
  delay(space);

  strip.fill(bd, 9);
  strip.show();
  delay(space);

  strip.fill(bd, 8);
  strip.show();
  delay(space);

  strip.fill(bd, 7);
  strip.show();
  delay(space);

  strip.fill(bd, 6);
  strip.show();
  delay(space);

  strip.fill(rd, 5);
  strip.show();
  delay(space);

  strip.fill(rd, 4);
  strip.show();
  delay(space);

  strip.fill(rd, 3);
  strip.show();
  delay(space);

  strip.fill(rd, 2);
  strip.show();
  delay(space);

  strip.fill(rd, 1);
  strip.show();
  delay(space);

  strip.fill(rd);
  strip.show();
  delay(space);
  //---------------------------
  strip.fill(wd);
  strip.show();
  delay(space * 2);
  //----------------------------
  strip.clear();
  strip.fill(rd);
  strip.show();
  delay(space);

  strip.fill(rd, 1);
  strip.show();
  delay(space);

  strip.clear();
  strip.fill(rd, 2);
  strip.show();
  delay(space);

  strip.fill(rd, 3);
  strip.show();
  delay(space);

  strip.clear();
  strip.fill(rd, 4);
  strip.show();
  delay(space);

  strip.fill(rd, 5);
  strip.show();
  delay(space);

  strip.clear();
  strip.fill(bd, 6);
  strip.show();
  delay(space);

  strip.clear();
  strip.fill(bd, 7);
  strip.show();
  delay(space);

  strip.clear();
  strip.fill(bd, 8);
  strip.show();
  delay(space);

  strip.clear();
  strip.fill(bd, 9);
  strip.show();
  delay(space);

  strip.clear();
  strip.fill(bd, 10);
  strip.show();
  delay(space);

  strip.clear();
  strip.fill(bd, 11);
  strip.show();
  delay(space);

  strip.clear();
  strip.fill(bd, 12);
  strip.show();
  delay(space);

  strip.clear();
  strip.fill(bd, 13);
  strip.show();
  delay(space);

  strip.clear();
  strip.fill(gd, 14);
  strip.show();
  delay(space);

  strip.clear();
  strip.fill(gd, 15);
  strip.show();
  delay(space);

  strip.fill(o);
  strip.show();
}
