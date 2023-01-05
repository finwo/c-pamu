#include "pamu.h"
#include "test.h"

#include <endian.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char * temptemplate = "test-pamu-XXXXXX";
char * tempfolder   = "/tmp";

void test_init_dynamic() {
  uint32_t u32;
  int64_t  i64;

  // Open tmp file
  char * tempfile = calloc(1,strlen(temptemplate)+strlen(tempfolder)+2);
  strcat(tempfile, tempfolder);
  strcat(tempfile, "/");
  strcat(tempfile, temptemplate);
  int fd = mkstemp(tempfile);

  // Basic initialize
  int rc = pamu_init(fd, PAMU_DEFAULT | PAMU_DYNAMIC);
  ASSERT("Medium initialized without errors", rc == 0); // Expect no errors

  // Check length of the file now, we expect 16 (keyword + uint32 + uint64)
  ASSERT("Initialized tmp file is 16 bytes", lseek(fd, 0, SEEK_END) == 16);

  // Verify keyword
  lseek(fd, 0, SEEK_SET);
  read(fd, &u32, 4);
  ASSERT("Medium starts with keyword after initialization", be32toh(u32) == 0x50414d55); // "PAMU"

  // Verify headersize and flags
  read(fd, &u32, 4);
  ASSERT("Header size is merged with flags properly", be32toh(u32) == (PAMU_DEFAULT | PAMU_DYNAMIC | 16));

  // Verify managedBytes
  read(fd, &i64, 8);
  ASSERT("Managed bytes marked at header end", be64toh(i64) == 16);

  // Remove the temporary file
  close(fd);
  unlink(tempfile);
  free(tempfile);
}

void test_init_static() {
  uint32_t u32;
  int64_t  i64;

  // Open tmp file
  char * tempfile = calloc(1,strlen(temptemplate)+strlen(tempfolder)+2);
  strcat(tempfile, tempfolder);
  strcat(tempfile, "/");
  strcat(tempfile, temptemplate);
  int fd = mkstemp(tempfile);

  // Initialize 4k, emulating block device
  char *buf = calloc(1, 4096);
  write(fd, buf, 4096);
  free(buf);

  // Basic initialize
  int rc = pamu_init(fd, PAMU_DEFAULT);
  ASSERT("Medium initialized without errors", rc == 0); // Expect no errors

  // Check length of the file now, we expect 4096 (whole block device)
  ASSERT("Initializing on larger medium doesn't truncate", lseek(fd, 0, SEEK_END) == 4096);

  // Verify keyword
  lseek(fd, 0, SEEK_SET);
  read(fd, &u32, 4);
  ASSERT("Medium starts with keyword after initialization", be32toh(u32) == 0x50414d55); // "PAMU"
                                                                                         //
  // Verify headersize and flags
  read(fd, &u32, 4);
  ASSERT("Header size is merged with flags properly", be32toh(u32) == (PAMU_DEFAULT | 16));

  // Verify managedBytes
  read(fd, &i64, 8);
  ASSERT("Managed bytes marked at header end", be64toh(i64) == 16);

  // Remove the temporary file
  close(fd);
  unlink(tempfile);
  free(tempfile);
}

void test_alloc_dynamic() {

  // Open tmp file
  char * tempfile = calloc(1,strlen(temptemplate)+strlen(tempfolder)+2);
  strcat(tempfile, tempfolder);
  strcat(tempfile, "/");
  strcat(tempfile, temptemplate);
  int fd = mkstemp(tempfile);

  // Basic initialize
  int rc = pamu_init(fd, PAMU_DEFAULT | PAMU_DYNAMIC);
  ASSERT("Medium initialized without errors", rc == 0); // Expect no errors

  // Initial allocate
  int64_t addr_0 = pamu_alloc(fd, 64);

  ASSERT("1st allocation does not return an error", addr_0 >  0);
  ASSERT("1st allocation is done right after the header", addr_0 == 24);
  int64_t addr_1 = pamu_alloc(fd, 64);
  ASSERT("2nd allocation does not return an error", addr_1 > 0);
  ASSERT("2nd allocation is done right after the 1st alloc", addr_1 == 104);

  ASSERT("2 consecutive allocations must not be the same", addr_0 != addr_1);

  // Remove the temporary file
  close(fd);
  unlink(tempfile);
  free(tempfile);
}

void test_alloc_static() {
  // TODO: write test here
}

int main() {

  // Update temp folder from fallback
  char *tmpdir = getenv("TMPDIR");
  if (tmpdir) tempfolder = tmpdir;

  RUN(test_init_dynamic);
  RUN(test_init_static);

  RUN(test_alloc_dynamic);
  /* RUN(test_alloc_static); */

  return TEST_REPORT();
}
