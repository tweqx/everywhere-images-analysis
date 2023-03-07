#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <unistd.h>

#include "jpeg-6b-steg/jpeglib.h"

void skip_whitespace(FILE* file) {
  char c;

  while (true) {
    c = fgetc(file);

    if (feof(file))
      return;
    else if (c == '#') {
      // skip comment
      while (fgetc(file) != '\n' && !feof(file));
    }
    else if (isspace(c)) {
      // skip whitespace
    }
    else {
      ungetc(c, file);
      return;
    }

  }
}

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
  /* Reading pixels from the reconstruction */
  FILE* recreation_file = fopen("./cicada_reconstruction.ppm", "rb");
  if (recreation_file == NULL) {
    printf("Error: cicada_reconstruction.ppm missing\n");
    return 1;
  }

  char P, number;
  if (fscanf(recreation_file, "%c%c", &P, &number) != 2 || P != 'P' || number != '3') {
    printf("Error: unexpected magic header bytes\n");
    fclose(recreation_file);
    return 1;
  }
  skip_whitespace(recreation_file);
  
  int x, y, max;
  if (fscanf(recreation_file, "%d", &x) != 1) {
    printf("Error: couldn't read recreation image width\n");
    fclose(recreation_file);
    return 1;
  }
  skip_whitespace(recreation_file);
  if (fscanf(recreation_file, "%d", &y) != 1) {
    printf("Error: couldn't read recreation image height\n");
    fclose(recreation_file);
    return 1;
  }
  skip_whitespace(recreation_file);
  if (fscanf(recreation_file, "%d", &max) != 1) {
    printf("Error: couldn't read recreation image max component value\n");
    fclose(recreation_file);
    return 1;
  }

  uint8_t* recreation_image = malloc(sizeof(uint8_t) * x * y * 3);
  if (recreation_image == NULL) {
    printf("Error: out-of-memory error!\n");
    fclose(recreation_file);
    return 1;
  }
  for (int j = 0; j < y; j++) {
    for (int i = 0; i < x; i++) {
      for (int c = 0; c < 3; c++) {
        skip_whitespace(recreation_file);
        if (fscanf(recreation_file, "%hhd", &recreation_image[3 * (j * x + i) + c]) != 1) {
          printf("Error: couldn't read pixel component\n");
          free(recreation_image);
          fclose(recreation_file);
          return 1;
        }
      }
    }
  }
  fclose(recreation_file);

  /* Running the JPEG compression algorithm */
  char recreation_jpg_filename[256];
  sprintf(recreation_jpg_filename, "/tmp/recreation_%d.jpg", rand());
  FILE* recreation_jpg = fopen(recreation_jpg_filename, "wb");
  if (recreation_jpg == NULL) {
    printf("Error: couldn't open output JPEG recreation file\n");
    free(recreation_image);
    return 1;
  }

  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);

  jpeg_stdio_dest(&cinfo, recreation_jpg);

  cinfo.image_width = x;
  cinfo.image_height = y;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, 75, TRUE);

  jpeg_start_compress(&cinfo, TRUE);

  while (cinfo.next_scanline < cinfo.image_height) {
    JSAMPROW row_pointer[1];
    row_pointer[0] = &recreation_image[cinfo.next_scanline * x * 3];
    (void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);

  free(recreation_image);
  fclose(recreation_jpg);

  /* Reading the DCT coefficients of the recreation */
  recreation_jpg = fopen(recreation_jpg_filename, "rb");

  struct jpeg_decompress_struct recreation_cinfo;
  jvirt_barray_ptr* recreation_coeffs = read_DCT_coefficients(&recreation_cinfo, recreation_jpg);

  fclose(recreation_jpg);
  unlink(recreation_jpg_filename);
  
  /* Reading the DCT coefficients of the reference "everywhere" image */
  FILE* reference_jpg = fopen("./agrippa.jpg", "rb");
  if (reference_jpg == NULL) {
    printf("Error: cannot open reference 'everywhere' image\n");
    jpeg_finish_decompress(&recreation_cinfo);
    jpeg_destroy_decompress(&recreation_cinfo);
    return 1;
  }

  struct jpeg_decompress_struct reference_cinfo;
  jvirt_barray_ptr* reference_coeffs = read_DCT_coefficients(&reference_cinfo, reference_jpg);
  fclose(reference_jpg);

  /* Comparing DCTs */
  int reference_y_offset = 19;

  jpeg_component_info* recreation_info = recreation_cinfo.comp_info;
  jpeg_component_info* reference_info = reference_cinfo.comp_info;

  int width_blocks = recreation_info[1].width_in_blocks;
  int height_blocks = recreation_info[1].height_in_blocks;

  for (int by = 0; by < height_blocks; by++) {
    for (int bx = 0; bx < width_blocks; bx++) {
      jvirt_barray_ptr recreation_component_coeffs, reference_component_coeffs;
      JBLOCKROW recreation_row_coeffs, reference_row_coeffs;
      JCOEFPTR recreation_block_coeffs, reference_block_coeffs;

      for (int plane = 0; plane < 3; plane++) {
        int width_blocks = recreation_info[plane].width_in_blocks;
        int height_blocks = recreation_info[plane].height_in_blocks;
  
        int h_sampling_vector = recreation_info[plane].h_samp_factor;
        int v_sampling_vector = recreation_info[plane].v_samp_factor;
  
        for (int sub_by = v_sampling_vector*by; sub_by < min(height_blocks, v_sampling_vector*(by + 1)); sub_by++) {
          for (int sub_bx = h_sampling_vector*bx; sub_bx < min(width_blocks, h_sampling_vector*(bx + 1)); sub_bx++) {
            recreation_component_coeffs = recreation_coeffs[plane];
            recreation_row_coeffs = recreation_cinfo.mem->access_virt_barray((j_common_ptr)&recreation_cinfo, recreation_component_coeffs, sub_by, 1, TRUE)[0];
            recreation_block_coeffs = recreation_row_coeffs[sub_bx];

            reference_component_coeffs = reference_coeffs[plane];
            reference_row_coeffs = reference_cinfo.mem->access_virt_barray((j_common_ptr)&reference_cinfo, reference_component_coeffs, sub_by + reference_y_offset, 1, TRUE)[0];
            reference_block_coeffs = reference_row_coeffs[sub_bx];
  
            for (int i = 0; i < DCTSIZE2; i++) {
              //if ((recreation_block_coeffs[i] | 1) != (reference_block_coeffs[i] | 1))
              //  printf("Difference (recreation | reference): %d | %d\n", recreation_block_coeffs[i], reference_block_coeffs[i]);
              printf("%d | %d\n", recreation_block_coeffs[i], reference_block_coeffs[i]);
            }
          }
        }
      }
    }
  }

  /* Cleanup */
  jpeg_finish_decompress(&reference_cinfo);
  jpeg_destroy_decompress(&reference_cinfo);
  jpeg_finish_decompress(&recreation_cinfo);
  jpeg_destroy_decompress(&recreation_cinfo);
}
