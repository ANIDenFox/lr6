#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

static int dir_count = 0;
static int file_count = 0;

/* ---------- Частина 1: створення ієрархії ---------- */

static void make_dir(const char *path) {
    if (mkdir(path, 0755) != 0) {
        perror(path);
        exit(EXIT_FAILURE);
    }
}

static void make_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    if (content)
        fputs(content, f);
    fclose(f);
}

static void create_structure(const char *root) {
    char p[512];

    make_dir(root);

    snprintf(p, sizeof p, "%s/.git", root);
    make_dir(p);
    snprintf(p, sizeof p, "%s/.git/config", root);
    make_file(p, "[core]\n\trepositoryformatversion = 0\n");

    snprintf(p, sizeof p, "%s/src", root);
    make_dir(p);
    snprintf(p, sizeof p, "%s/src/main.c", root);
    make_file(p, "#include <stdio.h>\nint main(void) { puts(\"Hello\"); return 0; }\n");

    snprintf(p, sizeof p, "%s/bin", root);
    make_dir(p);

    snprintf(p, sizeof p, "%s/docs", root);
    make_dir(p);
    snprintf(p, sizeof p, "%s/docs/manual.txt", root);
    make_file(p, "User manual\n");
    snprintf(p, sizeof p, "%s/docs/presentation.pdf", root);
    make_file(p, "%PDF-1.4\n");

    snprintf(p, sizeof p, "%s/img", root);
    make_dir(p);
    snprintf(p, sizeof p, "%s/img/cat.jpg", root);
    make_file(p, "cat");
    snprintf(p, sizeof p, "%s/img/dog.png", root);
    make_file(p, "dog");

    snprintf(p, sizeof p, "%s/README.md", root);
    make_file(p, "# Lab 6\n");
}

/* ---------- Частина 2: аналог команди tree ---------- */

static int cmp_names(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static void print_tree(const char *path, const char *prefix) {
    DIR *dir = opendir(path);
    if (!dir) {
        perror(path);
        return;
    }

    /* збираємо імена (без прихованих), щоб відсортувати */
    char **names = NULL;
    size_t n = 0, cap = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.')   /* пропускаємо ".", ".." і приховані */
            continue;
        if (n == cap) {
            cap = cap ? cap * 2 : 16;
            names = realloc(names, cap * sizeof *names);
            if (!names) { perror("realloc"); exit(EXIT_FAILURE); }
        }
        names[n++] = strdup(entry->d_name);
    }
    closedir(dir);
    qsort(names, n, sizeof *names, cmp_names);

    for (size_t i = 0; i < n; i++) {
        int last = (i == n - 1);
        char full[1024];
        snprintf(full, sizeof full, "%s/%s", path, names[i]);

        struct stat st;
        if (stat(full, &st) != 0) {
            perror(full);
            free(names[i]);
            continue;
        }

        printf("%s%s%s\n", prefix, last ? "└── " : "├── ", names[i]);

        if (S_ISDIR(st.st_mode)) {
            dir_count++;
            char new_prefix[1024];
            snprintf(new_prefix, sizeof new_prefix, "%s%s", prefix,
                     last ? "    " : "│   ");
            print_tree(full, new_prefix);
        } else {
            file_count++;
        }
        free(names[i]);
    }
    free(names);
}

int main(int argc, char *argv[]) {
    const char *number = (argc > 1) ? argv[1] : "4";   /* номер за журналом */
    char root[128];
    snprintf(root, sizeof root, "lab6_%s", number);

    create_structure(root);

    /* корінь виводимо завжди, навіть якщо його ім'я починалось би з "." */
    printf("%s\n", root);
    print_tree(root, "");
    printf("\n%d directories, %d files\n", dir_count, file_count);
    return 0;
}
