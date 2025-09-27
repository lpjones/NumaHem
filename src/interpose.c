#include "interpose.h"

void* (*libc_mmap)(void *addr, size_t length, int prot, int flags, int fd, off_t offset) = NULL;
int (*libc_munmap)(void *addr, size_t length) = NULL;
void* (*libc_malloc)(size_t size) = NULL;
void (*libc_free)(void* ptr) = NULL;

static int mmap_filter(void *addr, size_t length, int prot, int flags, int fd, off_t offset, uint64_t *result)
{
    // LOG("hemem interpose: calling hemem mmap(0x%lx, %ld, %x, %x, %d, %ld)\n", (uint64_t)addr, length, prot, flags, fd, offset);
    // if ((*result = (uint64_t)tmem_mmap(addr, length, prot, flags, fd, offset)) == (uint64_t)MAP_FAILED) {
    //     // hemem failed for some reason, try libc
    //     LOG("hemem mmap failed\n\tmmap(0x%lx, %ld, %x, %x, %d, %ld)\n", (uint64_t)addr, length, prot, flags, fd, offset);
    // }
    // printf("Get mmap hooked fool\n");
    *result = tmem_mmap(addr, length, prot, flags, fd, offset);
    return 1;
    
}


static int munmap_filter(void *addr, size_t length, uint64_t* result)
{
//   if (internal_call) {
//     return 1;
//   }

//   if ((*result = hemem_munmap(addr, length)) == -1) {
//     LOG("hemem munmap failed\n\tmunmap(0x%lx, %ld)\n", (uint64_t)addr, length);
//   }
// printf("Get hooked fool\n");
  return 1;
}


static void* bind_symbol(const char *sym)
{
  void *ptr;
  if ((ptr = dlsym(RTLD_NEXT, sym)) == NULL) {
    fprintf(stderr, "tmem memory manager interpose: dlsym failed (%s)\n", sym);
    abort();
  }
  return ptr;
}

static int hook(long syscall_number, long arg0, long arg1, long arg2, long arg3, long arg4, long arg5, long *result)
{
	if (syscall_number == SYS_mmap) {
	  return mmap_filter((void*)arg0, (size_t)arg1, (int)arg2, (int)arg3, (int)arg4, (off_t)arg5, (uint64_t*)result);
	} else if (syscall_number == SYS_munmap){
    return munmap_filter((void*)arg0, (size_t)arg1, (uint64_t*)result);
    } else {
        // ignore non-mmap system calls
		return 1;
	}
}

static __attribute__((constructor)) void init(void)
{
    libc_mmap = bind_symbol("mmap");
    libc_munmap = bind_symbol("munmap");
    libc_malloc = bind_symbol("malloc");
    libc_free = bind_symbol("free");
    intercept_hook_point = hook;
    printf("in constructor\n");
    pebs_init();

    tmem_init();

//   int ret = mallopt(M_MMAP_THRESHOLD, 0);
//   if (ret != 1) {
//     perror("mallopt");
//   }
//   assert(ret == 1);
//   tmem_init();
}

static __attribute__((destructor)) void tmem_shutdown(void)
{
//   hemem_stop();
    printf("destroyed noob\n");
    tmem_cleanup();
}
