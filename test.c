#include "pamu.h"
#include "tinytest.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* void test_assert() { */
/*   ASSERT("Sheep are cool", 1); */
/*   ASSERT_EQUALS(4, 4); */
/* } */

int main() {
  /* RUN(test_assert); */

  // open RW, no trunc, create 644 if missing
  int fd = open("test.pamu", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd < 0) {
    perror("fopen");
    exit(1);
  }

  // Manually crafted test
  char buf[] = {
    'P' , 'A' , 'M' , 'U' , 0x80, 0x00, 0x00, 0x10, // dynamic flag, header = 16 bytes
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10  // garbage starts at 16 bytes
  };

  fprintf(stdout, "sizeof: %ld\n", sizeof(buf));

  /* // Reset */
  char *blank = calloc(1, 4096);
  write(fd, blank, 4096);
  lseek(fd, 0, SEEK_SET);
  write(fd, buf, sizeof(buf));
  free(blank);

  /* /1* // Init *1/ */
  /* pamu_init(fd, PAMU_DEFAULT | PAMU_DYNAMIC); */

  close(fd);

  /* return TEST_REPORT(); */
  return 0;
}
