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
  // A -[Gimp]> B -[Outguess]> C

  /* Source image coefficients - "A" */
  FILE* source_jpg = fopen("source.jpg", "rb");

  struct jpeg_decompress_struct source_cinfo;
  jvirt_barray_ptr* source_coeffs = read_DCT_coefficients(&source_cinfo, source_jpg);

  fclose(source_jpg);
 
  /* Resulting image coefficients - "B" */
  FILE* resulting_jpg = fopen("./resulting.jpg", "rb");

  struct jpeg_decompress_struct resulting_cinfo;
  jvirt_barray_ptr* resulting_coeffs = read_DCT_coefficients(&resulting_cinfo, resulting_jpg);
  fclose(resulting_jpg);

  /* Comparing DCTs */

  jpeg_component_info* source_info = source_cinfo.comp_info;
  jpeg_component_info* resulting_info = resulting_cinfo.comp_info;

  int width_blocks = resulting_info[1].width_in_blocks;
  int height_blocks = resulting_info[1].height_in_blocks;

  int q75_luminance_quantization_table[64] = {
    8, 6, 5, 8, 12, 20, 26, 31,
    6, 6, 7, 10, 13, 29, 30, 28,
    7, 7, 8, 12, 20, 29, 35, 28,
    7, 9, 11, 15, 26, 44, 40, 31,
    9, 11, 19, 28, 34, 55, 52, 39,
    12, 18, 28, 32, 41, 52, 57, 46,
    25, 32, 39, 44, 52, 61, 60, 51,
    36, 46, 48, 49, 56, 50, 52, 50
  };

  for (int by = 0; by < height_blocks; by++) {
    for (int bx = 0; bx < width_blocks; bx++) {
      jvirt_barray_ptr source_component_coeffs, resulting_component_coeffs;
      JBLOCKROW source_row_coeffs, resulting_row_coeffs;
      JCOEFPTR source_block_coeffs, resulting_block_coeffs;

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

            resulting_component_coeffs = resulting_coeffs[plane];
            resulting_row_coeffs = resulting_cinfo.mem->access_virt_barray((j_common_ptr)&resulting_cinfo, resulting_component_coeffs, sub_by, 1, TRUE)[0];
            resulting_block_coeffs = resulting_row_coeffs[sub_bx];
  
            for (int i = 0; i < DCTSIZE2; i++)
                printf("%d %d %d %d %d\n", source_block_coeffs[i], i, q75_luminance_quantization_table[i], resulting_block_coeffs[i], resulting_block_coeffs[i] ^ 1);
          }
        }
      }
    }
  }

  /* Cleanup */
  jpeg_finish_decompress(&resulting_cinfo);
  jpeg_destroy_decompress(&resulting_cinfo);
  jpeg_finish_decompress(&source_cinfo);
  jpeg_destroy_decompress(&source_cinfo);
}
