#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <byteswap.h>
#include <stdbool.h>

#include "jpeg-6b-steg/jpeglib.h"
#include "iterator.h"

struct jpeg_error_mgr jerr;

static void on_error_exit(j_common_ptr cinfo) {
	char buffer[JMSG_LENGTH_MAX];

	(*cinfo->err->format_message)(cinfo, buffer);

	jpeg_destroy(cinfo);

  puts(buffer);
}
static void on_output_message(j_common_ptr cinfo) {
	char buffer[JMSG_LENGTH_MAX];

	(*cinfo->err->format_message)(cinfo, buffer);

	puts(buffer);
}

static jvirt_barray_ptr* read_DCT_coefficients(struct jpeg_decompress_struct* cinfo, FILE* image) {
  cinfo->err = jpeg_std_error(&jerr);
  cinfo->err->error_exit = on_error_exit;
  cinfo->err->output_message = on_output_message;
  jpeg_create_decompress(cinfo);

  jpeg_stdio_src(cinfo, image);

  (void)jpeg_read_header(cinfo, TRUE);

  jpeg_calc_output_dimensions(cinfo);

  return jpeg_read_coefficients(cinfo);
}

typedef struct coeff_info {
  int plane; // Plane concerned (0 = Y, 1 = Cb, Cr = 2)
  int x, y; // Coords. of the block containing the coeffs.
  int index; // Coefficient index in natural order
} coeff_info;

typedef struct bitmap {
  uint8_t* bits;
  coeff_info* origins;
  int capacity;
  int count;
} bitmap;

bool bitmap_init(bitmap* bm) {
  memset(bm, 0, sizeof(*bm));

  bm->capacity = 1024;
  bm->bits = malloc(bm->capacity * sizeof(uint8_t));
  bm->origins = malloc(bm->capacity * sizeof(coeff_info));

  return bm->bits != NULL;
}
bool bitmap_append_bit(bitmap* bm, uint8_t bit, coeff_info origin) {
  bm->bits[bm->count] = bit;
  bm->origins[bm->count++] = origin;

  if (bm->count < bm->capacity)
    return true;

  // Realloc
  bm->capacity += 1024;
  uint8_t* new_bits = realloc(bm->bits, bm->capacity * sizeof(uint8_t));
  coeff_info* new_origins = realloc(bm->origins, bm->capacity * sizeof(coeff_info));

  if (new_bits == NULL || new_origins == NULL) {
    free(new_bits);
    free(new_origins);
    return false;
  }

  bm->origins = new_origins;
  bm->bits = new_bits;
  return true;
}
void bitmap_free(bitmap* bm) {
  free(bm->bits);
  free(bm->origins);
}
uint8_t bitmap_read_bit(bitmap* bm, unsigned int index) {
  assert(index < bm->count);

  coeff_info info = bm->origins[index];
  // To detect bits used to embed messages
  //printf("(%d, %d, %d, %d)\n", info.plane, info.x, info.y, info.index);

  return bm->bits[index] & 1;
}
coeff_info bitmap_get_origin(bitmap* bm, unsigned int index) {
  return bm->origins[index];
}

uint8_t read_bitmap_byte(bitmap* bm, iterator* it) {
  uint8_t byte = 0;

  for (unsigned int i = 0; i < 8; i++) {
    byte |= bitmap_read_bit(bm, ITERATOR_CURRENT(it)) << i;
    iterator_next(it);
  }

  return byte;
}

void read_data(bitmap* bm, iterator* it, struct arc4_stream* as, uint8_t* data, int length) {
  for (int i = 0; i < length; i++)
    data[i] = read_bitmap_byte(bm, it) ^ arc4_getbyte(as);
}

int min(int a, int b) {
  return a < b ? a : b;
}

int main(int argc, char** argv) {
  if (argc != 3) {
    printf("Usage: %s <image.jpg> <key 1>\n", argv[0]);
    return 0;
  }

  char* input_filename = argv[1];
  FILE* input = fopen(input_filename, "rb");
  if (input == NULL) {
    printf("Cannot open '%s'.\n", input_filename);
    return 1;
  }

  // TODO: error correction
  // TODO: second message/key

  /* Reading input image. */
  struct jpeg_decompress_struct in_cinfo;
  jvirt_barray_ptr* coeffs = read_DCT_coefficients(&in_cinfo, input);

  // Images produced by Outguess have an known DCT "structure"
  assert(in_cinfo.num_components == 3 && in_cinfo.jpeg_color_space == JCS_YCbCr);

  jpeg_component_info* components_info = in_cinfo.comp_info;
  assert(components_info[0].h_samp_factor == 2 && components_info[0].v_samp_factor == 2 &&
         components_info[1].h_samp_factor == 1 && components_info[1].v_samp_factor == 1 &&
         components_info[2].h_samp_factor == 1 && components_info[2].v_samp_factor == 1);

  // Storage for coefficients that can be used to hide data
  bitmap bm;
  if (!bitmap_init(&bm)) {
    puts("Out-of-memory error.");

    jpeg_finish_decompress(&in_cinfo);
    jpeg_destroy_decompress(&in_cinfo);
    fclose(input);

    return 1;
  }

  for (int plane = 0; plane < 3; plane++) {
    JQUANT_TBL* quantization_table = components_info[plane].quant_table;

    for (int i = 0; i < DCTSIZE2; i++)
      printf("%d%c", quantization_table->quantval[i], i % 8 == 7 ? '\n' : ' ');
  }

  // Size in blocks of the downsampled Cb and Cr planes
  int width_blocks = components_info[1].width_in_blocks;
  int height_blocks = components_info[1].height_in_blocks;

  for (int by = 0; by < height_blocks; by++) {
    for (int bx = 0; bx < width_blocks; bx++) {
      jvirt_barray_ptr component_coeffs;
      JBLOCKROW row_coeffs;
      JCOEFPTR block_coeffs;

      // Coefficient order: "natural" order
      // https://en.wikipedia.org/wiki/JPEG#/media/File:Dctjpeg.png
      //    0  1  2  3  4  5  6  7
      //    8  9 10 11 12 13 14 15
      //   16 17 18 19 20 21 22 23
      //   24 25 26 27 28 29 30 31
      //   32 33 34 35 36 37 38 39
      //   40 41 42 44 43 45 46 47
      //   48 49 50 51 52 53 54 55
      //   56 57 58 59 60 61 62 63

      for (int plane = 0; plane < 3; plane++) {
        int width_blocks = components_info[plane].width_in_blocks;
        int height_blocks = components_info[plane].height_in_blocks;
  
        int h_sampling_vector = components_info[plane].h_samp_factor;
        int v_sampling_vector = components_info[plane].v_samp_factor;
  
        for (int sub_by = v_sampling_vector*by; sub_by < min(height_blocks, v_sampling_vector*(by + 1)); sub_by++) {
          for (int sub_bx = h_sampling_vector*bx; sub_bx < min(width_blocks, h_sampling_vector*(bx + 1)); sub_bx++) {
            component_coeffs = coeffs[plane];
            row_coeffs = in_cinfo.mem->access_virt_barray((j_common_ptr)&in_cinfo, component_coeffs, sub_by, 1, TRUE)[0];
            block_coeffs = row_coeffs[sub_bx];
  
            //printf("\n(%d, %d, plane = %d): ", sub_bx, sub_by, plane);

            for (int i = 0; i < DCTSIZE2; i++) {
              //printf("%d ", block_coeffs[i]);

              if (block_coeffs[i] == 0 || block_coeffs[i] == 1)
                continue;

              //printf("%d, ", block_coeffs[i]);
  
              // Copy coefficient
              if (!bitmap_append_bit(&bm, block_coeffs[i], (coeff_info){ .plane = plane, .x = sub_bx, .y = sub_by, .index = i })) {
                puts("Out-of-memory error.");
  
                bitmap_free(&bm);
                jpeg_finish_decompress(&in_cinfo);
                jpeg_destroy_decompress(&in_cinfo);
                fclose(input);
  
                return 1;
              }
            }
          }
        }
      }
    }
  }

  printf("Usable coefficients: %d\n", bm.count);

  /* Decoding message */
  
  iterator it; // Iterator over the bitmap
  iterator_init(&it, argv[2], strlen(argv[2]));

  struct arc4_stream as; // Encryption stream
  arc4_initkey(&as, "Encryption", argv[2], strlen(argv[2]));
  
  struct header {
    uint16_t seed;
    uint16_t length;
  } hd;
  read_data(&bm, &it, &as, (uint8_t*)&hd, sizeof(hd));

  printf("Seed: %d\n", hd.seed);
  printf("Length: %d\n", hd.length);

  char* message = malloc(hd.length * sizeof(char)); 
  if (message == NULL) {
    bitmap_free(&bm);
    jpeg_finish_decompress(&in_cinfo);
    jpeg_destroy_decompress(&in_cinfo);
    fclose(input);

    return 1;
  }

  iterator_seed(&it, hd.seed);
  arc4_initkey(&as, "Encryption", argv[2], strlen(argv[2])); // Reset stream

  for (int i = 0; i < hd.length; i++) {
    iterator_adapt(&it, bm.count, hd.length - i);

    read_data(&bm, &it, &as, (uint8_t*)&message[i], sizeof(uint8_t));
  }

  printf("%s\n", message);

  bitmap_free(&bm);
  jpeg_finish_decompress(&in_cinfo);
  jpeg_destroy_decompress(&in_cinfo);
  fclose(input); 
}

