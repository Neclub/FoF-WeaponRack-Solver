#ifndef WRC_DISCORD_H
#define WRC_DISCORD_H

#include <stddef.h>
#include <stdint.h>

#define DISCORD_MAX_FILES 4
#define DISCORD_URL_CAP 512
#define DISCORD_NAME_CAP 64
#define DISCORD_CONTENT_CAP 256
#define DISCORD_ERR_CAP 256

typedef struct {
    char filename[DISCORD_NAME_CAP];
    const uint8_t *data;
    size_t len;
} DiscordFile;

/* POST multipart webhook. content is the message body. Returns 1 on HTTP 2xx. */
int discord_webhook_send(const char *url, const char *content, const DiscordFile *files, int file_count, char *err,
                         size_t err_cap);

#endif
