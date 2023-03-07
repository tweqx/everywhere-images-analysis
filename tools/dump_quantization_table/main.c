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

  struct jpeg_decompress_struct in_cinfo;
  jvirt_barray_ptr* coeffs = read_DCT_coefficients(&in_cinfo, source_jpg);

  jpeg_component_info* components_info = in_cinfo.comp_info;

  for (int plane = 0; plane < 3; plane++) {
    JQUANT_TBL* quantization_table = components_info[plane].quant_table;

    printf("Quantization table for plane %d:\n", plane);
    for (int i = 0; i < DCTSIZE2; i++)
      printf("%d%c", quantization_table->quantval[i], i % 8 == 7 ? '\n' : ' ');
  }

  jpeg_finish_decompress(&in_cinfo);
  jpeg_destroy_decompress(&in_cinfo);
  fclose(source_jpg);
}
