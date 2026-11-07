#!/usr/bin/env python3
"""make_art.py <fonts dir> <screenshots dir> <out dir>: the README's art and the app icon, in the style of the
NFS Most Wanted 2012 and Spider-Man 2 ports' art.

  banner.jpg              2560x800, the title beside the app's Browse page
  social-preview.jpg      1280x640, for GitHub's social preview (Settings, General, Social preview)
  install-steps.png       2400x760, the four install steps (and install-steps.es.png, in Spanish)
  icon.png                512x512, the app's icon (resources/img/icon.jpg is the same at 256x256)

make_art.py post <fonts dir> <screenshots dir> <out.jpg> <en|es> <label> <shot.jpg=caption>...: a 1920x1080 picture
for announcing a version (Reddit, Facebook, Discord), with three screenshots and their captions.
make_art.py coming <fonts dir> <icon.png> <out.jpg> <en|es>: a 1920x1080 picture of what's planned.
make_art.py update <fonts dir> <icon.png> <out.jpg> <label> <title> <heading=line>...: a 1920x1080 picture of an
update's changes.

The screenshots dir holds browse.jpg (docs/images/screenshots). The font is Barlow Condensed (SIL Open Font
License 1.1), not included: the fonts dir holds BarlowCondensed-<weight>-<normal|italic>.ttf, e.g. from the
@fontsource/barlow-condensed package converted with fontTools."""
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageOps

BG = (10, 13, 24)          # night navy, near black
PANEL = (20, 25, 40)
LINE = (52, 60, 84)
RED = (220, 30, 46)
DARK_RED = (120, 14, 26)
WHITE = (255, 255, 255)
LIGHT = (222, 226, 236)
GRAY = (150, 158, 178)

FONTS = ""


def font(weight, italic, size):
    return ImageFont.truetype(os.path.join(FONTS, f"BarlowCondensed-{weight}-{'italic' if italic else 'normal'}.ttf"), size)


def spaced(d, xy, text, f, fill, spacing):
    x, y = xy
    for ch in text:
        d.text((x, y), ch, font=f, fill=fill)
        x += d.textlength(ch, font=f) + spacing
    return x


def fade_mask(w, h, start, end, power=1.6):
    """0 left of start, 255 right of end, eased between"""
    row = []
    for x in range(w):
        t = min(max((x - start) / max(end - start, 1), 0.0), 1.0)
        row.append(int(255 * (t ** power)))
    line = Image.new("L", (w, 1))
    line.putdata(row)
    return line.resize((w, h))


def vignette(img, strength=0.55):
    """darker toward the top and bottom edges"""
    w, h = img.size
    m = Image.new("L", (1, h))
    m.putdata([int(255 * strength * (abs(y / (h - 1) - 0.5) * 2) ** 2.2) for y in range(h)])
    shade = Image.new("RGB", (w, h), BG)
    img.paste(shade, (0, 0), m.resize((w, h)))


def badge(d, x, y, text, f, filled, pad_x=26, h=70):
    w = int(d.textlength(text, font=f)) + 2 * pad_x
    if filled:
        d.rectangle([x, y, x + w, y + h], fill=RED)
    else:
        d.rectangle([x, y, x + w, y + h], outline=(120, 128, 150), width=2)
    bbox = d.textbbox((0, 0), text, font=f)
    d.text((x + pad_x, y + (h - (bbox[3] - bbox[1])) // 2 - bbox[1]), text, font=f, fill=WHITE)
    return x + w


def red_rule(img, y, x0, x1, thick, fade_from):
    """a red line that fades out toward x1"""
    d = ImageDraw.Draw(img)
    for x in range(x0, x1):
        t = 1.0 if x < fade_from else max(0.0, 1 - (x - fade_from) / max(x1 - fade_from, 1))
        c = tuple(int(BG[i] + (RED[i] - BG[i]) * t) for i in range(3))
        d.line([(x, y), (x, y + thick - 1)], fill=c)


def screen_card(shot, width, angle):
    """the screenshot with a thin light frame, turned by angle degrees, and the mask to paste it with"""
    im = Image.open(shot).convert("RGB")
    h = int(width * im.height / im.width)
    im = im.resize((width, h), Image.LANCZOS)
    framed = Image.new("RGB", (width + 12, h + 12), (235, 238, 245))
    framed.paste(im, (6, 6))
    mask = Image.new("L", framed.size, 255)
    return framed.rotate(angle, Image.BICUBIC, expand=True), mask.rotate(angle, Image.BICUBIC, expand=True)


def shards(img, cx, cy, scale, angle):
    """red blades behind the card, the cut-paper look of the Persona games' menus"""
    layer = Image.new("RGB", img.size, BG)
    m = Image.new("L", img.size, 0)
    d = ImageDraw.Draw(layer)
    dm = ImageDraw.Draw(m)
    a = math.radians(angle)

    def pt(x, y):  # x, y in card units around the centre, turned with the card
        x, y = x * scale, y * scale
        return (cx + x * math.cos(a) + y * math.sin(a), cy - x * math.sin(a) + y * math.cos(a))

    blades = [
        ([(-0.46, -0.35), (0.58, -0.40), (0.55, 0.37), (-0.50, 0.41)], DARK_RED),       # a red sheet under the card
        ([(-0.60, 0.20), (0.02, 0.13), (-0.12, 0.47), (-0.64, 0.41)], RED),             # lower left blade
        ([(0.30, -0.47), (0.64, -0.51), (0.61, -0.20), (0.38, -0.25)], RED),            # upper right blade
    ]
    for poly, colour in blades:
        p = [pt(x, y) for x, y in poly]
        d.polygon(p, fill=colour)
        dm.polygon(p, fill=255)
    img.paste(layer, (0, 0), m)


def place_card(img, shot, cx, cy, width, angle):
    card, mask = screen_card(shot, width, angle)
    x, y = int(cx - card.width / 2), int(cy - card.height / 2)
    shadow = Image.new("L", img.size, 0)
    shadow.paste(mask, (x + 18, y + 24))
    shadow = shadow.filter(ImageFilter.GaussianBlur(22))
    img.paste(Image.new("RGB", img.size, (0, 0, 0)), (0, 0), shadow.point(lambda v: int(v * 0.7)))
    img.paste(card, (x, y), mask)


def banner(shot, out):
    W, H = 2560, 800
    img = Image.new("RGB", (W, H), BG)
    cx, cy, cw, ang = 2045, 400, 860, 5
    shards(img, cx, cy, cw * 1.04, ang)
    place_card(img, shot, cx, cy, cw, ang)
    # the card fades into the dark on its left, under the title
    dark = Image.new("RGB", (W, H), BG)
    img.paste(dark, (0, 0), ImageOps.invert(fade_mask(W, H, 1470, 1600, 1.0)))
    vignette(img, 0.5)
    d = ImageDraw.Draw(img)
    x0 = 140
    spaced(d, (x0, 96), "UNOFFICIAL HOMEBREW  ·  NO PC NEEDED", font(700, False, 46), RED, 9)
    d.text((x0 - 6, 150), "COMPENDIUM", font=font(900, True, 268), fill=WHITE)
    big = font(900, True, 196)
    d.text((x0, 404), "PERSONA", font=big, fill=RED)
    x = x0 + int(d.textlength("PERSONA", font=big)) + 34
    d.text((x, 446), "MOD MANAGER", font=font(700, True, 74), fill=LIGHT)
    d.text((x, 520), "FOR NINTENDO SWITCH", font=font(700, True, 74), fill=LIGHT)
    bf = font(700, False, 40)
    by = 642
    x = badge(d, x0, by, "BUILDS THE CPK ON THE CONSOLE", bf, True) + 20
    x = badge(d, x, by, "GAMEBANANA BUILT IN", bf, False) + 20
    badge(d, x, by, "PC MODS ADAPTED", bf, False)
    red_rule(img, by + 34, 0, x0 - 24, 3, 0)
    red_rule(img, by + 100, 0, x0 + 380, 2, 60)
    red_rule(img, H - 10, 0, W, 10, int(W * 0.55))
    img.save(os.path.join(out, "banner.jpg"), quality=92)


def social(shot, out):
    W, H = 1280, 640
    img = Image.new("RGB", (W, H), BG)
    cx, cy, cw, ang = 1095, 330, 530, 5
    shards(img, cx, cy, cw * 1.04, ang)
    place_card(img, shot, cx, cy, cw, ang)
    img.paste(Image.new("RGB", (W, H), BG), (0, 0), ImageOps.invert(fade_mask(W, H, 790, 850, 1.0)))
    vignette(img, 0.5)
    d = ImageDraw.Draw(img)
    x0 = 72
    spaced(d, (x0, 92), "UNOFFICIAL HOMEBREW", font(700, False, 30), RED, 6)
    d.text((x0 - 4, 128), "COMPENDIUM", font=font(900, True, 150), fill=WHITE)
    big = font(900, True, 112)
    d.text((x0, 274), "PERSONA", font=big, fill=RED)
    x = x0 + int(d.textlength("PERSONA", font=big)) + 20
    d.text((x, 290), "MOD MANAGER", font=font(700, True, 48), fill=LIGHT)
    d.text((x, 338), "FOR SWITCH", font=font(700, True, 48), fill=LIGHT)
    bf = font(700, False, 27)
    x = badge(d, x0, 446, "BUILDS THE CPK ON THE CONSOLE", bf, True, 18, 50) + 14
    badge(d, x, 446, "NO PC NEEDED", bf, False, 18, 50)
    d.text((x0, 532), "github.com/SantiBros/Compendium-NX", font=font(500, False, 30), fill=GRAY)
    red_rule(img, H - 8, 0, W, 8, int(W * 0.5))
    img.save(os.path.join(out, "social-preview.jpg"), quality=92)


# the four install steps' icons, drawn as lines in a box of s pixels at (x, y)
def icon_download(d, x, y, s, c, w):
    cx = x + s / 2
    d.line([(cx, y + s * 0.08), (cx, y + s * 0.62)], fill=c, width=w)
    d.line([(cx - s * 0.24, y + s * 0.40), (cx, y + s * 0.64), (cx + s * 0.24, y + s * 0.40)], fill=c, width=w, joint="curve")
    d.line([(x + s * 0.12, y + s * 0.70), (x + s * 0.12, y + s * 0.88), (x + s * 0.88, y + s * 0.88), (x + s * 0.88, y + s * 0.70)], fill=c, width=w, joint="curve")


def icon_sd(d, x, y, s, c, w):
    pts = [(x + s * 0.24, y + s * 0.06), (x + s * 0.64, y + s * 0.06), (x + s * 0.80, y + s * 0.22),
           (x + s * 0.80, y + s * 0.94), (x + s * 0.24, y + s * 0.94), (x + s * 0.24, y + s * 0.06)]
    d.line(pts, fill=c, width=w, joint="curve")
    for i in range(3):
        xx = x + s * (0.36 + i * 0.11)
        d.line([(xx, y + s * 0.18), (xx, y + s * 0.34)], fill=c, width=w)


def icon_play(d, x, y, s, c, w):
    d.rounded_rectangle([x + s * 0.04, y + s * 0.16, x + s * 0.96, y + s * 0.84], radius=s * 0.16, outline=c, width=w)
    d.polygon([(x + s * 0.42, y + s * 0.34), (x + s * 0.42, y + s * 0.66), (x + s * 0.66, y + s * 0.50)], outline=c, width=w)


def icon_cards(d, x, y, s, c, w):
    # a fanned hand of cards, the front one with a plus: pick mods to add
    d.rounded_rectangle([x + s * 0.06, y + s * 0.10, x + s * 0.50, y + s * 0.74], radius=s * 0.06, outline=c, width=w)
    d.rounded_rectangle([x + s * 0.20, y + s * 0.04, x + s * 0.66, y + s * 0.70], radius=s * 0.06, outline=c, width=w, fill=PANEL)
    d.rounded_rectangle([x + s * 0.40, y + s * 0.22, x + s * 0.94, y + s * 0.96], radius=s * 0.06, outline=c, width=w, fill=PANEL)
    cx, cy = x + s * 0.67, y + s * 0.59
    d.line([(cx - s * 0.13, cy), (cx + s * 0.13, cy)], fill=c, width=w)
    d.line([(cx, cy - s * 0.13), (cx, cy + s * 0.13)], fill=c, width=w)


STEPS = {
    "en": [("DOWNLOAD", [("get the ", 0), ("zip", 1), (" from the ", 0), ("Releases", 1), (" page, under Assets", 0)], icon_download),
           ("COPY TO SD", [("unzip it and copy its ", 0), ("switch", 1), (" folder to the root of your SD card", 0)], icon_sd),
           ("OPEN", [("hold ", 0), ("R", 1), (" while starting any game, then open ", 0), ("Compendium", 1)], icon_play),
           ("PICK MODS", [("press ", 0), ("A", 1), (" on a mod to install it, then start ", 0), ("Persona\u00a05\u00a0Royal", 1)], icon_cards)],
    "es": [("DESCARGA", [("baja el ", 0), ("zip", 1), (" de la página de ", 0), ("Releases", 1), (", en Assets", 0)], icon_download),
           ("COPIA A LA SD", [("descomprímelo y copia su carpeta ", 0), ("switch", 1), (" en la raíz de tu tarjeta SD", 0)], icon_sd),
           ("ABRE", [("mantén ", 0), ("R", 1), (" al abrir un juego y elige ", 0), ("Compendium", 1)], icon_play),
           ("ELIGE MODS", [("pulsa ", 0), ("A", 1), (" en un mod para instalarlo y abre ", 0), ("Persona\u00a05\u00a0Royal", 1)], icon_cards)],
}
FOOTER = {
    "en": "A Switch with Atmosphère and your own Persona 5 Royal. Nothing to do on a PC: the app builds the game's CPK itself.",
    "es": "Una Switch con Atmosphère y tu propio Persona 5 Royal. Nada que hacer en un PC: la app arma el CPK del juego sola.",
}


def wrap_runs(d, runs, fonts, width):
    """runs of (text, bold) into lines of (text, bold) pieces no wider than width, breaking at spaces"""
    words = []
    for text, bold in runs:
        parts = text.split(" ")
        for i, p in enumerate(parts):
            if p:
                words.append([p, bold, False])
            if i < len(parts) - 1:
                if words:
                    words[-1][2] = True  # a space follows
    lines, cur, cur_w = [], [], 0
    for word, bold, space in words:
        f = fonts[bold]
        ww = d.textlength(word, font=f)
        sw = d.textlength(" ", font=f) if space else 0
        if cur and cur_w + ww > width:
            lines.append(cur)
            cur, cur_w = [], 0
        cur.append((word + (" " if space else ""), bold))
        cur_w += ww + sw
    if cur:
        lines.append(cur)
    return lines


def steps(lang, out):
    W, H = 2400, 760
    img = Image.new("RGB", (W, H), BG)
    # a soft light from the top left
    glow = Image.new("RGB", (W, H), (26, 32, 52))
    img.paste(glow, (0, 0), fade_mask(W, H, -W, int(W * 0.9), 1.0).transpose(Image.FLIP_LEFT_RIGHT))
    d = ImageDraw.Draw(img)
    n, margin, gap = 4, 80, 66
    cw = (W - 2 * margin - (n - 1) * gap) // n
    ch, top = 476, 68
    title_f = font(800, True, 66)
    body = {0: font(500, False, 38), 1: font(700, False, 38)}
    for i, (title, runs, icon) in enumerate(STEPS[lang]):
        x = margin + i * (cw + gap)
        d.rounded_rectangle([x, top, x + cw, top + ch], radius=22, fill=PANEL, outline=LINE, width=2)
        # the number tab
        tw, th = 92, 80
        d.rounded_rectangle([x + cw - tw, top, x + cw, top + th], radius=22, fill=RED)
        d.rectangle([x + cw - tw, top + th - 22, x + cw, top + th], fill=RED)
        d.rectangle([x + cw - tw, top, x + cw - tw + 22, top + th], fill=RED)
        nf = font(800, True, 58)
        num = str(i + 1)
        nb = d.textbbox((0, 0), num, font=nf)
        d.text((x + cw - tw / 2 - (nb[2] - nb[0]) / 2 - nb[0], top + th / 2 - (nb[3] - nb[1]) / 2 - nb[1]), num, font=nf, fill=WHITE)
        icon(d, x + 44, top + 54, 96, LIGHT, 7)
        d.text((x + 44, top + 196), title, font=title_f, fill=WHITE)
        y = top + 290
        for line in wrap_runs(d, runs, body, cw - 88):
            xx = x + 44
            for text, bold in line:
                d.text((xx, y), text, font=body[bold], fill=WHITE if bold else GRAY)
                xx += d.textlength(text, font=body[bold])
            y += 48
        if i < n - 1:
            ax, ay = x + cw + gap / 2, top + ch / 2
            d.polygon([(ax - 12, ay - 22), (ax + 12, ay), (ax - 12, ay + 22), (ax - 4, ay)], fill=RED)
    fy = top + ch + 70
    d.rectangle([margin, fy + 22, margin + 46, fy + 28], fill=RED)
    d.text((margin + 70, fy), FOOTER[lang], font=font(500, False, 42), fill=LIGHT)
    red_rule(img, H - 8, 0, W, 8, int(W * 0.5))
    img.save(os.path.join(out, "install-steps.png" if lang == "en" else f"install-steps.{lang}.png"), optimize=True)


def icon(out, app_icon):
    """three fanned cards, the front one red with the download arrow, over the name"""
    S = 1024  # drawn at twice the size, then scaled down
    img = Image.new("RGB", (S, S), BG)
    # a soft light behind the cards
    glow = Image.new("L", (S, S), 0)
    ImageDraw.Draw(glow).ellipse([S * 0.18, S * 0.02, S * 0.82, S * 0.66], fill=110)
    img.paste(Image.new("RGB", (S, S), (34, 42, 68)), (0, 0), glow.filter(ImageFilter.GaussianBlur(S * 0.08)))

    def card(angle, fill, edge, arrow):
        cw, chh = int(S * 0.34), int(S * 0.50)
        c = Image.new("RGBA", (cw + 40, chh + 40), (0, 0, 0, 0))
        d = ImageDraw.Draw(c)
        d.rounded_rectangle([20, 20, 20 + cw, 20 + chh], radius=S * 0.035, fill=fill, outline=edge, width=int(S * 0.012))
        d.rounded_rectangle([20 + S * 0.03, 20 + S * 0.03, 20 + cw - S * 0.03, 20 + chh - S * 0.03], radius=S * 0.02,
                            outline=edge, width=int(S * 0.006))
        if arrow:
            ax, ay, s = 20 + cw / 2, 20 + chh * 0.47, S * 0.11
            d.rectangle([ax - s * 0.26, ay - s * 1.05, ax + s * 0.26, ay + s * 0.05], fill=WHITE)
            d.polygon([(ax - s * 0.75, ay - s * 0.05), (ax + s * 0.75, ay - s * 0.05), (ax, ay + s * 0.80)], fill=WHITE)
            d.rectangle([ax - s * 0.85, ay + s * 1.02, ax + s * 0.85, ay + s * 1.24], fill=WHITE)
        return c.rotate(angle, Image.BICUBIC, expand=True)

    cx, cy = S * 0.5, S * 0.36
    for angle, fill, edge, arrow, dx in [(16, (30, 36, 56), (110, 118, 140), False, -0.15),
                                         (6, (44, 52, 76), (170, 176, 196), False, -0.06),
                                         (-8, RED, WHITE, True, 0.07)]:
        c = card(angle, fill, edge, arrow)
        img.paste(c, (int(cx + dx * S - c.width / 2), int(cy - c.height / 2)), c)
    d = ImageDraw.Draw(img)
    f = font(900, True, int(S * 0.215))
    while d.textlength("COMPENDIUM", font=f) > S * 0.90:
        f = font(900, True, f.size - 4)
    tw = d.textlength("COMPENDIUM", font=f)
    d.text(((S - tw) / 2, S * 0.655), "COMPENDIUM", font=f, fill=WHITE)
    sf = font(700, False, int(S * 0.075))
    sub = "PERSONA MODS"
    spaced_w = sum(d.textlength(ch, font=sf) for ch in sub) + (len(sub) - 1) * S * 0.012
    spaced(d, ((S - spaced_w) / 2, S * 0.885), sub, sf, RED, S * 0.012)
    img = img.resize((512, 512), Image.LANCZOS)
    img.save(os.path.join(out, "icon.png"), optimize=True)
    if app_icon:
        img.resize((256, 256), Image.LANCZOS).save(app_icon, quality=95)


POST_TEXT = {
    "en": ("MOD MANAGER FOR NINTENDO SWITCH", ["BUILDS THE CPK ON THE CONSOLE", "GAMEBANANA BUILT IN", "NO PC NEEDED"]),
    "es": ("GESTOR DE MODS PARA NINTENDO SWITCH", ["ARMA EL CPK EN LA CONSOLA", "GAMEBANANA INTEGRADO", "SIN PC"]),
}


def post(shots, out, lang, label, pictures):
    """1920x1080, for announcing a version: the title, a label (the version), and three screenshots with
    captions. pictures: [(file in the screenshots dir, caption)]."""
    W, H = 1920, 1080
    img = Image.new("RGB", (W, H), BG)
    glow = Image.new("RGB", (W, H), (26, 32, 52))
    img.paste(glow, (0, 0), fade_mask(W, H, -W, int(W * 0.9), 1.0).transpose(Image.FLIP_LEFT_RIGHT))
    d = ImageDraw.Draw(img)
    x0 = 100
    spaced(d, (x0, 62), label.upper(), font(700, False, 40), RED, 8)
    d.text((x0 - 6, 100), "COMPENDIUM", font=font(900, True, 212), fill=WHITE)
    big = font(900, True, 150)
    d.text((x0, 318), "PERSONA", font=big, fill=RED)
    x = x0 + int(d.textlength("PERSONA", font=big)) + 30
    sub, chips = POST_TEXT[lang]
    d.text((x, 384), sub, font=font(700, True, 60), fill=LIGHT)
    bf = font(700, False, 34)
    x = x0
    for i, c in enumerate(chips):
        x = badge(d, x, 488, c, bf, i == 0, 22, 58) + 16
    # three screenshots in a row, with a caption under each
    n, gap, top = len(pictures), 40, 590
    sw = (W - 2 * x0 - (n - 1) * gap) // n
    sh = sw * 9 // 16
    cf = font(600, False, 34)
    for i, (name, caption) in enumerate(pictures):
        x = x0 + i * (sw + gap)
        shot = Image.open(os.path.join(shots, name)).convert("RGB").resize((sw, sh), Image.LANCZOS)
        d.rectangle([x - 4, top - 4, x + sw + 3, top + sh + 3], fill=LINE)
        img.paste(shot, (x, top))
        d.rectangle([x, top + sh + 4, x + 46, top + sh + 10], fill=RED)
        lines = wrap_runs(d, [(caption, 0)], {0: cf}, sw)
        y = top + sh + 22
        for line in lines[:2]:
            d.text((x, y), "".join(t for t, _ in line), font=cf, fill=LIGHT)
            y += 42
    d.text((x0, H - 62), "github.com/SantiBros/Compendium-NX  ·  ko-fi.com/santibros", font=font(500, False, 32), fill=GRAY)
    red_rule(img, H - 10, 0, W, 10, int(W * 0.55))
    img.save(out, quality=92)


COMING = {
    "en": {"label": "COMPENDIUM  ·  COMING NEXT", "title": "WHAT'S NEXT",
           "ui": "A NEW INTERFACE", "ui_sub": "The app's look will change in future versions.",
           "games": "MORE GAMES FROM THE SERIES", "now": "AVAILABLE NOW"},
    "es": {"label": "COMPENDIUM  ·  PRÓXIMAMENTE", "title": "LO QUE VIENE",
           "ui": "UNA INTERFAZ NUEVA", "ui_sub": "El diseño de la app va a cambiar en próximas versiones.",
           "games": "MÁS JUEGOS DE LA SAGA", "now": "YA DISPONIBLE"},
}
COMING_GAMES = ["PERSONA 4 GOLDEN", "PERSONA 3 PORTABLE", "PERSONA 5 STRIKERS", "PERSONA 5 TACTICA",
                "PERSONA 4 ARENA ULTIMAX"]


def coming(out, lang, icon_png):
    """1920x1080, for announcements: what's planned (a new interface, the other Persona games)."""
    t = COMING[lang]
    W, H = 1920, 1080
    img = Image.new("RGB", (W, H), BG)
    glow = Image.new("RGB", (W, H), (26, 32, 52))
    img.paste(glow, (0, 0), fade_mask(W, H, -W, int(W * 0.9), 1.0).transpose(Image.FLIP_LEFT_RIGHT))
    # the icon on the right, over red blades
    shards(img, 1560, 470, 560, -6)
    ic = Image.open(icon_png).convert("RGB").resize((470, 470), Image.LANCZOS)
    mask = Image.new("L", ic.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, ic.width - 1, ic.height - 1], radius=60, fill=255)
    img.paste(ic, (1560 - 235, 470 - 235), mask)
    d = ImageDraw.Draw(img)
    x0 = 100
    spaced(d, (x0, 70), t["label"], font(700, False, 40), RED, 8)
    d.text((x0 - 6, 112), t["title"], font=font(900, True, 168), fill=WHITE)
    y = 330
    d.rectangle([x0, y + 22, x0 + 40, y + 32], fill=RED)
    d.text((x0 + 62, y), t["ui"], font=font(800, True, 70), fill=WHITE)
    d.text((x0 + 62, y + 88), t["ui_sub"], font=font(500, False, 40), fill=GRAY)
    y = 520
    d.rectangle([x0, y + 22, x0 + 40, y + 32], fill=RED)
    d.text((x0 + 62, y), t["games"], font=font(800, True, 70), fill=WHITE)
    bf = font(700, False, 36)
    x, yy = x0 + 62, y + 104
    x = badge(d, x, yy, "PERSONA 5 ROYAL  ·  " + t["now"], bf, True, 22, 62) + 16
    x, yy = x0 + 62, yy + 80
    for g in COMING_GAMES:
        w = int(d.textlength(g, font=bf)) + 44
        if x + w > 1180:
            x, yy = x0 + 62, yy + 80
        x = badge(d, x, yy, g, bf, False, 22, 62) + 16
    d.text((x0, H - 62), "github.com/SantiBros/Compendium-NX  ·  ko-fi.com/santibros", font=font(500, False, 32), fill=GRAY)
    red_rule(img, H - 10, 0, W, 10, int(W * 0.55))
    img.save(out, quality=92)


def update_card(out, label, title, items, icon_png):
    """1920x1080, for announcing an update: a label, the title and up to three changes as (heading, line)."""
    W, H = 1920, 1080
    img = Image.new("RGB", (W, H), BG)
    glow = Image.new("RGB", (W, H), (26, 32, 52))
    img.paste(glow, (0, 0), fade_mask(W, H, -W, int(W * 0.9), 1.0).transpose(Image.FLIP_LEFT_RIGHT))
    shards(img, 1600, 470, 520, -6)
    ic = Image.open(icon_png).convert("RGB").resize((430, 430), Image.LANCZOS)
    mask = Image.new("L", ic.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, ic.width - 1, ic.height - 1], radius=56, fill=255)
    img.paste(ic, (1600 - 215, 470 - 215), mask)
    d = ImageDraw.Draw(img)
    x0 = 100
    spaced(d, (x0, 70), label.upper(), font(700, False, 40), RED, 8)
    d.text((x0 - 6, 112), title.upper(), font=font(900, True, 168), fill=WHITE)
    y = 340
    hf, sf = font(800, True, 58), font(500, False, 36)
    for heading, line in items:
        d.rectangle([x0, y + 20, x0 + 40, y + 30], fill=RED)
        d.text((x0 + 62, y), heading.upper(), font=hf, fill=WHITE)
        yy = y + 74
        for row in wrap_runs(d, [(line, 0)], {0: sf}, 1060):
            d.text((x0 + 62, yy), "".join(t for t, _ in row), font=sf, fill=GRAY)
            yy += 46
        y = yy + 34
    d.text((x0, H - 62), "github.com/SantiBros/Compendium-NX  ·  ko-fi.com/santibros", font=font(500, False, 32), fill=GRAY)
    red_rule(img, H - 10, 0, W, 10, int(W * 0.55))
    img.save(out, quality=92)


if __name__ == "__main__":
    if len(sys.argv) >= 8 and sys.argv[1] == "post":
        # make_art.py post <fonts dir> <screenshots dir> <out.jpg> <en|es> <label> <shot.jpg=caption>...
        FONTS, shots, out, lang, label = sys.argv[2:7]
        post(shots, out, lang, label, [tuple(a.split("=", 1)) for a in sys.argv[7:]])
        sys.exit(0)
    if len(sys.argv) >= 9 and sys.argv[1] == "update":
        # make_art.py update <fonts dir> <icon.png> <out.jpg> <label> <title> <heading=line>...
        FONTS, icon_png, out, label, title = sys.argv[2:7]
        update_card(out, label, title, [tuple(a.split("=", 1)) for a in sys.argv[7:]], icon_png)
        sys.exit(0)
    if len(sys.argv) == 6 and sys.argv[1] == "coming":
        # make_art.py coming <fonts dir> <icon.png> <out.jpg> <en|es>
        FONTS, icon_png, out, lang = sys.argv[2:6]
        coming(out, lang, icon_png)
        sys.exit(0)
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    FONTS, shots, out = sys.argv[1:4]
    os.makedirs(out, exist_ok=True)
    banner(os.path.join(shots, "browse.jpg"), out)
    social(os.path.join(shots, "browse.jpg"), out)
    for lang in STEPS:
        steps(lang, out)
    here = os.path.dirname(os.path.abspath(__file__))
    icon(out, os.path.join(here, "..", "..", "resources", "img", "icon.jpg"))
