#!/usr/bin/env python3
"""
Carga un firmware nuevo en la pantalla BTT TFT35 B1 por el UART del enlace
(RX2/TX2), sin ST-Link. La pantalla debe tener ya el firmware propio con el
actualizador (fase 0 o posterior).

Por ahora la PC llega a la pantalla con un puente USB-UART (por ejemplo la
ESP32 con esp32_uart_bridge). Cuando esté la STM32H745, ella reenviará estas
tramas.

Uso:
    python3 flash_display.py /dev/ttyUSB0 [display/.pio/build/tft35_b1/firmware.bin]

También lo llama `pio run -t upload --upload-port /dev/ttyUSB0` desde display/.
"""
import argparse
import os
import struct
import sys
import time
import zlib

import serial

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sim import Parser, encode  # noqa: E402

MSG_FW_BEGIN, MSG_FW_DATA, MSG_FW_END, MSG_FW_ACK = 0x10, 0x11, 0x12, 0x13
CHUNK = 56
APP_MAX = 224 * 1024
ERRORS = {
    1: "tamaño inválido", 2: "offset fuera de secuencia", 3: "CRC incorrecto",
    4: "falla de la flash SPI", 5: "imagen inválida (tabla de vectores)",
    6: "la pantalla no estaba en modo actualización",
}

DEFAULT_BIN = os.path.join(os.path.dirname(__file__), "..", "..", "display",
                           ".pio", "build", "tft35_b1", "firmware.bin")


class Link:
    def __init__(self, port):
        self.port = port
        self.parser = Parser()

    def request(self, msg_type, payload, timeout):
        """Envía una trama y espera el FW_ACK. Devuelve (status, next_offset) o None."""
        self.port.write(encode(msg_type, payload))
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            data = self.port.read(self.port.in_waiting or 1)
            for t, p in self.parser.feed(data):
                if t == MSG_FW_ACK and len(p) == 5:
                    return struct.unpack("<BI", p)
        return None


def progress(done, total, t0):
    pct = done * 100 // total
    bar = "#" * (pct // 4)
    rate = done / max(time.monotonic() - t0, 1e-3) / 1024
    sys.stdout.write(f"\r  [{bar:<25}] {pct:3d}%  {done // 1024}/{total // 1024} KB  {rate:4.1f} KB/s")
    sys.stdout.flush()


def flash(port_name, path, baud):
    with open(path, "rb") as f:
        image = f.read()
    image += b"\xFF" * (-len(image) % 4)            # múltiplo de 4 bytes
    if not 0 < len(image) <= APP_MAX:
        sys.exit(f"Tamaño inválido: {len(image)} bytes (máx. {APP_MAX})")
    crc = zlib.crc32(image) & 0xFFFFFFFF

    print(f"Firmware: {path} ({len(image)} bytes, CRC32 {crc:08X})")
    with serial.Serial(port_name, baud, timeout=0.01) as port:
        time.sleep(0.1)
        port.reset_input_buffer()
        link = Link(port)

        print("Preparando la pantalla (borrando flash SPI)...")
        r = link.request(MSG_FW_BEGIN, struct.pack("<II", len(image), crc), timeout=20)
        if r is None:
            sys.exit("La pantalla no responde. ¿Puerto correcto? ¿Tiene el firmware con actualizador?")
        if r[0] != 0:
            sys.exit(f"La pantalla rechazó la actualización: {ERRORS.get(r[0], r[0])}")

        off, t0 = 0, time.monotonic()
        while off < len(image):
            chunk = image[off:off + CHUNK]
            for attempt in range(8):
                r = link.request(MSG_FW_DATA, struct.pack("<I", off) + chunk, timeout=1.0)
                if r is not None:
                    break
            else:
                sys.exit(f"\nSin respuesta en el offset {off}")
            status, nxt = r
            if status == 0:
                off = nxt
            elif status == 2:
                off = nxt                          # retomar donde dice la pantalla
            else:
                sys.exit(f"\nError: {ERRORS.get(status, status)}")
            progress(off, len(image), t0)
        print()

        print("Verificando e instalando...")
        r = link.request(MSG_FW_END, b"", timeout=20)
        if r is None:
            sys.exit("Sin respuesta al verificar")
        if r[0] != 0:
            sys.exit(f"Error al verificar: {ERRORS.get(r[0], r[0])}")
        print(f"Listo en {time.monotonic() - t0:.1f} s. La pantalla se reinicia con el firmware nuevo.")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port", help="puerto serie, p. ej. /dev/ttyUSB0 o COM5")
    ap.add_argument("bin", nargs="?", default=DEFAULT_BIN, help="firmware .bin")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()
    if not args.port:
        ap.error("falta el puerto: pio run -t upload --upload-port /dev/ttyUSB0")
    flash(args.port, os.path.abspath(args.bin), args.baud)


if __name__ == "__main__":
    main()
