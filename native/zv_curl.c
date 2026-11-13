// A thin layer over libcurl (the "easy" interface) for the Zevic package `Curl`.
//
// - Only plain C types cross the boundary (pointers, int, long long, const char*), never a struct.
// - curl_easy_setopt takes a different argument type for every option; here each option has a typed function.
// - The body and the header lines of the answer are collected here, in C, and copied out into
//   memory that Zevic owns (read_body / read_headers). A body can hold NUL bytes.
// - Only http and https are allowed (also for redirects): a URL like file:///etc/passwd or ftp://... is refused.
#include <curl/curl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    CURL *easy;
    struct curl_slist *headers;
    char *body;
    size_t body_len, body_cap;
    long long max_body; // 0 = no limit
    char *hdrs;
    size_t hdrs_len, hdrs_cap;
    char err[CURL_ERROR_SIZE];
} zv_curl;

// ---- growing buffers ----

static int buf_add(char **p, size_t *len, size_t *cap, const char *data, size_t n) {
    if (n == 0) return 0;
    if (*len + n > *cap) {
        size_t want = *cap ? *cap : 4096;
        while (want < *len + n) {
            if (want > ((size_t)-1) / 2) return -1;
            want *= 2;
        }
        char *q = (char *)realloc(*p, want);
        if (!q) return -1;
        *p = q;
        *cap = want;
    }
    memcpy(*p + *len, data, n);
    *len += n;
    return 0;
}

// Returning less than size*nmemb makes libcurl stop with CURLE_WRITE_ERROR.
static size_t on_body(char *ptr, size_t size, size_t nmemb, void *user) {
    zv_curl *z = (zv_curl *)user;
    size_t n = size * nmemb;
    if (z->max_body > 0 && (long long)(z->body_len + n) > z->max_body) return 0;
    if (buf_add(&z->body, &z->body_len, &z->body_cap, ptr, n) != 0) return 0;
    return n;
}

static size_t on_header(char *ptr, size_t size, size_t nmemb, void *user) {
    zv_curl *z = (zv_curl *)user;
    size_t n = size * nmemb;
    if (buf_add(&z->hdrs, &z->hdrs_len, &z->hdrs_cap, ptr, n) != 0) return 0;
    return n;
}

// curl_global_init is not thread safe; the first caller does it and the others wait.
static volatile int global_state = 0; // 0 = not started, 1 = running, 2 = done

static void global_once(void) {
    if (__sync_bool_compare_and_swap(&global_state, 0, 1)) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        __sync_synchronize();
        global_state = 2;
    } else {
        while (global_state != 2) {
        }
    }
}

static void apply_defaults(zv_curl *z) {
    CURL *e = z->easy;
    curl_easy_setopt(e, CURLOPT_WRITEFUNCTION, on_body);
    curl_easy_setopt(e, CURLOPT_WRITEDATA, z);
    curl_easy_setopt(e, CURLOPT_HEADERFUNCTION, on_header);
    curl_easy_setopt(e, CURLOPT_HEADERDATA, z);
    curl_easy_setopt(e, CURLOPT_ERRORBUFFER, z->err);
    curl_easy_setopt(e, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(e, CURLOPT_ACCEPT_ENCODING, ""); // every encoding that libcurl can decode
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(e, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(e, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(e, CURLOPT_PROTOCOLS, (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
    curl_easy_setopt(e, CURLOPT_REDIR_PROTOCOLS, (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
#endif
}

// ---- the library ----

const char *zv_curl_version(void) { return curl_version(); }

int zv_curl_has_https(void) {
    const curl_version_info_data *d = curl_version_info(CURLVERSION_NOW);
    return (d && (d->features & CURL_VERSION_SSL)) ? 1 : 0;
}

// ---- a handle ----

void *zv_curl_new(void) {
    global_once();
    zv_curl *z = (zv_curl *)calloc(1, sizeof(zv_curl));
    if (!z) return NULL;
    z->easy = curl_easy_init();
    if (!z->easy) {
        free(z);
        return NULL;
    }
    apply_defaults(z);
    return z;
}

void zv_curl_free(void *h) {
    zv_curl *z = (zv_curl *)h;
    if (!z) return;
    if (z->headers) curl_slist_free_all(z->headers);
    if (z->easy) curl_easy_cleanup(z->easy);
    free(z->body);
    free(z->hdrs);
    free(z);
}

// Starts a new request: forgets the last one (connections stay open for reuse).
void zv_curl_begin(void *h) {
    zv_curl *z = (zv_curl *)h;
    if (z->headers) {
        curl_slist_free_all(z->headers);
        z->headers = NULL;
    }
    z->body_len = 0;
    z->hdrs_len = 0;
    z->max_body = 0;
    z->err[0] = '\0';
    curl_easy_reset(z->easy);
    apply_defaults(z);
}

// ---- describing the request ----

int zv_curl_set_url(void *h, const char *url) {
    return (int)curl_easy_setopt(((zv_curl *)h)->easy, CURLOPT_URL, url);
}

// GET and HEAD have no body. POST goes through the POST option, so that a redirect turns it into a GET as usual.
// Any other method is sent as it is written.
int zv_curl_set_method(void *h, const char *method) {
    CURL *e = ((zv_curl *)h)->easy;
    if (strcmp(method, "GET") == 0) return (int)curl_easy_setopt(e, CURLOPT_HTTPGET, 1L);
    if (strcmp(method, "HEAD") == 0) return (int)curl_easy_setopt(e, CURLOPT_NOBODY, 1L);
    if (strcmp(method, "POST") == 0) return (int)curl_easy_setopt(e, CURLOPT_POST, 1L);
    return (int)curl_easy_setopt(e, CURLOPT_CUSTOMREQUEST, method);
}

// libcurl copies the bytes. The length is set first, so the data may hold NUL bytes.
int zv_curl_set_body(void *h, const char *data, long long len) {
    CURL *e = ((zv_curl *)h)->easy;
    CURLcode rc = curl_easy_setopt(e, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)len);
    if (rc != CURLE_OK) return (int)rc;
    return (int)curl_easy_setopt(e, CURLOPT_COPYPOSTFIELDS, len > 0 ? data : "");
}

// "Name: value". libcurl sends the line as it is, so Zevic checks it for line breaks first.
int zv_curl_add_header(void *h, const char *line) {
    zv_curl *z = (zv_curl *)h;
    struct curl_slist *l = curl_slist_append(z->headers, line);
    if (!l) return -1;
    z->headers = l;
    return (int)curl_easy_setopt(z->easy, CURLOPT_HTTPHEADER, z->headers);
}

// ---- options ----

int zv_curl_set_timeout_ms(void *h, long long ms) {
    return (int)curl_easy_setopt(((zv_curl *)h)->easy, CURLOPT_TIMEOUT_MS, (long)ms);
}
int zv_curl_set_connect_timeout_ms(void *h, long long ms) {
    return (int)curl_easy_setopt(((zv_curl *)h)->easy, CURLOPT_CONNECTTIMEOUT_MS, (long)ms);
}
int zv_curl_set_follow(void *h, int on, long long max) {
    CURL *e = ((zv_curl *)h)->easy;
    curl_easy_setopt(e, CURLOPT_FOLLOWLOCATION, on ? 1L : 0L);
    return (int)curl_easy_setopt(e, CURLOPT_MAXREDIRS, (long)max);
}
int zv_curl_set_user_agent(void *h, const char *ua) {
    return (int)curl_easy_setopt(((zv_curl *)h)->easy, CURLOPT_USERAGENT, ua);
}
// Turns the check of the certificate and of the host name off. Only for tests and for servers you trust.
int zv_curl_set_insecure(void *h, int on) {
    CURL *e = ((zv_curl *)h)->easy;
    curl_easy_setopt(e, CURLOPT_SSL_VERIFYPEER, on ? 0L : 1L);
    return (int)curl_easy_setopt(e, CURLOPT_SSL_VERIFYHOST, on ? 0L : 2L);
}
int zv_curl_set_cainfo(void *h, const char *path) {
    return (int)curl_easy_setopt(((zv_curl *)h)->easy, CURLOPT_CAINFO, path);
}
void zv_curl_set_max_body(void *h, long long max) { ((zv_curl *)h)->max_body = max; }

// ---- doing it ----

int zv_curl_perform(void *h) { return (int)curl_easy_perform(((zv_curl *)h)->easy); }

// ---- the answer ----

long long zv_curl_status(void *h) {
    long code = 0;
    curl_easy_getinfo(((zv_curl *)h)->easy, CURLINFO_RESPONSE_CODE, &code);
    return (long long)code;
}

// The address of the last request after redirects (valid until the next request).
const char *zv_curl_effective_url(void *h) {
    char *url = NULL;
    curl_easy_getinfo(((zv_curl *)h)->easy, CURLINFO_EFFECTIVE_URL, &url);
    return url ? url : "";
}

long long zv_curl_body_len(void *h) { return (long long)((zv_curl *)h)->body_len; }
long long zv_curl_headers_len(void *h) { return (long long)((zv_curl *)h)->hdrs_len; }

// Copies at most cap bytes into memory of the caller; returns how many.
long long zv_curl_read_body(void *h, char *dst, long long cap) {
    zv_curl *z = (zv_curl *)h;
    size_t n = z->body_len < (size_t)cap ? z->body_len : (size_t)cap;
    if (n > 0) memcpy(dst, z->body, n);
    return (long long)n;
}
long long zv_curl_read_headers(void *h, char *dst, long long cap) {
    zv_curl *z = (zv_curl *)h;
    size_t n = z->hdrs_len < (size_t)cap ? z->hdrs_len : (size_t)cap;
    if (n > 0) memcpy(dst, z->hdrs, n);
    return (long long)n;
}

// ---- errors ----

// The message of libcurl for the failed request ("Could not resolve host: ...").
const char *zv_curl_error_text(void *h, int code) {
    zv_curl *z = (zv_curl *)h;
    if (z->err[0] != '\0') return z->err;
    return curl_easy_strerror((CURLcode)code);
}
