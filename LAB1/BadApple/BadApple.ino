#include <Wire.h>
#include <string.h>

// =========================================================================
// PHAT BAD APPLE TREN MAN HINH OLED 0.96 INCH (SSD1306 128x64 I2C)
// Board: Arduino Uno (ATmega328P)
// Baudrate: 250000 baud
// =========================================================================

// Dia chi I2C cua man hinh OLED (thuong la 0x3C, mot so it la 0x3D)
#define OLED_ADDR 0x3C

// Do phan giai OLED 0.96": 128 cot x 64 hang
// 128 cot x 8 trang (pages) = 1024 bytes
#define FRAME_SIZE 1024

const uint8_t SYNC_BYTE1 = 0xAA;
const uint8_t SYNC_BYTE2 = 0x55;

uint8_t frameBuffer[FRAME_SIZE];

// Ham gui lenh toi SSD1306
void oledCommand(uint8_t cmd) {
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(0x00); // 0x00: Co = 0, D/C# = 0 (Command)
  Wire.write(cmd);
  Wire.endTransmission();
}

// Khoi tao SSD1306 128x64 khong can thu vien ngoai (tiet kiem RAM & sieu nhanh)
void oledInit() {
  Wire.begin();
  Wire.setClock(800000); // Chay I2C o che do Fast-mode Plus 800kHz (ve 1 frame ~13.7ms)

  oledCommand(0xAE); // Tat hien thi (Display OFF)
  
  oledCommand(0xD5); // Set Display Clock Divide Ratio
  oledCommand(0x80); // Tan so mac dinh
  
  oledCommand(0xA8); // Set Multiplex Ratio
  oledCommand(0x3F); // 64 MUX (cho man 128x64)
  
  oledCommand(0xD3); // Set Display Offset
  oledCommand(0x00);
  
  oledCommand(0x40); // Set Start Line = 0
  
  oledCommand(0x8D); // Charge Pump Setting
  oledCommand(0x14); // Bat Charge Pump (7.5V)
  
  oledCommand(0x20); // Memory Addressing Mode
  oledCommand(0x00); // 0x00: Horizontal Addressing Mode (tu dong xuong dong va sang page)
  
  oledCommand(0xA1); // Segment Re-map: column 127 duoc anh xa vao SEG0 (chuan)
  oledCommand(0xC8); // COM Output Scan Direction: tu tren xuong duoi (remapped)
  
  oledCommand(0xDA); // COM Pins Hardware Configuration
  oledCommand(0x12);
  
  oledCommand(0x81); // Set Contrast Control (Do tuong phan)
  oledCommand(0xCF);
  
  oledCommand(0xD9); // Pre-charge Period
  oledCommand(0xF1);
  
  oledCommand(0xDB); // VCOMH Deselect Level
  oledCommand(0x40);
  
  oledCommand(0xA4); // Output follows RAM content
  oledCommand(0xA6); // Normal display (0xA6: Thuong, 0xA7: Dao mau)
  
  oledCommand(0xAF); // Bat hien thi (Display ON)
}

// Ve toan bo frame 1024 bytes len man hinh OLED
void oledDrawFrame(const uint8_t* data) {
  // Dat vung ghi du lieu: Cot tu 0 -> 127
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(0x00);
  Wire.write(0x21); // Set Column Address
  Wire.write(0x00); // Start column = 0
  Wire.write(0x7F); // End column = 127
  
  // Dat vung ghi du lieu: Trang tu 0 -> 7
  Wire.write(0x22); // Set Page Address
  Wire.write(0x00); // Start page = 0
  Wire.write(0x07); // End page = 7
  Wire.endTransmission();

  // Gui 1024 bytes qua I2C thanh tung goi 16 bytes (phu hop buffer cua Wire)
  for (int i = 0; i < FRAME_SIZE; i += 16) {
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(0x40); // 0x40: Data stream
    Wire.write(&data[i], 16);
    Wire.endTransmission();
  }
}

void setup() {
  // 500000 baud: Sai so clock 0.0% tren Arduino Uno 16MHz (truyen 1 frame chi mat 20.5ms)
  Serial.begin(500000);
  Serial.setTimeout(150); // Tang timeout len 150ms de chong dut frame do do tre dem USB

  oledInit();

  // Xoa man hinh ban dau
  memset(frameBuffer, 0, FRAME_SIZE);
  oledDrawFrame(frameBuffer);

  // Bao cho PC biet Arduino da san sang nhan du lieu
  Serial.write('R');
}

void loop() {
  // Tim 2 byte dong bo Header: 0xAA, 0x55
  if (Serial.available() > 0) {
    uint8_t b = Serial.read();
    if (b == SYNC_BYTE1) {
      // Cho byte dong bo thu hai
      unsigned long t0 = millis();
      while (!Serial.available() && (millis() - t0 < 20));

      if (Serial.available() && Serial.read() == SYNC_BYTE2) {
        // Nhan du 1024 bytes frame video
        size_t received = Serial.readBytes((char*)frameBuffer, FRAME_SIZE);

        if (received == FRAME_SIZE) {
          oledDrawFrame(frameBuffer);
          Serial.write('K'); // Gui ACK bao da ve xong
        } else {
          Serial.write('N'); // NAK neu bi thieu byte do dut truyen
        }
      }
    }
  }
}
