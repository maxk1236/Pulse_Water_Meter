// 915MHZ Wireless Sensor Reciever
// Stag's Leap Wine Cellars
// Rev. 1.0 -- Max Kilpatrick

#define MY_GATEWAY_SERIAL                // MyMessage is sent through serial
#define NODE_ID 1                         // Which node is this

#include <MySensors.h>
#include <SPI.h>
#include <RH_RF95.h>

MyMessage msg(NODE_ID, V_FLOW);           // Set's up mesaaging protocol to reciever, lets it know what variables are used
MyMessage vmsg(NODE_ID,V_VOLUME);         // V_FLOW means that the data being sent is a flowrate, etc.
RH_RF95 rf95;                             // Initialize radio
String str;                               // Strings for parsing data
String str1; 
String str2;
int led = 9;                              // Location of LED
char array[20];                           // Array for parsing data
float vol;                                // Volume data stored here
float flow;                               // Flow rate data stored here
void setup()                              // Setup serial output, etc.
{
    pinMode(led, OUTPUT);                 // Make LED blink
    Serial.begin(115200);                 // Sets baud rate
    while (!Serial) ;                     // Wait for serial port to be available
  if (!rf95.init())                       // Checks if radio is working
    Serial.println("init failed"); 
}

void presentation()                       // Tell controller how to handle input
{
  sendSketchInfo("Water Meter", "1.1");   // Tells controller (domoticz) name of program
  present(NODE_ID, S_WATER);              // Tells controller which node data is coming from, as well as how to view the data
}

void loop()
{
  String str1 = "";                       // Strings for parsing data from sensor
  String str2 = "";
  if (rf95.available())                   // When input from radio comes in
  {
    // Should be a message for us now   
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN]; // Load incoming radio data to buffer
    uint8_t len = sizeof(buf);            // Determine size of buffer
    char array[len];                      // Array for parsing incoming data
    strcpy(array, (char*)buf);            // Copy buffer into array
    for(int i = 0; array[i] != 44; i++){  // Copy data from array into a string until a comma is reached
    str1 += array[i];                     // This string has the data for the total gallons used
    }
    int str_len = str1.length() + 1;      // Determine size of first string (where to start reading next data)
    for(int i = str_len + 1; array[i] != 33; i++){ // Copy data from array into another string until end of flow data (denote end of data with !)
    str2 += array[i];                     // This string has data for current flow rate
    }
    float vol = str1.toFloat();           // Converts data from string to floating point number
    send(vmsg.set(vol, 2));               // Send volume data to controller with 2 decimal points
    float flow = str2.toFloat();
    send(msg.set(flow, 2));
//    Serial.println(flow);         <-----// Prints flow and vol to serial: uncomment for debugging purposes
//    Serial.println(vol);

    if (rf95.recv(buf, &len))
    {
     digitalWrite(led, HIGH);             // Turns on LED
//      RH_RF95::printBuffer("request: ", buf, len); // Acknowledge request from reciever, not necessary, but leaving for futur use
//      Serial.print("got request: ");
//      Serial.println((char*)buf);       // Prints what is in buffer
//      Serial.print("RSSI: ");           // Signal strength, uncomment for testing (along with line below)
//      Serial.println(rf95.lastRssi(), DEC);
      
      // Send a reply
      uint8_t data[] = "Yo, I got the data."; //Sends reply to sensor, leaving for testing purposes
      rf95.send(data, sizeof(data));
      rf95.waitPacketSent();
      Serial.println("Sent a reply");
       digitalWrite(led, LOW);             // Turns off the LED
    }
    else
    {
      Serial.println("recv failed");       // Lets us know if not recieving messages
    }
  }
}

