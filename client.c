#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"
#define DEBUG 0

static inline long long diff(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec - start.tv_nsec) < 0) {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return temp.tv_sec * 1e9 + temp.tv_nsec;
}

int main()
{
    char write_buf[] = "testing writing";
    int offset = 1000; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    FILE *output = fopen("Fibonacci_StringAdd.txt", "w");

    struct timespec start, end;

#if DEBUG > 0
    char buf[1];
    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        clock_gettime(CLOCK_REALTIME, &start);
        long long sz = read(fd, buf, 1);
        clock_gettime(CLOCK_REALTIME, &end);
        // read_buf[sz] = '\0';
        long long utime = diff(start, end);
        long long ktime = write(fd, write_buf, strlen(write_buf));
        fprintf(output, "%d %lld %lld %lld\r\n", i, utime, ktime,
                utime - ktime);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);
    }
#else
    char read_buf[40960];
    for (int i = 0; i <= offset; i++) {
        memset(read_buf, 0, sizeof(read_buf));
        lseek(fd, i, SEEK_SET);
        clock_gettime(CLOCK_REALTIME, &start);
        long long sz = read(fd, read_buf, sizeof(read_buf));
        clock_gettime(CLOCK_REALTIME, &end);
        long long utime = diff(start, end);
        long long ktime = write(fd, write_buf, strlen(write_buf));
        fprintf(output, "%d %lld %lld %lld\r\n", i, utime, ktime,
                utime - ktime);
        // read_buf[sz] = '\0';
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, read_buf);
    }
#endif
    // for (int i = offset; i >= 0; i--) {
    //     lseek(fd, i, SEEK_SET);
    //     sz = read(fd, buf, 1);
    //     // read_buf[sz] = '\0';
    //     printf("Reading from " FIB_DEV
    //            " at offset %d, returned the sequence "
    //            "%lld.\n",
    //            i, sz);
    // }

    close(fd);
    fclose(output);
    return 0;
}
