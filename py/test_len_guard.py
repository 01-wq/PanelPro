# ============================================================
#  一次性测试脚本：验证 ota_protocol.c 的 LEN 上界护栏
#  ⚠️ 用完即删（脚手架寿命：调试代码当年多有用，后来就多碍事）
# ============================================================
#
#  前置条件：板子必须已经在 OTA 循环里
#            （RTT banner 打完【不能】出现 "APP valid, jump to app"）
#
#  测试表（判别依据看 RTT 的 [FRAME] 有没有打印）：
#  ┌──────┬──────────────────────────┬──────────────┐
#  │ LEN  │ RTT 预期                  │ PC 回应      │
#  ├──────┼──────────────────────────┼──────────────┤
#  │ 244  │ [FRAME] CMD:0x02 LEN:244 │ NACK（合法） │
#  │ 245  │ [ERR]，且【无】[FRAME]    │ 无           │
#  │ 300  │ [ERR]，且【无】[FRAME]    │ 无           │
#  └──────┴──────────────────────────┴──────────────┘
#  每帧之后补一发 CMD_VERSION 空帧当"活性探针"：有回应 = 板子还活着
#
#  payload 全部用 00 的两个理由：
#    1. 不含 0xAA —— 护栏丢弃后剩下那 300 字节还会被当新帧扫描，
#       撞上 0xAA 会爆出一堆假的 XOR 错误，把台面搅浑
#    2. XOR8(全00) = 0x00，省事

import serial
import time

PORT = 'COM5'          # ← 和你 ota_tool.py 一致；不确定就用 serial.tools.list_ports 查
BAUD = 9600
TIMEOUT = 1.0

CMD_DATA    = 0x02
CMD_VERSION = 0x10


def wire_time(nbytes):
    """这 n 字节推上线要多久：每字节 10 位（起始+8数据+停止），9600bps"""
    return nbytes * 10 / BAUD


def build_raw_frame(cmd, payload):
    """手工拼帧。
    故意不用 ota_tool.py 的 build_frame —— 那个的 LEN 是从 payload 长度算出来的，
    只会造合法帧。要造"撒谎"的帧必须自己拼。"""
    frame = bytearray()
    frame.append(0xAA)
    frame.append(cmd)
    frame.append(len(payload) & 0xFF)
    frame.append((len(payload) >> 8) & 0xFF)
    frame.extend(payload)

    xor = 0
    for b in payload:
        xor ^= b

    frame.append(xor)
    frame.append(0x55)
    return bytes(frame)


def probe_alive(ser, label):
    """活性探针：发一帧合法的 CMD_VERSION 空帧。
    有回应 = 板子还能干活（阳性证据）。没回应 = 可能卡死了。"""
    ser.reset_input_buffer()
    ser.write(bytes([0xAA, CMD_VERSION, 0x00, 0x00, 0x00, 0x55]))
    time.sleep(0.3)
    reply = ser.read(6)

    if len(reply) == 0:
        print(f'    活性探针：✗ 无回应  ←←← 板子可能已经卡死了')
        return False

    if reply[:2] == bytes([0xAA, 0x81]):
        print(f'    活性探针：✓ NACK {reply.hex(" ")}  （OTA 模式下 magic 无效，本来就回 NACK）')
    else:
        print(f'    活性探针：✓ 收到 {reply.hex(" ")}')
    return True


def main():
    ser = serial.Serial(PORT, BAUD, timeout=TIMEOUT)
    ser.reset_input_buffer()
    print(f'端口 {PORT} 已打开，{BAUD}bps')
    print('确认板子在 OTA 循环里（RTT 没打印 "APP valid"）...\n')

    for length in (244, 245, 300):
        payload = b'\x00' * length
        frame = build_raw_frame(CMD_DATA, payload)

        print(f'[测试] LEN = {length}   整帧 {len(frame)} 字节')

        ser.reset_input_buffer()
        ser.write(frame)
        time.sleep(wire_time(len(frame)) + 0.3)   # 等这帧整个发完再收

        reply = ser.read(6)
        if len(reply) == 0:
            print('    PC 回应：无')
        else:
            print(f'    PC 回应：{reply.hex(" ")}')

        if not probe_alive(ser, f'LEN={length}'):
            print('\n板子已无响应，后面的测试没有意义，停在这里。')
            break

        print()

    ser.close()
    print('测试结束。对照上面的表看 RTT 的 [FRAME] 有没有打印。')


if __name__ == '__main__':
    main()
