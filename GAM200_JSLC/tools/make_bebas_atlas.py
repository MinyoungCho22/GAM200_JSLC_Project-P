from PIL import Image, ImageDraw, ImageFont
from pathlib import Path


CHARS = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    font_path = root / "Asset" / "fonts" / "BebasNeue-Regular.ttf"
    out_path = root / "Asset" / "fonts" / "Font_Outlined.png"
    backup_path = root / "Asset" / "fonts" / "Font_Outlined.backup.png"

    if not font_path.exists():
        raise FileNotFoundError(f"TTF not found: {font_path}")

    font_px = 64
    pad_x = 3
    baseline_top = 2

    font = ImageFont.truetype(str(font_path), font_px)

    widths = []
    for ch in CHARS:
        bbox = font.getbbox(ch)
        if bbox is None:
            w = int(max(8, round(font.getlength(ch))))
        else:
            w = int(max(1, bbox[2] - bbox[0]))
        widths.append(w + pad_x * 2)

    atlas_w = sum(widths)
    atlas_h = font_px + baseline_top + 8

    img = Image.new("RGBA", (atlas_w, atlas_h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    white = (255, 255, 255, 255)
    black = (0, 0, 0, 255)

    x = 0
    color = white
    for w in widths:
        for i in range(w):
            img.putpixel((x + i, 0), color)
        x += w
        color = black if color == white else white

    x = 0
    for ch, w in zip(CHARS, widths):
        draw.text((x + pad_x, 1 + baseline_top), ch, font=font, fill=(255, 255, 255, 255))
        x += w

    if out_path.exists() and not backup_path.exists():
        out_path.replace(backup_path)
    elif out_path.exists():
        out_path.unlink()

    img.save(out_path)
    print(f"Generated atlas: {out_path}")
    print(f"Size: {img.size[0]}x{img.size[1]}")
    if backup_path.exists():
        print(f"Backup: {backup_path}")


if __name__ == "__main__":
    main()
