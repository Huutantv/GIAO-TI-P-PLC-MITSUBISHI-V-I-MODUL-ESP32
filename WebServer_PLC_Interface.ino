#include <ModbusMaster.h>        // Thư viện giao tiếp Modbus RTU
#include <HardwareSerial.h>      // Thư viện Serial phần cứng của ESP32
#include <WiFi.h>                // Thư viện WiFi cho ESP32
#include <WebServer.h>           // Thư viện WebServer
#include <SPIFFS.h>              // Thư viện quản lý hệ thống file
#include <ArduinoJson.h>         // Thư viện xử lý JSON

// Cấu hình WiFi Access Point
const char* ssid = "FACTORY_AUTOMATION";      // Tên WiFi
const char* password = "12345678";       // Mật khẩu WiFi (tối thiểu 8 ký tự)
IPAddress local_ip(192, 168, 4, 1);      // Địa chỉ IP 192.168.4.1
IPAddress gateway(192, 168, 4, 1);       // Địa chỉ gateway
IPAddress subnet(255, 255, 255, 0);      // Subnet mask

// Định nghĩa các chân kết nối RS485 trên ESP32
#define RX_PIN 17                // Chân nhận dữ liệu (Kết nối với TX của module RS485)
#define TX_PIN 16                // Chân truyền dữ liệu (Kết nối với RX của module RS485)
#define MAX485_DE_RE 5           // Chân điều khiển trạng thái thu/phát của module RS485

// Thông số truyền thông Modbus
#define SLAVE_ID 1               // Địa chỉ slave = 1
#define MODBUS_BAUD 9600         // Tốc độ truyền (phải khớp với cài đặt trong PLC)

// Khởi tạo đối tượng Modbus master
ModbusMaster modbus;

// Khởi tạo cổng Serial cho giao tiếp RS485
HardwareSerial RS485Serial(1);   // Sử dụng UART1 của ESP32

// Khởi tạo WebServer
WebServer server(80);

// Biến lưu trữ dữ liệu đọc từ PLC
uint16_t registerValues[100];    // Mảng lưu giá trị thanh ghi
bool coilValues[20];            // Mảng lưu giá trị coil
unsigned long lastUpdateTime = 0; // Thời điểm cập nhật cuối cùng
const unsigned long updateInterval = 1000; // Chu kỳ cập nhật (1 giây)

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

// Hàm đọc thanh ghi D từ PLC
bool readRegister(uint16_t address, uint16_t& value) {
  uint8_t result = modbus.readHoldingRegisters(address, 1);
  if (result == modbus.ku8MBSuccess) {
    value = modbus.getResponseBuffer(0);
    return true;
  }
  return false;
}

// Hàm ghi thanh ghi D vào PLC
bool writeRegister(uint16_t address, uint16_t value) {
  uint8_t result = modbus.writeSingleRegister(address, value);
  return (result == modbus.ku8MBSuccess);
}

// Hàm đọc coil M từ PLC
bool readCoil(uint16_t address, bool& value) {
  uint8_t result = modbus.readCoils(address, 1);
  if (result == modbus.ku8MBSuccess) {
    value = modbus.getResponseBuffer(0) & 0x01;
    return true;
  }
  return false;
}

// Hàm ghi coil M vào PLC
bool writeCoil(uint16_t address, bool value) {
  uint8_t result = modbus.writeSingleCoil(address, value ? 0xFF00 : 0x0000);
  
  // Kiểm tra nếu là M0-M10, ghi giá trị vào D500-D510 tương ứng
  if (address >= 0 && address <= 10) {
    // Tính toán địa chỉ D tương ứng: M0->D500, M1->D501, ...
    uint16_t dRegisterAddress = 500 + address;
    uint16_t dValue = value ? 1 : 0;
    
    // Ghi giá trị vào thanh ghi D tương ứng
    modbus.writeSingleRegister(dRegisterAddress, dValue);
    
    // Lưu giá trị vào mảng registerValues để hiển thị
    // Sử dụng chỉ mục 50+address để lưu giá trị D500+
    registerValues[50 + address] = dValue;
  }
  
  return (result == modbus.ku8MBSuccess);
}

// Hàm cập nhật dữ liệu từ PLC
void updateData() {
  // Đọc 10 thanh ghi D từ D100-D109
  for (int i = 0; i < 10; i++) {
    readRegister(100 + i, registerValues[i]);
  }
  
  // Đọc thanh ghi D500-D510
  for (int i = 0; i <= 10; i++) {
    uint16_t dValue;
    if (readRegister(500 + i, dValue)) {
      // Lưu trữ giá trị D500-D510 tại chỉ mục 50-60
      registerValues[50 + i] = dValue;
    }
  }
  
  // Đọc 11 coil M từ M0-M10
  for (int i = 0; i <= 10; i++) {
    readCoil(i, coilValues[i]);
  }
}

// Hàm xử lý yêu cầu từ trang chủ
void handleRoot() {
  String html = R"raw(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Giao diện PLC Mitsubishi FX3U</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      margin: 0;
      padding: 20px;
      background-color: #f5f5f5;
    }
    .container {
      max-width: 800px;
      margin: 0 auto;
      background-color: white;
      padding: 20px;
      border-radius: 8px;
      box-shadow: 0 2px 4px rgba(0,0,0,0.1);
    }
    h1 {
      color: #333;
      text-align: center;
    }
    .section {
      margin-bottom: 20px;
      padding: 15px;
      background-color: #f9f9f9;
      border-radius: 5px;
    }
    .card {
      border: 1px solid #ddd;
      border-radius: 4px;
      padding: 10px;
      margin-bottom: 10px;
      background-color: white;
    }
    .card-title {
      font-weight: bold;
      margin-bottom: 5px;
    }
    label {
      display: inline-block;
      width: 120px;
    }
    input[type="number"] {
      width: 80px;
      padding: 5px;
    }
    input[type="text"] {
      width: 120px;
      padding: 5px;
    }
    button {
      background-color: #4CAF50;
      color: white;
      border: none;
      padding: 8px 16px;
      text-align: center;
      text-decoration: none;
      display: inline-block;
      font-size: 14px;
      margin: 4px 2px;
      cursor: pointer;
      border-radius: 4px;
    }
    .toggle {
      position: relative;
      display: inline-block;
      width: 60px;
      height: 34px;
    }
    .toggle input {
      opacity: 0;
      width: 0;
      height: 0;
    }
    .slider {
      position: absolute;
      cursor: pointer;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background-color: #ccc;
      transition: .4s;
      border-radius: 34px;
    }
    .slider:before {
      position: absolute;
      content: "";
      height: 26px;
      width: 26px;
      left: 4px;
      bottom: 4px;
      background-color: white;
      transition: .4s;
      border-radius: 50%;
    }
    input:checked + .slider {
      background-color: #2196F3;
    }
    input:checked + .slider:before {
      transform: translateX(26px);
    }
    .status {
      color: green;
      font-weight: bold;
    }
    .error {
      color: red;
    }
    table {
      width: 100%;
      border-collapse: collapse;
    }
    table, th, td {
      border: 1px solid #ddd;
      padding: 8px;
      text-align: left;
    }
    th {
      background-color: #f2f2f2;
    }
    tr:nth-child(even) {
      background-color: #f9f9f9;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Giao diện PLC Mitsubishi FX3U</h1>
    <div class="section">
      <h2>Trạng thái kết nối</h2>
      <div id="connection-status">Đang kiểm tra kết nối...</div>
    </div>
    
    <div class="section">
      <h2>Đọc thanh ghi D</h2>
      <div class="card">
        <div class="card-title">Đọc giá trị thanh ghi D</div>
        <label for="read-d-address">Địa chỉ D:</label>
        <input type="number" id="read-d-address" min="0" max="4095" value="100">
        <button onclick="readRegister()">Đọc</button>
        <div id="read-d-result"></div>
      </div>
      
      <h3>Bảng giá trị D100-D109</h3>
      <table id="d-table">
        <tr>
          <th>Thanh ghi</th>
          <th>Giá trị (Dec)</th>
          <th>Giá trị (Hex)</th>
        </tr>
        <tr>
          <td>D100</td>
          <td id="d100-value">--</td>
          <td id="d100-hex">--</td>
        </tr>
        <tr>
          <td>D101</td>
          <td id="d101-value">--</td>
          <td id="d101-hex">--</td>
        </tr>
        <tr>
          <td>D102</td>
          <td id="d102-value">--</td>
          <td id="d102-hex">--</td>
        </tr>
        <tr>
          <td>D103</td>
          <td id="d103-value">--</td>
          <td id="d103-hex">--</td>
        </tr>
        <tr>
          <td>D104</td>
          <td id="d104-value">--</td>
          <td id="d104-hex">--</td>
        </tr>
        <tr>
          <td>D500</td>
          <td id="d500-value">--</td>
          <td id="d500-hex">--</td>
        </tr>
      </table>
    </div>
    
    <div class="section">
      <h2>Ghi thanh ghi D</h2>
      <div class="card">
        <div class="card-title">Ghi giá trị thanh ghi D</div>
        <label for="write-d-address">Địa chỉ D:</label>
        <input type="number" id="write-d-address" min="0" max="4095" value="17">
        <label for="write-d-value">Giá trị:</label>
        <input type="number" id="write-d-value" min="0" max="65535" value="123">
        <button onclick="writeRegister()">Ghi</button>
        <div id="write-d-result"></div>
      </div>
    </div>
    
    <div class="section">
      <h2>Đọc/Ghi Rơle phụ trợ M</h2>
      <div class="card">
        <div class="card-title">Trạng thái M0-M10</div>
        <div id="m-status">
          <div>
            <label>M0:</label>
            <label class="toggle">
              <input type="checkbox" id="m0-toggle" onchange="writeCoil(0, this.checked)">
              <span class="slider"></span>
            </label>
          </div>
          <div>
            <label>M1:</label>
            <label class="toggle">
              <input type="checkbox" id="m1-toggle" onchange="writeCoil(1, this.checked)">
              <span class="slider"></span>
            </label>
          </div>
          <div>
            <label>M2:</label>
            <label class="toggle">
              <input type="checkbox" id="m2-toggle" onchange="writeCoil(2, this.checked)">
              <span class="slider"></span>
            </label>
          </div>
          <div>
            <label>M3:</label>
            <label class="toggle">
              <input type="checkbox" id="m3-toggle" onchange="writeCoil(3, this.checked)">
              <span class="slider"></span>
            </label>
          </div>
          <div>
            <label>M4:</label>
            <label class="toggle">
              <input type="checkbox" id="m4-toggle" onchange="writeCoil(4, this.checked)">
              <span class="slider"></span>
            </label>
          </div>
          <div>
            <label>M5:</label>
            <label class="toggle">
              <input type="checkbox" id="m5-toggle" onchange="writeCoil(5, this.checked)">
              <span class="slider"></span>
            </label>
          </div>
          <div>
            <label>M6:</label>
            <label class="toggle">
              <input type="checkbox" id="m6-toggle" onchange="writeCoil(6, this.checked)">
              <span class="slider"></span>
            </label>
          </div>
          <div>
            <label>M7:</label>
            <label class="toggle">
              <input type="checkbox" id="m7-toggle" onchange="writeCoil(7, this.checked)">
              <span class="slider"></span>
            </label>
          </div>
          <div>
            <label>M8:</label>
            <label class="toggle">
              <input type="checkbox" id="m8-toggle" onchange="writeCoil(8, this.checked)">
              <span class="slider"></span>
            </label>
          </div>
          <div>
            <label>M9:</label>
            <label class="toggle">
              <input type="checkbox" id="m9-toggle" onchange="writeCoil(9, this.checked)">
              <span class="slider"></span>
            </label>
          </div>
          <div>
            <label>M10:</label>
            <label class="toggle">
              <input type="checkbox" id="m10-toggle" onchange="writeCoil(10, this.checked)">
              <span class="slider"></span>
            </label>
          </div>
        </div>
      </div>
      <div class="card">
        <div class="card-title">Điều khiển Rơle M khác</div>
        <label for="write-m-address">Địa chỉ M:</label>
        <input type="number" id="write-m-address" min="0" max="1023" value="47">
        <button onclick="writeCoilOn()">ON</button>
        <button onclick="writeCoilOff()">OFF</button>
        <div id="write-m-result"></div>
      </div>
    </div>
  </div>

  <script>
    // Kiểm tra kết nối với PLC
    function checkConnection() {
      fetch('/api/status')
        .then(response => response.json())
        .then(data => {
          const statusElement = document.getElementById('connection-status');
          if (data.connected) {
            statusElement.innerHTML = '<span class="status">Đã kết nối với PLC</span>';
          } else {
            statusElement.innerHTML = '<span class="error">Không kết nối được với PLC</span>';
          }
        })
        .catch(error => {
          document.getElementById('connection-status').innerHTML = 
            '<span class="error">Lỗi kết nối với ESP32</span>';
        });
    }

    // Đọc giá trị thanh ghi
    function readRegister() {
      const address = document.getElementById('read-d-address').value;
      fetch('/api/register?address=' + address)
        .then(response => response.json())
        .then(data => {
          const resultElement = document.getElementById('read-d-result');
          if (data.success) {
            resultElement.innerHTML = '<span class="status">D' + address + ' = ' + data.value + ' (0x' + data.value.toString(16).toUpperCase() + ')</span>';
          } else {
            resultElement.innerHTML = '<span class="error">Lỗi: ' + data.message + '</span>';
          }
        })
        .catch(error => {
          document.getElementById('read-d-result').innerHTML = 
            '<span class="error">Lỗi kết nối</span>';
        });
    }

    // Ghi giá trị thanh ghi
    function writeRegister() {
      const address = document.getElementById('write-d-address').value;
      const value = document.getElementById('write-d-value').value;
      
      fetch('/api/register', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({ address: parseInt(address), value: parseInt(value) }),
      })
        .then(response => response.json())
        .then(data => {
          const resultElement = document.getElementById('write-d-result');
          if (data.success) {
            resultElement.innerHTML = '<span class="status">Đã ghi D' + address + ' = ' + value + '</span>';
          } else {
            resultElement.innerHTML = '<span class="error">Lỗi: ' + data.message + '</span>';
          }
        })
        .catch(error => {
          document.getElementById('write-d-result').innerHTML = 
            '<span class="error">Lỗi kết nối</span>';
        });
    }

    // Ghi coil ON
    function writeCoilOn() {
      const address = document.getElementById('write-m-address').value;
      writeCoil(address, true);
    }

    // Ghi coil OFF
    function writeCoilOff() {
      const address = document.getElementById('write-m-address').value;
      writeCoil(address, false);
    }

    // Ghi coil
    function writeCoil(address, value) {
      fetch('/api/coil', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({ address: parseInt(address), value: value }),
      })
        .then(response => response.json())
        .then(data => {
          if (address < 11) { // Thay đổi giới hạn từ 10 thành 11 để bao gồm M10
            // Giữ trạng thái của checkbox dựa trên kết quả từ server
            const checkbox = document.getElementById('m' + address + '-toggle');
            if (checkbox) {
              if (data.success) {
                // Giữ nguyên trạng thái đã chọn của checkbox
                checkbox.checked = value;
              } else {
                // Nếu ghi không thành công, đặt checkbox về trạng thái trước đó
                checkbox.checked = !value;
              }
            }
          } else {
            const resultElement = document.getElementById('write-m-result');
            if (data.success) {
              resultElement.innerHTML = '<span class="status">Đã ghi M' + address + ' = ' + (value ? 'ON' : 'OFF') + '</span>';
            } else {
              resultElement.innerHTML = '<span class="error">Lỗi: ' + data.message + '</span>';
            }
          }
        })
        .catch(error => {
          if (address < 11) {
            // Nếu có lỗi kết nối, đặt checkbox về trạng thái trước đó
            const checkbox = document.getElementById('m' + address + '-toggle');
            if (checkbox) {
              checkbox.checked = !value;
            }
          } else {
            document.getElementById('write-m-result').innerHTML = 
              '<span class="error">Lỗi kết nối</span>';
          }
        });
    }

    // Cập nhật dữ liệu từ PLC
    function updateData() {
      fetch('/api/data')
        .then(response => response.json())
        .then(data => {
          if (data.success) {
            // Cập nhật bảng thanh ghi D
            for (let i = 0; i < 5; i++) {
              const d = 100 + i;
              const value = data.registers[i] || 0;
              document.getElementById('d' + d + '-value').textContent = value;
              document.getElementById('d' + d + '-hex').textContent = '0x' + value.toString(16).toUpperCase();
            }
            
            // Cập nhật giá trị D500
            const d500Value = data.registers[50] || 0; // Lấy giá trị D500 từ chỉ mục 50
            document.getElementById('d500-value').textContent = d500Value;
            document.getElementById('d500-hex').textContent = '0x' + d500Value.toString(16).toUpperCase();
            
            // Khởi tạo ban đầu trạng thái coil M nếu chưa tương tác
            // Sau khi người dùng tương tác, không tự động cập nhật nữa
            const coilStatesInitialized = sessionStorage.getItem('coilStatesInitialized');
            
            if (!coilStatesInitialized) {
              // Cập nhật trạng thái coil M ban đầu
              for (let i = 0; i < 11; i++) {
                const checkbox = document.getElementById('m' + i + '-toggle');
                if (checkbox) {
                  checkbox.checked = data.coils[i] || false;
                }
              }
              // Đánh dấu đã khởi tạo để không tự động cập nhật nữa
              sessionStorage.setItem('coilStatesInitialized', 'true');
            }
          }
        })
        .catch(error => {
          console.error('Lỗi cập nhật dữ liệu:', error);
        });
    }

    // Kiểm tra kết nối và cập nhật dữ liệu lần đầu
    checkConnection();
    updateData();
    
    // Cập nhật dữ liệu định kỳ mỗi 2 giây
    setInterval(updateData, 2000);
  </script>
</body>
</html>
)raw";
  server.send(200, "text/html", html);
}

// API trả về trạng thái kết nối
void handleStatus() {
  bool connected = false;
  uint16_t dummy;
  
  // Kiểm tra kết nối bằng cách đọc thanh ghi D100
  if (readRegister(100, dummy)) {
    connected = true;
  }
  
  String response = "{\"connected\":" + String(connected ? "true" : "false") + "}";
  server.send(200, "application/json", response);
}

// API đọc thanh ghi
void handleGetRegister() {
  String addressStr = server.arg("address");
  if (addressStr == "") {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Thiếu địa chỉ thanh ghi\"}");
    return;
  }
  
  uint16_t address = addressStr.toInt();
  uint16_t value;
  
  if (readRegister(address, value)) {
    String response = "{\"success\":true,\"address\":" + String(address) + ",\"value\":" + String(value) + "}";
    server.send(200, "application/json", response);
  } else {
    server.send(500, "application/json", "{\"success\":false,\"message\":\"Lỗi đọc thanh ghi\"}");
  }
}

// API ghi thanh ghi
void handleSetRegister() {
  if (server.hasArg("plain")) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
      server.send(400, "application/json", "{\"success\":false,\"message\":\"Lỗi cú pháp JSON\"}");
      return;
    }
    
    if (!doc.containsKey("address") || !doc.containsKey("value")) {
      server.send(400, "application/json", "{\"success\":false,\"message\":\"Thiếu địa chỉ hoặc giá trị\"}");
      return;
    }
    
    uint16_t address = doc["address"];
    uint16_t value = doc["value"];
    
    if (writeRegister(address, value)) {
      String response = "{\"success\":true,\"address\":" + String(address) + ",\"value\":" + String(value) + "}";
      server.send(200, "application/json", response);
    } else {
      server.send(500, "application/json", "{\"success\":false,\"message\":\"Lỗi ghi thanh ghi\"}");
    }
  } else {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Thiếu dữ liệu\"}");
  }
}

// API ghi coil
void handleSetCoil() {
  if (server.hasArg("plain")) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
      server.send(400, "application/json", "{\"success\":false,\"message\":\"Lỗi cú pháp JSON\"}");
      return;
    }
    
    if (!doc.containsKey("address") || !doc.containsKey("value")) {
      server.send(400, "application/json", "{\"success\":false,\"message\":\"Thiếu địa chỉ hoặc giá trị\"}");
      return;
    }
    
    uint16_t address = doc["address"];
    bool value = doc["value"];
    
    if (writeCoil(address, value)) {
      String response = "{\"success\":true,\"address\":" + String(address) + ",\"value\":" + String(value ? "true" : "false") + "}";
      server.send(200, "application/json", response);
    } else {
      server.send(500, "application/json", "{\"success\":false,\"message\":\"Lỗi ghi coil\"}");
    }
  } else {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Thiếu dữ liệu\"}");
  }
}

// API cập nhật dữ liệu
void handleGetData() {
  String response = "{\"success\":true,\"registers\":[";
  
  // Thêm dữ liệu thanh ghi
  for (int i = 0; i < 10; i++) {
    response += String(registerValues[i]);
    if (i < 9) response += ",";
  }
  
  response += "],\"coils\":[";
  
  // Thêm dữ liệu coil
  for (int i = 0; i < 11; i++) {
    response += coilValues[i] ? "true" : "false";
    if (i < 10) response += ",";
  }
  
  response += "]}";
  
  server.send(200, "application/json", response);
}

void setup() {
  // Khởi tạo Serial để debug
  Serial.begin(115200);
  Serial.println("\n--- ESP32 Web Interface for Mitsubishi FX3U PLC ---");
  
  // Khởi tạo hệ thống file SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Lỗi khởi tạo SPIFFS");
  }
  
  // Cấu hình chân DE/RE của module RS485
  pinMode(MAX485_DE_RE, OUTPUT);
  digitalWrite(MAX485_DE_RE, LOW);  // Ban đầu ở chế độ nhận
  
  // Khởi tạo cổng serial RS485
  RS485Serial.begin(MODBUS_BAUD, SERIAL_8N1, RX_PIN, TX_PIN);
  Serial.println("Đã khởi tạo cổng RS485 với thông số: 9600,N,1");
  
  // Khởi tạo Modbus master
  modbus.begin(SLAVE_ID, RS485Serial);
  Serial.println("Đã khởi tạo Modbus master với địa chỉ slave: " + String(SLAVE_ID));
  
  // Đăng ký hàm callback cho việc điều khiển RS485
  modbus.preTransmission(preTransmission);
  modbus.postTransmission(postTransmission);
  
  // Cấu hình WiFi Access Point
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  delay(100);
  Serial.println("WiFi Access Point đã được tạo");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);
  Serial.print("IP Address: ");
  Serial.println(local_ip);
  
  // Cấu hình Web Server
  server.on("/", handleRoot);
  server.on("/api/status", handleStatus);
  server.on("/api/register", HTTP_GET, handleGetRegister);
  server.on("/api/register", HTTP_POST, handleSetRegister);
  server.on("/api/coil", HTTP_POST, handleSetCoil);
  server.on("/api/data", handleGetData);
  
  // Bắt đầu Web Server
  server.begin();
  Serial.println("Web Server đã bắt đầu");
  
  // Đọc dữ liệu ban đầu từ PLC
  updateData();
}

void loop() {
  // Xử lý yêu cầu từ Web Server
  server.handleClient();
  
  // Cập nhật dữ liệu định kỳ
  unsigned long currentTime = millis();
  if (currentTime - lastUpdateTime > updateInterval) {
    lastUpdateTime = currentTime;
    updateData();
  }
} 
