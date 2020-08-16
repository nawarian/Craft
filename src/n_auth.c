#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "d_db.h"
#include "n_auth.h"
#include "n_client.h"

#define MAX_POST_LENGTH 1024
#define MAX_RESPONSE_LENGTH 1024

size_t write_function(char *data, size_t size, size_t count, void *arg) {
    size_t length = size * count;
    char *dst = (char *)arg;
    char *src = malloc(length + 1);
    memcpy(src, data, length);
    src[length] = '\0';
    strncat(dst, src, MAX_RESPONSE_LENGTH - strlen(dst) - 1);
    free(src);
    return length;
}

int n_auth_access_token_get(
    char *result, int length, char *username, char *identity_token)
{
    static char url[] = "https://craft.michaelfogleman.com/api/1/identity";
    strncpy(result, "", length);
    CURL *curl = curl_easy_init();
    if (curl) {
        char post[MAX_POST_LENGTH] = {0};
        char response[MAX_RESPONSE_LENGTH] = {0};
        long http_code = 0;
        snprintf(post, MAX_POST_LENGTH, "username=%s&identity_token=%s",
            username, identity_token);
        #ifdef _WIN32
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        #endif
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_function);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post);
        CURLcode code = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);
        if (code == CURLE_OK && http_code == 200) {
            strncpy(result, response, length);
            return 1;
        }
    }
    return 0;
}

void n_auth_login() {
    char username[128] = {0};
    char identity_token[128] = {0};
    char access_token[128] = {0};

    if (d_db_auth_get_selected(username, 128, identity_token, 128)) {
        printf("contacting login server for username: %s\n", username);

        if (
            n_auth_access_token_get(
                access_token,
                128,
                username,
                identity_token
            )
        ) {
            printf("Successfully authenticated with the login server\n");
            n_client_login(username, access_token);
        } else {
            printf("Failed to authenticate with the login server\n");
            n_client_login("", "");
        }
    } else {
        printf("Logging in anonymously\n");
        n_client_login("", "");
    }
}

