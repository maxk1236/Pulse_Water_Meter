// 915MHZ Wireless Pulse Water Meter Sensor
// Stag's Leap Wine Cellars
// Rev. 1.0 -- Max Kilpatrick

#include <EEPROM.h>
#include <TimerOne.h>
#include <RH_RF95.h>

#define NODEID       22  //network ID used for this unit
#define NETWORKID    99  //the network ID we are on
#define GATEWAYID     1  //the node ID we're sending to
#define LED           9
#define INPUTPIN      1  //INT1 = digital pin 3 (must be an interrupt pin!)
#define RFM95_CS 10
#define RFM95_RST 9
#define RFM95_INT 2

#define SERIAL_EN        //uncomment this line to enable serial IO (when you debug Moteino and need serial output)
#define SERIAL_BAUD  57600
#define RF95_FREQ 915.0


RH_RF95 rf95(RFM95_CS, RFM95_INT); // Initialize radio

int ledState = LOW;
volatile unsigned long PulseCounterVolatile = 0; // use volatile for shared variables
unsigned long PulseCounter = 0;
float GAL;
byte PulsesPerGAL = 1;
volatile unsigned long NOW = 0;
unsigned long LASTMINUTEMARK = 0;
unsigned long PULSECOUNTLASTMINUTEMARK = 0; //keeps pulse count at the last minute mark

byte COUNTEREEPROMSLOTS = 10;
unsigned long COUNTERADDRBASE = 8; //address in EEPROM that points to the first possible slot for a counter
unsigned long COUNTERADDR = 0;     //address in EEPROM that points to the latest Counter in EEPROM
byte secondCounter = 0;

unsigned long TIMESTAMP_pulse_prev = 0;
unsigned long TIMESTAMP_pulse_curr = 0;
int GPMthreshold = 8000;         // GPM will reset after this many MS if no pulses are registered
int XMIT_Interval = 5000;        // GPMthreshold should be less than 2*XMIT_Interval
int pulseAVGInterval = 0;
int pulsesPerXMITperiod = 0;
float GPM=0;
char sendBuf[RH_RF95_MAX_MESSAGE_LEN]; // RH_RF95_MAX_MESSAGE_LEN
byte sendLen;
long lastDebounce0 = 0;
long debounceDelay = 1000;                        // Ensure accurate pulse count
void setup() {
  pinMode(LED, OUTPUT);
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  //initialize counter from EEPROM
 unsigned long savedCounter = EEPROM_Read_Counter(); // Unsigned long EEPROM_Read_Counter()
  
  if (savedCounter <=0) savedCounter = 1;            // Avoid division by 0
  PulseCounterVolatile = PulseCounter = PULSECOUNTLASTMINUTEMARK = savedCounter;
  attachInterrupt(INPUTPIN, pulseCounter, RISING);   // Interrupt when rising edge of pulse is sensed
  Timer1.initialize(XMIT_Interval * 1000L);
  Timer1.attachInterrupt(XMIT);
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);
    if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1);
  }
  
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);
  
 
  while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    while (1);
  }
  Serial.println("LoRa radio init OK!");
  rf95.setTxPower(23);                    // Set power of radio
  #ifdef SERIAL_EN                        // Is serial output enabled?
    Serial.begin(SERIAL_BAUD);            // Then set baud rate
    Serial.println("\nTransmitting...");  
  #endif
}

void XMIT(void)
{

  noInterrupts();
  PulseCounter = PulseCounterVolatile;
  interrupts();
  if (millis() - TIMESTAMP_pulse_curr >= 5000) // Change LED state if over 5 seconds
  {  ledState = !ledState; digitalWrite(LED, ledState); }


  // Calculate Gallons counter 
  GAL = ((float)PulseCounter)/PulsesPerGAL;    

  // Print current pulse and gallon count to serial
  Serial.print("PulseCounter:");Serial.print(PulseCounter);Serial.print(", GAL: "); Serial.println(GAL); 
  
  String tempstr = String("");    // tempstr eventually gets sent through radio to reciever
  tempstr += (unsigned long)GAL;  // It contains the flow and volume date in the form of a string
  tempstr += '.';
  tempstr += ((unsigned long)(GAL * 100)) % 100;
  tempstr += ',';

  // Calculate & output GPM
  GPM = pulseAVGInterval > 0 ? 60.0 * 1000 * (1.0/PulsesPerGAL)/(pulseAVGInterval/pulsesPerXMITperiod)
                             : 0;

  pulsesPerXMITperiod = 0;
  pulseAVGInterval = 0;
    
  tempstr += " ";
  tempstr += (int)GPM;
  tempstr += '.';
  tempstr += ((int)(GPM * 100)) % 100;
  tempstr += '!';

  secondCounter += XMIT_Interval/1000;
  // Once per minute, output a GallonsLastMinute count
  // This is not currently needed, keeping for later possible use
  if (secondCounter>=60)
  {
    secondCounter=0;
    //tempstr += " GLM:";
    float GLM = ((float)(PulseCounter - PULSECOUNTLASTMINUTEMARK))/PulsesPerGAL;
//    tempstr += (int)GLM;
//    tempstr += '.';
//    tempstr += ((int)(GLM * 100)) % 100;
    PULSECOUNTLASTMINUTEMARK = PulseCounter;
    EEPROM_Write_Counter(PulseCounter);
  }
  // Make char array from tempstr (Flow and Vol data)
  tempstr.toCharArray(sendBuf, RH_RF95_MAX_MESSAGE_LEN); 
  sendLen = tempstr.length();
  // Send data to reciever and print to serial
  rf95.send(sendBuf, sizeof(sendBuf));
  rf95.waitPacketSent(); 
  Serial.println(tempstr);
  
  
}

void loop() {}

void pulseCounter(void)
{
  if( (millis() - lastDebounce0) > debounceDelay){ // Make sure we aren't double counting pulses
  noInterrupts();
  ledState = !ledState;
  PulseCounterVolatile++;  // increase when LED turns on
  digitalWrite(LED, ledState);
  NOW = millis();
  
  //remember how long between pulses (sliding window)
  TIMESTAMP_pulse_prev = TIMESTAMP_pulse_curr;
  TIMESTAMP_pulse_curr = NOW;
  
  if (TIMESTAMP_pulse_curr - TIMESTAMP_pulse_prev > GPMthreshold)
    //more than 'GPMthreshold' seconds passed since last pulse... resetting GPM
    pulsesPerXMITperiod=pulseAVGInterval=0;
  else
  {
    pulsesPerXMITperiod++;
    pulseAVGInterval += TIMESTAMP_pulse_curr - TIMESTAMP_pulse_prev;
  }
    lastDebounce0 = millis();
  interrupts();
  } 
}


// EEPROM deals with writing to memory, keep in mind there are a limited amount of writes
unsigned long EEPROM_Read_Counter()
{
  return EEPROM_Read_ULong(EEPROM_Read_ULong(COUNTERADDR));
}

void EEPROM_Write_Counter(unsigned long counterNow)
{
  if (counterNow == EEPROM_Read_Counter()) 
  {
    Serial.print("{EEPROM-SKIP(no changes)}");
    return; // Skip write if nothing changed, extends life of memory
  }
  
  Serial.print("{EEPROM-SAVE(");
  Serial.print(EEPROM_Read_ULong(COUNTERADDR));
  Serial.print(")=");
  Serial.print(PulseCounter);
  Serial.print("}");
    
  unsigned long CounterAddr = EEPROM_Read_ULong(COUNTERADDR);
  if (CounterAddr == COUNTERADDRBASE+8*(COUNTEREEPROMSLOTS-1))
    CounterAddr = COUNTERADDRBASE;
  else CounterAddr += 8;
  
  EEPROM_Write_ULong(CounterAddr, counterNow);
  EEPROM_Write_ULong(COUNTERADDR, CounterAddr);
}

unsigned long EEPROM_Read_ULong(int address)
{
  unsigned long temp;
  for (byte i=0; i<8; i++)
    temp = (temp << 8) + EEPROM.read(address++);
  return temp;
}

void EEPROM_Write_ULong(int address, unsigned long data)
{
  for (byte i=0; i<8; i++)
  {
    EEPROM.write(address+7-i, data);
    data = data >> 8;
  }
}
