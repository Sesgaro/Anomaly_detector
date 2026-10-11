# Copia el .bin compilado con el nombre que reconoce el bootloader de BTT
# para actualizar la pantalla desde la microSD.
import shutil
from os.path import join

Import("env")

# Mismo formato que los .bin oficiales (BIGTREE_TFT35_V3.0_B1.<versión>.x.bin)
SD_NAME = "BIGTREE_TFT35_V3.0_B1.01.x.bin"


def copy_for_sd(target, source, env):
    src = target[0].get_abspath()
    dst = join(env.subst("$BUILD_DIR"), SD_NAME)
    shutil.copyfile(src, dst)
    print("Firmware para la microSD: " + dst)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_for_sd)
