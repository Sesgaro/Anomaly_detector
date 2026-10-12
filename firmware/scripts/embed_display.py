# Antes de compilar la H745: compila el firmware de la pantalla (display/) y
# lo incrusta en la imagen de la H745 (src/cm7/display_fw.S), junto con su
# tamaño y CRC32, para que la H745 pueda actualizar la pantalla por UART.
import os
import subprocess
import zlib

Import("env")

project_dir = env.subst("$PROJECT_DIR")
display_dir = os.path.normpath(os.path.join(project_dir, "..", "display"))
gen_dir = os.path.join(env.subst("$BUILD_DIR"), "gen")
os.makedirs(gen_dir, exist_ok=True)

print("Compilando el firmware de la pantalla para incrustarlo...")
subprocess.check_call([env.subst("$PYTHONEXE"), "-m", "platformio", "run",
                       "-d", display_dir, "-e", "tft35_b1", "-s"])

src_bin = os.path.join(display_dir, ".pio", "build", "tft35_b1", "firmware.bin")
with open(src_bin, "rb") as f:
    image = f.read()
image += b"\xFF" * (-len(image) % 4)
crc = zlib.crc32(image) & 0xFFFFFFFF

bin_path = os.path.join(gen_dir, "display_fw.bin")
with open(bin_path, "wb") as f:
    f.write(image)

header = os.path.join(gen_dir, "display_fw_info.h")
content = (
    "/* Generado por scripts/embed_display.py; no editar. */\n"
    "#define DISPLAY_FW_SIZE  %du\n"
    "#define DISPLAY_FW_CRC32 0x%08Xu\n"
    "#define DISPLAY_FW_BIN_PATH \"%s\"\n" % (len(image), crc, bin_path.replace("\\", "/"))
)
old = open(header).read() if os.path.exists(header) else ""
if old != content:          # solo reescribir si cambió, para no recompilar de más
    with open(header, "w") as f:
        f.write(content)

print("Firmware de la pantalla: %d bytes, CRC32 %08X" % (len(image), crc))
env.Append(CPPPATH=[gen_dir])
