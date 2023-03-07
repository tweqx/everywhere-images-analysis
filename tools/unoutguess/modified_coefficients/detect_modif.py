from PIL import Image

img = Image.open("./brittanica.jpg")

for bx in range((img.size[0] + 7) // 8):
  for by in range((img.size[1] + 7) // 8):
    color = None

    for i in range(8):
      for j in range(8):
        x, y = 8 * bx + i, 8 * by + j

        if x >= img.size[0] or y >= img.size[1]:
          continue

        if color == None:
          color = img.getpixel((x, y))
        else:
          if color != img.getpixel((x, y)):
            color = False

    if color == (0, 0, 0):
      for i in range(8):
        for j in range(8):
          x, y = 8 * bx + i, 8 * by + j

          if x >= img.size[0] or y >= img.size[1]:
            continue

          img.putpixel((x, y), (255, 0, 0))

img.save("brittanica_detected.png")

