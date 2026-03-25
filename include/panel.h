#ifndef PANEL_H
#define PANEL_H

#include <stddef.h>

// Function prototypes
char* base64_encode(const unsigned char* data, size_t input_length);
void transmit_kitty_image(int w, int h, const char* b64_data);
void download_page(char* name);
void get_manga(char* name);
#endif