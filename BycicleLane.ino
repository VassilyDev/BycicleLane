// Bycicle lane eco-tutor by Alessandro Felicetti (aka Vassily98).
// Hackster project "Make sense of our proximity sensor" by KEMET.

// Include libraries. We are using EEPROM library from avr to read bigger integers.
#include <gprs.h>
#include <SoftwareSerial.h>
#include <dht11.h>
#include <avr/eeprom.h>

// Declare pin mapping and variables.
#define APN "TM"                    //<--- EDIT THIS
#define API_KEY "DH************QF"  //<--- EDIT THIS
#define DHT11_PIN 2
#define INFRA_PIN 3
#define INFRA_LED 4
#define RAIN_PIN A0
#define LA12_PIN A1

unsigned long oldMs = 0;
GPRS gprs;
dht11 DHT;
int temp = 0;
int hum = 0;
int rain = 0;
int mAmp = 0;
int transits = 0;

// Define reset function.
void(* resetFunc) (void) = 0; 

void setup() {
  // Handle board setup.
  Serial.begin(9600);
  //while(!Serial);
  setupGSM();
  setupEEPROM();
  oldMs = millis();
  pinMode(INFRA_PIN, INPUT);
  pinMode(INFRA_LED, OUTPUT);
  Serial.println("Setup completed"); 
}

void loop() {    
  // Every 60 seconds read sensors and send everything to cloud.
  if(millis() > (oldMs + 60000)){
    readSensors();
    sendValues();
    oldMs = millis();
  }
  // Reset every 30 minutes. That helps to keep the connection open and prevents oldMs to overflow.
  if(oldMs > 1800000){
    resetFunc();
  }
  readINFRA();
  delay(50);
}


void setupGSM(){
  // Setup GSM and print out IP address
  Serial.println("GPRS - HTTP Connection Test...");  
  gprs.preInit();
  delay(2000);
  while(0 != gprs.init()) {
     delay(1000);
     Serial.println("init error");
  }  
  while(!gprs.join(APN)) {
      Serial.println("gprs join network error");
      delay(2000);
  }
  Serial.print("IP Address is ");
  Serial.println(gprs.getIPAddress());
  Serial.println("GPRS OK");
}

void setupEEPROM(){
  uint32_t readed = eeprom_read_dword((uint32_t)1);
  if(readed == 4294967295){
    // Handle first init
    eeprom_write_dword((uint32_t)1, (uint32_t)0);
    eeprom_write_dword((uint32_t)2, (uint32_t)0);
    transits = 0;
    Serial.println("EEPROM OK, FIRST INIT");
  }else{
    transits = (int)eeprom_read_dword((uint32_t)2);
    Serial.println("EEPROM OK");
  }
}

void readSensors(){
  // Read sensors
  DHT.read(DHT11_PIN);
  temp = DHT.temperature;
  hum = DHT.humidity;
  rain = analogRead(RAIN_PIN);
  int RawValue = analogRead(LA12_PIN);
  int voltage = (RawValue / 1024.0) * 5000;
  float amp = (((float)voltage - 2500.00) / 200.00);
  mAmp = amp * 1000;
  if(mAmp < 0.1){
    mAmp = 0;
  }
}

void readINFRA(){
  // Check if KEMET sensor triggered.
  if(digitalRead(INFRA_PIN) == 1){
    digitalWrite(INFRA_LED, HIGH);
    transits++;
    delay(2000);
    digitalWrite(INFRA_LED, LOW);
  }
}

void sendValues(){
  // We are saving EEPROM here because it has only 100k write/erase cycles lifespan
  eeprom_write_dword((uint32_t)2, (uint32_t)transits);
  // Prepare buffers
  char buf[512]; 
  char resp[16];
  // Send data and close TCP connection
  sprintf(buf, "GET /update.json?api_key=%s&field1=%d&field2=%d&field3=%d&field4=%d&field5=%d HTTP/1.0\r\n\r\n", API_KEY, temp, hum, rain, mAmp, transits);
  gprs.connectTCP("api.thingspeak.com", 80);
  gprs.sendTCPData(buf);
  gprs.closeTCP();
  Serial.println("Data sent");
}
