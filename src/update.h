#ifndef WRC_UPDATE_H
#define WRC_UPDATE_H

#include <stddef.h>

#define UPDATE_VER_CAP 32
#define UPDATE_URL_CAP 256
#define UPDATE_ERR_CAP 128

typedef struct {
    char version[UPDATE_VER_CAP]; /* e.g. "1.2.0" without leading v */
    char page_url[UPDATE_URL_CAP];
} UpdateInfo;

/* Returns 1 if latest is strictly newer than current (dotted major.minor.patch). */
int update_is_newer(const char *current, const char *latest);

/* GET GitHub latest release for this repo. Fills out->version and out->page_url. Returns 1 on success. */
int update_check_latest(UpdateInfo *out, char *err, size_t err_cap);

#endif
