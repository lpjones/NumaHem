## Description
NumaHem is a variant of HeMem which does tiered memory management.

[HeMem: Scalable Tiered Memory Management for Big Data Applications and Real NVM](https://dl.acm.org/doi/pdf/10.1145/3477132.3483550) 

[HeMem code](https://bitbucket.org/ajaustin/hemem/src/vanilla-multiprocess/)

There are emerging memory technologies such as CXL that can expand the available RAM but have higher latency than local DRAM. Because of the higher cost of accessing CXL memory there can be performance benefits to migrating pages between DRAM and CXL to ensure more memory accesses come from local DRAM instead of CXL. This invites the idea of 'hot' and 'cold' pages in memory. Where 'hot' pages are accessed more frequently and 'cold' pages are rarely accessed. The idea of HeMem is to identify these hot pages based on access frequency by sampling memory accesses and place them in local DRAM.

NumaHem takes a slightly different approach to identifying pages to place in local DRAM. Instead of 'hot' pages, it looks at a history of sampled memory accesses and predicts future accesses and places those pages in local DRAM.

## Implementation Overview
Overall the implementation is very similar to HeMem with the major difference being instead of using devdax files and manual page migration, NumaHem uses the numa library to manipulate page placement.

The necessary parts required to do page placement are
1. Tracking where pages are placed
2. Gathering memory access information to inform page placement decisions
3. A mechanism to move pages between memory tiers

The way these parts are implemented in NumaHem are
1. Creating mmap/munmap hooks to intercept memory allocations and record page metadata such as where the page is placed
2. A sampling thread using PEBS which samples memory accesses and runs the prediction algorithm to determine where to place pages.
3. A migration thread that reads in the pages determined 'hot' by the sampling thread and using mmap + mbind which has the OS deal with the complications of moving pages.

### Sampling



