/* Imagen del firmware de la pantalla, incrustada al compilar
 * (ver scripts/embed_display.py). */
#include "display_fw_info.h"

    .section .rodata.display_fw, "a"
    .balign 4
    .global display_fw_image
    .global display_fw_image_end
display_fw_image:
    .incbin DISPLAY_FW_BIN_PATH
display_fw_image_end:
