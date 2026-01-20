#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>

typedef struct {
    char name[NAME_MAX + 1];
    int is_dir;
} entry;

int cmp_ent(const void *ea, const void *eb) {
    const entry *entryA = (const entry *) ea;
    const entry *entryB = (const entry *) eb;

    // Prioritize folders
    if (entryA->is_dir != entryB->is_dir) {
        return entryB->is_dir - entryA->is_dir;
    } else {
        return strcmp(entryA->name, entryB->name);
    }
}

int has_space(const char *name) {
    int len = strlen(name);
    for (int i = 0; i < len; i++) {
        if (name[i] == ' ') {
            return 1;
        }
    }
    return 0;
}

size_t read_dir(const char *path, entry *entries, size_t max) {
    DIR *d = opendir(path);
    struct dirent *dir;
    struct stat st;
    size_t count = 0;

    if (!d) return 0;

    while ((dir = readdir(d)) && count < max) {
        const char *name = dir->d_name;

        if (name[0] == '.') continue; // TODO: control with flag?

        char fullpath[PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, name);

        if (lstat(fullpath, &st) != 0) continue;

        strlcpy(entries[count].name, name, sizeof(entries[count].name));
        entries[count].is_dir = S_ISDIR(st.st_mode);

        count ++;
    }

    closedir(d);

    qsort(entries, count, sizeof(entries[0]), cmp_ent);
    return count;
}

void print_tree(const char *path, int *branches, const uint8_t depth) {
    entry entries[256];
    size_t n = read_dir(path, entries, 256);

    for (size_t i = 0; i < n; i++) {
        int last = (i == n - 1);

        for (int d = 0; d < depth; d++)
            printf(branches[d] ? "│   " : "    ");

        printf(last ? "└── " : "├── ");
        int need_quote = has_space(entries[i].name);

        if (need_quote) putchar('\'');
        printf("%s", entries[i].name);

        if (entries[i].is_dir) {
            putchar('/');
            if (need_quote) putchar('\'');
            putchar('\n');

            char next_path[PATH_MAX];
            snprintf(next_path, PATH_MAX, "%s/%s", path, entries[i].name);

            branches[depth] = !last;

            print_tree(next_path, branches, depth + 1);
        } else {
            if (need_quote) putchar('\'');
            putchar('\n');
        }
    }
}

int main(int argc, char *argv[]) {
    int branches[64] = {0};
    printf(".\n");
    print_tree(".", branches, 0);
    return 0;
}
