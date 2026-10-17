#include <Arduino.h>
#include <MKRWAN.h>

/*Code for transmitter*/

LoRaModem modem;

/* ABP credentials - same NwkSKey/AppSKey on both boards
  These are test-only keys, don't use in production  */
const char* devAddr = "AABBCCDD";
const char* nwkSKey = "00000000000000000000000000000001";
const char* appSKey = "00000000000000000000000000000001";

// put function declarations here:
int myFunction(int, int);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!Serial);

  Serial.println("Initialising modem...");
  if (!modem.begin(AU915)) {
    Serial.println("Modem initialisation failed - check wiring");
    while (true);
  }

  Serial.print("Module version: ");
  Serial.println(modem.version());

  /*Join using ABP - no gateway needed*/
  if (!modem.joinABP(devAddr, nwkSKey, appSKey)) {
      Serial.println("ABP join failed");
      while (true);
  }

  /* DR3 = SF9/BW125 - good balance of range and speed for AU195*/
  modem.dataRate(3);
  modem.setPort(1);   /*LoRaWAN PORT - keep consistent*/
 // modem.noConfirm();  /* unconfirmed uplink (no ACK from gateway)*/

  Serial.println("Ready - transmitting every 10s");
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("Sending...");
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}