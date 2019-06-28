/*
 * Simple HTTP/HTTPS connection statistics program.
 * Adapted from the libcurl examples at <https://github.com/curl/curl/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <curl/curl.h>

#define URL "https://github.com/"

static size_t WriteCallback(void *ptr, size_t size, size_t nmemb, void *data) {
    (void)ptr;  /* unused */
    (void)data; /* unused */
    return (size_t)(size * nmemb);
}

int main () {
    CURL *curl_handle;
    CURLcode res;
    const char *url = URL;

    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteCallback);

    res = curl_easy_perform(curl_handle);

    if (CURLE_OK == res) {
        double val;

        res = curl_easy_getinfo(curl_handle, CURLINFO_TOTAL_TIME, &val);
        if ((CURLE_OK == res) && (val > 0))
            printf("Total download seconds: %06f\n", val);

        res = curl_easy_getinfo(curl_handle, CURLINFO_PRETRANSFER_TIME, &val);
        if ((CURLE_OK == res) && (val > 0))
            printf("Total pretransfer seconds: %06f\n", val);

        res = curl_easy_getinfo(curl_handle, CURLINFO_NAMELOOKUP_TIME, &val);
        if ((CURLE_OK == res) && (val > 0))
            printf("Name lookup seconds: %06f\n", val);

        res = curl_easy_getinfo(curl_handle, CURLINFO_CONNECT_TIME, &val);
            if ((CURLE_OK == res) && (val > 0))
                printf("Connect time seconds: %06f\n", val);
    } else {
        fprintf(stderr, "Error while fetching '%s': %s\n",
            url, curl_easy_strerror(res));
    }

    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();

    return 0;
}
