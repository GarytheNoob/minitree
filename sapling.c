#include <stdint.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>

int has_space(const char *s) {
    while (*s) {
        if (*s == ' ') return 1;
        s++;
    }
    return 0;
}

void scan_dir(const char *path, const uint8_t depth) {
	DIR *d;
	struct dirent *dir;
	struct stat st;
	
	d = opendir(path); 
	if (d == NULL) {
        fprintf(stderr, "Error opening directory: %s\n", path);
	    // perror("Could not open directory");
	    exit(EXIT_FAILURE);
	}
	
	while ((dir = readdir(d)) != NULL) {
	    char *name = dir->d_name;
	    stat(name, &st);
	
	    if (name[0] != '.') {
	        if (depth) printf("%*c", depth, ' ');
	        int need_quote = has_space(name);
	        if (need_quote) printf("'");
	        printf("%s", name);
	        if (S_ISDIR(st.st_mode)){
	            printf("/, %s is DIR\n", name);
	            char buf[256];
	            sprintf(buf, "%s/%s", path, name);
	            scan_dir(buf, depth + 1);
	        }
	        if (need_quote) printf("'");
	        printf("\n");
	    }
	}
    closedir(d);
}

int main(int argc, char *argv[]) {
    const char *start_path = ".";
    if (argc > 1) {
        start_path = argv[1];
    }
    scan_dir(start_path, 0);
    return 0;
}
