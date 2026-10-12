# Carga el firmware de la pantalla por SWD con OpenOCD, solo si el chip
# conectado es el STM32F20x de la TFT35 (DEV_ID 0x411). Si el ST-Link ve otro
# chip (por ejemplo la STM32H745 de la Nucleo, DEV_ID 0x450), aborta sin
# escribir nada.
#
# Uso (lo llama `pio run -t upload`):
#   openocd -f interface/stlink.cfg -f target/stm32f2x.cfg \
#           -c "set FW <archivo.bin>" -f flash_tft35.tcl

# NRST no va cableado (en la Nucleo también resetea la H745): reset por
# software. Velocidad baja porque los cables dupont y la pista hacia la H745
# agregan capacitancia.
reset_config none
adapter speed 480

init

if {[catch {set idcode [lindex [read_memory 0xE0042000 32 1] 0]} err]} {
    echo "ERROR: no se pudo leer el ID del chip ($err)."
    echo "Revisa el cableado SWD y que la pantalla esté encendida."
    shutdown error
}

set devid [expr {$idcode & 0xFFF}]
if {$devid != 0x411} {
    echo [format "ERROR: el chip conectado tiene DEV_ID 0x%03X; la pantalla (STM32F20x) es 0x411." $devid]
    echo "No se escribió nada. Si es 0x450, el ST-Link está viendo la STM32H745 de la Nucleo."
    shutdown error
}

echo "Chip STM32F20x de la pantalla detectado (DEV_ID 0x411)."
echo "Escribiendo desde 0x08008000 (el bootloader de BTT en 0x08000000 no se toca)..."

# program solo borra los sectores que ocupa la imagen (desde 0x08008000)
program $FW 0x08008000 verify reset exit
