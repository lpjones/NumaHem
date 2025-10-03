#include "tmem.h"

struct tmem_page *pages = NULL;
struct fifo_list hot_list;
struct fifo_list cold_list;
pthread_mutex_t pages_lock = PTHREAD_MUTEX_INITIALIZER;

long dram_free = 0;
long dram_size = 0;
long dram_used = 0;

_Atomic struct tmem_page *free_head = NULL;
_Atomic uint64_t free_count = 0;

static inline void push_free_page(struct tmem_page *p) {
    _Atomic struct tmem_page *old_head;
    // set p->next then CAS it into place
    do {
        old_head = atomic_load_explicit(&free_head, memory_order_acquire);
        atomic_store_explicit((_Atomic struct tmem_page **)&(p->next), old_head, memory_order_relaxed);
        // try to swap head from old_head -> p
    } while (!atomic_compare_exchange_weak_explicit(
                &free_head,
                &old_head,
                (_Atomic struct tmem_page *)p,
                memory_order_release,
                memory_order_relaxed));
    atomic_fetch_add_explicit(&free_count, 1, memory_order_relaxed);
}

static inline struct tmem_page* pop_free_page(void) {
    struct tmem_page *old_head;
    struct tmem_page *next;
    do {
        old_head = (struct tmem_page *)atomic_load_explicit(&free_head, memory_order_acquire);
        if (old_head == NULL) return NULL;
        next = (struct tmem_page *)atomic_load_explicit(&old_head->next, memory_order_relaxed);
        // try to swap head from old_head -> next
    } while (!atomic_compare_exchange_weak_explicit(
                &free_head,
                &old_head,
                (_Atomic struct tmem_page *)next,
                memory_order_acq_rel,
                memory_order_relaxed));
    atomic_fetch_sub_explicit(&free_count, 1, memory_order_relaxed);
    // clear next for safety/debug
    atomic_store_explicit(&old_head->next, NULL, memory_order_relaxed);
    return old_head;
}


// If the allocations are smaller than the PAGE_SIZE it's possible to 
void add_page(struct tmem_page *page) {
    struct tmem_page *p;
    pthread_mutex_lock(&pages_lock);

    // struct tmem_page *cur_page, *tmp;
    // LOG_DEBUG("pages: ");
    // HASH_ITER(hh, pages, cur_page, tmp) {
    //     LOG_DEBUG("0x%lx, ", cur_page->va);
    // }
    // LOG_DEBUG("\n");

    HASH_FIND(hh, pages, &(page->va), sizeof(uint64_t), p);
    if (p != NULL) {
        LOG_DEBUG("add_page: duplicate page: 0x%lx\n", page->va);
        // free(page);
        pthread_mutex_unlock(&pages_lock);
        return;
    }
    assert(p == NULL);
    HASH_ADD(hh, pages, va, sizeof(uint64_t), page);
    pthread_mutex_unlock(&pages_lock);
}

void remove_page(struct tmem_page *page)
{
  pthread_mutex_lock(&pages_lock);
  HASH_DEL(pages, page);
  pthread_mutex_unlock(&pages_lock);
}

struct tmem_page* find_page(uint64_t va)
{
  struct tmem_page *page;
  pthread_mutex_lock(&pages_lock);
  HASH_FIND(hh, pages, &va, sizeof(uint64_t), page);
  pthread_mutex_unlock(&pages_lock);
  return page;
}

void tmem_init() {
    internal_call = true;

    // LOG_DEBUG("DRAM size: %lu, REMOTE size: %lu\n", DRAM_SIZE, REMOTE_SIZE);

    LOG_DEBUG("finished tmem_init\n");

    struct tmem_page *dummy_page = calloc(1, sizeof(struct tmem_page));
    add_page(dummy_page);

    // check how much free space on dram
    dram_size = numa_node_size(DRAM_NODE, &dram_free);
    internal_call = false;
}

#define PAGE_ROUND_UP(x) (((x) + (PAGE_SIZE)-1) & (~((PAGE_SIZE)-1)))

void *numa_mmap_onnode(void *addr, size_t length, int prot, int flags, int fd, off_t offset, int node) {
    char *mem;
	unsigned long nodemask = 1UL << node;

	mem = libc_mmap(addr, length, prot, flags, fd, offset);  
	if (mem == (char *)-1)
		mem = NULL;
	else 
		mbind(mem, length, MPOL_BIND, &nodemask, 64, 0);

    return mem; 
}


void* tmem_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    internal_call = true;
    // length = PAGE_ROUND_UP(length);
    // void *p = libc_mmap(addr, length, prot, flags, fd, offset);
    int node = (length + dram_used + DRAM_BUFFER < dram_free) ? DRAM_NODE : REM_NODE;
    void *p = numa_mmap_onnode(addr, length, prot, flags, fd, offset, node);

    if (node == DRAM_NODE) {
        dram_used += length;
    }

    // LOG_DEBUG("dram_size: %ld, dram_free: %ld\n", dram_size, dram_free);
    if (p == MAP_FAILED) {
        LOG_DEBUG("mmap failed\n");
        return MAP_FAILED;
    }
    LOG_DEBUG("MMAP: numa_mmap_onnode(%p, %lu, %d, %d, %d, %lu, %d)\n", addr, length, prot, flags, fd, offset, node);
    pebs_stats.mem_allocated += length;

    assert((uint64_t)p % BASE_PAGE_SIZE == 0);

    // recycle pages from free_tmem_pages
    uint64_t num_tmem_pages_needed = (length + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (uint64_t i = 0; num_tmem_pages_needed > 0; i++) {
        // printf("recycling pages\n");
        struct tmem_page *page = pop_free_page();
        if (page == NULL) break;

        page->va_start = p + (i * PAGE_SIZE);
        if (length - (i * PAGE_SIZE) < PAGE_SIZE) {
            page->va = (uint64_t)(page->va_start);
            page->size = length - (i * PAGE_SIZE);
            if (page->size < BASE_PAGE_SIZE) page->size = BASE_PAGE_SIZE;   // Always at least 4KB
        } else {
            page->size = PAGE_SIZE;
            // Align va to PAGE_SIZE address for future lookups in hashmap
            page->va = PAGE_ROUND_UP((uint64_t)(page->va_start));
        }
        page->mig_up = 0;
        page->mig_down = 0;
        page->accesses = 0;
        page->migrating = false;
        page->local_clock = 0;
        page->hot = false;
        page->in_dram = (node == DRAM_NODE) ? IN_DRAM : IN_REM;
        // pthread_mutex_init(&page->page_lock, NULL);
        atomic_store_explicit(&page->next, NULL, memory_order_relaxed);
        page->list = &cold_list;
        
        // LOG_DEBUG("adding recycled page: 0x%lx\n", (uint64_t)page);
        add_page(page);
        num_tmem_pages_needed--;
    }

    if (num_tmem_pages_needed == 0) {
        internal_call = false; 
        return p;
    }

    uint64_t pages_mmap_size = num_tmem_pages_needed * sizeof(struct tmem_page);
    void *pages_ptr = libc_mmap(NULL, pages_mmap_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    assert(pages_ptr != MAP_FAILED);
    pebs_stats.internal_mem_overhead += pages_mmap_size;
    
    for (uint64_t i = 0; num_tmem_pages_needed > 0; i++) {
        // struct tmem_page* page = create_tmem_page(page_boundry, pages_ptr);
        struct tmem_page *page = (struct tmem_page *)(pages_ptr + (i * sizeof(struct tmem_page)));

        page->va_start = p + (i * PAGE_SIZE);
        if (length - (i * PAGE_SIZE) < PAGE_SIZE) {
            page->va = (uint64_t)(page->va_start);
            page->size = length - (i * PAGE_SIZE);
            if (page->size < BASE_PAGE_SIZE) page->size = BASE_PAGE_SIZE;   // Always at least 4KB
        } else {
            page->size = PAGE_SIZE;
            // Align va to PAGE_SIZE address for future lookups in hashmap
            page->va = PAGE_ROUND_UP((uint64_t)(page->va_start));
        }
        page->mig_up = 0;
        page->mig_down = 0;
        page->accesses = 0;
        page->migrating = false;
        page->local_clock = 0;
        page->hot = false;
        page->in_dram = (node == DRAM_NODE) ? IN_DRAM : IN_REM;
        pthread_mutex_init(&page->page_lock, NULL);
        page->list = &cold_list;
        atomic_store_explicit(&page->next, NULL, memory_order_relaxed);
        
        // LOG_DEBUG("adding page: 0x%lx\n", (uint64_t)page);
        add_page(page);
        num_tmem_pages_needed--;
    }
    internal_call = false;
    return p;
}

int tmem_munmap(void *addr, size_t length) {
    internal_call = true;
    LOG_DEBUG("tmem_munmap: %p, length: %lu\n", addr, length);

    uint64_t num_tmem_pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint64_t i = 0; i < num_tmem_pages; i++) {
        void *va_start = addr + (i * PAGE_SIZE);
        uint64_t va;
        if (length - (i * PAGE_SIZE) < PAGE_SIZE) {
            va = (uint64_t)(va_start);
        } else {
            va = PAGE_ROUND_UP((uint64_t)(va_start));
        }
        struct tmem_page *page = find_page(va);
        if (page != NULL) {
            // LOG_DEBUG("removing page: 0x%lx\n", (uint64_t)page);
            // printf("hash count: %u\n", HASH_COUNT(pages));
            remove_page(page);
            if (page->in_dram == DRAM_NODE) {
                dram_used -= page->size;
            }
            pebs_stats.mem_allocated -= page->size;

            // Doesn't actually unmap page but puts in free list to use for future allocations
            // (better for apps that mmap and munmap frequently like resnet_train.py)
            push_free_page(page);
        }
    }
    internal_call = false;
    return 0;
}

void tmem_cleanup() {
    kill_threads();
    // TODO: unmap pages (very difficult since libc_munmap works on 4KB and will unmap multiple pages at a time if in same region)
    wait_for_threads();
}