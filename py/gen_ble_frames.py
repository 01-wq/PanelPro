# -*- coding: utf-8 -*-
"""
生成「手机蓝牙手工发帧」用的 hex 串。

用途：用 HC 蓝牙助手把 512 字节测试固件(test_fw.bin)通过【真实 BLE 链路】
      发给板子，验证蓝牙通道能走通 OTA 协议。

每帧格式（和 ota_tool.py 的 build_frame 完全一致）：
    0xAA | CMD | LEN_LOW | LEN_HIGH | PAYLOAD | XOR8 | 0x55
    XOR8 = PAYLOAD 所有字节异或

输出：ble_frames.txt（两个版本：空格分隔 / 连续）
"""
import zlib

FW_FILE = 'test_fw.bin'
BLOCK_SIZE = 240
VERSION = 6


def build_frame(cmd, payload=b''):
    f = bytearray([0xAA, cmd, len(payload) & 0xFF, (len(payload) >> 8) & 0xFF])
    f.extend(payload)
    xor = 0
    for b in payload:
        xor ^= b
    f.append(xor)
    f.append(0x55)
    return bytes(f)


fw = open(FW_FILE, 'rb').read()
crc = zlib.crc32(fw) & 0xFFFFFFFF

frames = []

# 0) 探针：先确认蓝牙链路双向通
frames.append(('探针 CMD_VERSION（6 字节）—— 板子应回版本号', build_frame(0x10)))

# 1) CMD_START : [4B size][4B crc32][4B version] 全小端
start_payload = (len(fw).to_bytes(4, 'little')
                 + crc.to_bytes(4, 'little')
                 + VERSION.to_bytes(4, 'little'))
frames.append((f'CMD_START（18 字节）size={len(fw)} crc32=0x{crc:08X} ver={VERSION}',
               build_frame(0x01, start_payload)))

# 2) CMD_DATA : [4B blk][<=240B 数据]
for off in range(0, len(fw), BLOCK_SIZE):
    chunk = fw[off:off + BLOCK_SIZE]
    blk = off // BLOCK_SIZE + 1
    payload = blk.to_bytes(4, 'little') + chunk
    frames.append((f'CMD_DATA 块{blk}（{len(chunk)} 字节数据 → 整帧 {len(payload)+6} 字节）',
                   build_frame(0x02, payload)))

# 3) CMD_END : 空 payload
frames.append(('CMD_END（6 字节）—— 发完板子复位跳转', build_frame(0x03)))

lines = []
lines.append(f'固件 {FW_FILE}  {len(fw)} 字节  CRC32=0x{crc:08X}  共 {len(frames)} 帧')
lines.append('=' * 70)
lines.append('')

lines.append('【格式 A：空格分隔】（多数蓝牙助手用这个）')
lines.append('-' * 70)
for i, (desc, fr) in enumerate(frames, 1):
    lines.append(f'--- 第 {i}/{len(frames)} 帧 · {desc} ---')
    lines.append(' '.join(f'{b:02X}' for b in fr))
    lines.append('')

lines.append('')
lines.append('【格式 B：连续无空格】（格式 A 粘不进去时试这个）')
lines.append('-' * 70)
for i, (desc, fr) in enumerate(frames, 1):
    lines.append(f'--- 第 {i}/{len(frames)} 帧 · {desc} ---')
    lines.append(fr.hex().upper())
    lines.append('')

out = '\n'.join(lines)
open('ble_frames.txt', 'w', encoding='utf-8').write(out)

print(out)
print()
print('=' * 70)
print('已写入 py/ble_frames.txt')
