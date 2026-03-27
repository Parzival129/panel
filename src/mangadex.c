// mangadex.c
// MangaDex API pipeline — fetch any page of any chapter of any manga.
//
// Pipeline (call in order):
//   1. search_manga(title)                              -> Manga*
//   2. get_chapter(manga_id, chapter_num, lang)         -> Chapter*
//   3. get_chapter_pages(chapter_id)                    -> ChapterPages*
//   4. download_page(base_url, hash, filename, out_path)-> file on disk
//
// Example (see main.c for usage):
//   Manga*        m  = search_manga("Kagurabachi");
//   Chapter*      ch = get_chapter(m->id, "1", "en");
//   ChapterPages* cp = get_chapter_pages(ch->id);
//   download_page(cp->base_url, cp->hash, cp->pages[0], "page1.jpg");

#include <panel.h>
#include <cJSON.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── Internal helpers ─────────────────────────────────────────────────────────

// Growable buffer used as curl's write target when we need to parse the response.
typedef struct {
    char*  data;
    size_t size;
} Response;

// curl WRITEFUNCTION callback: appends each incoming chunk to resp->data.
static size_t write_response(void* ptr, size_t size, size_t nmemb, Response* resp)
{
    size_t chunk    = size * nmemb;
    size_t new_size = resp->size + chunk;
    resp->data = realloc(resp->data, new_size + 1);
    memcpy(resp->data + resp->size, ptr, chunk);
    resp->size            = new_size;
    resp->data[new_size]  = '\0';
    return chunk;
}

// curl WRITEFUNCTION callback: writes incoming bytes directly to a FILE*.
// Used when downloading binary image data to disk.
static size_t write_file(void* ptr, size_t size, size_t nmemb, FILE* stream)
{
    return fwrite(ptr, size, nmemb, stream);
}

// Perform a GET request to `url` and return the body as a heap-allocated string.
// Caller is responsible for free()ing the returned string.
// Returns NULL if curl fails or the response is empty.
static char* fetch(const char* url)
{
    CURL*    curl = curl_easy_init();
    Response resp = { 0 };

    curl_easy_setopt(curl, CURLOPT_URL,           url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,     "panel/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &resp);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    return resp.data; // may be NULL if nothing was written
}

// Walk a manga's title object and altTitles array to find the best English title.
// Returns a pointer into the cJSON tree (do not free separately).
static cJSON* find_en_title(cJSON* attrs)
{
    // The primary title object may directly have an "en" key
    cJSON* title = cJSON_GetObjectItem(attrs, "title");
    cJSON* en    = cJSON_GetObjectItem(title, "en");
    if (en) return en;

    // Otherwise search the altTitles array for the first entry with "en"
    cJSON* altTitles = cJSON_GetObjectItem(attrs, "altTitles");
    cJSON* alt;
    cJSON_ArrayForEach(alt, altTitles) {
        en = cJSON_GetObjectItem(alt, "en");
        if (en) return en;
    }

    return NULL; // no english title found
}

// ── Step 1: search_manga ─────────────────────────────────────────────────────

// Search MangaDex for a manga by title. Returns the first result.
// API: GET /manga?title={title}&limit=1
Manga* search_manga(const char* title)
{
    char url[512];
    // hasAvailableChapters=true skips titles whose chapters are all external-only
    snprintf(url, sizeof(url),
             "https://api.mangadex.org/manga?title=%s&limit=1&hasAvailableChapters=true", title);

    char* body = fetch(url);
    if (!body) return NULL;

    cJSON* root = cJSON_Parse(body);
    free(body);
    if (!root) return NULL;

    // data is an array of manga objects; we want the first one
    cJSON* data  = cJSON_GetObjectItem(root, "data");
    cJSON* first = cJSON_GetArrayItem(data, 0);
    if (!first) { cJSON_Delete(root); return NULL; }

    cJSON* id    = cJSON_GetObjectItem(first, "id");
    cJSON* attrs = cJSON_GetObjectItem(first, "attributes");
    cJSON* en    = find_en_title(attrs);

    if (!id) { cJSON_Delete(root); return NULL; }

    Manga* m = calloc(1, sizeof(Manga));
    strncpy(m->id,    id->valuestring,                    sizeof(m->id)    - 1);
    strncpy(m->title, en ? en->valuestring : "(unknown)", sizeof(m->title) - 1);

    cJSON_Delete(root);
    return m;
}

void free_manga(Manga* m) { free(m); }

// ── Step 2: get_chapter ──────────────────────────────────────────────────────

// Fetch a specific chapter of a manga by number and language.
// API: GET /manga/{manga_id}/feed?translatedLanguage[]={lang}&chapter={ch}&limit=1
Chapter* get_chapter(const char* manga_id, const char* chapter, const char* lang)
{
    char url[512];
    // includeExternalUrl=0 skips chapters hosted elsewhere (e.g. MangaPlus)
    // that can't be fetched via the MangaDex@Home at-home server
    snprintf(url, sizeof(url),
             "https://api.mangadex.org/chapter"
             "?manga=%s&translatedLanguage[]=%s&chapter=%s&limit=1&includeExternalUrl=0&order[chapter]=asc",
             manga_id, lang, chapter);

    char* body = fetch(url);
    if (!body) return NULL;

    cJSON* root = cJSON_Parse(body);
    free(body);
    if (!root) return NULL;

    // data is an array of chapter objects
    cJSON* data  = cJSON_GetObjectItem(root, "data");
    cJSON* first = cJSON_GetArrayItem(data, 0);
    if (!first) { cJSON_Delete(root); return NULL; }

    cJSON* id    = cJSON_GetObjectItem(first, "id");
    cJSON* attrs = cJSON_GetObjectItem(first, "attributes");
    cJSON* ch    = cJSON_GetObjectItem(attrs, "chapter");

    if (!id) { cJSON_Delete(root); return NULL; }

    Chapter* c = calloc(1, sizeof(Chapter));
    strncpy(c->id,      id->valuestring,                sizeof(c->id)      - 1);
    strncpy(c->chapter, ch ? ch->valuestring : chapter, sizeof(c->chapter) - 1);

    cJSON_Delete(root);
    return c;
}

void free_chapter(Chapter* ch) { free(ch); }

// ── Step 3: get_chapter_pages ────────────────────────────────────────────────

// Retrieve the page image list for a chapter from MangaDex@Home.
// API: GET /at-home/server/{chapter_id}
//
// The response gives us:
//   baseUrl          - CDN node to use (do not hardcode; it's location-optimised)
//   chapter.hash     - folder name in the URL path
//   chapter.data[]   - original-quality page filenames
//
// Full image URL: {baseUrl}/data/{hash}/{pages[i]}
ChapterPages* get_chapter_pages(const char* chapter_id)
{
    char url[512];
    snprintf(url, sizeof(url),
             "https://api.mangadex.org/at-home/server/%s", chapter_id);

    char* body = fetch(url);
    if (!body) return NULL;

    cJSON* root = cJSON_Parse(body);
    free(body);
    if (!root) return NULL;

    cJSON* base_url = cJSON_GetObjectItem(root, "baseUrl");
    cJSON* chapter  = cJSON_GetObjectItem(root, "chapter");
    cJSON* hash     = cJSON_GetObjectItem(chapter, "hash");
    cJSON* data     = cJSON_GetObjectItem(chapter, "data"); // original quality

    if (!base_url || !hash || !data) { cJSON_Delete(root); return NULL; }

    ChapterPages* cp = calloc(1, sizeof(ChapterPages));
    strncpy(cp->base_url, base_url->valuestring, sizeof(cp->base_url) - 1);
    strncpy(cp->hash,     hash->valuestring,     sizeof(cp->hash)     - 1);

    // Copy each filename out of the cJSON tree into our own heap strings
    cp->count = cJSON_GetArraySize(data);
    cp->pages = malloc(cp->count * sizeof(char*));
    int i = 0;
    cJSON* page;
    cJSON_ArrayForEach(page, data)
        cp->pages[i++] = strdup(page->valuestring);

    cJSON_Delete(root);
    return cp;
}

void free_chapter_pages(ChapterPages* cp)
{
    if (!cp) return;
    for (int i = 0; i < cp->count; i++)
        free(cp->pages[i]);
    free(cp->pages);
    free(cp);
}

// ── Step 4: download_page ────────────────────────────────────────────────────

// Download a single page image to disk.
// Construct the URL as: {base_url}/data/{hash}/{filename}
// Per MangaDex guidelines, do not add auth headers to image requests.
void download_page(const char* base_url, const char* hash,
                   const char* filename, const char* out_path)
{
    char url[1024];
    snprintf(url, sizeof(url), "%s/data/%s/%s", base_url, hash, filename);

    FILE* fp = fopen(out_path, "wb");
    if (!fp) { perror("fopen"); return; }

    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL,           url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,     "panel/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     fp);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    fclose(fp);
}
