#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    char buf[10];
    char write_buf[10];
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    FILE *fp = fopen("data.txt", "w");

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

        fprintf(fp, "%d %lld %lld %lld\n", i, kt, kt2, kt3);

        // printf("Reading from " FIB_DEV
        //        " at offset %d, returned the sequence "
        //        "%lld.\n",
        //        i, sz);
        // printf("Time: %lld (ns).\n", kt);
    }

    fclose(fp);

    // for (int i = offset; i >= 0; i--) {
    //     lseek(fd, i, SEEK_SET);
    //     sz = read(fd, buf, 1);
    //     printf("Reading from " FIB_DEV
    //            " at offset %d, returned the sequence "
    //            "%lld.\n",
    //            i, sz);
    // }

    close(fd);
    return 0;
}
