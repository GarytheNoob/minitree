# minitree

## Build

```bash
gcc -o minitree minitree.c
```

## 介绍

### 选题

COMP2113 的课程包含在 CS 服务器上进行操作。作为非管理员用户通过 ssh 登录到服务器上的时候，常因为没有一些称手的工具而感到不习惯。

其中之一就是 `tree`，也就是以树状图列出当前文件目录的工具。

于是打算用 C 实现一个。

### 基础版本

#### 原始代码

`dirent` 库可以用来读取当前目录。借助了 AI 和 Manpage 之后写出了一个类似下面的基础版本：
```c
#include <stdint.h> // uint8_t
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h> // check file type
#include <stdlib.h> // exit()

int has_space(char *s); // return 1 if s has space in it, else 0

void scan_dir(const char *path, const uint8_t depth) {
	DIR *d;
	struct dirent *dir;
	struct stat st;
	
	d = opendir(path); 
	if (d == NULL) {
	    perror("Could not open directory");
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
	            printf("/\n");
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
```

`DIR *opendir(char *path)` 可以打开一个目录，然后用 `struct dirent *readdir(DIR *d)` 可以连续地从目录中读取一个条目。

`stat` 可以用来获取一个条目的属性。用 `S_ISDIR()` 来判断是否是目录。

这段代码有几个问题：
1. 在使用 `stat(char *path, struct stat *st)` 获取属性的时候没有传入完整路径，而 `stat` **只会在当前工作目录查找**。故在递归进入内层目录的时候，如果尝试 `stat("file_in_subdir", &st)`，会导致 ` ENOENT `（Error NO ENTry）错误，从而让 ` st_mode ` 储存无意义值，导致无法正确判断是否为目录，并在下一次 `opendir()` 时报错。
2. `printf("%*c", depth, ' ');` 语义不清，并且如果想调整缩进的长度，只能使用 `depth * LENGTH` 之类的方式。
3. `sprintf()` 没有显式地传入目标字符串的容量，如果格式化后的字符串超过 `buf` 的容量（256），会导致未知错误。
4. 对于一个目录条目，会在遍历完其内容后添加额外的换行（*由于第一点，这个错误只能在子目录为空的时候被观测到*）。

#### 改进后

借助了 AI debug 和自己修改之后，写出了这样的版本：
```c
 #include <dirent.h>
 #include <sys/stat.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <limits.h>
 
 void scan_dir(const char *path, uint8_t depth) {
     DIR *d = opendir(path);
     struct dirent *dir;
     struct stat st;
 
     if (!d) {
         perror("opendir");
         return;
     }
 
     while ((dir = readdir(d)) != NULL) {
         const char *name = dir->d_name;
 
         if (name[0] == '.')
             continue;
 
         char fullpath[PATH_MAX];
         snprintf(fullpath, sizeof(fullpath), "%s/%s", path, name);
 
         if (stat(fullpath, &st) != 0)
             continue;
 
         for (int i = 0; i < depth; i++)
             putchar(' ');
 
         int need_quote = has_space(name);
         if (need_quote) putchar('\'');
 
         printf("%s", name);
 
         if (S_ISDIR(st.st_mode)) {
         	if (need_quote) putchar('\'');
             printf("/\n");
             scan_dir(fullpath, depth + 1);
         } else {
	         if (need_quote) putchar('\'');
             putchar('\n');
         }
     }
 
     closedir(d);
 }
 
```

修改后的代码不仅解决了上述的问题，还使用提前 `return` 和 `continue` 的方法避免了代码的 over-nesting。但是，仍然有一个问题需要解决：` readdir() ` 是乱序的，并且无法预知最后一个条目的到来——对于一个功能完整的 ` tree `，最后一个条目没有指向 next sibling 的分叉图示：
```text
├── file1
├── file2
└── file3  // final entry
```

### 最终方案

AI 给出的解决方案是提前存储当前列表的所有东西，并提前进行判断和排序。在本项目中我们只在意一个条目：
- 的名称；和
- 是否为目录。
所以我们可以写一个这样的结构体：
 ```c
 typedef struct {
     char name[NAME_MAX + 1];
     int is_dir;
 } entry;
 ```

然后实现两个函数：
```c
void print_tree(const char *path, int *branches, const uint8_t depth);
size_t read_dir(const char *path, entry *entries, size_t max);
```

`print_tree()` 会调用 `read_dir()`，后者先读取整个目录并整理，将排序后的结果放到 `entries` 中。如此一来，所有 `dirent` 和 `stat` 库的内容只会存在于 `read_dir()` 里，而 `print_tree()` 只需处理字符串输出。

#### 输出一棵树

```c
void print_tree(const char *path, int *branches, const uint8_t depth) {
    entry entries[256];
    
    /* read_dir returns number of entries */
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
```

##### 有关 `int *branches`

这个数组储存了到当前目录位置的各层目录是否是该层最后一个，也就直接决定了那一层是否需要输出一个枝干：
```text
│       │   └── foo_dir
│       │ [NO]  ├── foo_foo
│       │     [YES] ├── File1
│       │       │   ├── File2
│       │       │   ├── File3
│ [NO]  │       │   └── File4
│     [YES]     └── foo_bar_dir
│       │           ├── 'Hello World'
│       │           └── 'foo file bar'
│       └── file_in_parent_dir
```

在 `read_dir()` 函数递归调用的时候，会对 `branches` 进行可覆盖的修改并遗传给下一层调用。

#### 整理枝叶

```c
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

        snprintf(entries[count].name, sizeof(entries[count].name), "%s", name);
        entries[count].is_dir = S_ISDIR(st.st_mode);

        count ++;
    }

    closedir(d);
	
	/* in-place sorting
	 * cmp_ent() is a function */
    qsort(entries, count, sizeof(entries[0]), cmp_ent);
    return count;
}
```

##### 有关 `qsort()`

```
void qsort (void *_LIBC_SIZE (__nel * __width) __base,
			size_t __nel, size_t __width,
			int (* _Nonnull __compar)(const void *, const void *)); 
```

`qsort()` 可以 in-place 地对一个数组进行排序，它需要：
- `__base`：目标数组；
- `__nel`：数组的有效长度（元素数量）；
- `__width`：一个数组元素的大小；和
- 一个接受两个 `const void *` 类型参数，返回 `int` 的函数，用作排序时的比较。

对于我们的例子，我们使用了这个比较函数：
```c
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
```

## License

`README.md`: CC-BY-NC-SA 4.0

Other code: MIT
