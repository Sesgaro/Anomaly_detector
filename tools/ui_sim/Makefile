# Simulador de la interfaz de la pantalla: renderiza ui.c con LVGL en la PC y
# guarda capturas PNG en out/.  Uso: make -j8 png
LVGL ?= ../../display/.pio/libdeps/tft35_b1/lvgl
DSRC  = ../../display/src
OUT  ?= out
MEM  ?= 160
BUILD = build
SRCS  = sim_main.c $(DSRC)/ui.c $(DSRC)/ui_dial.c $(DSRC)/theme.c $(wildcard $(DSRC)/fonts/*.c) \
        $(shell find $(LVGL)/src -name '*.c')
OBJS  = $(patsubst %.c,$(BUILD)/%.o,$(subst ../,,$(SRCS)))
CFLAGS = -O1 -I. -I$(LVGL) -I$(DSRC) -I../../common -DLV_CONF_INCLUDE_SIMPLE \
         -DSIM_MEM_KB=$(MEM) '-DDISPLAY_FW_VERSION="0.2"' -w

ui_sim: $(OBJS)
	$(CC) $(OBJS) -lm -o $@

$(BUILD)/%.o:
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $(filter %/$(notdir $(basename $@)).c $(notdir $(basename $@)).c,$(SRCS)) -o $@

png: ui_sim
	mkdir -p $(OUT) && timeout 60 ./ui_sim $(OUT) && \
	python3 -c "import glob; from PIL import Image; [Image.open(f).save(f[:-4]+'.png') for f in glob.glob('$(OUT)/*.ppm')]" && \
	rm -f $(OUT)/*.ppm

clean:
	rm -rf $(BUILD) ui_sim $(OUT)
