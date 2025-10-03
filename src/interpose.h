#ifndef _INTERPOSE_HEADER
#define _INTERPOSE_HEADER

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <libsyscall_intercept_hook_point.h>
#include <syscall.h>
#include <errno.h>
#include <dlfcn.h>
#include <assert.h>
#include <malloc.h>
#include <numa.h>

#include "tmem.h"
#include "pebs.h"

extern _Thread_local bool internal_call;

// function pointers to libc functions
extern void* (*libc_mmap)(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
extern int (*libc_munmap)(void *addr, size_t length);
extern void* (*libc_malloc)(size_t size);
extern void (*libc_free)(void* p);

#endif