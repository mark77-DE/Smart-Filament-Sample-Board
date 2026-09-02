// pins.h

// ----------------- NFC-Reader (PN532) SPI-Bus -----------------
#ifndef NFC_SPI_SCK
#define NFC_SPI_SCK 18
#endif
#ifndef NFC_SPI_MISO
#define NFC_SPI_MISO 19
#endif
#ifndef NFC_SPI_MOSI
#define NFC_SPI_MOSI 23
#endif
#ifndef NFC_SPI_CS
#define NFC_SPI_CS 5
#endif


// ----------------- ST7789 SPI-Bus -----------------
#ifndef TFT_SPI_SCK
#define TFT_SPI_SCK -1   // nur bei ST7789-Varianten belegt
#endif
#ifndef TFT_SPI_MOSI
#define TFT_SPI_MOSI -1  // nur bei ST7789-Varianten belegt
#endif
#ifndef TFT_SPI_CS
#define TFT_SPI_CS -1
#endif
#ifndef TFT_SPI_DC
#define TFT_SPI_DC -1
#endif
#ifndef TFT_SPI_RST
#define TFT_SPI_RST -1
#endif
// TFT_SPI_MISO bewusst nicht definiert – ST7789 nutzt kein MISO


// ----------------- I2C -----------------
#ifndef SDA_PIN
#define SDA_PIN 21
#endif
#ifndef SCL_PIN
#define SCL_PIN 22
#endif


// ----------------- Button & Buzzer -----------------
#ifndef BTN_PIN
#define BTN_PIN 27
#endif
#ifndef BUZ_PIN 
#define BUZ_PIN 14
#endif


// ----------------- LEDs -----------------
#ifndef LED_PIN
#define LED_PIN 13
#endif
#ifndef NFC_LED_PIN
#define NFC_LED_PIN 15
#endif

