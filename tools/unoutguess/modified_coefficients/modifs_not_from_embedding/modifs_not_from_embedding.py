from PIL import Image

title = "agrippa"

data_embedding_modifications = Image.open(f"./{title}_data_blocks.png")
modifications_observed = Image.open(f"./{title}_black_modifications_detected.png")

img = Image.open(f"./{title}.jpg")

RED = (255, 0, 0)

for i in range(img.size[0]):
  for j in range(img.size[1]):
    c1 = data_embedding_modifications.getpixel((i, j))
    c2 = modifications_observed.getpixel((i, j))

    if c2 == RED and c1 != RED:
      img.putpixel((i, j), RED)

img.save(f"./{title}_non_embedding_changes.png")
