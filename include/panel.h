#ifndef PANEL_H
#define PANEL_H

#include <stddef.h>

// ── Data structures ──────────────────────────────────────────────────────────

// Represents a manga search result.
typedef struct {
    char id[64];       // MangaDex UUID, used to query chapters
    char title[256];   // English title (or best available)
} Manga;

// Represents a single chapter returned from the chapter feed.
typedef struct {
    char id[64];       // MangaDex UUID, used to query page images
    char chapter[16];  // Chapter number string e.g. "1", "12.5"
} Chapter;

// Holds everything needed to construct page image URLs for a chapter.
// URL format: {base_url}/data/{hash}/{pages[i]}
typedef struct {
    char  base_url[256]; // CDN base URL (location-optimised, do not hardcode)
    char  hash[128];     // Chapter hash, part of each image's URL path
    char** pages;        // Array of page filenames (original quality)
    int   count;         // Number of pages
} ChapterPages;

// ── Pipeline functions ───────────────────────────────────────────────────────

// Step 1 — Search for a manga by title. Returns the first match, or NULL.
//          Caller must free with free_manga().
Manga* search_manga(const char* title);
void   free_manga(Manga* m);

// Step 2 — Get a specific chapter of a manga.
//          manga_id: from Manga.id
//          chapter:  chapter number string e.g. "1"
//          lang:     ISO 639-1 code e.g. "en"
//          Returns NULL if not found. Caller must free with free_chapter().
Chapter* get_chapter(const char* manga_id, const char* chapter, const char* lang);
void     free_chapter(Chapter* ch);

// Step 3 — Get the page list for a chapter.
//          chapter_id: from Chapter.id
//          Returns struct with base_url, hash, and pages[], or NULL on error.
//          Caller must free with free_chapter_pages().
ChapterPages* get_chapter_pages(const char* chapter_id);
void          free_chapter_pages(ChapterPages* cp);

// Step 4 — Download a single page image to disk.
//          base_url, hash, filename: fields from ChapterPages
//          out_path: local file path to save to e.g. "page_01.jpg"
void download_page(const char* base_url, const char* hash,
                   const char* filename, const char* out_path);

// ── Other ────────────────────────────────────────────────────────────────────

char* base64_encode(const unsigned char* data, size_t input_length);
void  transmit_kitty_image(int w, int h, const char* b64_data);

#endif
