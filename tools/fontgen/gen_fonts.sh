#!/bin/sh
# Regenera las fuentes de la pantalla (display/src/fonts) con tildes e íconos.
# Usa Montserrat y FontAwesome que trae LVGL (requiere haber compilado display/
# una vez para que PlatformIO descargue la librería). Requiere Pillow.
set -e
cd "$(dirname "$0")/../.."
L=display/.pio/libdeps/tft35_b1/lvgl/scripts/built_in_font
G="python3 tools/fontgen/lv_fontgen.py"
ES="°áéíóúÁÉÍÓÚñÑ¿¡ü·–"
ICONS=0xF015,0xF304,0xF013,0xF0F3,0xF00C,0xF00D,0xF071,0xF093,0xF021,0xF124,0xF0E7,0xF001,0xF1EB,0xF068,0xF067,0xF06E,0xF2ED,0xF0C1
$G --font "$L/FontAwesome5-Solid+Brands+Regular.woff" --size 20 --range $ICONS --name font_icons -o display/src/fonts/font_icons.c
$G --font "$L/Montserrat-Medium.ttf" --size 15 --range 0x20-0x7E --chars "$ES" --name font_body --fallback font_icons -o display/src/fonts/font_body.c
$G --font "$L/Montserrat-Medium.ttf" --size 22 --range 0x20-0x7E --chars "$ES" --name font_title --fallback font_icons -o display/src/fonts/font_title.c
$G --font "$L/Montserrat-Medium.ttf" --size 32 --range 0x30-0x39 --chars " -.°%" --name font_display -o display/src/fonts/font_display.c
