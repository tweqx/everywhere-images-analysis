from PIL import Image
import sys

if len(sys.argv) != 3:
  print(f"{sys.argv[0]} <tuples> <image>")
  raise SystemExit

img = Image.open(sys.argv[2])
out_coeff = Image.new("RGB", ((img.size[0] + 7) // 8 * 8, (img.size[1] + 7) // 8 * 8))
out_coeff.paste(img)
out_blocks = Image.new("RGB", img.size)
out_blocks.paste(img)

with open(sys.argv[1], "r") as f:
  for line in f:
    values = line.replace('(', '').replace(')', '').replace(' ', '')
    values = values.split(',')
    values = map(int, list(values))

    plane, bx, by, coeff = values
    x, y = bx * 8 + (coeff % 8), by * 8 + (coeff // 8)

    out_coeff.putpixel((x, y), (255, 0, 0))

    for i in range(8):
      for j in range(8):
        if bx * 8 + i >= out_blocks.size[0] or by * 8 + j >= out_blocks.size[1]:
          continue

        out_blocks.putpixel((bx * 8 + i, by * 8 + j), (255, 0, 0))

out_coeff.save("./out.png", "PNG")
out_blocks.save("./out_blocks.png", "PNG")
