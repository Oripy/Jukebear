/* Jukebear: A jukebox controlled by RFID cards

Adafruit Feather 328P
RC522 module
YX5300 module
Amplifier

PINS:
Feather
  2 ->   Button"+" + Resistor -> GND
 3V ->   Button"-"

Feather    RC522  
  3V   ->  3.3V
  GND  ->  GND
  SCK  ->  SCK
  MOSI ->  MOSI
  MISO ->  MISO
  9    ->  RST
  10   ->  SDA

Feather  XY5300 module
  3V   ->  VCC
  GND  ->  GND
  4    ->  TX
  5    ->  RX      */

#include <Arduino.h>
#include <MD_YX5300.h>
#include <SoftwareSerial.h>

#define DEBUG true

#if DEBUG
  #define PRINT(s,v)    { Serial.print(F(s)); Serial.print(v); }
  #define PRINTX(s,v)   { Serial.print(F(s)); Serial.print(v, HEX); }
  #define PRINTS(s)     { Serial.print(F(s)); }
#else
  #define PRINT(s,v)
  #define PRINTX(s,v)
  #define PRINTS(s)
#endif

const uint8_t ARDUINO_RX = 4;    // connect to TX of MP3 Player module
const uint8_t ARDUINO_TX = 5;    // connect to RX of MP3 Player module

bool sleeping = true;

int folderNum = 1;
int fileNum = 1;
bool flip = true;

SoftwareSerial MP3Stream(ARDUINO_RX, ARDUINO_TX);
MD_YX5300 mp3(MP3Stream);

void putToSleep() {
  PRINTS("Sleeping\n");
  mp3.sleep();
  while (mp3.getStatus()->code == MD_YX5300::STS_TIMEOUT) {
    mp3.sleep();
  }
  sleeping = true;
}

void cbResponse(const MD_YX5300::cbData *status)
// Used to process device responses either as a library callback function
// or called locally when not in callback mode.
{
  switch (status->code)
  {
  case MD_YX5300::STS_OK:         PRINTS("STS_OK");         break;
  case MD_YX5300::STS_TIMEOUT:    PRINTS("STS_TIMEOUT");    break;
  case MD_YX5300::STS_VERSION:    PRINTS("STS_VERSION");    break;
  case MD_YX5300::STS_CHECKSUM:   PRINTS("STS_CHECKSUM");    break;
  case MD_YX5300::STS_TF_INSERT:  PRINTS("STS_TF_INSERT");  break;
  case MD_YX5300::STS_TF_REMOVE:  PRINTS("STS_TF_REMOVE");  break;
  case MD_YX5300::STS_ERR_FILE:   
    PRINTS("STS_ERR_FILE\n");
    PRINTS("File Not found so playing Stop\n");
    mp3.playStop();
    putToSleep();
    break;
  case MD_YX5300::STS_ACK_OK:     PRINTS("STS_ACK_OK");     break;
  case MD_YX5300::STS_FILE_END:
    PRINTS("STS_FILE_END\n");
    if (flip) {
      flip = false;
    } else {
      flip = true;
      PRINTS("Play next song of folder\n");
      fileNum++;
      PRINT("Playing folder ", folderNum);
      PRINT("\nPlaying file ", fileNum);
      PRINTS("\n");
      mp3.playSpecific(folderNum, fileNum);
    }
    break;
  case MD_YX5300::STS_INIT:       PRINTS("STS_INIT");       break;
  case MD_YX5300::STS_STATUS:     PRINTS("STS_STATUS");     break;
  case MD_YX5300::STS_EQUALIZER:  PRINTS("STS_EQUALIZER");  break;
  case MD_YX5300::STS_VOLUME:     PRINTS("STS_VOLUME");     break;
  case MD_YX5300::STS_TOT_FILES:  PRINTS("STS_TOT_FILES");  break;
  case MD_YX5300::STS_PLAYING:    PRINTS("STS_PLAYING");    break;
  case MD_YX5300::STS_FLDR_FILES: PRINTS("STS_FLDR_FILES"); break;
  case MD_YX5300::STS_TOT_FLDR:   PRINTS("STS_TOT_FLDR");   break;
  default: PRINTX("STS_??? 0x", status->code); break;
  }

  PRINTX(", 0x", status->data);
  PRINTS("\n");
}

#include <SPI.h>
#include <MFRC522.h>
#define RST_PIN		9		
#define SS_PIN		10	
MFRC522 mfrc522(SS_PIN, RST_PIN);
byte nuidPICC[4];

/**
 * Helper routine to dump a byte array as dec values to Serial.
 */
void printDec(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}

uint8_t getSecondByte(byte *buffer) {
  return buffer[1];
}

void setup() {
  #if DEBUG
    Serial.begin(9600);
  #endif
  MP3Stream.begin(MD_YX5300::SERIAL_BPS);
  mp3.begin();
  mp3.setSynchronous(true);
  mp3.setCallback(cbResponse);
  mp3.setTimeout(1000);

  PRINTS("Set Volume to Max\n");
  mp3.volume(mp3.volumeMax());
  mp3.sleep();

  SPI.begin();
  mfrc522.PCD_Init();
}

void loop() {
  if (!sleeping) {
    mp3.check();
  }

  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!mfrc522.PICC_IsNewCardPresent())
    return;

  // Verify if the NUID has been readed
  if (!mfrc522.PICC_ReadCardSerial())
    return;

  if (mfrc522.uid.uidByte[0] != nuidPICC[0] || 
    mfrc522.uid.uidByte[1] != nuidPICC[1] || 
    mfrc522.uid.uidByte[2] != nuidPICC[2] || 
    mfrc522.uid.uidByte[3] != nuidPICC[3] ) {
    PRINTS("A new card has been detected\n");

    // Store NUID into nuidPICC array
    for (byte i = 0; i < 4; i++) {
      nuidPICC[i] = mfrc522.uid.uidByte[i];
    }

    folderNum = getSecondByte(mfrc522.uid.uidByte) % 100;

    if (sleeping) {
      PRINTS("Waking up\n");
      mp3.wakeUp();
      while (mp3.getStatus()->code == MD_YX5300::STS_TIMEOUT) {
        mp3.wakeUp();
      }
      sleeping = false;
    }
    fileNum = 1;
    PRINT("Playing folder ", folderNum);
    PRINT("\nPlaying file ", fileNum);
    PRINTS("\n");
    mp3.playSpecific(folderNum, fileNum);
  } else {
    PRINTS("Card read previously\n");
  }
  // Halt PICC
  mfrc522.PICC_HaltA();

  // Stop encryption on PCD
  mfrc522.PCD_StopCrypto1();
}