
#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN 3 // Configurable, see typical pin layout above
// #define RST_PIN 9 // Configurable, see typical pin layout above
#define SS_PIN 5  // Configurable, see typical pin layout above


MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

void setup()
{
  Serial.begin(115200); // Initialize serial communications with the PC
  while (!Serial)
    ; // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
  Serial.println("Testing out connecting to SPI ");
  
  SPI.begin(7, 11, 9, 5); // Init SPI bus
  // SPI.begin(); // Init SPI bus
  Serial.println("Testing out connecting to mfrc");
  mfrc522.PCD_Init();                // Init MFRC522
  delay(4);                          // Optional delay. Some board do need more time after init to be ready, see Readme
  // The rust serial reader works around printing this
  mfrc522.PCD_DumpVersionToSerial(); // Show details of PCD - MFRC522 Card Reader details
  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));
}

void loop()
{
  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!mfrc522.PICC_IsNewCardPresent())
  {
    return;
  }

  // // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial())
  {
    return;
  }
  MFRC522::Uid *uid = &mfrc522.uid;

  // mfrc522.PICC_DumpDetailsToSerial

  // Dump debug info about the card; PICC_HaltA() is automatically called
  // mfrc522.PICC_DumpToSerial(&(mfrc522.uid));

  // Serial.print(F("Card UID:"));
  for (byte i = 0; i < uid->size; i++)
  {
    // After F hex wraps around to 10 so print space and 0 to go to new
    if (uid->uidByte[i] < 0x10)
      Serial.print(F(" 0"));
    else
      Serial.print(F(" "));
    // Each byte is two hex values
    Serial.print(uid->uidByte[i], HEX);
  }
  Serial.print("\n");
  // Our cards are basically 4 length long
}