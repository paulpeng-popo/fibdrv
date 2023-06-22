#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    char buf[3];
    char write_buf[1];
    int offset = 92; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);

        // naive
        long long sz = read(fd, buf, 1);
        long long kt = write(fd, write_buf, 1);

        // fastd
        long long sz2 = read(fd, buf, 2);
        long long kt2 = write(fd, write_buf, 1);

        // fastd_clz
        long long sz3 = read(fd, buf, 3);
        long long kt3 = write(fd, write_buf, 1);

        printf("%d %lld %lld %lld\n", i, kt, kt2, kt3);
    }

    close(fd);
    return 0;
}
