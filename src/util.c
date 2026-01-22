#include "common.h"
#include <stdarg.h>

void log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[BOX] ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

void log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[ERR] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

int write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%s", content);
    fclose(f);
    return 0;
}

char* read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    
    // Aloca memória dinamicamente
    char *buf = malloc(256);
    if (!buf) { fclose(f); return NULL; }

    if (!fgets(buf, 256, f)) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    
    buf[strcspn(buf, "\n")] = 0;
    return buf;
}

int get_box_pid(void) {
    char *c = read_file(PID_FILE);
    if (!c) return -1;
    int pid = atoi(c);
    free(c);
    return pid;
}

void set_state(BoxState state) {
    char *s_str = "EMPTY";
    if (state == STATE_INITIALIZED) s_str = "INITIALIZED";
    if (state == STATE_RUNNING) s_str = "RUNNING";
    if (state == STATE_STOPPED) s_str = "STOPPED";
    write_file(STATE_FILE, s_str);
}

BoxState get_state(void) {
    char *s = read_file(STATE_FILE);
    if (!s) return STATE_EMPTY;
    BoxState state = STATE_EMPTY;
    if (strcmp(s, "INITIALIZED") == 0) state = STATE_INITIALIZED;
    else if (strcmp(s, "RUNNING") == 0) state = STATE_RUNNING;
    else if (strcmp(s, "STOPPED") == 0) state = STATE_STOPPED;
    free(s);
    return state;
}

void save_config(Config *cfg) {
    FILE *f = fopen(CONFIG_FILE, "w");
    if (!f) return;
    fprintf(f, "distro=\"%s\"\n", cfg->distro);
    fprintf(f, "net=%s\n", cfg->net_enabled ? "true" : "false");
    fprintf(f, "ssh=%s\n", cfg->ssh_enabled ? "true" : "false");
    fclose(f);
}

Config load_config(void) {
    Config cfg = {0};
    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) return cfg;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "distro=\"%63[^\"]\"", cfg.distro) == 1) continue;
        if (strstr(line, "net=true")) cfg.net_enabled = true;
        if (strstr(line, "ssh=true")) cfg.ssh_enabled = true;
    }
    fclose(f);
    return cfg;
}

int acquire_lock(void) {
    int fd = open(LOCK_FILE, O_CREAT | O_RDWR, 0644);
    if (lockf(fd, F_TLOCK, 0) < 0) {
        log_error("Falha no lock. Outro processo boxverse está rodando?");
        close(fd);
        return -1;
    }
    return fd;
}

void release_lock(int fd) {
    lockf(fd, F_ULOCK, 0);
    close(fd);
}