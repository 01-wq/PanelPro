import serial, time

PORT = 'COM5'          # 和你 ota_tool.py 一致，先确认板子还在这个口
ser = serial.Serial(PORT, 9600, timeout=1.0)

# ---- 第 1 帧：故意撒谎的 LEN = 300 ----
payload = b'\x00' * 300                 # 全 00，见下面问题 ①
frame = bytearray([0xAA, 0x02, 0x2C, 0x01])   # AA / CMD_DATA /LEN=0x012C(300)
frame.extend(payload)
xor = 0
for b in payload: xor ^= b
frame.append(xor)                       # XOR8
frame.append(0x55)                      # 帧尾

print(f'发第 1 帧：{len(frame)} 字节')
ser.write(frame)
time.sleep(0.6)                         # ← 304 字节上线要0.317s，必须等它发完

# ---- 第 2 帧：合法空帧，验证板子还活着 ----
ser.reset_input_buffer()
ser.write(bytes([0xAA, 0x10, 0x00, 0x00, 0x00, 0x55]))   #CMD_VERSION，空负载
time.sleep(0.3)
print('第 2 帧收到的回应：', ser.read(10).hex(' '))