#include "tmem.h"

struct tmem_page *pages = NULL;
struct fifo_list hot_list;
struct fifo_list cold_list;
struct fifo_list free_list;
pthread_mutex_t pages_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mmap_lock = PTHREAD_MUTEX_INITIALIZER;

long dram_free = 0;
long dram_size = 0;
long dram_used = 0;


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
#ifdef DRAM_BUFFER
#ifdef DRAM_SIZE
    fprintf(stderr, "Can't have both DRAM_BUFFER and DRAM_SIZE\n");
    exit(1);
#endif
#endif
#ifndef DRAM_BUFFER
#ifndef DRAM_SIZE
    fprintf(stderr, "Need either DRAM_BUFFER or DRAM_SIZE\n");
    exit(1);
#endif
#endif

    // LOG_DEBUG("DRAM size: %lu, REMOTE size: %lu\n", DRAM_SIZE, REMOTE_SIZE);

    LOG_DEBUG("finished tmem_init\n");

    struct tmem_page *dummy_page = calloc(1, sizeof(struct tmem_page));
    add_page(dummy_page);

    // check how much free space on dram
#ifdef DRAM_BUFFER
    dram_size = numa_node_size(DRAM_NODE, &dram_free);
    dram_size -= dram_free;
#endif
#ifdef DRAM_SIZE
    dram_size = DRAM_SIZE;
#endif
    internal_call = false;
}

#define PAGE_ROUND_UP(x) (((x) + (PAGE_SIZE)-1) & (~((PAGE_SIZE)-1)))
#define PAGE_ROUND_DOWN(x) ((x) & (~((PAGE_SIZE)-1)))


void* tmem_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    internal_call = true;

    unsigned long dram_nodemask = 1UL << DRAM_NODE;
    unsigned long rem_nodemask = 1UL << REM_NODE;
    void *p_dram = NULL, *p_rem = NULL;

    void *p = libc_mmap(addr, length, prot, flags, fd, offset);
    assert(p != MAP_FAILED);

    pthread_mutex_lock(&mmap_lock);
    if (dram_used + length <= dram_size) {
        // can allocate all on dram
        dram_used += length;
        pthread_mutex_unlock(&mmap_lock);
        LOG_DEBUG("MMAP: All DRAM\n");


        if (mbind(p, length, MPOL_BIND, &dram_nodemask, 64, MPOL_MF_MOVE)) {
            perror("mbind");
            assert(0);
        }
        
        p_dram = p;
        p_rem = p_dram + length + 1;    // Used later to check which node page is in
    } else if (dram_used + PAGE_SIZE > dram_size) {
        pthread_mutex_unlock(&mmap_lock);
        LOG_DEBUG("MMAP: All Remote\n");
        // dram full, all on remote
        if (mbind(p, length, MPOL_BIND, &rem_nodemask, 64, MPOL_MF_MOVE)) {
            perror("mbind");
            assert(0);
        }
        p_rem = p;
    } else {
        // split between dram and remote
        uint64_t dram_mmap_size = PAGE_ROUND_DOWN(dram_size - dram_used);
        dram_used += dram_mmap_size;
        pthread_mutex_unlock(&mmap_lock);
        
        uint64_t rem_mmap_size = length - dram_mmap_size;


        LOG_DEBUG("MMAP: dram: %lu, remote: %lu\n", dram_mmap_size, rem_mmap_size);
        p_dram = p;
        p_rem = p_dram + dram_mmap_size;
        if (mbind(p_dram, dram_mmap_size, MPOL_BIND, &dram_nodemask, 64, MPOL_MF_MOVE) == -1) {
            perror("mbind");
            assert(0);
        }
        if (mbind(p_rem, rem_mmap_size, MPOL_BIND, &rem_nodemask, 64, MPOL_MF_MOVE) == -1) {
            perror("mbind");
            assert(0);
        }
        
    }
    


    // LOG_DEBUG("dram_size: %ld, dram_free: %ld\n", dram_size, dram_free);
    if (p == MAP_FAILED) {
        LOG_DEBUG("mmap failed\n");
        return MAP_FAILED;
    }
    pebs_stats.mem_allocated += length;

    assert((uint64_t)p % BASE_PAGE_SIZE == 0);

    // recycle pages from free_tmem_pages
    uint64_t num_tmem_pages_needed = (length + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t i;
    for (i = 0; free_list.numentries > 0 && num_tmem_pages_needed > 0; i++) {
        // printf("recycling pages\n");
        struct tmem_page *page = dequeue_fifo(&free_list);
        if (page == NULL) break;

        // use lock to cause atomic update of page
        pthread_mutex_lock(&page->page_lock);
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

        page->prev = NULL;
        page->next = NULL;


        page->in_dram = (page->va_start >= p_rem) ? IN_REM : IN_DRAM;
        page->hot = false;
        page->free = false;
        page->migrating = false;
        page->list = NULL;
        if (page->in_dram == IN_DRAM) {
            enqueue_fifo(&cold_list, page);
        }

        pthread_mutex_unlock(&page->page_lock);

        // pthread_mutex_init(&page->page_lock, NULL);

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
    
    for (uint64_t j = 0; num_tmem_pages_needed > 0; j++) {
        // struct tmem_page* page = create_tmem_page(page_boundry, pages_ptr);
        struct tmem_page *page = (struct tmem_page *)(pages_ptr + (j * sizeof(struct tmem_page)));

        // Don't need lock since first creation of page so no threads have cached data on it
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
        page->local_clock = 0;

        page->prev = NULL;
        page->next = NULL;

        page->in_dram = (page->va_start >= p_rem) ? IN_REM : IN_DRAM;
        page->hot = false;
        page->free = false;
        page->migrating = false;

        pthread_mutex_init(&page->page_lock, NULL);
        page->list = NULL;
        if (page->in_dram == IN_DRAM) {
            enqueue_fifo(&cold_list, page);
        }

        
        // LOG_DEBUG("adding page: 0x%lx\n", (uint64_t)page);
        add_page(page);
        num_tmem_pages_needed--;
        i++;
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
            pthread_mutex_lock(&page->page_lock);
            page->free = true;
            remove_page(page);
            if (page->in_dram == DRAM_NODE) {
                dram_used -= page->size;
            }
            pebs_stats.mem_allocated -= page->size;

            if (page->list != NULL) {
                page_list_remove_page(page->list, page);
            }
            enqueue_fifo(&free_list, page);

            pthread_mutex_unlock(&page->page_lock);
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