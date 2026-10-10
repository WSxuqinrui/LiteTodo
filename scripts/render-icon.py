"""将原创清单图形渲染为多尺寸应用资源；不包含外部字体或图片。"""
from pathlib import Path
from PIL import Image, ImageDraw
ROOT = Path(__file__).resolve().parents[1]
image = Image.new("RGBA", (1024,1024), (0,0,0,0))
draw = ImageDraw.Draw(image)
scale = 16
draw.rounded_rectangle((32,32,992,992),radius=256,fill="#396ad4")
def stroke(points):
    points = [(x*scale,y*scale) for x,y in points]
    draw.line(points,fill="white",width=80,joint="curve")
    for x,y in points: draw.ellipse((x-40,y-40,x+40,y+40),fill="white")
stroke([(14,19),(38,19)]);stroke([(14,32),(27,32)]);stroke([(29,43),(40,51),(55,26)])
image.resize((512,512),Image.Resampling.LANCZOS).save(ROOT/"assets/app.png")
image.save(ROOT/"assets/app.ico",sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
image.save(ROOT/"assets/app.icns")
pixels = image.resize((64,64),Image.Resampling.LANCZOS).tobytes()
rows = [",".join(str(value) for value in pixels[offset:offset+32]) for offset in range(0,len(pixels),32)]
(ROOT/"assets/icon-pixels.h").write_text("#pragma once\n// 原创图标的 64x64 RGBA；Linux 窗口图标不依赖图片解码库。\ninline constexpr unsigned char app_icon_pixels[]={\n" + ",\n".join(rows) + "\n};\n",encoding="utf-8")
print("Original icon resources rendered")
