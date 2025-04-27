#include <ModbusMaster.h>        // Thư viện giao tiếp Modbus RTU
#include <HardwareSerial.h>      // Thư viện Serial phần cứng của ESP32

// Định nghĩa các chân kết nối RS485 trên ESP32
#define RX_PIN 17                // Chân nhận dữ liệu (Kết nối với TX của module RS485)
#define TX_PIN 16                // Chân truyền dữ liệu (Kết nối với RX của module RS485)
#define MAX485_DE_RE 5           // Chân điều khiển trạng thái thu/phát của module RS485

// Thông số truyền thông Modbus
#define SLAVE_ID 1               // Địa chỉ slave = 1
#define MODBUS_BAUD 9600         // Tốc độ truyền (phải khớp với cài đặt trong PLC)

/*
Mã chức năng Modbus:
01: Đọc Rơle phụ trợ M (0~1023)
05: Ghi Rơle phụ trợ M (0~1023)
03: Đọc thanh ghi D (0~4095)
06: Ghi thanh ghi D (0~4095)
*/

// Định nghĩa địa chỉ thanh ghi và cuộn trong PLC
#define D100_ADDRESS 100         // Địa chỉ thanh ghi D100 (thực tế là 100, không cần offset)
#define D17_ADDRESS 17           // Địa chỉ thanh ghi D17 (thực tế là 17, không cần offset)

// Khởi tạo đối tượng Modbus master
ModbusMaster modbus;

// Khởi tạo cổng Serial cho giao tiếp RS485
HardwareSerial RS485Serial(1);   // Sử dụng UART1 của ESP32

// Các biến lưu trữ dữ liệu
uint16_t D100_value = 0;         // Biến lưu giá trị đọc được từ thanh ghi D100
unsigned long lastReadTime = 0;   // Thời điểm đọc dữ liệu gần nhất
const unsigned long readInterval = 1000; // Chu kỳ đọc dữ liệu (1 giây)

// Biến cho việc ghi D17
bool D17_written = false;        // Đã ghi D17 chưa

// Hàm điều khiển chân DE/RE trước khi truyền dữ liệu
void preTransmission() {
  digitalWrite(MAX485_DE_RE, HIGH); // Kích hoạt chế độ truyền dữ liệu (DE=1, RE=1)
  delayMicroseconds(100);          // Thời gian chờ 100µs
}

// Hàm điều khiển chân DE/RE sau khi truyền dữ liệu
void postTransmission() {
  delayMicroseconds(100);          // Thời gian chờ 100µs
  digitalWrite(MAX485_DE_RE, LOW); // Kích hoạt chế độ nhận dữ liệu (DE=0, RE=0)
}

void setup() {
  // Khởi tạo Serial để hiển thị thông tin debug
  Serial.begin(115200);
  Serial.println("ESP32 Modbus RTU - Kết nối với PLC Mitsubishi FX3U");
  Serial.println("-----------------------------------------------------");
  Serial.println("Mã chức năng Modbus:");
  Serial.println("01: Đọc Rơle phụ trợ M (0~1023)");
  Serial.println("05: Ghi Rơle phụ trợ M (0~1023)");
  Serial.println("03: Đọc thanh ghi D (0~4095)");
  Serial.println("06: Ghi thanh ghi D (0~4095)");
  Serial.println("-----------------------------------------------------");
  
  // Cấu hình chân DE/RE của module RS485
  pinMode(MAX485_DE_RE, OUTPUT);
  digitalWrite(MAX485_DE_RE, LOW);  // Ban đầu ở chế độ nhận
  
  // Khởi tạo cổng serial RS485
  // Thông số: Baud rate, 8N1 (8 bit dữ liệu, không parity, 1 stop bit) theo đúng cấu hình PLC
  RS485Serial.begin(MODBUS_BAUD, SERIAL_8N1, RX_PIN, TX_PIN);
  Serial.println("Đã khởi tạo cổng RS485 với thông số: 9600,N,1");
  
  // Khởi tạo Modbus master
  modbus.begin(SLAVE_ID, RS485Serial);
  Serial.println("Đã khởi tạo Modbus master với địa chỉ slave: " + String(SLAVE_ID));
  
  // Đăng ký hàm callback cho việc điều khiển RS485
  modbus.preTransmission(preTransmission);
  modbus.postTransmission(postTransmission);
  
  Serial.println("-----------------------------------------------------");
  Serial.print("Địa chỉ D17: ");
  Serial.println(D17_ADDRESS);
  Serial.print("Địa chỉ D100: ");
  Serial.println(D100_ADDRESS);
  Serial.println("-----------------------------------------------------");
  Serial.println("Bắt đầu ghi giá trị 217 vào D17...");
  Serial.println("-----------------------------------------------------");
  delay(1000); // Chờ khởi tạo hoàn tất
}

// Hàm ghi giá trị 217 vào thanh ghi D17 sử dụng mã chức năng 06
void writeD17() {
  Serial.println("Đang ghi giá trị 217 vào thanh ghi D17 sử dụng mã chức năng 06...");
  
  // Thực hiện 3 lần để đảm bảo lệnh được gửi
  for (int i = 0; i < 3; i++) {
    // Sử dụng writeSingleRegister (mã chức năng 06) để ghi giá trị vào thanh ghi
    uint8_t result = modbus.writeSingleRegister(D17_ADDRESS, 123);
    
    if (result == modbus.ku8MBSuccess) {
      Serial.print("Lần ghi thứ ");
      Serial.print(i+1);
      Serial.println(": Thành công - Đã ghi giá trị 217 vào thanh ghi D17");
      break; // Thành công thì dừng lại
    } else {
      Serial.print("Lần ghi thứ ");
      Serial.print(i+1);
      Serial.print(": Thất bại - Mã lỗi: 0x");
      Serial.println(result, HEX);
      
      // Giải thích mã lỗi
      switch (result) {
        case modbus.ku8MBIllegalFunction:
          Serial.println("Lỗi: Chức năng không hợp lệ");
          break;
        case modbus.ku8MBIllegalDataAddress:
          Serial.println("Lỗi: Địa chỉ không hợp lệ (kiểm tra lại D17_ADDRESS)");
          break;
        case modbus.ku8MBIllegalDataValue:
          Serial.println("Lỗi: Giá trị không hợp lệ");
          break;
        case modbus.ku8MBSlaveDeviceFailure:
          Serial.println("Lỗi: Thiết bị slave gặp lỗi");
          break;
        case modbus.ku8MBResponseTimedOut:
          Serial.println("Lỗi: Hết thời gian chờ phản hồi (kiểm tra kết nối vật lý)");
          break;
        default:
          Serial.println("Lỗi không xác định");
      }
      
      if (i < 2) {
        Serial.println("Thử lại sau 100ms...");
        delay(100); // Chờ 100ms trước khi thử lại
      }
    }
  }
  
  // Kiểm tra xác nhận bằng cách đọc lại giá trị D17
  Serial.println("Đang đọc lại giá trị D17 để xác nhận...");
  uint8_t readResult = modbus.readHoldingRegisters(D17_ADDRESS, 1); // Mã chức năng 03 để đọc thanh ghi
  
  if (readResult == modbus.ku8MBSuccess) {
    uint16_t D17_value = modbus.getResponseBuffer(0);
    Serial.print("Giá trị đọc lại từ D17: ");
    Serial.println(D17_value);
    
    if (D17_value == 217) {
      Serial.println("Xác nhận thành công: D17 = 217");
    } else {
      Serial.println("Giá trị D17 không khớp với giá trị đã ghi!");
    }
  } else {
    Serial.print("Không thể đọc lại D17. Mã lỗi: 0x");
    Serial.println(readResult, HEX);
  }
  
  Serial.println("-----------------------------------------------------");
}

void loop() {
  // Ghi giá trị 217 vào D17 nếu chưa ghi
  if (!D17_written) {
    writeD17();
    D17_written = true;
  }
  
  // Đọc dữ liệu định kỳ theo chu kỳ readInterval
  if (millis() - lastReadTime > readInterval) {
    lastReadTime = millis();
    
    // Hiển thị thông tin trạng thái kết nối
    Serial.println("Đang đọc giá trị D100 từ PLC sử dụng mã chức năng 03...");
    
    // Thực hiện đọc thanh ghi D100 sử dụng mã chức năng 03
    uint8_t result = modbus.readHoldingRegisters(D100_ADDRESS, 1);
    
    // Kiểm tra kết quả đọc
    if (result == modbus.ku8MBSuccess) {
      // Đọc thành công, lấy giá trị từ bộ đệm phản hồi
      D100_value = modbus.getResponseBuffer(0);
      
      // Hiển thị giá trị đọc được
      Serial.print("Giá trị thanh ghi D100: ");
      Serial.println(D100_value);
      
      // Hiển thị dạng HEX và BIN cho phân tích dễ dàng hơn
      Serial.print("Giá trị HEX: 0x");
      Serial.println(D100_value, HEX);
      
      Serial.print("Giá trị BIN: ");
      Serial.println(D100_value, BIN);
    } else {
      // Đọc thất bại, hiển thị mã lỗi
      Serial.print("Đọc thất bại với mã lỗi: 0x");
      Serial.println(result, HEX);
      
      // Giải thích mã lỗi
      switch (result) {
        case modbus.ku8MBIllegalFunction:
          Serial.println("Lỗi: Chức năng không hợp lệ");
          break;
        case modbus.ku8MBIllegalDataAddress:
          Serial.println("Lỗi: Địa chỉ không hợp lệ (kiểm tra lại D100_ADDRESS)");
          break;
        case modbus.ku8MBIllegalDataValue:
          Serial.println("Lỗi: Giá trị dữ liệu không hợp lệ");
          break;
        case modbus.ku8MBSlaveDeviceFailure:
          Serial.println("Lỗi: Thiết bị slave gặp lỗi");
          break;
        case modbus.ku8MBInvalidSlaveID:
          Serial.println("Lỗi: ID slave không hợp lệ");
          break;
        case modbus.ku8MBInvalidFunction:
          Serial.println("Lỗi: Chức năng không hợp lệ");
          break;
        case modbus.ku8MBResponseTimedOut:
          Serial.println("Lỗi: Hết thời gian chờ phản hồi (kiểm tra kết nối vật lý)");
          break;
        case modbus.ku8MBInvalidCRC:
          Serial.println("Lỗi: CRC không hợp lệ");
          break;
        default:
          Serial.println("Lỗi không xác định");
      }
    }
    
    Serial.println("-----------------------------------------------------");
  }
}
