#include <curl/curl.h>
#include <manga_sh.h>
#include <stdio.h>
#include <string.h>

// Callback function to handle data writing
size_t write_data(void* ptr, size_t size, size_t nmemb, FILE* stream)
{
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

void get_manga(char* name)
{
    CURL* curl;
    FILE* fp;
    CURLcode res;
    char url[512];

    snprintf(url, sizeof(url), "https://api.mangadex.org/manga?title=%s&limit=1", name);

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, stdout);

    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
}

void download_page(char* name)
{
    CURL* curl;
    FILE* fp;
    CURLcode res;
    char* url = "https://uploads.mangadex.org/data-saver/3303dd03ac8d27452cce3f2a882e94b2/6-09f2deb563e802464c161bf7bfa2b094a4727efb4f962d30cf8ee2857a0a66c8.jpg";

    curl = curl_easy_init();
    if (curl) {
        fp = fopen(name, "wb");
        if (fp) {
            // simply setting the operations for the curl request
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
            // curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

            res = curl_easy_perform(curl);
            fclose(fp);
            curl_easy_cleanup(curl);
        }
    }
}