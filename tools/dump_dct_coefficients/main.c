#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>

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

int min(int a, int b) {
  return a > b ? b : a;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    printf("Usage: %s <image.jpg>\n", argv[0]);
    return 1;
  }

  FILE* source_jpg = fopen(argv[1], "rb");
  if (source_jpg == NULL) {
    printf("Error: cannot open '%s'!\n", argv[1]);
    return 1;
  }

  struct jpeg_decompress_struct source_cinfo;
  jvirt_barray_ptr* source_coeffs = read_DCT_coefficients(&source_cinfo, source_jpg);

  fclose(source_jpg);
 
  jpeg_component_info* source_info = source_cinfo.comp_info;

  int width_blocks = source_info[1].width_in_blocks;
  int height_blocks = source_info[1].height_in_blocks;

  for (int by = 0; by < height_blocks; by++) {
    for (int bx = 0; bx < width_blocks; bx++) {
      jvirt_barray_ptr source_component_coeffs;
      JBLOCKROW source_row_coeffs;
      JCOEFPTR source_block_coeffs;

      for (int plane = 0; plane < 1; plane++) { // Only chrominance plane
        int width_blocks = source_info[plane].width_in_blocks;
        int height_blocks = source_info[plane].height_in_blocks;
  
        int h_sampling_vector = source_info[plane].h_samp_factor;
        int v_sampling_vector = source_info[plane].v_samp_factor;
  
        for (int sub_by = v_sampling_vector*by; sub_by < min(height_blocks, v_sampling_vector*(by + 1)); sub_by++) {
          for (int sub_bx = h_sampling_vector*bx; sub_bx < min(width_blocks, h_sampling_vector*(bx + 1)); sub_bx++) {
            source_component_coeffs = source_coeffs[plane];
            source_row_coeffs = source_cinfo.mem->access_virt_barray((j_common_ptr)&source_cinfo, source_component_coeffs, sub_by, 1, TRUE)[0];
            source_block_coeffs = source_row_coeffs[sub_bx];

            printf("Plane: %d, Block (%d, %d):\n", plane, sub_bx, sub_by);

            for (int i = 0; i < DCTSIZE2; i++)
              printf("%d%c", source_block_coeffs[i], (i + 1) % 8 == 0 ? '\n' : ' ');
          }
        }
      }
    }
  }

  /* Cleanup */
  jpeg_finish_decompress(&source_cinfo);
  jpeg_destroy_decompress(&source_cinfo);
}
