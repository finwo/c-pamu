#ifndef __FINWO_PAMU_H__
#define __FINWO_PAMU_H__

#include <stdint.h>
#include <stdio.h>

#ifndef PAMU_T_MARKER
#define PAMU_T_MARKER   int64_t
#endif
#ifndef PAMU_T_POINTER
#define PAMU_T_POINTER  int64_t
#endif

#define PAMU_T_MARKER_SIZE  sizeof(PAMU_T_MARKER)
#define PAMU_T_POINTER_SIZE sizeof(PAMU_T_POINTER)

#define  PAMU_DEFAULT  (0)
#define  PAMU_DYNAMIC  (1 << 31)
#define  PAMU_FLAGS    (PAMU_DYNAMIC)

#define  PAMU_ERR_NONE                 (  0)
#define  PAMU_ERR_MEDIUM_SIZE          (- 1)
#define  PAMU_ERR_SEEK                 (- 2)
#define  PAMU_ERR_NEGATIVE_SIZE        (- 3)
#define  PAMU_ERR_READ_MALFORMED       (- 4)
#define  PAMU_ERR_MEDIUM_UNINITIALIZED (- 5)
#define  PAMU_ERR_MEDIUM_FULL          (- 6)
#define  PAMU_ERR_WRITE                (- 7)
#define  PAMU_ERR_OUT_OF_BOUNDS        (- 8)
#define  PAMU_ERR_INVALID_ADDRESS      (- 9)
#define  PAMU_ERR_DOUBLE_FREE          (-10)

// In-file structure
//   header:
//     "PAMU"     Keyword           To check if an FD was already initialized
//     uint32_t   flags|headerSize  Feature flags + size of the header on medium
//   entry_free:
//     uint64_t   free|size         Free marker/flag + size of the entry
//     uint64_t   pointer           Pointer to the previous free entry
//     uint64_t   pointer           Pointer to the next free entry
//     char[]     blob              Unused space
//     uint64_t   free|size         Free marker/flag + size of the entry
//   entry_allocated:
//     uint64_t   size              Size of the entry
//     char[16+]  blob              Application data
//     uint64_t   size              Size of the entry

// Open/close functionality
int pamu_init(int fd, uint32_t flags);

// Core, alloc & free within the medium
PAMU_T_POINTER  pamu_alloc(int fd, PAMU_T_MARKER   size);
int             pamu_free(int fd , PAMU_T_POINTER  addr);
PAMU_T_MARKER   pamu_size(int fd , PAMU_T_POINTER  addr);

// Iteration, so clients can find a reference
PAMU_T_POINTER  pamu_next(int fd , PAMU_T_POINTER  addr);

#endif // __FINWO_PAMU_H__
