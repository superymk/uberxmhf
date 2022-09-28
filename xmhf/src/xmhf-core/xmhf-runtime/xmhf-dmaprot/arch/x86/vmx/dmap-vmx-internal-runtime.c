#include <xmhf.h>
#include "dmap-vmx-internal.h"

//! @brief Modify an individual bit of Global Command Register.
extern void _vtd_drhd_issue_gcmd(VTD_DRHD *drhd, u32 offset, u32 val);

// Issue Write Buffer Flusing (WBF) if the IOMMU requires it.
extern void _vtd_drhd_issue_wbf(VTD_DRHD *drhd);

// vt-d invalidate cachess note: we do global invalidation currently
// [NOTE] <drhd0> refers to &vtd_drhd[0] and is used for __XMHF_VERIFICATION__ only.
void _vtd_invalidate_caches_single_iommu(VTD_DRHD *drhd, VTD_DRHD *drhd0)
{
    VTD_CCMD_REG ccmd;
    VTD_IOTLB_REG iotlb;

    // sanity check
    HALT_ON_ERRORCOND(drhd != NULL);
    HALT_ON_ERRORCOND(drhd0 != NULL);

    // 0. If IOMMU needs mHV to issue WBF, then mHV needs to do so before invalidate caches.
    if (vtd_cap_require_wbf(drhd))
        _vtd_drhd_issue_wbf(drhd);

        // 1. invalidate CET cache

#ifndef __XMHF_VERIFICATION__
    // wait for context cache invalidation request to send
    IOMMU_WAIT_OP(drhd, VTD_CCMD_REG_OFF, !ccmd.bits.icc, (void *)&ccmd.value, "IOMMU is not ready to invalidate CET cache");
#else
    _vtd_reg(drhd0, VTD_REG_READ, VTD_CCMD_REG_OFF, (void *)&ccmd.value);
#endif

    // initialize CCMD to perform a global invalidation
    ccmd.value = 0;
    ccmd.bits.cirg = 1; // global invalidation
    ccmd.bits.icc = 1;  // invalidate context cache

    // perform the invalidation
    _vtd_reg(drhd, VTD_REG_WRITE, VTD_CCMD_REG_OFF, (void *)&ccmd.value);

#ifndef __XMHF_VERIFICATION__
    // wait for context cache invalidation completion status
    IOMMU_WAIT_OP(drhd, VTD_CCMD_REG_OFF, !ccmd.bits.icc, (void *)&ccmd.value, "Failed to invalidate CET cache");
#else
    _vtd_reg(drhd0, VTD_REG_READ, VTD_CCMD_REG_OFF, (void *)&ccmd.value);
#endif

    // if all went well CCMD CAIG = CCMD CIRG (i.e., actual = requested invalidation granularity)
    if (ccmd.bits.caig != 0x1)
    {
        printf("	Invalidatation of CET failed. Halting! (%u)\n", ccmd.bits.caig);
        HALT();
    }

    // 2. invalidate IOTLB
    // wait for the IOTLB invalidation is available
    IOMMU_WAIT_OP(drhd, VTD_IOTLB_REG_OFF, !iotlb.bits.ivt, (void *)&iotlb.value, "IOMMU is not ready to invalidate IOTLB");

    // initialize IOTLB to perform a global invalidation
    iotlb.value = 0;
    iotlb.bits.iirg = 1; // global invalidation
    iotlb.bits.ivt = 1;  // invalidate

    // perform the invalidation
    _vtd_reg(drhd, VTD_REG_WRITE, VTD_IOTLB_REG_OFF, (void *)&iotlb.value);

#ifndef __XMHF_VERIFICATION__
    // wait for the invalidation to complete
    IOMMU_WAIT_OP(drhd, VTD_IOTLB_REG_OFF, !iotlb.bits.ivt, (void *)&iotlb.value, "Failed to invalidate IOTLB");
#else
    _vtd_reg(drhd0, VTD_REG_READ, VTD_IOTLB_REG_OFF, (void *)&iotlb.value);
#endif

    // if all went well IOTLB IAIG = IOTLB IIRG (i.e., actual = requested invalidation granularity)
    if (iotlb.bits.iaig != 0x1)
    {
        printf("	Invalidation of IOTLB failed. Halting! (%u)\n", iotlb.bits.iaig);
        HALT();
    }
}




/********* Other util functions *********/
void _vtd_print_and_clear_fault_registers(VTD_DRHD *drhd)
{
    uint32_t i = 0;
    uint64_t frr_low64, frr_high64;
    uint32_t fsr_clear_pfo_val = VTD_FSTS_PFO | VTD_FSTS_PPF | VTD_FSTS_PRO;
    VTD_FSTS_REG fsr;

    for (i = 0; i < vtd_cap_frr_nums(drhd); i++)
    {
        hva_t frr_vaddr = drhd->regbaseaddr + vtd_cap_frr_mem_offset(drhd) + i * VTD_CAP_REG_FRO_MULTIPLIER;

        frr_low64 = *(uint64_t *)(frr_vaddr);
        frr_high64 = *(uint64_t *)(frr_vaddr + 8);

        printf("    DRHD Fault Recording Register[%u]: frr_vaddr:0x%lX, low64:0x%llX, high64:0x%llX\n", i, frr_vaddr, frr_low64, frr_high64);

        // Clear the Fault Recording Registers
        *(uint64_t *)(frr_vaddr + 8) = 0x8000000000000000ULL;
        *(uint64_t *)(frr_vaddr) = 0ULL;

        // Clear the PFO, PPF, and PRO bit of Fault Status Register
        _vtd_reg(drhd, VTD_REG_WRITE, VTD_FSTS_REG_OFF, (void *)&fsr_clear_pfo_val);
        IOMMU_WAIT_OP(drhd, VTD_FSTS_REG_OFF, !fsr.bits.pfo && !fsr.bits.ppf && !fsr.bits.pro, (void *)&fsr.value,
                      "Failed to clear PFO, PPF, and PRO in Fault Status Register!");
    }
}

void _vtd_restart_dma_iommu(VTD_DRHD *drhd)
{
    _vtd_disable_dma_iommu(drhd);
    _vtd_enable_dma_iommu(drhd);
}

void _vtd_disable_dma_iommu(VTD_DRHD *drhd)
{
    VTD_GSTS_REG gsts;

    // disable translation
    _vtd_drhd_issue_gcmd(drhd, VTD_GCMD_BIT_TE, 0);
    // wait for translation enabled status to go red...
    IOMMU_WAIT_OP(drhd, VTD_GSTS_REG_OFF, !gsts.bits.tes, (void *)&gsts.value, "	DMA translation cannot be disabled. Halting!");
}

void _vtd_enable_dma_iommu(VTD_DRHD *drhd)
{
    VTD_GSTS_REG gsts;

    // enable translation
    _vtd_drhd_issue_gcmd(drhd, VTD_GCMD_BIT_TE, 1); // te
    // wait for translation enabled status to go green...
    IOMMU_WAIT_OP(drhd, VTD_GSTS_REG_OFF, gsts.bits.tes, (void *)&gsts.value, "	    DMA translation cannot be enabled. Halting!");
}