#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

#define BUF_SIZE 64
#define DIGIT_BITS 64

// char *bn_to_dec_str(const unsigned long long *digits, size_t size)
// {
//     size_t str_size = (size_t) (size * DIGIT_BITS) / 3 + 1;

//     if (size == 0) {
//         char *str = malloc(2);
//         str[0] = '0';
//         str[1] = '\0';
//         return str;
//     } else {
//         char *s = malloc(str_size);
//         char *p = s;

//         memset(s, '0', str_size - 1);
//         s[str_size - 1] = '\0';

//         /* n.digits[0] contains least significant bits */
//         for (int i = size - 1; i >= 0; i--) {
//             /* walk through every bit of bn */
//             for (unsigned long long d = 1ULL << 63; d; d >>= 1) {
//                 /* binary -> decimal string */
//                 int carry = !!(d & digits[i]);
//                 for (int j = str_size - 2; j >= 0; j--) {
//                     s[j] += s[j] - '0' + carry;
//                     carry = (s[j] > '9');
//                     if (carry)
//                         s[j] -= 10;
//                 }
//             }
//         }
//         // skip leading zero
//         while (p[0] == '0' && p[1] != '\0') {
//             p++;
//         }
//         memmove(s, p, strlen(p) + 1);
//         return s;
//     }
// }

int main()
{
    unsigned long long buf[BUF_SIZE];
    char write_buf[1];
    int offset = MAX_FIB_K;
    struct timespec start, end;

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        long long sz = read(fd, buf, sizeof(buf));
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        long long ut = (long long) (end.tv_sec - start.tv_sec) * 1e9 +
                       (end.tv_nsec - start.tv_nsec);
        long long kt = write(fd, write_buf, 1);
        // char *str = bn_to_dec_str(buf, sz);
        // printf("%d %s\n", i, str);
        // free(str);
        printf("%d %lld %lld %lld\n", i, ut, kt, ut - kt);
        // printf("%d %lld %lld %lld\n", i, kt, kt2, kt3);
    }

    close(fd);
    return 0;
}
