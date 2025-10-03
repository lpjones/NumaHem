#include "interpose.h"

void* (*libc_mmap)(void *addr, size_t length, int prot, int flags, int fd, off_t offset) = NULL;
int (*libc_munmap)(void *addr, size_t length) = NULL;
void* (*libc_malloc)(size_t size) = NULL;
void (*libc_free)(void* ptr) = NULL;

_Thread_local bool internal_call = false;
pid_t main_pid = 0;

static int mmap_filter(void *addr, size_t length, int prot, int flags, int fd, off_t offset, uint64_t *result)
{   
    if (main_pid == 0) {
      main_pid = getpid();
    }
    if (internal_call) {
      LOG_DEBUG("MMAP: internal call: mmap(%p, %lu, %d, %d, %d, %lu)\n", addr, length, prot, flags, fd, offset);
      return 1;
    }

    if ((flags & MAP_ANONYMOUS) != MAP_ANONYMOUS) {
      LOG_DEBUG("MMAP: not anonymous: mmap(%p, %lu, %d, %d, %d, %lu)\n", addr, length, prot, flags, fd, offset);
      return 1;
    }

    if ((prot & PROT_EXEC) == PROT_EXEC) {
      LOG_DEBUG("MMAP: PROT_EXEC: mmap(%p, %lu, %d, %d, %d, %lu)\n", addr, length, prot, flags, fd, offset);
      return 1;
    }

    if (prot == PROT_NONE) {
      LOG_DEBUG("MMAP: PROT_NONE: mmap(%p, %lu, %d, %d, %d, %lu)\n", addr, length, prot, flags, fd, offset);
      return 1;
    }

    if (main_pid != getpid()) {
      LOG_DEBUG("MMAP: not main_pid: mmap(%p, %lu, %d, %d, %d, %lu)\n", addr, length, prot, flags, fd, offset);
      return 1;
    }

    // if (length < PAGE_SIZE) {
    //   LOG_DEBUG("MMAP: allocation too small: mmap(%p, %lu, %d, %d, %d, %lu)\n", addr, length, prot, flags, fd, offset);
    //   return 1;
    // }

    *result = (uint64_t)tmem_mmap(addr, length, prot, flags, fd, offset);
    if (*result == (uint64_t)MAP_FAILED) {
      LOG_DEBUG("tmem mmap failed for %p, length: %lu\n", addr, length);
      return 1;
    }
    return 0;
    
}


static int munmap_filter(void *addr, size_t length, uint64_t* result)
{
    if (internal_call) {
      LOG_DEBUG("MUNMAP: internal call: munmap(%p, %lu)\n", addr, length);

      return 1;
    }

//   if ((*result = hemem_munmap(addr, length)) == -1) {
//     LOG("hemem munmap failed\n\tmunmap(0x%lx, %ld)\n", (uint64_t)addr, length);
//   }
// printf("Get hooked fool\n");
    LOG_DEBUG("MUNMAP: tmem_munmap(%p, %lu)\n", addr, length);

    *result = tmem_munmap(addr, length);
    if (*result == -1) {
      LOG_DEBUG("tmem_munmap failed\n");
    }
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
    internal_call = true;
    libc_mmap = bind_symbol("mmap");
    libc_munmap = bind_symbol("munmap");
    libc_malloc = bind_symbol("malloc");
    libc_free = bind_symbol("free");
    intercept_hook_point = hook;
    
    
    init_log_files();
    LOG_DEBUG("MAP_ANONYMOUS: %d, MAP_STACK: %d, PROT_EXEC: %d, MAP_SHARED: %d\n \
              MAP_FIXED: %d, MAP_FIXED_NOREPLACE: %d\n \
              PROT_READ: %d, PROT_WRITE: %d, PROT_EXEC: %d, PROT_NONE: %d\n", 
              MAP_ANONYMOUS, MAP_STACK, PROT_EXEC, MAP_SHARED, MAP_FIXED, 
              MAP_FIXED_NOREPLACE, PROT_READ, PROT_WRITE, PROT_EXEC, PROT_NONE);


    LOG_DEBUG("pebs_init\n");
    pebs_init();

    LOG_DEBUG("tmem_init\n");
    tmem_init();
    internal_call = false;

//   int ret = mallopt(M_MMAP_THRESHOLD, 0);
//   if (ret != 1) {
//     perror("mallopt");
//   }
//   assert(ret == 1);
}

static __attribute__((destructor)) void tmem_shutdown(void)
{
    tmem_cleanup();
    // pebs_cleanup();
}
