#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"
#define DEBUG 1

int main()
{
    char write_buf[] = "testing writing";
    int offset = 93; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    FILE *output = fopen("output.txt", "w");

#if DEBUG > 0
    char buf[1];
    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        long long sz = read(fd, buf, 1);
        // read_buf[sz] = '\0';
        long long ktime = write(fd, write_buf, strlen(write_buf));
        fprintf(output, "%lld\r\n", ktime);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);
    }
#else
    char read_buf[] = "";
    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        long long sz = read(fd, read_buf, 1);
        read_buf[sz] = '\0';
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
