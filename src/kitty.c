#include "manga_sh.h"
#include <stdio.h>
#include <string.h>

#define CHUNK_SIZE 4096

void transmit_kitty_image(int w, int h, const char* b64_data)
{
    size_t len = strlen(b64_data);
    size_t offset = 0;

    while (offset < len) {
        size_t remaining = len - offset;
        size_t chunk = remaining > CHUNK_SIZE ? CHUNK_SIZE : remaining;
        int more = (offset + chunk < len) ? 1 : 0;

        if (offset == 0) {
            // First chunk: include image metadata
            printf("\033_Ga=T,f=32,s=%d,v=%d,m=%d;", w, h, more);
        } else {
            // Continuation chunk
            printf("\033_Gm=%d;", more);
        }

        fwrite(b64_data + offset, 1, chunk, stdout);
        printf("\033\\");

        offset += chunk;
    }

    fflush(stdout);
}