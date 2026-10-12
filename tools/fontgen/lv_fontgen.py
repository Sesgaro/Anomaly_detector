#!/usr/bin/env python3
"""
Generador mínimo de fuentes para LVGL 8 (formato lv_font_fmt_txt, 4 bpp, sin
compresión ni kerning), a partir de un TTF/WOFF con Pillow.

Sustituye a lv_font_conv (Node) para generar subconjuntos pequeños con tildes
y símbolos. Uso:

    python3 lv_fontgen.py --font Montserrat-Medium.ttf --size 16 \
        --range 0x20-0x7E --chars "áéíóúñ°" --name font_body -o font_body.c \
        [--fallback lv_font_icons]
"""
import argparse

from PIL import Image, ImageDraw, ImageFont


def parse_ranges(ranges, chars):
    cps = set()
    for r in ranges or []:
        for part in r.split(","):
            if "-" in part:
                a, b = part.split("-")
                cps.update(range(int(a, 0), int(b, 0) + 1))
            elif part:
                cps.add(int(part, 0))
    for ch in chars or "":
        cps.add(ord(ch))
    return sorted(cps)


def render_glyph(font, cp):
    ch = chr(cp)
    adv = font.getlength(ch)
    x0, y0, x1, y1 = font.getbbox(ch, anchor="ls")
    w, h = max(0, x1 - x0), max(0, y1 - y0)
    if w == 0 or h == 0:
        return adv, 0, 0, 0, 0, []
    img = Image.new("L", (w, h), 0)
    ImageDraw.Draw(img).text((-x0, -y0), ch, font=font, fill=255, anchor="ls")
    px = list(img.tobytes())
    return adv, w, h, x0, -y1, px


def pack4(px):
    out, acc, n = [], 0, 0
    for v in px:
        acc = (acc << 4) | (v >> 4)
        n += 1
        if n == 2:
            out.append(acc)
            acc, n = 0, 0
    if n:
        out.append(acc << 4)
    return out


def cmap_ranges(cps):
    """Agrupa los códigos en rangos contiguos (un cmap FORMAT0_TINY por rango)."""
    groups, start, prev = [], cps[0], cps[0]
    for c in cps[1:]:
        if c != prev + 1:
            groups.append((start, prev))
            start = c
        prev = c
    groups.append((start, prev))
    return groups


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--font", required=True)
    ap.add_argument("--size", type=int, required=True)
    ap.add_argument("--range", action="append")
    ap.add_argument("--chars", default="")
    ap.add_argument("--name", required=True)
    ap.add_argument("--fallback")
    ap.add_argument("-o", "--output", required=True)
    args = ap.parse_args()

    font = ImageFont.truetype(args.font, args.size)
    ascent, descent = font.getmetrics()
    cps = parse_ranges(args.range, args.chars)

    bitmap, dsc = [], []
    for cp in cps:
        adv, w, h, ox, oy, px = render_glyph(font, cp)
        dsc.append((len(bitmap), int(round(adv * 16)), w, h, ox, oy, cp))
        bitmap.extend(pack4(px))

    lines = [
        "/* Generado con tools/fontgen/lv_fontgen.py; no editar a mano. */",
        f"/* Fuente: {args.font.split('/')[-1]}, {args.size} px, 4 bpp, {len(cps)} glifos */",
        '#include "lvgl.h"',
        "",
    ]
    if args.fallback:
        lines.append(f"extern const lv_font_t {args.fallback};")
    lines.append("static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {")
    for i in range(0, len(bitmap), 16):
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in bitmap[i:i + 16]) + ",")
    lines.append("};")
    lines.append("")
    lines.append("static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {")
    lines.append("    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},")
    for bi, adv, w, h, ox, oy, cp in dsc:
        lines.append(f"    {{.bitmap_index = {bi}, .adv_w = {adv}, .box_w = {w}, .box_h = {h}, "
                     f".ofs_x = {ox}, .ofs_y = {oy}}}, /* U+{cp:04X} */")
    lines.append("};")
    lines.append("")

    groups = cmap_ranges(cps)
    lines.append("static const lv_font_fmt_txt_cmap_t cmaps[] = {")
    gid = 1
    for a, b in groups:
        n = b - a + 1
        lines.append(f"    {{.range_start = {a}, .range_length = {n}, .glyph_id_start = {gid}, "
                     ".unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, "
                     ".type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY},")
        gid += n
    lines.append("};")
    lines.append("")
    lines += [
        "static const lv_font_fmt_txt_dsc_t font_dsc = {",
        "    .glyph_bitmap = glyph_bitmap,",
        "    .glyph_dsc = glyph_dsc,",
        "    .cmaps = cmaps,",
        "    .kern_dsc = NULL,",
        "    .kern_scale = 0,",
        f"    .cmap_num = {len(groups)},",
        "    .bpp = 4,",
        "    .kern_classes = 0,",
        "    .bitmap_format = 0,",
        "};",
        "",
        f"const lv_font_t {args.name} = {{",
        "    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,",
        "    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,",
        f"    .line_height = {ascent + descent},",
        f"    .base_line = {descent},",
        "    .subpx = LV_FONT_SUBPX_NONE,",
        "    .underline_position = -2,",
        "    .underline_thickness = 1,",
        "    .dsc = &font_dsc,",
        f"    .fallback = {'&' + args.fallback if args.fallback else 'NULL'},",
        "};",
        "",
    ]
    with open(args.output, "w") as f:
        f.write("\n".join(lines))
    print(f"{args.output}: {len(cps)} glifos, {len(bitmap)} bytes de bitmap")


if __name__ == "__main__":
    main()
