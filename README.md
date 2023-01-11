PAMU
====

Persistent Allocation Management Unit

[![License](https://img.shields.io/github/license/finwo/c-pamu?style=flat-square)](https://github.com/finwo/c-pamu/blob/main/LICENSE)

Key components
--------------

| File             | Description                                                        |
| ---------------- | ------------------------------------------------------------------ |
| [pamu.c][pamu.c] | Implementation file of the library                                 |
| [pamu.h][pamu.h] | Definitions file, describing the exported functions in the library |

Introduction
------------

PAMU is a persistent allocation medium manager library, intended to provide
logically separated binary blobs within an otherwise unmanaged medium, whether
that be a file on the local filesystem or a fixed-size block device.

Used by
-------

You?

Features
--------

- Allocate N bytes to get a file index
- Free a previously allocated file index
- Iterate over allocated blobs
- Persistent pointers within a file
- Dynamically grow storage files
- Truncate storage file upon free

Installation
------------

PAMU currently only supports being included into a project as-is and does not
support being installed as a system library.

To include PAMU in your project, simply copy both [pamu.c][pamu.c] and
[pamu.h][pamu.h] into your project, make sure pamu.h is in your include folders,
and compile pamu.c along with your other source files.

API
---

```c
int pamu_init(int fd, uint32_t flags);
```

Initialize a medium with feature flags given.

Returns:

- positive integer: should never occur, please raise an issue with the author
- 0: Medium initialized without issues
- negative integer: error, check with one of the error definitions

```c
PAMU_T_POINTER  pamu_alloc(int fd, PAMU_T_MARKER   size);
```

Attempts to allocate a binary blob if &lt;size&gt; bytes within the medium.

Returns:

- positive integer: allocated without issues, the returned int is your pointer
- 0: should never occur, please raise an issue with the author
- negative integer: error, check with one of the error definitions

```c
int             pamu_free(int fd , PAMU_T_POINTER  addr);
```

Attempts to free a binary blob from previously allocated pointer.

Returns:

- positive integer: should never occur, please raise an issue with the author
- 0: freed without issues
- negative integer: error, check with one of the error definitions

```c
PAMU_T_MARKER   pamu_size(int fd , PAMU_T_POINTER  addr);
```

Detects the size of the blob by it's pointer

Returns:

- positive integer: succesfully detected blob size, number is the size in bytes
- 0: should never occur, please raise an issue with the author
- negative integer: error, check with one of the error definitions

```c
PAMU_T_POINTER  pamu_next(int fd , PAMU_T_POINTER  addr);
```

Finds the next allocated binary blob within the medium based on the current
address.

If 0 is given as an address, it'll return the first allocated blob in the
medium.

Returns:

- positive integer: allocated without issues, the returned int is your pointer
- 0: should never occur, please raise an issue with the author
- negative integer: error, check with one of the error definitions

Feature flags
-------------

```
PAMU_DEFAULT
```

Feature flag to initialize the medium with the default features enabled by the
author.

```
PAMU_DYNAMIC
```

Marks the medium to be initialized as supporting dynamic sizing, like a file on
a posix filesystem, to enable growing and truncating of the medium.

Errors
------

```
PAMU_ERR_NONE                 (  0)
```

No error has occurred.

```
PAMU_ERR_MEDIUM_SIZE          (- 1)
```

An unsupported medium size was given.

```
PAMU_ERR_SEEK                 (- 2)
```

An error occurred seeking throughout the medium, check errno for more
information.

```
PAMU_ERR_NEGATIVE_SIZE        (- 3)
```

A negative size was given for allocation, which is not supported.

```
PAMU_ERR_READ_MALFORMED       (- 4)
```

A read of the medium has failed, check errno for more information.

```
PAMU_ERR_MEDIUM_UNINITIALIZED (- 5)
```

You tried to perform an action on an uninitialized medium. This may happen on
mediums that were either not initialized or not managed by PAMU.

```
PAMU_ERR_MEDIUM_FULL          (- 6)
```

No consecutive free space was found suitable for the attempted allocation.
Either free up another blob to provide space, re-allocate some to merge free
space or use a different medium.

```
PAMU_ERR_WRITE                (- 7)
```

A read to the medium has failed, check errno for more information.

```
PAMU_ERR_OUT_OF_BOUNDS        (- 8)
```

An attempt has been detected to access data outside of the managed space.
Inspect your application for logical errors.

```
PAMU_ERR_INVALID_ADDRESS      (- 9)
```

You're trying to free an address that is not a logical blob within the medium.
Inspect your application for logical errors.

```
PAMU_ERR_DOUBLE_FREE          (-10)
```

You're trying to free a logical blob that was already freed. Inspect your
application for logical errors.

Examples
--------

### Full example with init, alloc, free & iteration

```c
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
```

#### Compile and run

```sh
cd example
make
./full-example
```

[pamu.c]: src/pamu.c
[pamu.h]: src/pamu.h
