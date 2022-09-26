// author: Miao Yu [Superymk]
#ifndef XMHF_DMAP_VMX_INTERNAL_H
#define XMHF_DMAP_VMX_INTERNAL_H

#include <xmhf.h>

#define NUM_PT_ENTRIES      512 // The number of page table entries in each level

#define PAE_get_pml4tindex(x)    ((x) >> 39ULL) & (NUM_PT_ENTRIES - 1ULL)
#define PAE_get_pdptindex(x)    ((x) >> 30ULL) & (NUM_PT_ENTRIES - 1ULL)
#define PAE_get_pdtindex(x)     ( (x) >> 21ULL) & (NUM_PT_ENTRIES - 1ULL)
#define PAE_get_ptindex(x)      ( (x) >> 12ULL) & (NUM_PT_ENTRIES - 1ULL)


#ifndef __ASSEMBLY__
extern struct dmap_vmx_cap g_vtd_cap_sagaw_mgaw_nd;

//vt-d register access function
extern void _vtd_reg(VTD_DRHD *dmardevice, u32 access, u32 reg, void *value);

// Return true if verification of VT-d capabilities succeed.
// Success means:
// (0) <out_cap> must be valid
// (1) Same AGAW, MGAW, and ND across VT-d units
// (2) supported MGAW to ensure our host address width is supported (32-bits)
// (3) AGAW must support 39-bits or 48-bits
// (4) Number of domains must not be unsupported
extern bool _vtd_verify_cap(VTD_DRHD* vtd_drhd, u32 vtd_num_drhd, struct dmap_vmx_cap* out_cap);

//initialize a DRHD unit
//note that the VT-d documentation does not describe the precise sequence of
//steps that need to be followed to initialize a DRHD unit!. we use our
//common sense instead...:p
extern void _vtd_drhd_initialize(VTD_DRHD *drhd, u32 vtd_ret_paddr);

// vt-d invalidate cachess note: we do global invalidation currently
// [NOTE] <drhd0> refers to &vtd_drhd[0] and is used for __XMHF_VERIFICATION__ only.
extern void _vtd_invalidate_caches_single_iommu(VTD_DRHD *drhd, VTD_DRHD *drhd0);




/********* Debug functions *********/
extern void _vtd_print_and_clear_fault_registers(VTD_DRHD *drhd);
extern void _vtd_restart_dma_iommu(VTD_DRHD *drhd);

#endif // __ASSEMBLY__
#endif // XMHF_DMAP_VMX_INTERNAL_H