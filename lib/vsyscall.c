#include <inc/vsyscall.h>
#include <inc/lib.h>

static inline uint64_t
vsyscall(int num) {
    // LAB 12: Your code here
    if (num >= NVSYSCALLS) return -E_INVAL;
    return __atomic_load_n(&vsys[num], __ATOMIC_ACQUIRE); 
}

int
vsys_gettime(void) {
    return vsyscall(VSYS_gettime);
}
