# Truyền Thông PLC Mitsubishi FX3U và ESP32 

## Giới thiệu
Dự án này giúp kết nối ESP32 với PLC Mitsubishi FX3U thông qua giao thức Modbus RTU qua RS485. Với chương trình này, bạn có thể:
- Ghi dữ liệu vào thanh ghi D của PLC
- Đọc giá trị từ thanh ghi D của PLC
- Theo dõi quá trình truyền thông qua Serial Monitor

## Tác giả
Dự án được phát triển bởi **FACTORY AUTOMATION**

Liên hệ: 
- Email: [huutantv.ng@gmail.com]
- Website: [tvtechthuduc.com]

## Phần cứng cần thiết
- ESP32 WiFi/Bluetooth - Modbus RTU RS485 ISOLATED MINI
- PLC Mitsubishi FX3U đã cấu hình Modbus RTU
- Kết nối vật lý RS485 giữa ESP32 và PLC

## Kết nối phần cứng
- **ESP32 <-> Module RS485**:
  - RX (ESP32 chân 17) -> TX (Module RS485)
  - TX (ESP32 chân 16) -> RX (Module RS485) 
  - DE/RE (ESP32 chân 5) -> DE/RE (Module RS485)
  - VCC -> 3.3V hoặc 5V (tùy module)
  - GND -> GND

- **RS485 <-> PLC Mitsubishi FX3U**:
  - A+ (RS485) -> A+ (PLC)
  - B- (RS485) -> B- (PLC)
  - GND (RS485) -> GND (PLC) (Quan trọng để đảm bảo tham chiếu điện áp chung)

## Cấu hình PLC
PLC Mitsubishi FX3U cần được cấu hình Modbus RTU trước khi giao tiếp:
- Đặt D8120 = H4081 (9600 baud, N, 1 stop bit)
- Đặt D8121 = Địa chỉ slave (mặc định là 1)
- Cấu hình D8129 = Timeout nếu cần

## Cài đặt phần mềm
1. Mở Arduino IDE
2. Cài đặt thư viện: Sketch -> Include Library -> Manage Libraries...
   - Tìm và cài đặt thư viện "ModbusMaster" của tác giả Doc Walker
3. Mở file sketch_apr16a.ino và tải lên ESP32

## Cấu hình chương trình
File sketch_apr16a.ino có các tham số chính cần kiểm tra/điều chỉnh:

```cpp
// Định nghĩa các chân kết nối RS485 trên ESP32
#define RX_PIN 17                // Chân nhận dữ liệu (Kết nối với TX của module RS485)
#define TX_PIN 16                // Chân truyền dữ liệu (Kết nối với RX của module RS485)
#define MAX485_DE_RE 5           // Chân điều khiển trạng thái thu/phát của module RS485

// Thông số truyền thông Modbus
#define SLAVE_ID 1               // Địa chỉ slave = 1
#define MODBUS_BAUD 9600         // Tốc độ truyền (phải khớp với cài đặt trong PLC)

// Định nghĩa địa chỉ thanh ghi trong PLC
#define D100_ADDRESS 100         // Địa chỉ thanh ghi D100
#define D17_ADDRESS 17           // Địa chỉ thanh ghi D17
```

## Mã chức năng Modbus
Chương trình sử dụng các mã chức năng Modbus tiêu chuẩn:
- 01: Đọc Rơle phụ trợ M (0~1023)
- 05: Ghi Rơle phụ trợ M (0~1023)
- 03: Đọc thanh ghi D (0~4095)
- 06: Ghi thanh ghi D (0~4095)

## Phạm vi địa chỉ Modbus trên PLC Mitsubishi FX3U
```
COIL_S_START_ADDR                 0
COIL_S_END_ADDR                   999

COIL_Y_START_ADDR                 1280
COIL_Y_END_ADDR                   1463

COIL_T_START_ADDR                 1536
COIL_T_END_ADDR                   1791

COIL_M_START_ADDR                 2048
COIL_M_END_ADDR                   3580

COIL_C_START_ADDR                 3584
COIL_C_END_ADDR                   3839

COIL_M8000_START_ADDR             3840
COIL_M8000_END_ADDR               4095

COIL_M1538_START_ADDR             10240
COIL_M1538_END_ADDR               11775

INPUT_X_START_ADDR                1024
INPUT_X_END_ADDR                  1215

REG_C_START_ADDR                  1280
REG_C_END_ADDR                    1535

REG_C200_START_ADDR               1536
REG_C200_END_ADDR                 1646

REG_D8000_START_ADDR              1792
REG_D8000_END_ADDR                2047

REG_T_START_ADDR                  2048
REG_T_END_ADDR                    2304

REG_D_START_ADDR                  4096
REG_D_END_ADDR                    12096
```

Lưu ý: Trong ví dụ này, chúng ta sử dụng địa chỉ D không có offset (ví dụ: D17 = 17 thay vì D17 = 4096+17) vì phụ thuộc vào cấu hình PLC và cài đặt Modbus RTU cụ thể.

## Chức năng chương trình
Chương trình hiện tại thực hiện 2 chức năng chính:

1. **Ghi giá trị 123 vào thanh ghi D17**:
   - Sử dụng mã chức năng 06 (writeSingleRegister)
   - Ghi một lần khi khởi động ESP32
   - Thử lại tối đa 3 lần nếu gặp lỗi
   - Đọc lại để xác nhận giá trị đã ghi

2. **Đọc giá trị từ thanh ghi D100 mỗi giây**:
   - Sử dụng mã chức năng 03 (readHoldingRegisters)
   - Hiển thị giá trị dạng thập phân, HEX và nhị phân
   - Báo cáo lỗi nếu có

## Theo dõi và xử lý sự cố
1. Mở Serial Monitor với tốc độ 115200 baud để theo dõi quá trình giao tiếp
2. Kiểm tra thông báo lỗi:
   - Lỗi chức năng không hợp lệ: Kiểm tra mã chức năng và cấu hình PLC
   - Lỗi địa chỉ không hợp lệ: Kiểm tra phạm vi địa chỉ PLC
   - Lỗi timeout: Kiểm tra kết nối vật lý RS485

3. Nếu không nhận được phản hồi, kiểm tra:
   - Dây A+/B- có bị đảo ngược không
   - Kết nối GND giữa ESP32 và PLC
   - Cấu hình truyền thông trên PLC (baud rate, parity, stop bit)
   - Địa chỉ slave ID

## Mở rộng
Bạn có thể dễ dàng mở rộng chương trình này để:
- Ghi/đọc nhiều thanh ghi D khác
- Ghi/đọc cuộn M (relay phụ trợ)
- Thêm chức năng điều khiển từ xa qua WiFi
- Hiển thị dữ liệu trên màn hình OLED/LCD
- Lưu dữ liệu vào thẻ nhớ hoặc gửi lên cloud

## Các vấn đề thường gặp
1. **Không kết nối được với PLC**:
   - Kiểm tra cấu hình truyền thông (baud rate, parity, stop bit)
   - Kiểm tra địa chỉ slave của PLC
   - Kiểm tra kết nối vật lý RS485 (A+/B-, GND)

2. **Nhận được lỗi địa chỉ không hợp lệ**:
   - Kiểm tra lại địa chỉ phù hợp với cấu hình PLC
   - Với một số model PLC, cần thêm offset cho địa chỉ

3. **Đọc/ghi không ổn định**:
   - Thêm trở kháng thiết lập (terminating resistor) 120 ohm
   - Kiểm tra nguồn tín hiệu và khoảng cách truyền
   - Tăng thời gian chờ delayMicroseconds trong hàm pre/post Transmission

## Tài liệu tham khảo
- [Thư viện ModbusMaster](https://github.com/4-20ma/ModbusMaster)
- [Tài liệu PLC Mitsubishi FX3U](https://dl.mitsubishielectric.com/dl/fa/document/manual/plc_fx/jy997d16601/jy997d16601m.pdf)
- [ESP32 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf) 

