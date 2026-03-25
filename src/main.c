#define STB_IMAGE_IMPLEMENTATION
#include "panel.h"
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("Usage: %s <path_to_manga_page>\n", argv[0]);
        return 1;
    }
    // download_page("newpage.jpg");
    get_manga("Kagurabachi");
    return 0;

    // image decoding
    int w, h, channels;
    unsigned char* pixels = stbi_load(argv[1], &w, &h, &channels, 4); // returns flat aarray of raw pixel data
    if (!pixels) // pixels is h * w * 4 bytes long -> forcing to only be 4 channels for kitty
        return 1; // fail if empty image

    // pixels stored left to right, top to bottom

    // image encoding and transport
    char* b64 = base64_encode(pixels, w * h * 4);
    if (b64) {
        transmit_kitty_image(w, h, b64);
        free(b64);
    }
    printf("\n");

    stbi_image_free(pixels); // cleanup by freeing pixel buffer allocatde by stbi_load
    return 0;
}