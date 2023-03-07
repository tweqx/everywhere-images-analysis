#define _GNU_SOURCE 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#include "jpeg-6b-steg/jpeglib.h"

static int detect_quality_table(JQUANT_TBL* table, const unsigned int* basic_table) {
  for (int quality = 1; quality <= 100; quality++) {
    int percentage_scaling = quality < 50 ? 5000 / quality : 200 - quality*2;

    bool found_quality = true;
    for (int i = 0; i < DCTSIZE2; i++) {
      long expected_value = (basic_table[i] * percentage_scaling + 50) / 100;

      // Let's force baseline compatibility, as that's the default settings of JPEG 6b
      if (expected_value <= 0) expected_value = 1;
      if (expected_value > 255) expected_value = 255;
      
      if (table->quantval[i] != expected_value) {
        found_quality = false;
        break;
      }
    }

    if (found_quality)
      return quality;
  }

  return -1;
}

static int detect_quality(struct jpeg_decompress_struct* cinfo) {
  int quality = -1;

  // Luminance
  JQUANT_TBL* table = cinfo->quant_tbl_ptrs[0];
  if (table == NULL) {
    puts("Luminance table missing.");
    return -1;
  }

  static const unsigned int std_luminance_quant_tbl[DCTSIZE2] = {
    16,  11,  10,  16,  24,  40,  51,  61,
    12,  12,  14,  19,  26,  58,  60,  55,
    14,  13,  16,  24,  40,  57,  69,  56,
    14,  17,  22,  29,  51,  87,  80,  62,
    18,  22,  37,  56,  68, 109, 103,  77,
    24,  35,  55,  64,  81, 104, 113,  92,
    49,  64,  78,  87, 103, 121, 120, 101,
    72,  92,  95,  98, 112, 100, 103,  99
  };

  quality = detect_quality_table(table, std_luminance_quant_tbl);
  if (quality == -1) {
    puts("Could not detect quality of the luminance table.");
    return -1;
  }

  // Chrominance
  table = cinfo->quant_tbl_ptrs[1];
  if (table == NULL) {
    puts("Chrominance table missing.");
    return -1;
  }

  static const unsigned int std_chrominance_quant_tbl[DCTSIZE2] = {
    17,  18,  24,  47,  99,  99,  99,  99,
    18,  21,  26,  66,  99,  99,  99,  99,
    24,  26,  56,  99,  99,  99,  99,  99,
    47,  66,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99
  };

  int table_quality = detect_quality_table(table, std_chrominance_quant_tbl);
  if (table_quality != quality) {
    puts("Could not detect quality of the chrominance table.");
    return -1;
  }

  // Any other table?
  if (cinfo->quant_tbl_ptrs[2] != NULL) {
    puts("Extra quantization table, can't detect the quality of these.");
    return -1;
  }

  return quality;  
}

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

int main(int argc, char** argv) {
  if (argc != 2) {
    printf("Usage: %s <input.jpg>\n", argv[0]);
    return 0;
  }

  char* input_filename = argv[1];
  FILE* input = fopen(input_filename, "rb");
  if (input == NULL) {
    printf("Failed to open '%s'.\n", input_filename);
    return 1;
  }

  // Input image parameters
  int width, height;
  int quality_factor;
  jvirt_barray_ptr* coeffs;

  // Reading input image.
  struct jpeg_decompress_struct in_cinfo;
  struct jpeg_error_mgr jerr;

  in_cinfo.err = jpeg_std_error(&jerr);
  in_cinfo.err->error_exit = on_error_exit; 
  in_cinfo.err->output_message = on_output_message;
  jpeg_create_decompress(&in_cinfo);

  jpeg_stdio_src(&in_cinfo, input);

  (void)jpeg_read_header(&in_cinfo, TRUE);

  jpeg_calc_output_dimensions(&in_cinfo);

  width = in_cinfo.output_width;
  height = in_cinfo.output_height;

  coeffs = jpeg_read_coefficients(&in_cinfo);

  // Detecting quality
  quality_factor = detect_quality(&in_cinfo);

  //printf("Dimensions: %d x %d\n", width, height);
  //printf("Quality: %d\n", quality_factor);

  // Attempt to reproduce JPEG file
  char* output_filename;
  time_t rawtime;
  time(&rawtime);
  if (asprintf(&output_filename, "/tmp/%ld.jpg", rawtime) == -1) {
    puts("Out-of-memory!");
    // TODO: free everything
    return 1;
  }

  FILE* output = fopen(output_filename, "w+b");

  struct jpeg_compress_struct out_cinfo;

  out_cinfo.err = jpeg_std_error(&jerr);
  out_cinfo.err->error_exit = on_error_exit;
  out_cinfo.err->output_message = on_output_message;
  jpeg_create_compress(&out_cinfo);
  
  jpeg_stdio_dest(&out_cinfo, output);
  
  out_cinfo.image_width = width;
  out_cinfo.image_height = height;
  out_cinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&out_cinfo);

  jpeg_set_quality(&out_cinfo, quality_factor, TRUE);

  jpeg_write_coefficients(&out_cinfo, coeffs);
  (void)jpeg_finish_compress(&out_cinfo);

  // Comparing the two files 
  rewind(input);
  rewind(output);

  int c1, c2;
  do {
    c1 = fgetc(input);
    c2 = fgetc(output);
  } while (c1 != EOF && c2 != EOF && c1 == c2);  

  if (c1 == EOF && c2 == EOF)
    printf("%s could have been outputed by Outguess.\n", input_filename);
  else
    printf("%s wasn't outputed by Outguess.\n", input_filename);

  // Cleanup
  jpeg_destroy_compress(&out_cinfo);

  fclose(output);
  remove(output_filename);
  free(output_filename);

  (void)jpeg_finish_decompress(&in_cinfo);
  jpeg_destroy_decompress(&in_cinfo);

  fclose(input);
}
