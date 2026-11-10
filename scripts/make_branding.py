#!/usr/bin/env python3
"""Generate Viridian Forge branding assets (icon + wordmark) as SVG and PNG.

Usage:  python3 scripts/make_branding.py
Needs:  rsvg-convert (librsvg) on PATH for the PNG exports.

The icon is vector art drawn from the founder's guitar photo
(resources/branding/reference/guitar_reference.jpg): a close-up of a vivid
green body with two black, direct-mounted humbuckers (silver screw poles and
slugs), six strings, and a black bridge at the bottom edge. It is redrawn as
clean vector art (not a traced photo) so it stays crisp from 16 px to 1024 px.
"""
import os
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "resources", "branding")

# Body green, matched to the reference photo (lit highlight -> shadowed edge).
BODY_HI = "#3fe064"
BODY_MID = "#16b03e"
BODY_LO = "#0a6e26"
ROUTE = "#075a1e"       # shadowed pickup-route cavity
PICKUP = "#141414"

N = 6
# String x positions at the top and bottom of the tile (slight convergence toward the neck).
TOP_X = [292 + i * 88 for i in range(N)]
BOT_X = [262 + i * 100 for i in range(N)]


def sx(i, y):
    t = y / 1024.0
    return TOP_X[i] * (1 - t) + BOT_X[i] * t


def screw_pole(x, y, r=22):
    return (f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="url(#pole)" stroke="#555" stroke-width="2"/>'
            f'<line x1="{x - r * 0.75:.1f}" y1="{y + 2:.1f}" x2="{x + r * 0.75:.1f}" y2="{y - 2:.1f}" '
            f'stroke="#2a2a2a" stroke-width="5" stroke-linecap="round"/>')


def slug_pole(x, y, r=22):
    return (f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="url(#pole)" stroke="#555" stroke-width="2"/>'
            f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r * 0.62:.1f}" fill="none" stroke="#8a8a8a" stroke-width="2"/>'
            f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r * 0.3:.1f}" fill="none" stroke="#8a8a8a" stroke-width="2"/>')


def humbucker(cy, screws_on_top):
    """Direct-mount humbucker in its routed cavity, with mounting ears like the photo."""
    w, coil_h, gap = 640, 118, 10
    h = coil_h * 2 + gap
    x, y = 512 - w / 2, cy - h / 2
    parts = [
        # routed cavity (darker green, slightly larger) + mounting-ear notches
        f'<rect x="{x - 26}" y="{y - 22}" width="{w + 52}" height="{h + 44}" rx="30" fill="{ROUTE}"/>',
        f'<rect x="{x - 66}" y="{cy - 46}" width="{w + 132}" height="92" rx="22" fill="{ROUTE}"/>',
        f'<rect x="{x - 58}" y="{cy - 30}" width="{w + 116}" height="60" rx="16" fill="#0c0c0c"/>',  # ears
        f'<circle cx="{x - 34}" cy="{cy}" r="9" fill="#2b2b2b"/><circle cx="{x + w + 34}" cy="{cy}" r="9" fill="#2b2b2b"/>',
        # baseplate shadow
        f'<rect x="{x - 6}" y="{y - 6}" width="{w + 12}" height="{h + 12}" rx="24" fill="#000" opacity="0.55"/>',
    ]
    for c in range(2):
        cy_c = y + c * (coil_h + gap)
        parts.append(f'<rect x="{x}" y="{cy_c}" width="{w}" height="{coil_h}" rx="{coil_h / 2}" fill="url(#coil)"/>')
        parts.append(f'<rect x="{x + 6}" y="{cy_c + 6}" width="{w - 12}" height="{coil_h * 0.35}" rx="{coil_h * 0.17}" fill="#ffffff" opacity="0.06"/>')
        py = cy_c + coil_h / 2
        screws = (c == 0) == screws_on_top
        for i in range(N):
            px = sx(i, py)
            parts.append(screw_pole(px, py) if screws else slug_pole(px, py))
    return "\n".join(parts)


def bridge():
    """Black fixed bridge entering from the bottom edge, as in the photo."""
    parts = [f'<rect x="150" y="900" width="724" height="200" rx="22" fill="#0e0e0e" stroke="#000" stroke-width="3"/>']
    for i in range(N):
        bx = sx(i, 960)
        parts.append(f'<rect x="{bx - 36:.1f}" y="930" width="72" height="110" rx="10" fill="#1d1d1d" stroke="#2e2e2e" stroke-width="2"/>')
        parts.append(f'<circle cx="{bx:.1f}" cy="975" r="9" fill="#3a3a3a"/>')
    return "\n".join(parts)


def icon_svg():
    strings = "\n".join(
        f'<line x1="{TOP_X[i]}" y1="-10" x2="{BOT_X[i]}" y2="1034" stroke="url(#steel)" '
        f'stroke-width="{7.5 - i * 0.9:.1f}" opacity="0.95"/>' for i in range(N))
    string_shadows = "\n".join(
        f'<line x1="{TOP_X[i] + 6}" y1="-10" x2="{BOT_X[i] + 9}" y2="1034" stroke="#000" '
        f'stroke-width="{7.5 - i * 0.9:.1f}" opacity="0.22"/>' for i in range(N))
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024">
  <defs>
    <linearGradient id="body" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="{BODY_HI}"/><stop offset="0.45" stop-color="{BODY_MID}"/><stop offset="1" stop-color="{BODY_LO}"/>
    </linearGradient>
    <linearGradient id="sheen" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#ffffff" stop-opacity="0"/>
      <stop offset="0.78" stop-color="#ffffff" stop-opacity="0"/>
      <stop offset="0.9" stop-color="#ffffff" stop-opacity="0.22"/>
      <stop offset="1" stop-color="#ffffff" stop-opacity="0"/>
    </linearGradient>
    <linearGradient id="coil" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#2a2a2a"/><stop offset="0.5" stop-color="{PICKUP}"/><stop offset="1" stop-color="#070707"/>
    </linearGradient>
    <radialGradient id="pole" cx="38%" cy="32%" r="70%">
      <stop offset="0" stop-color="#ffffff"/><stop offset="0.45" stop-color="#c9ccd0"/><stop offset="1" stop-color="#6e7277"/>
    </radialGradient>
    <linearGradient id="steel" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#8d9196"/><stop offset="0.5" stop-color="#f1f2f3"/><stop offset="1" stop-color="#8d9196"/>
    </linearGradient>
    <radialGradient id="vignette" cx="50%" cy="45%" r="75%">
      <stop offset="0.6" stop-color="#000" stop-opacity="0"/><stop offset="1" stop-color="#000" stop-opacity="0.35"/>
    </radialGradient>
    <clipPath id="tile"><rect width="1024" height="1024" rx="224"/></clipPath>
  </defs>
  <g clip-path="url(#tile)">
    <rect width="1024" height="1024" fill="url(#body)"/>
    <rect width="1024" height="1024" fill="url(#sheen)"/>
    {humbucker(250, screws_on_top=True)}
    {humbucker(660, screws_on_top=False)}
    {bridge()}
    {string_shadows}
    {strings}
    <rect width="1024" height="1024" fill="url(#vignette)"/>
    <rect width="1024" height="1024" rx="224" fill="none" stroke="#ffffff" stroke-opacity="0.12" stroke-width="8"/>
  </g>
</svg>'''


def wordmark_svg():
    return '''<svg xmlns="http://www.w3.org/2000/svg" width="840" height="220" viewBox="0 0 840 220">
  <text x="0" y="128" font-family="Inter, DejaVu Sans, sans-serif" font-weight="800" font-size="118"
        letter-spacing="14" fill="#1fc24a">VIRIDIAN</text>
  <text x="6" y="196" font-family="Inter, DejaVu Sans, sans-serif" font-weight="600" font-size="44"
        letter-spacing="21" fill="#c9ccd1">FORGE · AMP STUDIO</text>
</svg>'''


def render(svg_path, png_path, width):
    subprocess.run(["rsvg-convert", "-w", str(width), "-o", png_path, svg_path], check=True)


def main():
    os.makedirs(OUT, exist_ok=True)
    icon = os.path.join(OUT, "icon.svg")
    with open(icon, "w") as f:
        f.write(icon_svg())
    word = os.path.join(OUT, "logo_wordmark.svg")
    with open(word, "w") as f:
        f.write(wordmark_svg())
    for size in (1024, 512, 256, 64, 32):
        render(icon, os.path.join(OUT, f"icon_{size}.png"), size)
    render(word, os.path.join(OUT, "logo_wordmark.png"), 630)
    print("Branding written to", OUT)


if __name__ == "__main__":
    main()
