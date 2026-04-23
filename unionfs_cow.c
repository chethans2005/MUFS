#include "unionfs_cow.h"

#include "unionfs_stack_view.h"
#include "unionfs_state.h"

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int unionfs_mkdir_parents(const char *path)
{
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);
    char *p = tmp + 1;
    while ((p = strchr(p, '/')) != NULL) {
        *p = '\0';
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
            return -errno;
        *p = '/';
        p++;
    }
    return 0;
}

int cow_copy(const char *rel)
{
    char src[PATH_MAX], dst[PATH_MAX];
    full_path(src, sizeof(src), STATE->lower_dir, rel);
    full_path(dst, sizeof(dst), STATE->upper_dir, rel);

    char dst_copy[PATH_MAX];
    snprintf(dst_copy, sizeof(dst_copy), "%s", dst);
    char *dir = dirname(dst_copy);
    int mk = unionfs_mkdir_parents(dir);
    if (mk < 0)
        return mk;
    if (mkdir(dir, 0755) != 0 && errno != EEXIST)
        return -errno;

    int fin = open(src, O_RDONLY);
    if (fin < 0)
        return -errno;

    struct stat st;
    if (fstat(fin, &st) < 0) {
        int err = -errno;
        close(fin);
        return err;
    }

    int fout = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (fout < 0) {
        int err = -errno;
        close(fin);
        return err;
    }

    char buf[65536];
    ssize_t n;
    while ((n = read(fin, buf, sizeof(buf))) > 0) {
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(fout, buf + written, (size_t)(n - written));
            if (w < 0) {
                int err = -errno;
                close(fin);
                close(fout);
                return err;
            }
            written += w;
        }
    }
    if (n < 0) {
        int err = -errno;
        close(fin);
        close(fout);
        return err;
    }

    close(fin);
    close(fout);
    return 0;
}