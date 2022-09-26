#ifndef __XMHF_BASEPLATFORM_CPU_H__
#define __XMHF_BASEPLATFORM_CPU_H__

#include "xmhf-types.h"

// This macro is used by microsec CPU delay. The delayed time is imprecise.
// [TODO] We assume the CPU frequency is 3.5GHz.
#define CPU_CYCLES_PER_MICRO_SEC        3500UL
#define SEC_TO_CYCLES(x)                (1000UL * 1000UL) * CPU_CYCLES_PER_MICRO_SEC * x


#ifndef __ASSEMBLY__

#define mb()	asm volatile("mfence" ::: "memory")
#define __force	__attribute__((force))

//! @brief Save energy when waiting in a busy loop
static inline void xmhf_cpu_relax(void) 
{
	asm volatile ("pause");
}

// Flushing functions
extern void xmhf_cpu_flush_cache_range(void *vaddr, unsigned int size);

//! @brief Sleep the current core for <us> micro-second.
extern void xmhf_cpu_delay_us(uint64_t us);

#endif	//__ASSEMBLY__



#endif //__XMHF_BASEPLATFORM_H__
