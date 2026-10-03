# ============================================================
#  一次性测试脚本：人为制造丢帧 / 丢 ACK，验证重传机制
#  ⚠️ 用完即删（脚手架寿命）
# ============================================================
#
#  原理：把 serial 对象包一层（LossySerial），在指定的那一块上做手脚。
#        ota_tool.py 一行都不改 —— 测试代码不污染正式工具。
#
#  两种模式（改下面 MODE 那一行切换）：
#
#  ┌──────────────┬────────────────────────────────────────────────────┐
#  │ drop_frame   │ 把第 50 帧的【帧尾 0x55 改成 0x00】                │
#  │ 帧丢了       │ → 解析器在 WAIT_TRAILER 判失败，静默丢弃（不打印） │
#  │              │ → 板子【不回话】→ PC 超时 → 重发 → 这次好好的 ✅   │
#  │              │ 验的是：【重发循环】本身                            │
#  ├──────────────┼────────────────────────────────────────────────────┤
#  │ drop_ack     │ 板子正常收下并写了第 50 块、正常回了 ACK           │
#  │ ACK 丢了     │ → 但我们【把这个 ACK 吞掉】                        │
#  │              │ → PC 超时 → 重发第 50 块 → 板子判为【重复帧】      │
#  │              │ → 不写、不加、回 ACK ✅                            │
#  │              │ 验的是：【幂等】——重复帧不破坏数据                  │
#  └──────────────┴────────────────────────────────────────────────────┘
#
#  【关键证据怎么看】
#    drop_frame → RTT 里第 50 块只出现【1 次】[FRAME]
#    drop_ack   → RTT 里第 50 块出现【2 次】[FRAME]，但最终 CMD_END 仍然 ACK
#                 （如果重复帧被错当成新块写进去，数据会错位 → CMD_END 必然 NACK）
#    两种模式  → PC 都会打印 "! 第 50块: 超时... 重发 1/3" 和 "（本次共重传 N 次）"

import serial
import ota_tool

PORT       = 'COM5'
MODE       = 'drop_ack'   # ← 改成 'drop_ack' 跑第二种
AT_BLOCK   = 50             # 在第几块做手脚（别选第 1 块或最后一块）


class LossySerial:
    """包装真实串口，制造一次故障。只拦截，不改变正常路径的任何行为。"""

    def __init__(self, real, mode, at_block):
        self.real     = real
        self.mode     = mode
        self.at_block = at_block
        self.fired    = False    # 只做一次手脚
        self.last_blk = 0        # 最近一次发出去的 CMD_DATA 是第几块

    # ota_tool 会设 ser.timeout —— 透传给真串口
    @property
    def timeout(self):
        return self.real.timeout

    @timeout.setter
    def timeout(self, v):
        self.real.timeout = v

    def reset_input_buffer(self):
        return self.real.reset_input_buffer()

    def write(self, data):
        # CMD_DATA 帧：AA 02 LEN_L LEN_H [4B blk]...
        if len(data) >= 8 and data[1] == 0x02:
            self.last_blk = int.from_bytes(data[4:8], 'little')

            if (self.mode == 'drop_frame' and self.last_blk == self.at_block
                    and not self.fired):
                self.fired = True
                print(f'  [{MODE}] 把第 {AT_BLOCK} 帧的帧尾 55 改成 00 → 板子会静默丢弃')
                return self.real.write(bytes(data[:-1]) + b'\x00')

        return self.real.write(data)

    def read(self, n=1):
        reply = self.real.read(n)

        if (self.mode == 'drop_ack' and n == 6 and len(reply) > 0
                and self.last_blk == self.at_block and not self.fired):
            self.fired = True
            self.last_blk = 0
            print(f'  [{MODE}] 吞掉第 {AT_BLOCK} 块的 ACK → PC 会超时重发')
            return b''

        return reply


def main():
    real = serial.Serial(PORT, ota_tool.BAUD, timeout=1.0)
    ser  = LossySerial(real, MODE, AT_BLOCK)

    firmware = open('pro.bin', 'rb').read()[:32768]
    print(f'模式 = {MODE}，在第 {AT_BLOCK} 块做手脚\n')

    ok = ota_tool.ota_update(ser, firmware, version=6)

    real.close()
    print(f'\n结果：{"成功" if ok else "失败"}')


if __name__ == '__main__':
    main()
