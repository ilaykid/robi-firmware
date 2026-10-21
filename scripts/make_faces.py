"""Robi's face: 21 expressions for the xiaozhi emoji collection.
Original design: glowing cyan capsule eyes on a black AMOLED screen,
pink cheeks, simple mouth. Transparent PNG, drawn 4x and downsampled."""
import math, os, sys
from PIL import Image, ImageDraw, ImageFilter

S = 256          # output size
K = 4            # supersampling
W = S * K
EYE = (64, 224, 255)      # cyan glow
EYE_HI = (220, 250, 255)
PINK = (255, 120, 170)
MOUTH = (64, 224, 255)
TEAR = (90, 170, 255)
RED = (255, 80, 120)
OUT = sys.argv[1] if len(sys.argv) > 1 else "faces"
os.makedirs(OUT, exist_ok=True)

LX, RX, EY = 0.34, 0.66, 0.42      # eye centres (fractions)
MY = 0.70                           # mouth y


def P(x, y):  # fraction -> px
    return (x * W, y * W)


def new():
    return Image.new("RGBA", (W, W), (0, 0, 0, 0))


def glow(img, color=EYE, radius=10):
    """soft glow behind the drawing"""
    a = img.split()[3].filter(ImageFilter.GaussianBlur(radius * K))
    g = Image.new("RGBA", img.size, color + (0,))
    g.putalpha(a.point(lambda v: int(v * 0.55)))
    return Image.alpha_composite(g, img)


def capsule(d, cx, cy, w, h, color=EYE, hi=True):
    x0, y0 = P(cx - w / 2, cy - h / 2); x1, y1 = P(cx + w / 2, cy + h / 2)
    d.rounded_rectangle([x0, y0, x1, y1], radius=min(x1 - x0, y1 - y0) / 2, fill=color)
    if hi:  # small highlight
        hx, hy = P(cx - w * 0.18, cy - h * 0.22); r = w * W * 0.16
        d.ellipse([hx - r, hy - r, hx + r, hy + r], fill=EYE_HI)


def arc(d, cx, cy, w, h, start, end, width=0.045, color=EYE):
    x0, y0 = P(cx - w / 2, cy - h / 2); x1, y1 = P(cx + w / 2, cy + h / 2)
    d.arc([x0, y0, x1, y1], start, end, fill=color, width=int(width * W))
    # round caps
    for ang in (start, end):
        a = math.radians(ang); r = width * W / 2
        px = (x0 + x1) / 2 + (x1 - x0) / 2 * math.cos(a); py = (y0 + y1) / 2 + (y1 - y0) / 2 * math.sin(a)
        d.ellipse([px - r, py - r, px + r, py + r], fill=color)


def line(d, a, b, width=0.045, color=EYE):
    d.line([P(*a), P(*b)], fill=color, width=int(width * W))
    for p in (a, b):
        x, y = P(*p); r = width * W / 2
        d.ellipse([x - r, y - r, x + r, y + r], fill=color)


def cheeks(d, alpha=150, size=0.075, y=0.60):
    for cx in (0.20, 0.80):
        x, yy = P(cx, y); r = size * W
        d.ellipse([x - r * 1.3, yy - r * 0.75, x + r * 1.3, yy + r * 0.75], fill=PINK + (alpha,))


def smile(d, w=0.17, h=0.16, y=MY, width=0.042):
    arc(d, 0.5, y - h / 2, w, h, 15, 165, width, MOUTH)


def frown(d, w=0.15, h=0.12, y=MY + 0.04):
    arc(d, 0.5, y + h / 2, w, h, 200, 340, 0.042, MOUTH)


def open_mouth(d, w=0.16, h=0.13, y=MY):
    x0, y0 = P(0.5 - w / 2, y - h / 2); x1, y1 = P(0.5 + w / 2, y + h / 2)
    d.chord([x0, y0 - (y1 - y0), x1, y1], 0, 180, fill=MOUTH)


def o_mouth(d, r=0.045, y=MY):
    x, yy = P(0.5, y); rr = r * W
    d.ellipse([x - rr, yy - rr * 1.2, x + rr, yy + rr * 1.2], outline=MOUTH, width=int(0.035 * W))


def heart(d, cx, cy, s, color=RED):
    pts = []
    for t in range(0, 360, 3):
        a = math.radians(t)
        x = 16 * math.sin(a) ** 3
        y = 13 * math.cos(a) - 5 * math.cos(2 * a) - 2 * math.cos(3 * a) - math.cos(4 * a)
        pts.append(P(cx + x * s / 32, cy - y * s / 32))
    d.polygon(pts, fill=color)


def happy_eyes(d, y=EY, w=0.17, h=0.22):
    for cx in (LX, RX):
        arc(d, cx, y + h / 2 - 0.02, w, h, 195, 345, 0.05)


def closed_eyes(d, y=EY):
    for cx in (LX, RX):
        arc(d, cx, y - 0.03, 0.16, 0.08, 20, 160, 0.045)


def open_eyes(d, w=0.13, h=0.22, y=EY, look=(0, 0)):
    for cx in (LX, RX):
        capsule(d, cx + look[0], y + look[1], w, h)


FACES = {}
def face(name):
    def reg(fn):
        FACES[name] = fn; return fn
    return reg


@face("neutral")
def _(d): open_eyes(d); smile(d, 0.12, 0.10); cheeks(d, 90)

@face("happy")
def _(d): happy_eyes(d); smile(d); cheeks(d)

@face("laughing")
def _(d): happy_eyes(d); open_mouth(d); cheeks(d, 170)

@face("funny")
def _(d):
    line(d, (LX - 0.07, EY - 0.06), (LX + 0.06, EY)); line(d, (LX + 0.06, EY), (LX - 0.07, EY + 0.06))
    line(d, (RX + 0.07, EY - 0.06), (RX - 0.06, EY)); line(d, (RX - 0.06, EY), (RX + 0.07, EY + 0.06))
    open_mouth(d, 0.18, 0.15); cheeks(d, 170)

@face("sad")
def _(d):
    for cx, s in ((LX, 1), (RX, -1)):
        capsule(d, cx, EY + 0.04, 0.13, 0.15)
        line(d, (cx - 0.09 * s, EY - 0.07), (cx + 0.07 * s, EY - 0.11), 0.03)
    frown(d)

@face("angry")
def _(d):
    open_eyes(d, 0.13, 0.17, EY + 0.03)
    line(d, (LX - 0.09, EY - 0.10), (LX + 0.08, EY - 0.04), 0.045)
    line(d, (RX + 0.09, EY - 0.10), (RX - 0.08, EY - 0.04), 0.045)
    frown(d, 0.16, 0.06)

@face("crying")
def _(d):
    closed_eyes(d); frown(d)
    for cx in (LX, RX):
        x, y = P(cx, EY + 0.12); r = 0.035 * W
        d.ellipse([x - r, y, x + r, y + 2 * r], fill=TEAR)
        d.polygon([(x - r, y + r), (x + r, y + r), (x, y - r * 1.4)], fill=TEAR)

@face("loving")
def _(d):
    for cx in (LX, RX): heart(d, cx, EY, 0.22)
    smile(d); cheeks(d, 170)

@face("embarrassed")
def _(d):
    open_eyes(d, 0.12, 0.17, EY, (0, 0.03)); cheeks(d, 230, 0.095)
    pts = [P(0.42 + i * 0.04, MY + (0.02 if i % 2 else -0.0)) for i in range(5)]
    d.line(pts, fill=MOUTH, width=int(0.035 * W), joint="curve")

@face("surprised")
def _(d):
    for cx in (LX, RX):
        x, y = P(cx, EY); r = 0.085 * W
        d.ellipse([x - r, y - r, x + r, y + r], fill=EYE)
        h = r * 0.35; d.ellipse([x - r * 0.45 - h, y - r * 0.45 - h, x - r * 0.45 + h, y - r * 0.45 + h], fill=EYE_HI)
    o_mouth(d)

@face("shocked")
def _(d):
    for cx in (LX, RX):
        x, y = P(cx, EY - 0.02); r = 0.10 * W
        d.ellipse([x - r, y - r, x + r, y + r], outline=EYE, width=int(0.035 * W))
        p = r * 0.32; d.ellipse([x - p, y - p, x + p, y + p], fill=EYE)
    o_mouth(d, 0.07, MY + 0.03)

@face("thinking")
def _(d):
    open_eyes(d, 0.12, 0.20, EY, (0.04, -0.03))
    line(d, (0.44, MY + 0.01), (0.58, MY - 0.02), 0.035)
    for i, r in enumerate((0.018, 0.024, 0.03)):
        x, y = P(0.78 + i * 0.05, 0.22 - i * 0.05); rr = r * W
        d.ellipse([x - rr, y - rr, x + rr, y + rr], fill=EYE)

@face("winking")
def _(d):
    capsule(d, LX, EY, 0.13, 0.22); arc(d, RX, EY + 0.09, 0.17, 0.20, 195, 345, 0.05)
    smile(d); cheeks(d)

@face("cool")
def _(d):
    x0, y0 = P(0.18, EY - 0.08); x1, y1 = P(0.82, EY + 0.08)
    d.rounded_rectangle([x0, y0, x1, y1], radius=0.07 * W, fill=EYE)
    d.rectangle([P(0.47, EY - 0.09)[0], y0 - 2, P(0.53, EY)[0], y1 + 2], fill=(0, 0, 0, 0))
    line(d, (0.20, EY - 0.075), (0.80, EY - 0.075), 0.03)
    for cx in (LX, RX):
        x, y = P(cx - 0.06, EY - 0.03); r = 0.02 * W
        d.ellipse([x - r, y - r, x + r, y + r], fill=EYE_HI)
    arc(d, 0.53, MY - 0.03, 0.12, 0.06, 20, 160, 0.035, MOUTH)

@face("relaxed")
def _(d): closed_eyes(d); smile(d, 0.13, 0.11); cheeks(d, 120)

@face("delicious")
def _(d):
    happy_eyes(d); smile(d, 0.18, 0.10)
    x, y = P(0.54, MY + 0.03); r = 0.045 * W
    d.ellipse([x - r, y - r * 0.4, x + r, y + r * 1.4], fill=PINK + (255,)); cheeks(d)

@face("kissy")
def _(d):
    capsule(d, LX, EY, 0.13, 0.22); arc(d, RX, EY + 0.09, 0.17, 0.20, 195, 345, 0.05)
    heart(d, 0.5, MY, 0.10, PINK + (255,)); heart(d, 0.80, 0.28, 0.12, RED); cheeks(d)

@face("confident")
def _(d):
    for cx in (LX, RX):
        capsule(d, cx, EY + 0.03, 0.14, 0.13)
        d.rectangle([P(cx - 0.09, EY - 0.08), P(cx + 0.09, EY + 0.01)], fill=(0, 0, 0, 0))
    arc(d, 0.55, MY - 0.04, 0.15, 0.08, 10, 150, 0.04, MOUTH)

@face("sleepy")
def _(d):
    for cx in (LX, RX): line(d, (cx - 0.07, EY + 0.02), (cx + 0.07, EY + 0.02), 0.04)
    o_mouth(d, 0.03, MY + 0.01)
    for i, (sz, x, y) in enumerate(((0.07, 0.74, 0.26), (0.05, 0.84, 0.17))):
        a, b = P(x, y), P(x + sz, y)
        d.line([P(x, y), P(x + sz, y), P(x, y + sz), P(x + sz, y + sz)], fill=EYE, width=int(0.025 * W))

@face("silly")
def _(d):
    capsule(d, LX, EY, 0.15, 0.25); capsule(d, RX, EY + 0.02, 0.10, 0.13)
    smile(d, 0.18, 0.10)
    x, y = P(0.47, MY + 0.03); r = 0.045 * W
    d.ellipse([x - r, y - r * 0.4, x + r, y + r * 1.4], fill=PINK + (255,)); cheeks(d)

@face("confused")
def _(d):
    capsule(d, LX, EY + 0.03, 0.12, 0.17); capsule(d, RX, EY - 0.02, 0.12, 0.22)
    line(d, (RX - 0.07, EY - 0.17), (RX + 0.07, EY - 0.14), 0.03)
    pts = [P(0.40 + i * 0.05, MY + (0.025 if i % 2 else -0.01)) for i in range(5)]
    d.line(pts, fill=MOUTH, width=int(0.035 * W), joint="curve")


for name, fn in FACES.items():
    img = new(); d = ImageDraw.Draw(img)
    fn(d)
    img = glow(img, EYE, 6)
    img = img.resize((S, S), Image.LANCZOS)
    img.save(os.path.join(OUT, f"{name}.png"), optimize=True)
print(len(FACES), "faces ->", OUT)
