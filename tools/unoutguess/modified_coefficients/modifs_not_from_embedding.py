from PIL import Image

data_embedding_modifications = Image.open("./brittanica_data_blocks.png")
modifications_observed = Image.open("./brittanica_black_modifications_detected.png")

img = Image.open("./brittanica.jpg")

RED = (255, 0, 0)

for i in range(img[0]):
  for j in range(img[1]):
    c1 = data_embedding_modifications.getpixel((i, j))
    c2 = modifications_observed.getpixel((i, j))

    if c2 == RED and c1 != RED:
      img.putpixel((i, j), RED)

img.save("./brittanica_non_embedding_changes.png")
