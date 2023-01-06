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

  // Open tmp file
  char * tempfile = calloc(1,strlen(temptemplate)+strlen(tempfolder)+2);
  strcat(tempfile, tempfolder);
  strcat(tempfile, "/");
  strcat(tempfile, temptemplate);
  int fd = mkstemp(tempfile);

  // Basic initialize
  int rc = pamu_init(fd, PAMU_DEFAULT | PAMU_DYNAMIC);
  ASSERT("Medium initialized without errors", rc == 0); // Expect no errors

  // Check length of the file now, we expect 8 (keyword + uint32)
  ASSERT("Initialized tmp file is 8 bytes", lseek(fd, 0, SEEK_END) == 8);

  // Verify keyword
  lseek(fd, 0, SEEK_SET);
  read(fd, &u32, 4);
  ASSERT("Medium starts with keyword after initialization", be32toh(u32) == 0x50414d55); // "PAMU"

  // Verify headersize and flags
  read(fd, &u32, 4);
  ASSERT("Header size is merged with flags properly", be32toh(u32) == (PAMU_DEFAULT | PAMU_DYNAMIC | 8));

  // Remove the temporary file
  close(fd);
  unlink(tempfile);
  free(tempfile);
}

void test_init_static() {
  uint32_t u32;

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
  ASSERT("Medium initialized without errors", rc == 0);

  // Check length of the file now, we expect 4096 (whole block device)
  ASSERT("Initializing on larger medium doesn't truncate", lseek(fd, 0, SEEK_END) == 4096);

  // Verify keyword
  lseek(fd, 0, SEEK_SET);
  read(fd, &u32, 4);
  ASSERT("Medium starts with keyword after initialization", be32toh(u32) == 0x50414d55); // "PAMU"
                                                                                         //
  // Verify headersize and flags
  read(fd, &u32, 4);
  ASSERT("Header size is merged with flags properly", be32toh(u32) == (PAMU_DEFAULT | 8));

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
  ASSERT("Medium initialized without errors", rc == 0);

  // 1st allocation
  int64_t addr_0 = pamu_alloc(fd, 64);
  ASSERT("1st allocation does not return an error", addr_0 >  0);
  ASSERT("1st allocation is done right after the header", addr_0 == 16);

  // 2nd allocation
  int64_t addr_1 = pamu_alloc(fd, 64);
  ASSERT("2nd allocation does not return an error", addr_1 > 0);
  ASSERT("2nd allocation is done right after the 1st alloc", addr_1 == 96);

  // Remove the temporary file
  close(fd);
  unlink(tempfile);
  free(tempfile);
}

void test_alloc_static() {

  // Open tmp file
  char * tempfile = calloc(1,strlen(temptemplate)+strlen(tempfolder)+2);
  strcat(tempfile, tempfolder);
  strcat(tempfile, "/");
  strcat(tempfile, temptemplate);
  int fd = mkstemp(tempfile);

  // Emulate a block device with fixed size
  char *buf = calloc(1, 1024);
  write(fd, buf, 1024);

  // Basic initialize
  int rc = pamu_init(fd, PAMU_DEFAULT);
  ASSERT("Medium initialized without errors", rc == 0);

  // 1st allocation
  int64_t addr_0 = pamu_alloc(fd, 64);
  ASSERT("1st allocation does not return an error", addr_0 >  0);
  ASSERT("1st allocation is done right after the header", addr_0 == 16);

  /* // 2nd allocation */
  int64_t addr_1 = pamu_alloc(fd, 64);
  ASSERT("2nd allocation does not return an error", addr_1 > 0);
  ASSERT("2nd allocation is done right after the 1st alloc", addr_1 == 96);

  // 3rd allocation (should fail)
  int64_t addr_2 = pamu_alloc(fd, 1024);
  ASSERT("3rd allocation returns PAMU_ERR_MEDIUM_FULL", addr_2 == PAMU_ERR_MEDIUM_FULL);

  // Remove the temporary file
  close(fd);
  unlink(tempfile);
  free(tempfile);
}

void test_free_dynamic() {

  // Open tmp file
  char * tempfile = calloc(1,strlen(temptemplate)+strlen(tempfolder)+2);
  strcat(tempfile, tempfolder);
  strcat(tempfile, "/");
  strcat(tempfile, temptemplate);
  int fd = mkstemp(tempfile);

  fprintf(stdout, "Tempfile: %s\n", tempfile);

  // Basic initialize
  int rc = pamu_init(fd, PAMU_DEFAULT | PAMU_DYNAMIC);
  ASSERT("Medium initialized without errors", rc == 0);

  // 1st allocation
  int64_t addr_0 = pamu_alloc(fd, 64);
  ASSERT("1st allocation does not return an error", addr_0 >  0);
  ASSERT("1st allocation is done right after the header", addr_0 == 16);

  // 2nd allocation
  int64_t addr_1 = pamu_alloc(fd, 64);
  ASSERT("2nd allocation does not return an error", addr_1 >  0);
  ASSERT("2nd allocation is done right after the 1st alloc", addr_1 == 96);

  // 3rd allocation
  int64_t addr_2 = pamu_alloc(fd, 64);
  ASSERT("3rd allocation does not return an error", addr_2 >  0);
  ASSERT("3rd allocation is done right after the 2nd alloc", addr_2 == 176);

  // 4th allocation
  int64_t addr_3 = pamu_alloc(fd, 64);
  ASSERT("4th allocation does not return an error", addr_3 >  0);
  ASSERT("4th allocation is done right after the 3rd alloc", addr_3 == 256);

  // Expect all innards to be 0/untouched here
  lseek(fd, addr_0, SEEK_SET);
  int64_t a0_prev; read(fd, &a0_prev, sizeof(int64_t));
  int64_t a0_next; read(fd, &a0_next, sizeof(int64_t));
  ASSERT("1st allocation's innards are zero", (a0_prev|a0_next) == 0);
  lseek(fd, addr_1, SEEK_SET);
  int64_t a1_prev; read(fd, &a1_prev, sizeof(int64_t));
  int64_t a1_next; read(fd, &a1_next, sizeof(int64_t));
  ASSERT("2nd allocation's innards are zero", (a1_prev|a1_next) == 0);
  lseek(fd, addr_2, SEEK_SET);
  int64_t a2_prev; read(fd, &a2_prev, sizeof(int64_t));
  int64_t a2_next; read(fd, &a2_next, sizeof(int64_t));
  ASSERT("3rd allocation's innards are zero", (a2_prev|a2_next) == 0);
  lseek(fd, addr_3, SEEK_SET);
  int64_t a3_prev; read(fd, &a3_prev, sizeof(int64_t));
  int64_t a3_next; read(fd, &a3_next, sizeof(int64_t));
  ASSERT("4th allocation's innards are zero", (a3_prev|a3_next) == 0);

  // Purposefully free out-of-order
  pamu_free(fd, addr_2);
  pamu_free(fd, addr_0);
  pamu_free(fd, addr_1);

  // TODO: fix these tests after merging of freed blocks has been implemented

  // Expect innards to have changed
  lseek(fd, addr_0, SEEK_SET);
  read(fd, &a0_prev, sizeof(int64_t));
  read(fd, &a0_next, sizeof(int64_t));
  ASSERT("1st allocation's previous pointer is still zero"        , be64toh(a0_prev) == 0);
  ASSERT("1st allocation's next pointer references 2nd allocation", be64toh(a0_next) == addr_1 - sizeof(int64_t));
  lseek(fd, addr_1, SEEK_SET);
  read(fd, &a1_prev, sizeof(int64_t));
  read(fd, &a1_next, sizeof(int64_t));
  ASSERT("2nd allocation's previous pointer references 1st allocation", be64toh(a1_prev) == addr_0 - sizeof(int64_t));
  ASSERT("2nd allocation's next pointer references 3rd allocation"    , be64toh(a1_next) == addr_2 - sizeof(int64_t));
  lseek(fd, addr_2, SEEK_SET);
  read(fd, &a2_prev, sizeof(int64_t));
  read(fd, &a2_next, sizeof(int64_t));
  ASSERT("3rd allocation's previous pointer references 2nd allocation", be64toh(a2_prev) == addr_1 - sizeof(int64_t));
  ASSERT("3rd allocation's next pointer is still zero allocation"     , be64toh(a2_next) == 0);
  lseek(fd, addr_3, SEEK_SET);
  read(fd, &a3_prev, sizeof(int64_t));
  read(fd, &a3_next, sizeof(int64_t));
  ASSERT("4th allocation's innards are still zero", (a3_prev|a3_next) == 0);

  // Remove the temporary file
  close(fd);
  unlink(tempfile);
  free(tempfile);
}

int main() {

  // Update temp folder from fallback
  char *tmpdir = getenv("TMPDIR");
  if (tmpdir) tempfolder = tmpdir;

  RUN(test_init_dynamic);
  RUN(test_init_static);

  RUN(test_alloc_dynamic);
  RUN(test_alloc_static);

  RUN(test_free_dynamic);

  return TEST_REPORT();
}
