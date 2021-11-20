#include "vm/struct.h"
#include "threads/palloc.h"

void vm_free_frame(void* address,uint32_t* pagedir);

void* vm_get_frame(void* frame, uint32_t* pagedir, enum palloc_flags flags);

struct frame_struct* address_to_frame(void*address);

void evict(void);

bool eviction_clock(struct frame_struct*vf);


