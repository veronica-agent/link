#!/usr/bin/env python3
"""Render 128x64 Link UI stills + GIF. Layout matches view.c."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
DOCS.mkdir(exist_ok=True)

W, H = 128, 64
SCALE = 5
ORANGE = (255, 140, 0)
BLACK = (0, 0, 0)

HELV = "/System/Library/Fonts/Helvetica.ttc"


def fonts():
    primary = ImageFont.truetype(HELV, 11, index=1)
    secondary = ImageFont.truetype(HELV, 8, index=0)
    return primary, secondary


def screen(usb, mode, transport, bpm, divide, pinout):
    primary, secondary = fonts()
    im = Image.new("RGB", (W, H), ORANGE)
    d = ImageDraw.Draw(im)
    d.text((2, 12), "LINK", font=primary, fill=BLACK, anchor="ls")
    d.text((126, 2), usb, font=secondary, fill=BLACK, anchor="rt")
    d.text((2, 26), mode, font=secondary, fill=BLACK, anchor="ls")
    d.text((126, 16), transport, font=secondary, fill=BLACK, anchor="rt")
    d.text((2, 40), f"{bpm} BPM", font=secondary, fill=BLACK, anchor="ls")
    d.text((126, 30), divide, font=secondary, fill=BLACK, anchor="rt")
    d.text((2, 54), pinout, font=secondary, fill=BLACK, anchor="ls")
    d.text((126, 44), "L/R mode", font=secondary, fill=BLACK, anchor="rt")
    return im.resize((W * SCALE, H * SCALE), Image.Resampling.NEAREST)


def save(name, im):
    path = DOCS / name
    im.save(path)
    return path


def main():
    frames = [
        screen("USB", "SLAVE", "WAIT", 120, "1/1", "ORIGINAL"),
        screen("USB", "SLAVE", "RUN", 120, "1/1", "ORIGINAL"),
        screen("---", "TAP", "RUN", 120, "1/1", "ORIGINAL"),
    ]
    save("screen-wait.png", frames[0])
    save("screen-run.png", frames[1])
    save("screen-tap.png", frames[2])
    frames[0].save(
        DOCS / "screen.gif",
        save_all=True,
        append_images=frames[1:] + [frames[1]],
        duration=[900, 900, 1100, 900],
        loop=0,
    )


if __name__ == "__main__":
    main()
