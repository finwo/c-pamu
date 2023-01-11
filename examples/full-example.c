#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "pamu.h"

const char * filename = "example.pamu";

const char *data[] = {
  "Obi-Wan Kenobi is a Jedi",
  "Luke Skywalker",
  "Yoda",
  "Moon Moon",
  NULL
};

int main() {
  int i;
  char c;

  PAMU_T_POINTER ptr;

  // Open & initialize the medium
  // CAUTION: truncates to 0 bytes upon opening
  int fd = open(filename, O_RDWR|O_CREAT|O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  int rc = pamu_init(fd, PAMU_DEFAULT | PAMU_DYNAMIC);

  // Catching errors
  if (rc) {
    fprintf(stderr, "An error occurred! (%d)\n", rc);
    return 1;
  }

  // Create a couple allocations we can iterate
  for(i = 0; data[i]; i++) {
    ptr = pamu_alloc(fd, strlen(data[i]) + 1);
    lseek(fd, ptr, SEEK_SET);
    write(fd, data[i], strlen(data[i]) + 1);
  }

  // And iterate over them, reading byte-for-byte on each entry
  fprintf(stdout, "Before:\n");
  ptr = pamu_next(fd, 0);
  while(ptr) {
    fprintf(stdout, "  ");
    lseek(fd, ptr, SEEK_SET);
    do {
      read(fd, &c, 1);
      if (!c) continue;
      fprintf(stdout, "%c", c);
    } while(c);
    fprintf(stdout, "\n");
    ptr = pamu_next(fd, ptr);
  }

  // Free the 2nd entry
  pamu_free(fd, pamu_next(fd, pamu_next(fd, 0)));

  // And iterate over them, reading byte-for-byte on each entry
  fprintf(stdout, "After:\n");
  ptr = pamu_next(fd, 0);
  while(ptr) {
    fprintf(stdout, "  ");
    lseek(fd, ptr, SEEK_SET);
    do {
      read(fd, &c, 1);
      if (!c) continue;
      fprintf(stdout, "%c", c);
    } while(c);
    fprintf(stdout, "\n");
    ptr = pamu_next(fd, ptr);
  }

  close(fd);
  return 0;
}
