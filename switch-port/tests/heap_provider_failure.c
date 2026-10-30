#include "ds_heap.h"
bool __real_ds_image_add_zero_region(DsImage *,uint32_t,size_t,uint32_t,char *,size_t);
bool __wrap_ds_image_add_zero_region(DsImage *image,uint32_t guest,size_t size,uint32_t flags,char *error,size_t capacity)
{
    if(guest==DS_HEAP_BASE)return false;
    return __real_ds_image_add_zero_region(image,guest,size,flags,error,capacity);
}
