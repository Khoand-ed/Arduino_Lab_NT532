#include <SPI.h>

// --- ĐỊNH NGHĨA CHÂN NỐI NOKIA 5110 (PCD8544) VỚI ARDUINO UNO ---
// VCC  -> Chân 3.3V của Arduino (KHÔNG CẮM 5V)
// GND  -> Chân GND của Arduino
// SCE  -> Pin 9 (Chip Enable)
// RST  -> Pin 8 (Reset)
// DC   -> Pin 10 (Data / Command)
// MOSI -> Pin 11 (Hardware SPI MOSI)
// SCLK -> Pin 13 (Hardware SPI SCK)
// LED  -> Chân GND (Bật đèn nền) hoặc bỏ trống

const int PIN_RST = 8;
const int PIN_CE  = 9;
const int PIN_DC  = 10;

// Độ phân giải Nokia 5110: 84 cột x 48 hàng
// 84 cột x 6 dải (bank) = 504 bytes
const int FRAME_SIZE = 504;
const uint8_t SYNC_BYTE1 = 0xAA;
const uint8_t SYNC_BYTE2 = 0x55;

uint8_t frameBuffer[FRAME_SIZE];

void lcdCommand(uint8_t cmd) {
  digitalWrite(PIN_DC, LOW);
  digitalWrite(PIN_CE, LOW);
  SPI.transfer(cmd);
  digitalWrite(PIN_CE, HIGH);
}

void lcdInit() {
  pinMode(PIN_RST, OUTPUT);
  pinMode(PIN_CE, OUTPUT);
  pinMode(PIN_DC, OUTPUT);

  // SPI phần cứng chạy ở 4MHz
  SPI.begin();
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));

  // Reset màn hình
  digitalWrite(PIN_RST, LOW);
  delay(10);
  digitalWrite(PIN_RST, HIGH);

  // Khởi tạo PCD8544
  lcdCommand(0x21); // Extended instruction set (H=1)
  lcdCommand(0xBC); // Vop / Contrast (0xB0 - 0xC5)
  lcdCommand(0x04); // Temp coefficient
  lcdCommand(0x14); // Bias 1:48
  lcdCommand(0x20); // Standard instruction set (H=0)
  lcdCommand(0x0C); // Normal display mode (0x0D: Inverted)
}

void lcdDrawFrame(const uint8_t* data) {
  lcdCommand(0x80); // X = 0
  lcdCommand(0x40); // Y = 0

  digitalWrite(PIN_DC, HIGH);
  digitalWrite(PIN_CE, LOW);
  for (int i = 0; i < FRAME_SIZE; i++) {
    SPI.transfer(data[i]);
  }
  digitalWrite(PIN_CE, HIGH);
}

void setup() {
  // 250000 baud cho sai số clock 0.0% trên Uno 16MHz (hoàn hảo, không lỗi khung UART)
  Serial.begin(250000);
  Serial.setTimeout(50); // Timeout ngắn để không bị treo nếu mất kết nối

  lcdInit();

  // Xóa trắng màn hình ban đầu
  memset(frameBuffer, 0, FRAME_SIZE);
  lcdDrawFrame(frameBuffer);

  // Báo cho PC biết Arduino đã sẵn sàng
  Serial.write('R');
}

void loop() {
  // Tìm 2 byte đồng bộ Header: 0xAA, 0x55
  if (Serial.available() > 0) {
    uint8_t b = Serial.read();
    if (b == SYNC_BYTE1) {
      // Chờ byte thứ hai
      unsigned long t0 = millis();
      while (!Serial.available() && (millis() - t0 < 20));
      
      if (Serial.available() && Serial.read() == SYNC_BYTE2) {
        // Đọc liên tục 504 bytes (vừa nhận vừa xả bộ đệm 64 bytes để không bị tràn)
        size_t received = Serial.readBytes((char*)frameBuffer, FRAME_SIZE);
        
        if (received == FRAME_SIZE) {
          lcdDrawFrame(frameBuffer);
          Serial.write('K'); // Gửi ACK báo đã vẽ xong
        } else {
          Serial.write('N'); // NAK nếu thiếu byte
        }
      }
    }
  }
}
