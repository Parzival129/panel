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
    // ── Pipeline demo ────────────────────────────────────────────────────────
    // Step 1: find the manga and get its ID
    Manga* m = search_manga("Kagurabachi");
    if (!m) {
        fprintf(stderr, "Manga not found\n");
        return 1;
    }
    printf("Found: %s (%s)\n", m->title, m->id);

    // Step 2: get chapter 1 in English, retrieve its ID
    Chapter* ch = get_chapter(m->id, "1", "en");
    if (!ch) {
        fprintf(stderr, "Chapter not found\n");
        free_manga(m);
        return 1;
    }
    printf("Chapter %s id: %s\n", ch->chapter, ch->id);

    // Step 3: fetch the page list (hash + filenames) from MangaDex@Home
    ChapterPages* cp = get_chapter_pages(ch->id);
    if (!cp) {
        fprintf(stderr, "Could not get pages\n");
        free_chapter(ch);
        free_manga(m);
        return 1;
    }
    printf("Hash: %s  Pages: %d\n", cp->hash, cp->count);
    for (int i = 0; i < cp->count; i++)
        printf("  [%02d] %s\n", i + 1, cp->pages[i]);

    // Step 4: download the first page to disk
    // URL will be: {cp->base_url}/data/{cp->hash}/{cp->pages[0]}
    download_page(cp->base_url, cp->hash, cp->pages[0], "page1.jpg");
    printf("Saved page 1 -> page1.jpg\n");

    free_chapter_pages(cp);
    free_chapter(ch);
    free_manga(m);

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