# -*- coding: utf-8 -*-
"""
OTA over BLE —— 用电脑直接连 HC-08（BLE 4.0 / GATT）给板子升级。

和 ota_tool.py 的唯一区别是【传输层】：
    ota_tool.py     : pyserial → CH340 → 有线 → PA3/PD5
    ota_tool_ble.py : bleak    → 电脑蓝牙 → HC-08 → PA3/PD5
协议组帧、重传规则、超时策略【一模一样】，一个字没改。

────────────────────────────────────────────
前置（三件事，缺一不可）：
  1. pip install bleak
  2. 电脑要有蓝牙。没有的话插一个 USB BLE 适配器（台式机多半没有）
     ⚠️ HC-08 是 BLE(GATT)，**不是**经典蓝牙 SPP —— "蓝牙串口"那种 COM 口连不上它
  3. 接线：HC-08 的 TX/RX 接回 PA3/PD5，**CH340 必须拔掉**
     （两个推挽输出怼同一根线 = 短路）

用法：
    python ota_tool_ble.py --scan              先扫一遍，确认能看见 HC-08
    python ota_tool_ble.py --probe             探针：只发 CMD_VERSION，确认双向通
    python ota_tool_ble.py                     全量升级 pro.bin
    python ota_tool_ble.py --fw test_fw.bin --max-blocks 3    只发前 3 块（测速用）

⚠️ 本脚本写成时【没有硬件可测】。第一次跑请务必先 --scan 再 --probe，
   两步都过了再碰全量升级。
"""
import argparse
import asyncio
import time
import zlib

from bleak import BleakClient, BleakScanner

# ── 目标设备 ────────────────────────────────────────────────
DEVICE_NAME  = "HC-08"                                  # 广播名；扫不到就看 --scan 的实际输出
SERVICE_UUID = "0000ffe0-0000-1000-8000-00805f9b34fb"   # HC-08 透传服务
CHAR_UUID    = "0000ffe1-0000-1000-8000-00805f9b34fb"   # 透传特征（写 + notify）

# ── 传输参数 ────────────────────────────────────────────────
BLE_CHUNK = 20      # BLE 默认 ATT MTU=23，单次写最多 20 字节。一帧 246 字节 → 拆 13 次写。
                    # 帧边界由协议定义、不由传输方式定义 —— FrameParser_Feed() 是逐字节状态机，
                    # 一帧拆任意段都没问题，只要顺序对、中间不插字节。

UART_BAUD = 9600    # MCU ↔ HC-08 那一段的波特率（不是 BLE 的速率！）
CHUNK_GAP = 1.3     # 分包节流余量。丢帧就把 --gap 调大（试 3），别再往下调。

# ── 协议参数（与 ota_tool.py 完全一致）──────────────────────
BLOCK_SIZE    = 240
PROGRESS_EVERY = 50

# ⚠️ 重试次数（2026-09-27 第三轮实测上调 3 → 8）
#    设计时按"每次丢包相互独立"估：3 次后失败率 ≈ q³。
#    实测打脸——丢包是【成串】来的（实测超时集中在 5/18/32/68 块，不是均匀散布），
#    连丢 4 次的概率远高于 q⁴，3 次根本不够骑过一次几秒钟的链路抖动。
#    上限的意义不变，仍然是【保证程序一定会终止】：9 次发完还不行就中止。
MAX_RETRY     = 8

# ── 回应帧结构（与固件 OTA_Send_Response_Data 一致）──────────
#     AA <rsp> <len_lo> <len_hi> <payload...> <xor> 55
FRAME_HEADER  = 0xAA
FRAME_TRAILER = 0x55
MAX_RSP_LEN   = 64      # 回应 payload 最长就是 4（版本号），给足余量防垃圾"头"

RSP_ACK       = 0x80
RSP_NACK      = 0x81
RSP_VERSION   = 0x82

# ⚠️ 超时值比有线版放宽：BLE 的连接间隔 + 分包写入会带来额外延迟
TIMEOUT_START = 15.0
TIMEOUT_DATA  = 2.0
TIMEOUT_END   = 5.0

retry_count = 0


# ══════════════════════════════════════════════════════════
#  传输层
# ══════════════════════════════════════════════════════════
class BleTransport:
    """把 BLE 包装成"能 write / 能 read"的东西，接口对齐 pyserial。

    收到的数据走 notify 回调进 _rx 缓冲区，read() 从缓冲区里取。
    """

    def __init__(self, client, gap=CHUNK_GAP):
        self.client = client
        self._rx = bytearray()
        self.gap = gap
        self.chunk = BLE_CHUNK      # 每个 BLE 包装多少字节，连上后按协商到的 MTU 定
        self.ack_write = False      # 用不用"等确认的写"，连上后按特征属性定

    def _on_notify(self, _char, data: bytearray):
        self._rx.extend(data)

    def _dump_gatt(self):
        """把模块的 GATT 表全列出来，返回透传特征的属性集合。

        为什么要全列（2026-09-27）：HC-08 的 FFE1 只有 write-without-response，
        没有 write（等确认的写）——这是整条路"丢包不可见"的根源。
        万一模块还藏着一个支持 write 的特征（有些固件有 FFE2），那就是解药。
        列一次 5 秒钟，比反复猜强。
        """
        props = set()
        try:
            for service in self.client.services:
                print(f'  服务 {service.uuid}')
                for char in service.characteristics:
                    mark = ' ← 透传' if char.uuid.lower() == CHAR_UUID.lower() else ''
                    print(f'    特征 {char.uuid}  {sorted(char.properties)}{mark}')
                    if char.uuid.lower() == CHAR_UUID.lower():
                        props = set(char.properties)
        except Exception as e:
            print(f'  （枚举 GATT 失败：{e}）')
        return props

    async def open(self):
        await self.client.connect()

        # 一帧 246 字节拆成几包发 —— 包数越少，"中间丢一个"的机会越少。
        # MTU 由两端协商，23 是 BLE 4.0 的最小值（扣掉 3 字节头，有效负载 20）。
        try:
            mtu = int(self.client.mtu_size)
        except Exception:
            mtu = 23
        self.chunk = max(BLE_CHUNK, min(mtu - 3, 244))
        print(f'BLE MTU={mtu} → 每包 {self.chunk} 字节，一帧 246 字节拆 {-(-246 // self.chunk)} 包')

        print('GATT 表：')
        props = self._dump_gatt()
        self.ack_write = 'write' in props
        print(f'→ 透传特征 {"【支持等确认的写】丢包会立刻暴露" if self.ack_write else "【只支持发出即忘的写】丢了只能靠超时发现"}')
        print(f'→ 超时重发上限 {MAX_RETRY} 次（丢包是成串来的，留够骑过抖动的余量）')

        await self.client.start_notify(CHAR_UUID, self._on_notify)

    async def close(self):
        try:
            await self.client.stop_notify(CHAR_UUID)
        except Exception:
            pass
        await self.client.disconnect()

    async def write(self, frame: bytes):
        """按协商到的包长拆段写，并按 UART 速率节流。

        ⚠️ 清空接收缓冲区放在【发送之前】，不放 read() 里。
        放 read() 里会留一个竞态窗口：万一回应在 write 返回、read 开始之间就到了，
        会被那句 clear() 悄悄丢掉 —— 现象就是"明明有回应，却报没回应"。

        ⚠️ 为什么必须节流（2026-09-27 实测踩到）：
            症状 = 18 字节的 CMD_START 能过，250 字节的 CMD_DATA 报 `ERR] frame error`。
            因为 250 字节要拆成 13 个 BLE 分包，背靠背猛灌进去；而 HC-08 的 UART
            只有 9600bps —— 1 字节 ≈ 1ms，整帧要 260ms 才排得完。
            BLE 侧灌得比 UART 侧排得快 → 模块缓冲区溢出、丢字节 → 帧结构坏掉
            → XOR8 过不了 → 板子回 `ERR` 且不发 ACK → PC 超时。

        ⚠️ 为什么优先用"等确认的写"（response=True）（2026-09-27 第二轮实测踩到）：
            发出即忘的写没有任何反馈 —— BLE 侧丢一个包，整帧 246 字节全部报废，
            板子只能打 `ERR`，PC 只能等满 2 秒超时才发现。实测 594 帧里丢 6 帧（1%），
            跑到 1627 块时连续 4 帧全灭（≈9 秒链路中断），整趟 12 分钟作废。
            等确认 = 每个包都拿回执，丢包从"只能靠超时事后发现"变成"根本不会发生"。

        ⚠️ 节流现在是【补差额】而不是死睡：
            ATT 往返（等确认）通常已经比 UART 排 20 字节慢，那就一秒都不用多睡；
            万一往返比 UART 还快，才补睡到该有的间隔。两头都不吃亏。
        """
        self._rx.clear()
        for i in range(0, len(frame), self.chunk):
            chunk = frame[i:i + self.chunk]
            need = len(chunk) * 10 / UART_BAUD * self.gap   # 这一包从 UART 排出去要多久
            t0 = time.time()
            await self.client.write_gatt_char(CHAR_UUID, chunk, response=self.ack_write)
            spent = time.time() - t0
            if spent < need:                                # 回执太快就补睡，够慢就不用
                await asyncio.sleep(need - spent)

    def received(self) -> bytes:
        """当前收到的原始字节流（不清空）。怎么解析交给 parse_reply()。"""
        return bytes(self._rx)

    def drop_received(self):
        """一帧回应已经消费掉了，把缓冲区清干净，别留给下一个 240 字节块。"""
        self._rx.clear()


def parse_reply(buf: bytes):
    """在收到的字节流里【重新对齐】找一帧合法回应。

    返回 (rsp, payload, leading)：leading 是这一帧【前面】混进来的杂字节。
    找不到返回 None。

    ⚠️ 为什么不能直接看开头两个字节（2026-09-27 踩到）：
        实测收到过一次 `ff aa 80 00 00 00` —— 一个杂字节插在 ACK 前面，
        整帧错位一格，`reply[:2] == AA 80` 判不过，把整趟 15 分钟的任务中止了。

        固件那边 FrameParser_Feed() 的做法是【扫到 0xAA 才开始，逐字段校验】。
        回应帧带 0xAA 头 + 0x55 尾 + XOR，本来就是为了能从任意位置重新对齐——
        协议已经给了这个能力，两端就得用同一套思路去解析。
    """
    i = 0
    while i < len(buf):
        if buf[i] != FRAME_HEADER:
            i += 1
            continue
        if i + 6 > len(buf):
            return None                     # 头是有了，长度字段还没收全 → 再等等
        length = buf[i + 2] | (buf[i + 3] << 8)
        if length > MAX_RSP_LEN:
            i += 1                          # 假的"头"（长度离谱）→ 换个位置再找
            continue
        if i + 6 + length > len(buf):
            return None                     # 结构合法但还没收全 → 再等等
        body = buf[i + 4:i + 4 + length]
        xor = 0
        for b in body:
            xor ^= b
        if buf[i + 4 + length] == xor and buf[i + 5 + length] == FRAME_TRAILER:
            return buf[i + 1], bytes(body), bytes(buf[:i])
        i += 1                              # XOR 或尾字节不对 → 这个不是帧头
    return None


async def read_reply(tp, timeout):
    """等一帧【合法】回应，最多 timeout 秒。返回 (rsp, payload, leading) 或 None。"""
    deadline = time.time() + timeout
    while True:
        r = parse_reply(tp.received())
        if r is not None:
            tp.drop_received()
            return r
        if time.time() >= deadline:
            return None
        await asyncio.sleep(0.01)


# ══════════════════════════════════════════════════════════
#  协议层（与 ota_tool.py 逐行对应）
# ══════════════════════════════════════════════════════════
def build_frame(cmd, payload=b''):
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


async def send_and_wait(tp, frame, what, timeout):
    """发一帧并等回应。True=收到 ACK，False=失败（调用方必须【中止整个升级】）。

    重发规则和有线版一致：
      超时      → 重发（块号不变，板子靠块号判重，幂等 ACK，不会重复写 Flash）
      NACK      → 立即中止（板子明确拒绝了，重发没意义）
      未知回应  → 立即中止，大声失败，别猜
    """
    global retry_count

    for attempt in range(MAX_RETRY + 1):
        t0 = time.time()
        await tp.write(frame)
        r = await read_reply(tp, timeout)
        dt = time.time() - t0

        if r is None:
            if attempt < MAX_RETRY:
                retry_count += 1
                print(f'  ! {what}: 超时（{dt:.2f}s），重发 {attempt + 1}/{MAX_RETRY}')
                continue
            print(f'  X {what}: 超时，重发 {MAX_RETRY} 次仍无回应 → 中止')
            return False

        rsp, _payload, leading = r

        # 杂字节不再中止任务，但必须【喊出来】——它会再出现，我们要看见它长什么样
        if leading:
            print(f'  ~ {what}: 回应前面混进 {len(leading)} 个杂字节 '
                  f'[{leading.hex(" ")}] → 已重新对齐')

        if rsp == RSP_ACK:
            return True
        if rsp == RSP_NACK:
            print(f'  X {what}: NACK ({dt * 1000:.0f}ms) → 中止')
            return False

        print(f'  X {what}: 未知回应码 0x{rsp:02X} → 中止')
        return False

    return False


async def probe(tp):
    """探针：确认 BLE 链路双向通，且板子确实停在 OTA 模式。

    ⚠️ 这个函数【故意不走 send_and_wait】。原因：

        在 OTA 模式下，CMD_VERSION 的【正确回应就是 NACK】，不是 ACK。

        ota_manager.c:88 的 CMD_VERSION 分支要求 Metadata_IsValid()==1 才回版本号，
        否则回 NACK —— 这是"哨兵值原则"：Metadata 无效时回 NACK，绝不回一个假的 0 版本。
        而 boot 只有在 metadata 无效时才进 OTA 模式。

        ⇒ "板子在 OTA 模式"和"CMD_VERSION 回 NACK"必然同时成立。
          NACK 才是好消息；真收到 ACK(版本号) 反而说明板子不该在这儿。

    这一帧【不擦任何东西】，是全程唯一可以随便重试的操作，也是唯一的速度预判机会。
    """
    frame = build_frame(0x10)
    print(f'探针 CMD_VERSION ({frame.hex(" ").upper()}) …')

    # BLE 的 write-without-response 是"发出即忘"，单次可能丢。重试几次再下结论。
    r, dt = None, 0.0
    for attempt in range(1, MAX_RETRY + 2):
        t0 = time.time()
        await tp.write(frame)
        r = await read_reply(tp, TIMEOUT_DATA)
        dt = (time.time() - t0) * 1000
        if r is not None:
            break
        tail = '，重试' if attempt <= MAX_RETRY else ''
        print(f'  ! 第 {attempt} 次没回应（{dt:.0f}ms）{tail}')

    if r is None:
        print('X - 连试 %d 次都没回应。分两种情况，看 RTT 区分：' % (MAX_RETRY + 1))
        print('    RTT 里有 [FRAME] CMD:0x10 → 帧到了，是【回程】断了（查 HC-08 的 TX→PA3）')
        print('    RTT 里什么都没有    → 帧根本没到（查 HC-08 的 RX→PD5，或板子不在 OTA 模式）')
        return False

    rsp, _payload, leading = r
    if leading:
        print(f'  ~ 探针回应前面混进 {len(leading)} 个杂字节 [{leading.hex(" ")}] → 已重新对齐')

    # ⚠️ 收到的是"我们自己刚发出去的那一帧" = 板子在【原样回显】= 它跑的是 App，不是 Bootloader。
    #    App 的串口中断里有 `HAL_UART_Transmit(&huart2,&bt_rx_byte,1,100); //Echo回去`
    #    （PanelPro/Core/Src/usart.c:30）—— 谁来字节它都原样吐回去。
    #    只有 Bootloader 才解析协议；App 根本不知道 CMD_VERSION 是什么。
    #    （2026-09-27 踩到：OTA 成功后板子跳进 App，再跑本脚本就一直是这个现象，
    #      看着像"工具坏了"，其实是没让板子进 OTA 模式。）
    if rsp == frame[1]:
        print('X - 收到的字节和我们刚发出去的【一模一样】—— 板子在【回显】，不是在解析协议。')
        print('   ⇒ 板子现在跑的是 App，不在 OTA 模式。')
        print('   ⇒ 怎么办：屏幕上点「关于页 → 系统升级 → 确认」，屏幕一黑就对了，再重跑本脚本。')
        return False

    if rsp == RSP_NACK:
        print(f'OK - 链路通，往返 {dt:.0f} ms；收到 NACK —— 这正是 OTA 模式下的正确回应')
        # 粗估：这一帧受 9600bps 的 UART 限制，数据帧长得多，要按每帧字节数折算
        uart_ms = len(frame) * 10 / 9.6                 # 9600bps 下 1 字节 = 10 bit
        ble_ms = max(dt - uart_ms, 0)                   # 剩下的算 BLE 往返开销
        per_frame = ble_ms + 246 * 10 / 9.6             # 数据帧 = 246 字节
        print(f'     粗估：每帧 ~{per_frame:.0f} ms（UART 占 {246 * 10 / 9.6:.0f} ms，是硬瓶颈）'
              f' → 全量 1882 帧约 {1882 * per_frame / 1000 / 60:.1f} 分钟')
        return True

    if rsp == RSP_ACK:
        print(f'! 收到 ACK/版本号（回应码 0x{rsp:02X}）—— 链路通，但 metadata 是有效的。')
        print('  这说明板子不该停在 OTA 模式。先确认你按了屏幕上那个"系统升级"按钮。')
        return False

    print(f'X - 回应码 0x{rsp:02X} 不认识 → 别往下走')
    return False


async def ota_update(tp, firmware, version, max_blocks=None, resume=None):
    global retry_count
    retry_count = 0

    size = len(firmware)
    crc = zlib.crc32(firmware)
    total = (size + BLOCK_SIZE - 1) // BLOCK_SIZE
    if max_blocks:
        total = min(total, max_blocks)
        print(f'⚠️⚠️ 测速模式：只发前 {total} 块。')
        print('    注意：上面的 CMD_START 已经把 App 区擦掉了，这一跑不会装回去。')
        print('    跑完板子会停在 OTA 循环（黑屏）。恢复方式：插回 CH340，用有线 ota_tool.py 跑一次全量。')

    print(f'固件 {size} 字节, CRC32 = 0x{crc:08X}, 共 {total} 块')

    first_blk = 1
    if resume:
        # ── 断点续传：跳过 CMD_START，从第 resume 块接着发 ──────────────
        # 这是【索引制寻址】白送的能力，不是额外加的机制：
        #     地址 = APP_BASE + (blk-1) × 240   ← 由块号【算】出来
        # 它不依赖"板子记住自己写到哪了"。所以 PC 从第几块接着发都行，
        # Flash 里前 N 块原封不动还在，板子的 expected_blk 也还停在断点处。
        # （对照：当初若选了游标制 APP_BASE + ota_received 就做不到 ——
        #   那是板子自己的状态，PC 猜不准，一旦猜错整包错位。）
        #
        # 先重发第 resume 块本身：两种情形都安全，而且都能收敛到同一个状态 ——
        #   板子没收到过它 → 正是 expected_blk → 写进去，ACK
        #   板子收到过了   → expected_blk-1 → 判为重复帧，不写，幂等 ACK
        # 两种情况跑完，板子的 expected_blk 都是 resume+1，可以继续往下发。
        print(f'⚠️ 断点续传：从第 {resume} 块接着发，【跳过 CMD_START】（不重擦 App 区）')
        print('   前提：板子从上一趟中止后【没有复位过、没有断电过】。')
        print('   板子一旦复位，expected_blk 归零，必须去掉 --resume 重头跑。')
        first_blk = resume
    else:
        # (1) CMD_START : [4B size][4B crc][4B version]，全部小端
        payload = (size.to_bytes(4, 'little')
                   + crc.to_bytes(4, 'little')
                   + version.to_bytes(4, 'little'))
        print('1) CMD_START')
        if not await send_and_wait(tp, build_frame(0x01, payload), 'CMD_START', TIMEOUT_START):
            return False

    # (2) CMD_DATA : 每 240 字节一块，负载 = [4B 块号][数据]
    t0 = time.time()
    for offset in range((first_blk - 1) * BLOCK_SIZE, size, BLOCK_SIZE):
        chunk = firmware[offset:offset + BLOCK_SIZE]
        blk = offset // BLOCK_SIZE + 1
        if blk > total:
            break

        payload = blk.to_bytes(4, 'little') + chunk

        if blk % PROGRESS_EVERY == 0 or blk == total:
            done = blk - first_blk + 1          # 断点续传时不能拿 blk 当进度
            elapsed = time.time() - t0
            eta = elapsed / done * (total - blk)
            per_frame = elapsed / done
            print(f'   {blk}/{total} ({blk * 100 // total}%)  '
                  f'已用 {elapsed:.0f}s  剩余约 {eta:.0f}s  ({per_frame * 1000:.0f} ms/帧)')

        if not await send_and_wait(tp, build_frame(0x02, payload), f'第 {blk}块', TIMEOUT_DATA):
            if resume:
                print(f'   ⇒ 接着跑：py ota_tool_ble.py --gap {tp.gap:g} --resume {blk}')
            return False

    if max_blocks:
        print(f'（测速结束，共重传 {retry_count} 次）')
        return True

    # (3) CMD_END : 空 payload
    print('3) CMD_END')
    if not await send_and_wait(tp, build_frame(0x03), 'CMD_END', TIMEOUT_END):
        return False

    print(f'（本次共重传 {retry_count} 次）')
    print('OK - 升级完成，板子正在复位重启')
    return True


async def scan():
    print('扫描 BLE 设备（约 8 秒）…\n')
    devices = await BleakScanner.discover(timeout=8.0)
    if not devices:
        print('一个都没扫到。检查：蓝牙开着吗？适配器插了吗？')
        return
    for d in devices:
        tag = '  ← 目标' if d.name and DEVICE_NAME.lower() in d.name.lower() else ''
        print(f'  {d.address}   {d.name}{tag}')
    print(f'\n共 {len(devices)} 个。如果看到 {DEVICE_NAME}，把地址填进 DEVICE_ADDRESS 或直接用名字连。')


async def find_device():
    """按名字找 HC-08；找不到就报错并提示去 --scan。"""
    print(f'找 {DEVICE_NAME} …')
    dev = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)
    if dev is None:
        raise SystemExit(f'没找到 {DEVICE_NAME}。先跑 `python ota_tool_ble.py --scan` 看实际广播名。')
    print(f'找到：{dev.address}  {dev.name}')
    return dev.address


async def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--scan', action='store_true', help='只扫描 BLE 设备')
    ap.add_argument('--probe', action='store_true', help='只发探针，确认链路')
    ap.add_argument('--fw', default='pro.bin', help='固件文件（默认 pro.bin）')
    ap.add_argument('--version', type=int, default=6, help='版本号字段')
    ap.add_argument('--max-blocks', type=int, default=None, help='只发前 N 块（测速）')
    ap.add_argument('--resume', type=int, default=None, metavar='N',
                    help='断点续传：从第 N 块接着发（跳过 CMD_START，不重擦 App 区）。'
                         '上一趟中止后、板子【没复位过】才能用')
    ap.add_argument('--gap', type=float, default=CHUNK_GAP,
                    help=f'分包节流余量（默认 {CHUNK_GAP}）。丢帧就调大，试 3')
    args = ap.parse_args()

    if args.scan:
        await scan()
        return

    address = await find_device()
    client = BleakClient(address)
    tp = BleTransport(client, gap=args.gap)

    await tp.open()
    print('已连接')
    try:
        if args.probe:
            await probe(tp)
            return

        firmware = open(args.fw, 'rb').read()
        # 先探针后升级 —— 探针是这套流程自带的链路自检，别省
        if not await probe(tp):
            return
        await ota_update(tp, firmware, args.version, args.max_blocks, args.resume)
    finally:
        await tp.close()


if __name__ == '__main__':
    asyncio.run(main())
