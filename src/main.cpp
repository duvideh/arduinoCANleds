//RPM Shift Lights using an MCP2515 CAN bus board
//Adafruit NeoPixel LED Sticks, two 8-led RGBW warm-white sticks
//NeoPixel codes is (#,#,#,#,#) = (numberOfLed,Red,Green,Blue,White) numberOfLed = 0-15, Color is in brightness 0-255
//4N25 Opto-coupler input for dimming. 12v on diode side, ground on switch
//Sending can data to Arduino MEGA via SoftwareSerial

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
#define NUM_LEDS 8
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, PIN, NEO_GRBW + NEO_KHZ800);
//full brightness
uint32_t r = strip.Color  (200, 0, 0, 0);
uint32_t rb = strip.Color (230, 0, 0, 0);
uint32_t g = strip.Color  (0, 180, 0, 0);
uint32_t b = strip.Color  (0, 0, 255, 0);
uint32_t w = strip.Color  (0, 0, 0, 120);
uint32_t wb = strip.Color (0, 0, 0, 175);
//dimmed
uint32_t rd = strip.Color (8, 0, 0, 0);
uint32_t gd = strip.Color (0, 7, 0, 0);
uint32_t bd = strip.Color (0, 0, 5, 0);
uint32_t bdw = strip.Color (0, 0, 10, 0);
uint32_t wd = strip.Color (0, 0, 0, 10);
//off
uint32_t o = strip.Color   (0, 0, 0, 0);
int ledStatus = 0; //color state of leds
int previousledStatus = 0;
int space = 100; //delay for led startup sequence

//headlight circuit
#define headlights 5      //input from radio wire
#define headlightSignal 10 //output wire to MEGA
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

  delay(1000); //startup delay, prevent lightup sequence before engine start

  ledStartup();

  //assign I/O for headlights
  pinMode(headlights, INPUT_PULLUP); 
  pinMode(headlightSignal, OUTPUT); 


  //setup the CANBus module
  if(CAN0.begin(MCP_STDEXT, CAN_500KBPS, MCP_16MHZ) == CAN_OK) Serial.print("MCP2515 Init Okay!!\r\n");
  else Serial.print("MCP2515 Init Failed!!\r\n");
  CAN0.init_Mask(0,0,0x010F0000);                // Init first mask...
  CAN0.init_Filt(0,0,0x140);                // Init first filter...
  CAN0.init_Filt(1,0,0x360);                // Init second filter...
  
  CAN0.setMode(MCP_LISTENONLY);         //listen only mode - cannot transmit messages

  softSerial.begin(9600);
  Serial.begin(9600);
  
  millisStart = millis();
  millisTwo = millis();
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
  getMessage();
  if(millis() >= millisTwo + 1) {
    strip.clear();
    dimmer();
    if (ledStatus != previousledStatus ) {
      strip.show();
    }
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
//
void getMessage (void) {           
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
}

//     _ _
//  __| (_)_ __ ___  _ __ ___   ___ _ __
// / _` | | '_ ` _ \| '_ ` _ \ / _ \ '__|
//| (_| | | | | | | | | | | | |  __/ |
// \__,_|_|_| |_| |_|_| |_| |_|\___|_|
//
void dimmer (void) {

  val = digitalRead(headlights);
  if (val == HIGH) {
    leds();
    digitalWrite(headlightSignal,LOW);
  }
  else {
    ledsDimmed();
    digitalWrite(headlightSignal,HIGH);
  }
}


//  _          _
// | | ___  __| |___
// | |/ _ \/ _` / __|
// | |  __/ (_| \__ \ '/' 
// |_|\___|\__,_|___/  font = ogre
//
void leds(void) {

  if (vehicleRPM > 0 && vehicleRPM < (RPMno + 500) )
  {
    strip.fill(o);
    ledStatus = 0;
  }

  else if (vehicleRPM >= (RPMno + 500) && vehicleRPM < (RPMno + 1000) )
  { //strip.fill(rb);
  strip.setPixelColor(0,rb);
  strip.setPixelColor(1,rb);
  strip.setPixelColor(3,rb);
  strip.setPixelColor(4,rb);
  strip.setPixelColor(5,rb);
  strip.setPixelColor(6,rb);
  strip.setPixelColor(7,rb);
  ledStatus = 1;
  }

  else if (vehicleRPM >= (RPMno + 1000) && vehicleRPM < (RPMno + 1500))
  {strip.fill(b);
  strip.setPixelColor(0,wb);
  strip.setPixelColor(1,wb);
  ledStatus = 3;
  }

  else if (vehicleRPM > 10000)
  {
    strip.fill(o);
    ledStatus = 4;
  }
}

//  _          _        ___ _                              _
// | | ___  __| |___   /   (_)_ __ ___  _ __ ___   ___  __| |
// | |/ _ \/ _` / __| / /\ / | '_ ` _ \| '_ ` _ \ / _ \/ _` |
// | |  __/ (_| \__ \/ /_//| | | | | | | | | | | |  __/ (_| |
// |_|\___|\__,_|___/___,' |_|_| |_| |_|_| |_| |_|\___|\__,_|
//
void ledsDimmed(void)  {

  if (vehicleRPM > 0 && vehicleRPM < (RPMno + 500) )
  {
    strip.fill(o);
    ledStatus = 0;
  }

  else if (vehicleRPM >= (RPMno + 500) && vehicleRPM < (RPMno + 1000) )
  { strip.fill(rd);
  ledStatus = 1;
  }

  else if (vehicleRPM >= (RPMno + 1000) && vehicleRPM < (RPMno + 1500) )
  {strip.setPixelColor(0,wd);
  strip.setPixelColor(1,wd);
  strip.setPixelColor(3,wd);
  strip.setPixelColor(4,wd);
  strip.setPixelColor(5,wd);
  strip.setPixelColor(6,wd);
  strip.setPixelColor(7,wd);
  ledStatus = 3;
  }

  else if (vehicleRPM > 10000)
  {
    strip.fill(o);
    ledStatus = 4;
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
  
  strip.fill(rd);
  strip.show();
  delay(space*5);
  
  strip.clear();
  strip.setPixelColor(0,wd);
  strip.setPixelColor(1,wd);
  strip.setPixelColor(3,wd);
  strip.setPixelColor(4,wd);
  strip.setPixelColor(5,wd);
  strip.setPixelColor(6,wd);
  strip.setPixelColor(7,wd);
  strip.show();
  delay(space*5);
  
  strip.clear();
  strip.fill(rd);
  strip.show();
  delay(space*5);
  
  strip.clear();
  strip.setPixelColor(0,wd);
  strip.setPixelColor(1,wd);
  strip.setPixelColor(3,wd);
  strip.setPixelColor(4,wd);
  strip.setPixelColor(5,wd);
  strip.setPixelColor(6,wd);
  strip.setPixelColor(7,wd);
  strip.show();
  delay(space*5);

  strip.fill(o);
  strip.show();
}
