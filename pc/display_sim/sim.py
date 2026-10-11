#!/usr/bin/env python3
"""
Simulador de la STM32H745 para probar la pantalla BTT TFT35 B1 sin la Nucleo.

Habla el protocolo de common/display_link.h por un adaptador USB-UART de 3.3 V
conectado al conector RS232/TFT de la pantalla (TX del adaptador -> RX2/PA3,
RX del adaptador -> TX2/PA2, GND común; la pantalla se alimenta con 5 V).

Envía STATUS a 10 Hz con valores simulados que recorren un ciclo
normal -> oscilación -> falla, responde a REQ_PARAMS y aplica los umbrales y la
configuración que se ajustan en la pantalla.

Uso:
    pip install pyserial
    python3 sim.py /dev/ttyUSB0
"""
import argparse
import math
import struct
import sys
import time

try:
    import serial
except ImportError:
    serial = None

SOF = 0xA5
MSG_STATUS, MSG_THRESHOLDS, MSG_CONFIG, MSG_ACK_ALARM, MSG_REQ_PARAMS = 1, 2, 3, 4, 5
LEVELS = ["NORMAL", "ADVERTENCIA", "ANOMALIA", "FALLA"]

FLAG_DETECTOR_OK = 1 << 0
FLAG_ETH_LINK = 1 << 1
FLAG_FOUR_MICS = 1 << 2
FLAG_DOA_VALID = 1 << 3
FLAG_INA_CRITICAL = 1 << 4

# Formatos empaquetados en little-endian, iguales a los structs de C
STATUS_FMT = "<BBhHH3hHI"
THRESHOLDS_FMT = "<6H"
CONFIG_FMT = "<BBh"


def crc16(data: bytes, crc: int = 0xFFFF) -> int:
    """CRC-16/CCITT-FALSE, igual que dl_crc16()."""
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def encode(msg_type: int, payload: bytes = b"") -> bytes:
    body = bytes([msg_type, len(payload)]) + payload
    crc = crc16(body)
    return bytes([SOF]) + body + bytes([crc & 0xFF, crc >> 8])


class Parser:
    """Decodificador byte a byte, equivalente a dl_parse_byte()."""

    def __init__(self):
        self.state = 0
        self.buf = bytearray()
        self.len = 0

    def feed(self, data: bytes):
        for b in data:
            if self.state == 0:
                if b == SOF:
                    self.buf = bytearray()
                    self.state = 1
            elif self.state == 1:
                self.buf.append(b)            # tipo
                self.state = 2
            elif self.state == 2:
                if b > 64:
                    self.state = 0
                    continue
                self.buf.append(b)            # longitud
                self.len = b
                self.state = 3 if b else 4
            elif self.state == 3:
                self.buf.append(b)
                if len(self.buf) - 2 >= self.len:
                    self.state = 4
            elif self.state == 4:
                self.crc_lo = b
                self.state = 5
            elif self.state == 5:
                self.state = 0
                if crc16(bytes(self.buf)) == (self.crc_lo | (b << 8)):
                    yield self.buf[0], bytes(self.buf[2:])


class SimH745:
    def __init__(self, port, cycle_s: float):
        self.port = port
        self.cycle_s = cycle_s
        self.t0 = time.monotonic()
        self.parser = Parser()
        # Valores iniciales de umbrales (ejemplo)
        self.thr = dict(audio_warn_m=800, audio_anom_m=1500,
                        vib_warn_mg=300, vib_anom_mg=700,
                        current_warn_ma=1500, current_crit_ma=2500)
        self.cfg = dict(n_mics=2, doa_mode=0, focus_ddeg=0)

    # ---------------------------------------------------------------- envío
    def send(self, msg_type, payload=b""):
        self.port.write(encode(msg_type, payload))

    def send_thresholds(self):
        self.send(MSG_THRESHOLDS, struct.pack(THRESHOLDS_FMT, *self.thr.values()))

    def send_config(self):
        c = self.cfg
        self.send(MSG_CONFIG, struct.pack(CONFIG_FMT, c["n_mics"], c["doa_mode"], c["focus_ddeg"]))

    def simulated_values(self):
        """Ciclo: 40 % normal, 30 % oscilación creciente, 15 % falla, 15 % recuperación."""
        t = time.monotonic() - self.t0
        phase = (t % self.cycle_s) / self.cycle_s
        if phase < 0.40:
            k = 0.0
        elif phase < 0.70:
            k = (phase - 0.40) / 0.30
        elif phase < 0.85:
            k = 1.3
        else:
            k = 1.3 * (1.0 - (phase - 0.85) / 0.15)
        noise = 0.05 * math.sin(t * 7.3)
        audio = int(400 + k * 1400 + 120 * noise * 10)
        vib = int(120 + k * 650 + 40 * math.sin(t * 3.1))
        current = int(900 + k * 1500 + 300 * k * abs(math.sin(t * 9.0)))
        if self.cfg["doa_mode"] == 1:
            doa = self.cfg["focus_ddeg"]
        else:
            span = 1800 if self.cfg["n_mics"] == 4 else 900
            doa = int(span * 0.6 * math.sin(t * 0.25))
        return max(audio, 0), max(vib, 0), current, doa

    def level_for(self, audio, vib, current, ina_crit):
        if ina_crit:
            return 3
        th = self.thr
        if audio >= th["audio_anom_m"] or vib >= th["vib_anom_mg"]:
            return 2
        if (audio >= th["audio_warn_m"] or vib >= th["vib_warn_mg"]
                or current >= th["current_warn_ma"]):
            return 1
        return 0

    def send_status(self):
        audio, vib, current, doa = self.simulated_values()
        ina_crit = current >= self.thr["current_crit_ma"]
        level = self.level_for(audio, vib, current, ina_crit)
        flags = FLAG_DETECTOR_OK | FLAG_ETH_LINK | FLAG_DOA_VALID
        if self.cfg["n_mics"] == 4:
            flags |= FLAG_FOUR_MICS
        if ina_crit:
            flags |= FLAG_INA_CRITICAL
        uptime = int((time.monotonic() - self.t0) * 1000) & 0xFFFFFFFF
        payload = struct.pack(STATUS_FMT, level, flags, doa, min(audio, 65535),
                              min(vib, 65535), current, 120, 80, 12000, uptime)
        self.send(MSG_STATUS, payload)
        return level, audio, vib, current, doa

    # ---------------------------------------------------------------- recepción
    def handle(self, msg_type, payload):
        if msg_type == MSG_REQ_PARAMS:
            print("<- REQ_PARAMS: envío umbrales y configuración")
            self.send_thresholds()
            self.send_config()
        elif msg_type == MSG_THRESHOLDS and len(payload) == struct.calcsize(THRESHOLDS_FMT):
            self.thr = dict(zip(self.thr.keys(), struct.unpack(THRESHOLDS_FMT, payload)))
            print("<- THRESHOLDS nuevos:", self.thr)
            self.send_thresholds()      # confirmación: la pantalla muestra "aplicados"
        elif msg_type == MSG_CONFIG and len(payload) == struct.calcsize(CONFIG_FMT):
            n, mode, focus = struct.unpack(CONFIG_FMT, payload)
            self.cfg = dict(n_mics=n, doa_mode=mode, focus_ddeg=focus)
            print("<- CONFIG nueva:", self.cfg)
            self.send_config()
        elif msg_type == MSG_ACK_ALARM:
            print("<- ACK_ALARM: alarma reconocida en la pantalla")
        else:
            print(f"<- trama desconocida tipo={msg_type} len={len(payload)}")

    def run(self):
        next_status = time.monotonic()
        last_print = 0.0
        while True:
            data = self.port.read(self.port.in_waiting or 1)
            for msg_type, payload in self.parser.feed(data):
                self.handle(msg_type, payload)
            now = time.monotonic()
            if now >= next_status:
                next_status += 0.1
                level, audio, vib, current, doa = self.send_status()
                if now - last_print >= 1.0:
                    last_print = now
                    print(f"-> {LEVELS[level]:<11} audio={audio / 1000:.3f} "
                          f"vib={vib} mg I={current} mA DOA={doa / 10:.1f}°")


def self_test():
    """Comprueba que el codificador y el parser son coherentes."""
    p = Parser()
    frame = encode(MSG_THRESHOLDS, struct.pack(THRESHOLDS_FMT, 1, 2, 3, 4, 5, 6))
    out = list(p.feed(b"\x00\xff" + frame + encode(MSG_ACK_ALARM)))
    assert out[0] == (MSG_THRESHOLDS, struct.pack(THRESHOLDS_FMT, 1, 2, 3, 4, 5, 6)), out
    assert out[1] == (MSG_ACK_ALARM, b""), out
    assert crc16(b"123456789") == 0x29B1          # valor de referencia CCITT-FALSE
    assert struct.calcsize(STATUS_FMT) == 20
    print("Autoprueba OK")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port", nargs="?", help="puerto serie, p. ej. /dev/ttyUSB0 o COM5")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--cycle", type=float, default=40.0,
                    help="duración del ciclo normal->falla en segundos")
    ap.add_argument("--self-test", action="store_true", help="probar el protocolo sin hardware")
    args = ap.parse_args()

    if args.self_test:
        self_test()
        return
    if not args.port:
        ap.error("falta el puerto serie")
    if serial is None:
        sys.exit("Falta pyserial: pip install pyserial")

    with serial.Serial(args.port, args.baud, timeout=0.01) as port:
        print(f"Simulando la STM32H745 en {args.port} a {args.baud} baud (Ctrl+C para salir)")
        try:
            SimH745(port, args.cycle).run()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
