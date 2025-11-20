Current best parameters: resnet-stats1-clust1-hem0-his8-pred16-down0.0001-up0.01-neigh8 -> 4.2 im/sec resnet


Lists:
There are 3 lists in tmem:
Hot list: pages added to the hot list try to be migrated from remote memory to dram. So at the time of adding pages to the hot list they should be in remote memory and not already be in the hot list.

Cold list: pages added to the cold list are used to free up space in dram when a hot page is identified. So at the time of adding pages to the cold list they should be in dram and not already be in the cold list.

Free list: pages added to the free list have been munmapped by the application. Instead of munmapping these pages they are kept alive and are recycled for future mmap calls. This reduces the amount of mmap and munmap syscalls for applications that do many mmap and munmap calls such as python resnet_train.py.

In the tmem_page struct there is a 'list' field which indicates which list the page is currently in. This field can and often times is 'NULL' in these instances:
The page is in remote memory and is cold. (This is the most common case)
The page has been dequeued by the migrate thread and is being processed. This is more complicated and these are the following ways it is dealt with:
    In the case of the hot page being dequeued there are two possible race conditions.
        1. the pebs_scan_thread makes a hot or cold request on that page. This is dealt with by checking if the page list is NULL since the only time it would be NULL and be in DRAM is if this exact situation is occuring. For a hot request it returns early since it is already being migrated. For a cold request it is added to the cold list. The migration thread handles this when it locks the page and checks if the page is still hot. If it isn't hot anymore the migrate thread aborts the migration attempt.
        2. an munmap happens during a migration attempt for that page. The munmap thread locks the page and marks it as freed and adds it to the free list doing no checks. The migration thread can continue until it locks the page and checks whether it has been freed. If it has it aborts the migration attempt.
    In the case of the cold page being dequeued it is identical to the hot page except when the pebs_scan_thread makes a hot request the page is added to the hot list and marked hot. The migration thread checks whether the gathered cold pages are still cold. If they aren't it finds new cold pages from the cold list continually until it has enough valid cold pages to swap the cold pages with the hot page.


