#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <strings.h>
#include <stdarg.h>

#define LOG_NAME "sort_report.log"

static FILE *log_file = NULL;
static const char *dest_root = NULL;

/* Запис повідомлення і в консоль, і в лог-файл */
static void logmsg(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");

    if (log_file) {
        va_start(ap, fmt);
        vfprintf(log_file, fmt, ap);
        va_end(ap);
        fprintf(log_file, "\n");
        fflush(log_file);
    }
}

/* Визначення категорії за розширенням */
static const char *category_of(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename || dot[1] == '\0')
        return "Other";

    static const struct { const char *ext; const char *cat; } map[] = {
        {".txt", "Documents"}, {".md", "Documents"},
        {".c", "Sources"}, {".h", "Sources"}, {".go", "Sources"},
        {".jpg", "Images"}, {".png", "Images"},
        {".pdf", "PDFs"},
    };
    for (size_t i = 0; i < sizeof map / sizeof map[0]; i++)
        if (strcasecmp(dot, map[i].ext) == 0)
            return map[i].cat;
    return "Other";
}

/* Створення директорії, якщо її ще немає (з логуванням) */
static int ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode))
            return 0;
        logmsg("Помилка: %s існує, але це не папка", path);
        return -1;
    }
    if (mkdir(path, 0755) != 0) {
        logmsg("Помилка створення папки %s: %s", path, strerror(errno));
        return -1;
    }
    logmsg("Створено папку: %s", path);
    return 0;
}

/* Копіювання файлу (запасний варіант, якщо rename() між різними ФС не вдався) */
static int copy_file(const char *from, const char *to) {
    FILE *in = fopen(from, "rb");
    if (!in) return -1;
    FILE *out = fopen(to, "wb");
    if (!out) { fclose(in); return -1; }

    char buf[8192];
    size_t n;
    int rc = 0;
    while ((n = fread(buf, 1, sizeof buf, in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { rc = -1; break; }
    }
    fclose(in);
    if (fclose(out) != 0) rc = -1;
    if (rc == 0) rc = remove(from);
    return rc;
}

/* Переміщення одного файлу в цільову піддиректорію */
static void move_file(const char *src_path, const char *name) {
    const char *cat = category_of(name);
    char cat_dir[1024], target[2048];

    snprintf(cat_dir, sizeof cat_dir, "%s/%s", dest_root, cat);
    if (ensure_dir(cat_dir) != 0)
        return;

    snprintf(target, sizeof target, "%s/%s", cat_dir, name);

    /* не перезаписуємо існуючий файл — додаємо числовий суфікс */
    struct stat st;
    for (int i = 1; stat(target, &st) == 0 && i < 10000; i++)
        snprintf(target, sizeof target, "%s/%d_%s", cat_dir, i, name);

    if (rename(src_path, target) == 0) {
        logmsg("Переміщено: %s -> %s", src_path, target);
    } else if (errno == EXDEV && copy_file(src_path, target) == 0) {
        logmsg("Переміщено: %s -> %s", src_path, target);
    } else {
        logmsg("Помилка переміщення %s: %s", src_path, strerror(errno));
    }
}

static int cmp_names(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/* Рекурсивний обхід вихідної директорії */
static void scan(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        logmsg("Помилка відкриття %s: %s", path, strerror(errno));
        return;
    }

    char **names = NULL;
    size_t n = 0, cap = 0;
    struct dirent *e;
    while ((e = readdir(dir)) != NULL) {
        if (e->d_name[0] == '.')      /* ".", ".." та приховані (.git тощо) */
            continue;
        if (n == cap) {
            cap = cap ? cap * 2 : 16;
            names = realloc(names, cap * sizeof *names);
            if (!names) { perror("realloc"); exit(EXIT_FAILURE); }
        }
        names[n++] = strdup(e->d_name);
    }
    closedir(dir);
    qsort(names, n, sizeof *names, cmp_names);

    for (size_t i = 0; i < n; i++) {
        char full[2048];
        snprintf(full, sizeof full, "%s/%s", path, names[i]);

        struct stat st;
        if (stat(full, &st) != 0) {
            logmsg("Помилка stat %s: %s", full, strerror(errno));
        } else if (S_ISDIR(st.st_mode)) {
            scan(full);
        } else if (S_ISREG(st.st_mode)) {
            move_file(full, names[i]);
        }
        free(names[i]);
    }
    free(names);
}

static void usage(const char *prog) {
    fprintf(stderr, "Використання: %s --source=<шлях> --dest=<шлях>\n", prog);
}

int main(int argc, char *argv[]) {
    const char *source = NULL, *dest = NULL;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--source=", 9) == 0)
            source = argv[i] + 9;
        else if (strncmp(argv[i], "--dest=", 7) == 0)
            dest = argv[i] + 7;
        else if (strcmp(argv[i], "--source") == 0 && i + 1 < argc)
            source = argv[++i];
        else if (strcmp(argv[i], "--dest") == 0 && i + 1 < argc)
            dest = argv[++i];
        else {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }
    if (!source || !dest) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    struct stat st;
    if (stat(source, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Помилка: %s не є директорією\n", source);
        return EXIT_FAILURE;
    }

    log_file = fopen(LOG_NAME, "w");
    if (!log_file) {
        perror(LOG_NAME);
        return EXIT_FAILURE;
    }

    dest_root = dest;
    logmsg("--- Програма сортування запущена ---");

    if (ensure_dir(dest_root) == 0)
        scan(source);

    logmsg("Сортування завершено! Див. '%s'.", LOG_NAME);
    fclose(log_file);
    return EXIT_SUCCESS;
}
