#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

namespace common {

static bool Mkdirs(const char *dir) {
    char path_buf[1024];
    const int dir_mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
    char *p = NULL;
    size_t len = 0;
    snprintf(path_buf, sizeof(path_buf), "%s", dir);
    len = strlen(path_buf);
    if(path_buf[len - 1] == '/') {
        path_buf[len - 1] = 0;
    }
    for(p = path_buf + 1; *p; p++) {
        if(*p == '/') {
            *p = 0;
            int status = mkdir(path_buf, dir_mode);
            if (status != 0 && errno != EEXIST) {
                return false;
            }
            *p = '/';
        }
    }
    int status = mkdir(path_buf, dir_mode);
    if (status != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

}// namespace common
