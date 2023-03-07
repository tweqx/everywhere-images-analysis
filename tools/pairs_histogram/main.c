#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <byteswap.h>
#include <stdbool.h>

#include "jpeg-6b-steg/jpeglib.h"

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

int min(int a, int b) {
  return a < b ? a : b;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    printf("Usage: %s <image.jpg>\n", argv[0]);
    return 0;
  }

  char* input_filename = argv[1];
  FILE* input = fopen(input_filename, "rb");
  if (input == NULL) {
    printf("Cannot open '%s'.\n", input_filename);
    return 1;
  }

  /* Reading input image. */
  struct jpeg_decompress_struct in_cinfo;
  jvirt_barray_ptr* coeffs = read_DCT_coefficients(&in_cinfo, input);

  // Images produced by Outguess have an known DCT "structure"
  assert(in_cinfo.num_components == 3 && in_cinfo.jpeg_color_space == JCS_YCbCr);

  jpeg_component_info* components_info = in_cinfo.comp_info;
  assert(components_info[0].h_samp_factor == 2 && components_info[0].v_samp_factor == 2 &&
         components_info[1].h_samp_factor == 1 && components_info[1].v_samp_factor == 1 &&
         components_info[2].h_samp_factor == 1 && components_info[2].v_samp_factor == 1);

  /* Reading the coefficients */
  int width_blocks = components_info[1].width_in_blocks;
  int height_blocks = components_info[1].height_in_blocks;

  unsigned total_histogram[256] = { 0 };

  puts("Per-block histogram :");

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
  
            unsigned histogram[256] = {0};

            for (int i = 0; i < DCTSIZE2; i++) {
              histogram[128 + (block_coeffs[i] >> 1)]++;
              total_histogram[128 + (block_coeffs[i] >> 1)]++;
            }

            printf("(%d, %d, plane = %d): ", sub_bx, sub_by, plane);
            for (int i = 0; i < 256; i++)
              printf("%d ", histogram[i]);
            printf("\n");
          }
        }
      }
    }
  }

  puts("Total histogram:");
  for (int i = 0; i < 256; i++)
    printf("%d ", total_histogram[i]);
  printf("\n");

  unsigned count = 0;
  for (int i = 0; i < 256; i++)
    count += total_histogram[i];
  printf("Coefficient count: %d\n", count);

  jpeg_finish_decompress(&in_cinfo);
  jpeg_destroy_decompress(&in_cinfo);
  fclose(input); 
}

