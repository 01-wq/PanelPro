import serial
import time
import zlib

PORT = 'COM5'
BAUD = 9600
BLOCK_SIZE = 240       # 每帧最多 240 字节（和固件 FRAME_MAX_SIZE 一致）
PROGRESS_EVERY = 50        # 每 50 块打印一次进度

TIMEOUT_START = 15.0    # CMD_START：要擦 464KB，实测约 4~5 秒
TIMEOUT_DATA  = 0.5     # CMD_DATA：实测一帧 0.267s（250B@9600bps，被波特率锁死不可压缩），留约 2× 余量
TIMEOUT_END   = 5.0     # CMD_END：算 CRC32 + 写 Metadata

MAX_RETRY     = 3       # 同一帧最多重发几次（见 Docs/OTA_协议规格.md 第七节）

retry_count = 0         # 全程重传统计——2e 丢帧测试靠它当"重传真的发生了"的证据

def build_frame(cmd,payload=b''):
    frame = bytearray()
    frame.append(0xAA)
    frame.append(cmd)
    frame.append(len(payload) & 0xFF)
    frame.append((len(payload) >> 8) & 0xFF)
    frame.extend(payload)

    xor = 0
    for b in  payload:
        xor ^= b

    frame.append(xor)
    frame.append(0x55)
    return  bytes(frame)


def send_frame(ser, frame):
    """发一帧，等回应。返回收到的原始字节（bytes）"""
    ser.reset_input_buffer()  # 清掉可能残留的旧数据
    ser.write(frame)  # 发出去
    reply = ser.read(6)  # 最多等 1 秒；凑满 6 字节就立刻返回
    return reply


def send_and_wait(ser, frame, what, timeout):
    """发一帧并等回应。返回 True=收到 ACK，False=失败（调用方必须【中止整个升级】）。

    重发规则（协议规格第七节）：
      超时      → 重发。这是唯一"帧可能没到"的信号。重发同一个 frame（块号不变），
                  板子那边靠块号判重——重复帧会被幂等地 ACK 掉，不会重复写 Flash。
      NACK      → 立即中止。板子【明确拒绝】了这帧，重发同样的内容没有意义。
      未知回应  → 立即中止。大声失败，别猜。
    绝不跳过任何一块——跳过会让后续全部错位，而且失败点会推迟到最后一帧才暴露。
    """
    global retry_count

    for attempt in range(MAX_RETRY + 1):
        ser.reset_input_buffer()
        ser.timeout = timeout
        t0 = time.time()
        ser.write(frame)
        reply = ser.read(6)
        dt = time.time() - t0

        if len(reply) == 0:
            if attempt < MAX_RETRY:
                retry_count += 1
                print(f'  ! {what}: 超时（{dt:.2f}s），重发 {attempt + 1}/{MAX_RETRY}')
                continue
            print(f'  X {what}: 超时，重发 {MAX_RETRY} 次仍无回应 → 中止')
            return False

        if reply[:2] == bytes([0xAA, 0x80]):
            return True
        if reply[:2] == bytes([0xAA, 0x81]):
            print(f'  X {what}: NACK ({dt:.2f}s) → 中止')
            return False

        print(f'  X {what}: 未知回应 {reply.hex(" ")} → 中止')
        return False

    return False


def ota_update(ser, firmware, version):
    global retry_count
    retry_count = 0

    size = len(firmware)
    crc = zlib.crc32(firmware)
    total = (size + BLOCK_SIZE - 1) // BLOCK_SIZE
    print(f'固件 {size} 字节, CRC32 = 0x{crc:08X}')
    print(f'预计耗时约 {total * 0.27 / 60:.1f} 分钟')

    # (1) CMD_START : [4B size][4B crc][4B version]，全部小端
    payload = (size.to_bytes(4, 'little')
               + crc.to_bytes(4, 'little')
               + version.to_bytes(4, 'little'))
    print('1) CMD_START')
    if not send_and_wait(ser, build_frame(0x01, payload), 'CMD_START',TIMEOUT_START):
        return False

    # (2) CMD_DATA : 每 240 字节一块
    t0 = time.time()
    for offset in range(0, size, BLOCK_SIZE):
        chunk = firmware[offset:offset + BLOCK_SIZE]
        blk = offset // BLOCK_SIZE + 1

        payload = blk.to_bytes(4, 'little') + chunk

        if blk % PROGRESS_EVERY == 0 or blk == total:
            elapsed = time.time() - t0
            eta = elapsed / blk * (total - blk)
            print(f'   {blk}/{total} ({blk * 100 // total}%)  'f'已用 {elapsed:.0f}s  剩余约 {eta:.0f}s')
        if not send_and_wait(ser, build_frame(0x02, payload), f'第 {blk}块',TIMEOUT_DATA):
            return False

    # (3) CMD_END : 空 payload
    print('3) CMD_END')
    if not send_and_wait(ser, build_frame(0x03), 'CMD_END',TIMEOUT_END):
        return False

    print(f'（本次共重传 {retry_count} 次）')
    print('OK - 升级完成，板子正在复位重启')
    return True


if __name__ == '__main__':
    firmware = open('pro.bin', 'rb').read()      # 全量，不再截断
    ser = serial.Serial(PORT, BAUD, timeout=1.0)
    ota_update(ser, firmware, version=6)