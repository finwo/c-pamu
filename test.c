#include "pamu.h"
#include "test.h"

#include <endian.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Overloaded ntoh & hton
int64_t tntoh_i64(int64_t v) {
  return be64toh(v);
}
int64_t thton_i64(int64_t v) {
  return htobe64(v);
}
uint64_t tntoh_u64(uint64_t v) {
  return be64toh(v);
}
uint64_t thton_u64(uint64_t v) {
  return htobe64(v);
}
int32_t tntoh_i32(int32_t v) {
  return be32toh(v);
}
int32_t thton_i32(int32_t v) {
  return htobe32(v);
}
uint32_t tntoh_u32(uint32_t v) {
  return be32toh(v);
}
uint32_t thton_u32(uint32_t v) {
  return htobe32(v);
}
#define thton(v) _Generic(v, uint32_t: thton_u32, int32_t: thton_i32, int64_t: thton_i64, uint64_t: thton_u64)(v)
#define tntoh(v) _Generic(v, uint32_t: tntoh_u32, int32_t: tntoh_i32, int64_t: tntoh_i64, uint64_t: tntoh_u64)(v)

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
  ASSERT("Medium starts with keyword after initialization", tntoh(u32) == 0x50414d55); // "PAMU"

  // Verify headersize and flags
  read(fd, &u32, 4);
  ASSERT("Header size is merged with flags properly", tntoh(u32) == (PAMU_DEFAULT | PAMU_DYNAMIC | 8));

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
  ASSERT("Medium starts with keyword after initialization", tntoh(u32) == 0x50414d55); // "PAMU"
                                                                                         //
  // Verify headersize and flags
  read(fd, &u32, 4);
  ASSERT("Header size is merged with flags properly", tntoh(u32) == (PAMU_DEFAULT | 8));

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
  ASSERT("1st allocation is done right after the header", addr_0 == (8 + PAMU_T_MARKER_SIZE));

  // 2nd allocation
  int64_t addr_1 = pamu_alloc(fd, 64);
  ASSERT("2nd allocation does not return an error", addr_1 > 0);
  ASSERT("2nd allocation is done right after the 1st alloc", addr_1 == (8 + (3*PAMU_T_MARKER_SIZE) + 64));

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
  free(buf);

  // Basic initialize
  int rc = pamu_init(fd, PAMU_DEFAULT);
  ASSERT("Medium initialized without errors", rc == 0);

  // 1st allocation
  int64_t addr_0 = pamu_alloc(fd, 64);
  ASSERT("1st allocation does not return an error", addr_0 >  0);
  ASSERT("1st allocation is done right after the header", addr_0 == (8 + PAMU_T_MARKER_SIZE));

  /* // 2nd allocation */
  int64_t addr_1 = pamu_alloc(fd, 64);
  ASSERT("2nd allocation does not return an error", addr_1 > 0);
  ASSERT("2nd allocation is done right after the 1st alloc", addr_1 == (8 + (3*PAMU_T_MARKER_SIZE) + 64));

  // 3rd allocation (should fail)
  int64_t addr_2 = pamu_alloc(fd, 1024);
  ASSERT("3rd allocation returns PAMU_ERR_MEDIUM_FULL", addr_2 == PAMU_ERR_MEDIUM_FULL);

  // Remove the temporary file
  close(fd);
  unlink(tempfile);
  free(tempfile);
}

void test_free_dynamic() {
  int i;

  // Open tmp file
  char * tempfile = calloc(1,strlen(temptemplate)+strlen(tempfolder)+2);
  strcat(tempfile, tempfolder);
  strcat(tempfile, "/");
  strcat(tempfile, temptemplate);
  int fd = mkstemp(tempfile);

  // Open tmp file
  char * temp2file = calloc(1,strlen(temptemplate)+strlen(tempfolder)+2);
  strcat(temp2file, tempfolder);
  strcat(temp2file, "/");
  strcat(temp2file, temptemplate);
  int fd2 = mkstemp(temp2file);

  // Basic initialize
  int rc  = pamu_init(fd , PAMU_DEFAULT | PAMU_DYNAMIC);
  int rc2 = pamu_init(fd2, PAMU_DEFAULT | PAMU_DYNAMIC);
  ASSERT("Medium 1 initialized without errors", rc  == 0);
  ASSERT("Medium 2 initialized without errors", rc2 == 0);

  int alloc_count = 7;
  PAMU_T_POINTER allocations[7];

                       pamu_alloc(fd2, 64);
  PAMU_T_POINTER a21 = pamu_alloc(fd2, 64);
  PAMU_T_POINTER a22 = pamu_alloc(fd2, 64);

  // Double-check allocations went correctly
  PAMU_T_POINTER prev, next;
  for(i=0; i<alloc_count; i++) {
    allocations[i] = pamu_alloc(fd, 64);
    ASSERT("allocation N is at the correct position", allocations[i] == (8 + PAMU_T_MARKER_SIZE) + ((64 + (2 * PAMU_T_MARKER_SIZE)) * i));
    lseek(fd, allocations[i], SEEK_SET);
    read(fd, &prev, PAMU_T_POINTER_SIZE);
    read(fd, &next, PAMU_T_POINTER_SIZE);
    ASSERT("allocation N's pointers are 0", (prev|next) == 0);
  }

  // Test the multiple free scenarios
  pamu_free(fd, allocations[0]);
  pamu_free(fd, allocations[2]);
  pamu_free(fd, allocations[4]);
  pamu_free(fd, allocations[6]);
  pamu_free(fd, allocations[3]);

  pamu_free(fd2, a21);
  pamu_free(fd2, a22);

  lseek(fd, allocations[0], SEEK_SET);
  read(fd, &prev, PAMU_T_POINTER_SIZE);
  read(fd, &next, PAMU_T_POINTER_SIZE);
  ASSERT("a0.prev == 0"       , tntoh(prev) == 0);
  ASSERT("a0.next == a2.outer", tntoh(next) == allocations[2] - PAMU_T_MARKER_SIZE);

  lseek(fd, allocations[1], SEEK_SET);
  read(fd, &prev, PAMU_T_POINTER_SIZE);
  read(fd, &next, PAMU_T_POINTER_SIZE);
  ASSERT("a1.prev == 0", tntoh(prev) == 0);
  ASSERT("a1.next == 0", tntoh(next) == 0);

  lseek(fd, allocations[2], SEEK_SET);
  read(fd, &prev, PAMU_T_POINTER_SIZE);
  read(fd, &next, PAMU_T_POINTER_SIZE);
  ASSERT("a2.prev == a0.outer", tntoh(prev) == allocations[0] - PAMU_T_MARKER_SIZE);
  ASSERT("a2.next == 0"       , tntoh(next) == 0);

  lseek(fd, allocations[3], SEEK_SET);
  read(fd, &prev, PAMU_T_POINTER_SIZE);
  read(fd, &next, PAMU_T_POINTER_SIZE);
  ASSERT("a3.prev == a2.outer", tntoh(prev) == allocations[2] - PAMU_T_MARKER_SIZE);
  ASSERT("a3.next == a4.outer", tntoh(next) == allocations[4] - PAMU_T_MARKER_SIZE);

  lseek(fd, allocations[4], SEEK_SET);
  read(fd, &prev, PAMU_T_POINTER_SIZE);
  read(fd, &next, PAMU_T_POINTER_SIZE);
  ASSERT("a4.prev == a3.outer", tntoh(prev) == allocations[3] - PAMU_T_MARKER_SIZE);
  ASSERT("a4.next == 0"       , tntoh(next) == 0);

  lseek(fd, allocations[5], SEEK_SET);
  read(fd, &prev, PAMU_T_POINTER_SIZE);
  read(fd, &next, PAMU_T_POINTER_SIZE);
  ASSERT("a5.prev == 0", tntoh(prev) == 0);
  ASSERT("a5.next == 0", tntoh(next) == 0);

  // Check length of the file now, we expect 0x0x1e8
  ASSERT("a6 has been truncated off", lseek(fd, 0, SEEK_END) == (
    8 + // header
    ((alloc_count - 1) * ((2 * PAMU_T_MARKER_SIZE) + 64))
  ));

  // Check length of the file now, we expect 0x0x1e8
  ASSERT("both a21 and a22 have been truncated off", lseek(fd2, 0, SEEK_END) == (
    8 + // header
    ((2 * PAMU_T_MARKER_SIZE) + 64)
  ));

  // Remove the temporary file
  close(fd);
  close(fd2);
  unlink(tempfile);
  unlink(temp2file);
  free(tempfile);
  free(temp2file);
}

void test_free_static() {
  PAMU_T_POINTER prev, next;

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
  int rc = pamu_init(fd , PAMU_DEFAULT);
  ASSERT("Medium initialized without errors", rc == 0);

  PAMU_T_POINTER a0 = pamu_alloc(fd, 64);
  PAMU_T_POINTER a1 = pamu_alloc(fd, 64);
  PAMU_T_POINTER a2 = pamu_alloc(fd, 64);

  pamu_free(fd, a1);
  pamu_free(fd, a2);

  lseek(fd, a0, SEEK_SET);
  read(fd, &prev, PAMU_T_POINTER_SIZE);
  read(fd, &next, PAMU_T_POINTER_SIZE);
  ASSERT("a0.prev == 0", tntoh(prev) == 0);
  ASSERT("a0.next == 0", tntoh(next) == 0);

  lseek(fd, a1, SEEK_SET);
  read(fd, &prev, PAMU_T_POINTER_SIZE);
  read(fd, &next, PAMU_T_POINTER_SIZE);
  ASSERT("a1.prev == 0", tntoh(prev) == 0);
  ASSERT("a1.next == 0", tntoh(next) == 0);

  lseek(fd, a2, SEEK_SET);
  read(fd, &prev, PAMU_T_POINTER_SIZE);
  read(fd, &next, PAMU_T_POINTER_SIZE);

  ASSERT("a2.prev == a1.outer"        , tntoh(prev) == (a1 - PAMU_T_MARKER_SIZE));
  ASSERT("a2.next == medium.remainder", tntoh(next) == (a2 + 64 + PAMU_T_MARKER_SIZE));

  // Remove the temporary file
  close(fd);
  unlink(tempfile);
  free(tempfile);
}

void test_comfort_size() {

  // Open tmp file
  char * tempfile = calloc(1,strlen(temptemplate)+strlen(tempfolder)+2);
  strcat(tempfile, tempfolder);
  strcat(tempfile, "/");
  strcat(tempfile, temptemplate);
  int fd = mkstemp(tempfile);

  // Basic initialize
  int rc = pamu_init(fd , PAMU_DEFAULT | PAMU_DYNAMIC);
  ASSERT("Medium initialized without errors", rc  == 0);

  PAMU_T_POINTER a0 = pamu_alloc(fd,   64);
  PAMU_T_POINTER a1 = pamu_alloc(fd,  128);
  PAMU_T_POINTER a2 = pamu_alloc(fd,   64);
  PAMU_T_POINTER a3 = pamu_alloc(fd, 1024);
  PAMU_T_POINTER a4 = pamu_alloc(fd,  256);

  ASSERT("a0.size ==   64", pamu_size(fd, a0) ==   64);
  ASSERT("a1.size ==  128", pamu_size(fd, a1) ==  128);
  ASSERT("a2.size ==   64", pamu_size(fd, a2) ==   64);
  ASSERT("a3.size == 1024", pamu_size(fd, a3) == 1024);
  ASSERT("a4.size ==  256", pamu_size(fd, a4) ==  256);

  // Remove the temporary file
  close(fd);
  unlink(tempfile);
  free(tempfile);
}

void test_comfort_next() {

  // Open tmp file
  char * tempfile = calloc(1,strlen(temptemplate)+strlen(tempfolder)+2);
  strcat(tempfile, tempfolder);
  strcat(tempfile, "/");
  strcat(tempfile, temptemplate);
  int fd = mkstemp(tempfile);

  // Basic initialize
  int rc = pamu_init(fd , PAMU_DEFAULT | PAMU_DYNAMIC);
  ASSERT("Medium initialized without errors", rc  == 0);

  // Assign some blocks
  PAMU_T_POINTER a0 = pamu_alloc(fd,   64);
  PAMU_T_POINTER a1 = pamu_alloc(fd,  128);
  PAMU_T_POINTER a2 = pamu_alloc(fd,   64);
  PAMU_T_POINTER a3 = pamu_alloc(fd, 1024);
  PAMU_T_POINTER a4 = pamu_alloc(fd,  256);

  // Free one, the next fn should skip this one
  pamu_free(fd, a2);

  PAMU_T_POINTER current = 0;
  ASSERT("next(0)  == a0", (current = pamu_next(fd, current)) == a0);
  ASSERT("next(a0) == a1", (current = pamu_next(fd, current)) == a1);
  ASSERT("next(a1) == a3", (current = pamu_next(fd, current)) == a3);
  ASSERT("next(a3) == a4", (current = pamu_next(fd, current)) == a4);
  ASSERT("next(a4) ==  0", (current = pamu_next(fd, current)) ==  0);

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
  RUN(test_free_static);

  RUN(test_comfort_size);
  RUN(test_comfort_next);

  return TEST_REPORT();
}
