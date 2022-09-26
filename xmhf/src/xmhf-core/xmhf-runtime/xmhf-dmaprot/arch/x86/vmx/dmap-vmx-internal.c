/*
 * @XMHF_LICENSE_HEADER_START@
 *
 * eXtensible, Modular Hypervisor Framework (XMHF)
 * Copyright (c) 2009-2012 Carnegie Mellon University
 * Copyright (c) 2010-2012 VDG Inc.
 * All Rights Reserved.
 *
 * Developed by: XMHF Team
 *               Carnegie Mellon University / CyLab
 *               VDG Inc.
 *               http://xmhf.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 *
 * Neither the names of Carnegie Mellon or VDG Inc, nor the names of
 * its contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @XMHF_LICENSE_HEADER_END@
 */

// EMHF DMA protection component implementation for x86 VMX
// author: amit vasudevan (amitvasudevan@acm.org)

#include <xmhf.h>
#include "dmap-vmx-internal.h"

struct dmap_vmx_cap g_vtd_cap_sagaw_mgaw_nd;

//------------------------------------------------------------------------------
#define DMAR_OPERATION_TIMEOUT  SEC_TO_CYCLES(1)

// vt-d register access function
void _vtd_reg(VTD_DRHD *dmardevice, u32 access, u32 reg, void *value)
{
    u32 regtype = VTD_REG_32BITS, regaddr = 0;

    // obtain register type and base address
    switch (reg)
    {
    // 32-bit registers
    case VTD_VER_REG_OFF:
    case VTD_GCMD_REG_OFF:
    case VTD_GSTS_REG_OFF:
    case VTD_FSTS_REG_OFF:
    case VTD_FECTL_REG_OFF:
    case VTD_PMEN_REG_OFF:
        regtype = VTD_REG_32BITS;
        regaddr = dmardevice->regbaseaddr + reg;
        break;

    // 64-bit registers
    case VTD_CAP_REG_OFF:
    case VTD_ECAP_REG_OFF:
    case VTD_RTADDR_REG_OFF:
    case VTD_CCMD_REG_OFF:
        regtype = VTD_REG_64BITS;
        regaddr = dmardevice->regbaseaddr + reg;
        break;

    case VTD_IOTLB_REG_OFF:
    {
        VTD_ECAP_REG t_vtd_ecap_reg;
        regtype = VTD_REG_64BITS;
#ifndef __XMHF_VERIFICATION__
        _vtd_reg(dmardevice, VTD_REG_READ, VTD_ECAP_REG_OFF, (void *)&t_vtd_ecap_reg.value);
#endif
        regaddr = dmardevice->regbaseaddr + (t_vtd_ecap_reg.bits.iro * 16) + 0x8;
        break;
    }

    case VTD_IVA_REG_OFF:
    {
        VTD_ECAP_REG t_vtd_ecap_reg;
        regtype = VTD_REG_64BITS;
#ifndef __XMHF_VERIFICATION__
        _vtd_reg(dmardevice, VTD_REG_READ, VTD_ECAP_REG_OFF, (void *)&t_vtd_ecap_reg.value);
#endif
        regaddr = dmardevice->regbaseaddr + (t_vtd_ecap_reg.bits.iro * 16);
        break;
    }

    default:
        printf("%s: Halt, Unsupported register=%08x\n", __FUNCTION__, reg);
        HALT();
        break;
    }

    // perform the actual read or write request
    switch (regtype)
    {
    case VTD_REG_32BITS:
    { // 32-bit r/w
        if (access == VTD_REG_READ)
            *((u32 *)value) = xmhf_baseplatform_arch_flat_readu32(regaddr);
        else
            xmhf_baseplatform_arch_flat_writeu32(regaddr, *((u32 *)value));

        break;
    }

    case VTD_REG_64BITS:
    { // 64-bit r/w
        if (access == VTD_REG_READ)
            *((u64 *)value) = xmhf_baseplatform_arch_flat_readu64(regaddr);
        else
            xmhf_baseplatform_arch_flat_writeu64(regaddr, *((u64 *)value));

        break;
    }

    default:
        printf("%s: Halt, Unsupported access width=%08x\n", __FUNCTION__, regtype);
        HALT();
    }

    return;
}

// Return true if verification of VT-d capabilities succeed.
// Success means:
// (0) <out_cap> must be valid
// (1) Same AGAW, MGAW, and ND across VT-d units
// (2) supported MGAW to ensure our host address width is supported (32-bits)
// (3) AGAW must support 39-bits or 48-bits
// (4) Number of domains must not be unsupported
bool _vtd_verify_cap(VTD_DRHD *vtd_drhd, u32 vtd_num_drhd, struct dmap_vmx_cap *out_cap)
{
#define INVALID_SAGAW_VAL 0xFFFFFFFF
#define INVALID_MGAW_VAL 0xFFFFFFFF
#define INVALID_NUM_DOMAINS 0xFFFFFFFF

    VTD_CAP_REG cap;
    u32 i = 0;
    u32 last_sagaw = INVALID_SAGAW_VAL;
    u32 last_mgaw = INVALID_MGAW_VAL;
    u32 last_nd = INVALID_NUM_DOMAINS;

    // Sanity checks
    if (!out_cap)
        return false;

    if (!vtd_drhd)
        return false;

    if (!vtd_num_drhd || vtd_num_drhd >= VTD_MAX_DRHD) // Support maximum of VTD_MAX_DRHD VT-d units
        return false;

    for (i = 0; i < vtd_num_drhd; i++)
    {
        VTD_DRHD *drhd = &vtd_drhd[i];
        printf("%s: verifying DRHD unit %u...\n", __FUNCTION__, i);

        // read CAP register
        _vtd_reg(drhd, VTD_REG_READ, VTD_CAP_REG_OFF, (void *)&cap.value);

        // Check: Same AGAW, MGAW and ND across VT-d units
        if (cap.bits.sagaw != last_sagaw)
        {
            if (last_sagaw == INVALID_SAGAW_VAL)
            {
                // This must the first VT-d unit
                last_sagaw = cap.bits.sagaw;
            }
            else
            {
                // The current VT-d unit has different capabilities with some other units
                printf("  [VT-d] Check error! Different SAGAW capability found on VT-d unix %u. last sagaw:0x%08X, current sagaw:0x%08X\n",
                       i, last_sagaw, cap.bits.sagaw);
                return false;
            }
        }

        if (cap.bits.mgaw != last_mgaw)
        {
            if (last_mgaw == INVALID_MGAW_VAL)
            {
                // This must the first VT-d unit
                last_mgaw = cap.bits.mgaw;
            }
            else
            {
                // The current VT-d unit has different capabilities with some other units
                printf("  [VT-d] Check error! Different MGAW capability found on VT-d unix %u. last mgaw:0x%08X, current mgaw:0x%08X\n",
                       i, last_mgaw, cap.bits.mgaw);
                return false;
            }
        }

        if (cap.bits.nd != last_nd)
        {
            if (last_nd == INVALID_NUM_DOMAINS)
            {
                // This must the first VT-d unit
                last_nd = cap.bits.nd;
            }
            else
            {
                // The current VT-d unit has different capabilities with some other units
                printf("  [VT-d] Check error! Different ND capability found on VT-d unix %u. last nd:0x%08X, current nd:0x%08X\n",
                       i, last_nd, cap.bits.nd);
                return false;
            }
        }

        // Check: supported MGAW to ensure our host address width is supported (32-bits)
        if (cap.bits.mgaw < 31)
        {
            printf("  [VT-d] Check error! GAW < 31 (%u) unsupported.\n", cap.bits.mgaw);
            return false;
        }

        // Check: AGAW must support 39-bits or 48-bits
        if (!(cap.bits.sagaw & 0x2 || cap.bits.sagaw & 0x4))
        {
            printf("	[VT-d] Check error! AGAW does not support 3-level or 4-level page-table. See sagaw capabilities:0x%08X. Halting!\n", cap.bits.sagaw);
            return false;
        }
        else
        {
            out_cap->sagaw = cap.bits.sagaw;
        }

        // Check: Number of domains must not be unsupported
        if (cap.bits.nd == 0x7)
        {
            printf("  [VT-d] Check error! ND == 0x7 unsupported on VT-d unix %u.\n", i);
            return false;
        }
        else
        {
            out_cap->nd = cap.bits.nd;
        }
    }

    printf("Verify all Vt-d units success\n");

    return true;
}

//------------------------------------------------------------------------------
#define IOMMU_WAIT_OP(drhd, reg, cond, sts, msg_for_false_cond)                 \
    do                                                                          \
    {                                                                           \
        uint64_t start_time = rdtsc64();                                        \
        while (1)                                                               \
        {                                                                       \
            _vtd_reg(drhd, VTD_REG_READ, reg, sts);                             \
            if (cond)                                                           \
                break;                                                          \
            if (rdtsc64() > start_time + DMAR_OPERATION_TIMEOUT)                \
            {                                                                   \
                printf("DMAR hardware malfunction:%s\n", (msg_for_false_cond)); \
                HALT();                                                         \
            }                                                                   \
            xmhf_cpu_relax();                                                   \
        }                                                                       \
    } while (0)

// According to Intel Virtualization Technology for Directed I/O specification, Section 11.4.4.1 Global Command Register.
#define VTD_GSTS_MASK_FOR_GCMD  (0x96FFFFFF)

//! @brief Modify an individual bit of Global Command Register.
static void _vtd_drhd_issue_gcmd(VTD_DRHD *drhd, u32 offset, u32 val)
{
    VTD_GCMD_REG gcmd;
    VTD_GSTS_REG gsts;

    // Check: <offset> must be in [0, 31]
    if(offset >= 32)
        return;

    // According to Intel Virtualization Technology for Directed I/O specification, Section 11.4.4.1 Global Command Register.
    _vtd_reg(drhd, VTD_REG_READ, VTD_GSTS_REG_OFF, (void *)&gsts.value);
    gsts.value &= VTD_GSTS_MASK_FOR_GCMD;

    if(val)
        gcmd.value = gsts.value | (1UL << offset);
    else
        gcmd.value = gsts.value & ~(1UL << offset);
    
    _vtd_reg(drhd, VTD_REG_WRITE, VTD_GCMD_REG_OFF, (void *)&gcmd.value);
}

// Issue Write Buffer Flusing (WBF) if the IOMMU requires it.
static void _vtd_drhd_issue_wbf(VTD_DRHD *drhd)
{
    VTD_GSTS_REG gsts;

    // sanity check
    HALT_ON_ERRORCOND(drhd != NULL);

    if (!vtd_cap_require_wbf(drhd))
        // Not need to issue Write Buffer Flusing (WBF)
        return;

    _vtd_drhd_issue_gcmd(drhd, VTD_GCMD_BIT_WBF, 1);
    IOMMU_WAIT_OP(drhd, VTD_GSTS_REG_OFF, !gsts.bits.wbfs, (void *)&gsts.value, "	Cannot perform WBF. Halting!");
}

// initialize a DRHD unit
// note that the VT-d documentation does not describe the precise sequence of
// steps that need to be followed to initialize a DRHD unit!. we use our
// common sense instead...:p
void _vtd_drhd_initialize(VTD_DRHD *drhd, u32 vtd_ret_paddr)
{
    VTD_GCMD_REG gcmd;
    VTD_GSTS_REG gsts;
    VTD_CCMD_REG ccmd;
    VTD_IOTLB_REG iotlb;

    // sanity check
    HALT_ON_ERRORCOND(drhd != NULL);

    // Clear <iommu_flags>
    memset(&drhd->iommu_flags, 0, sizeof(VTD_IOMMU_FLAGS));

    // Step 0. Read <cap> and <ecap>
    {
        VTD_CAP_REG cap;
        VTD_ECAP_REG ecap;

        // read ECAP register
        _vtd_reg(drhd, VTD_REG_READ, VTD_CAP_REG_OFF, (void *)&cap.value);
        drhd->iommu_flags.cap = cap;

        // read ECAP register
        _vtd_reg(drhd, VTD_REG_READ, VTD_ECAP_REG_OFF, (void *)&ecap.value);
        drhd->iommu_flags.ecap = ecap;
    }

    // check VT-d snoop control capabilities
    {
        if (vtd_ecap_sc(drhd))
            printf("	VT-d hardware Snoop Control (SC) capabilities present\n");
        else
            printf("	VT-d hardware Snoop Control (SC) unavailable\n");

        if (vtd_ecap_c(drhd))
        {
            printf("	VT-d hardware access to remapping structures COHERENT\n");
        }
        else
        {
            printf("	VT-d hardware access to remapping structures NON-COHERENT\n");
        }
    }

    // Init fault-recording registers
    {
        printf("	VT-d numbers of fault recording registers:%u\n", vtd_cap_frr_nums(drhd));
    }

    // 3. setup fault logging
    printf("	Setting Fault-reporting to NON-INTERRUPT mode...");
    {
        VTD_FECTL_REG fectl;

        // read FECTL
        fectl.value = 0;
        _vtd_reg(drhd, VTD_REG_READ, VTD_FECTL_REG_OFF, (void *)&fectl.value);

        // set interrupt mask bit and write
        fectl.bits.im = 1;
        _vtd_reg(drhd, VTD_REG_WRITE, VTD_FECTL_REG_OFF, (void *)&fectl.value);

        // check to see if the im bit actually stuck
        _vtd_reg(drhd, VTD_REG_READ, VTD_FECTL_REG_OFF, (void *)&fectl.value);

        if (!fectl.bits.im)
        {
            printf("	Failed to set fault-reporting. Halting!\n");
            HALT();
        }
    }
    printf("Done.\n");

    // 4. setup RET (root-entry)
    printf("	Setting up RET...");
    {
        VTD_RTADDR_REG rtaddr, rtaddr_readout;

        // setup RTADDR with base of RET
        rtaddr.value = (u64)vtd_ret_paddr | VTD_RTADDR_LEGACY_MODE;
        _vtd_reg(drhd, VTD_REG_WRITE, VTD_RTADDR_REG_OFF, (void *)&rtaddr.value);

        // read RTADDR and verify the base address
        rtaddr_readout.value = 0;
        IOMMU_WAIT_OP(drhd, VTD_RTADDR_REG_OFF, (rtaddr_readout.value == rtaddr.value), (void *)&rtaddr_readout.value, "	Failed to set RTADDR. Halting!");

        // latch RET address by using GCMD.SRTP
        _vtd_drhd_issue_gcmd(drhd, VTD_GCMD_BIT_SRTP, 1);

        // ensure the RET address was latched by the h/w
        IOMMU_WAIT_OP(drhd, VTD_GSTS_REG_OFF, gsts.bits.rtps, (void *)&gsts.value, "	Failed to latch RTADDR. Halting!");
    }
    printf("Done.\n");

    // If IOMMU needs mHV to issue WBF, then mHV needs to do so before invalidate caches.
    if (vtd_cap_require_wbf(drhd))
        _vtd_drhd_issue_wbf(drhd);

    // 5. invalidate CET cache
    printf("	Invalidating CET cache...");
    {
        // wait for context cache invalidation request to send
#ifndef __XMHF_VERIFICATION__
        IOMMU_WAIT_OP(drhd, VTD_CCMD_REG_OFF, !ccmd.bits.icc, (void *)&ccmd.value, "IOMMU is not ready to invalidate CET cache");
#endif

        // initialize CCMD to perform a global invalidation
        ccmd.value = 0;
        ccmd.bits.cirg = 1; // global invalidation
        ccmd.bits.icc = 1;  // invalidate context cache

        // perform the invalidation
        _vtd_reg(drhd, VTD_REG_WRITE, VTD_CCMD_REG_OFF, (void *)&ccmd.value);

        // wait for context cache invalidation completion status
#ifndef __XMHF_VERIFICATION__
        IOMMU_WAIT_OP(drhd, VTD_CCMD_REG_OFF, !ccmd.bits.icc, (void *)&ccmd.value, "Failed to invalidate CET cache");
#endif

        // if all went well CCMD CAIG = CCMD CIRG (i.e., actual = requested invalidation granularity)
        if (ccmd.bits.caig != 0x1)
        {
            printf("	Invalidatation of CET failed. Halting! (%u)\n", ccmd.bits.caig);
            HALT();
        }
    }
    printf("Done.\n");

    // 6. invalidate IOTLB
    printf("	Invalidating IOTLB...");
    {
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
#endif

        // if all went well IOTLB IAIG = IOTLB IIRG (i.e., actual = requested invalidation granularity)
        if (iotlb.bits.iaig != 0x1)
        {
            printf("	Invalidation of IOTLB failed. Halting! (%u)\n", iotlb.bits.iaig);
            HALT();
        }
    }
    printf("Done.\n");

    // 7. disable options we dont support
    printf("	Disabling unsupported options...");
    {
        // disable advanced fault logging (AFL)
        _vtd_drhd_issue_gcmd(drhd, VTD_GCMD_BIT_EAFL, 0);
        IOMMU_WAIT_OP(drhd, VTD_GSTS_REG_OFF, !gsts.bits.afls, (void *)&gsts.value, "	Could not disable AFL. Halting!");

        // disabled queued invalidation (QI)
        _vtd_drhd_issue_gcmd(drhd, VTD_GCMD_BIT_QIE, 0);
        IOMMU_WAIT_OP(drhd, VTD_GSTS_REG_OFF, !gsts.bits.qies, (void *)&gsts.value, "	Could not disable QI. Halting!");

        // disable interrupt remapping (IR)
        _vtd_drhd_issue_gcmd(drhd, VTD_GCMD_BIT_IRE, 0);
        IOMMU_WAIT_OP(drhd, VTD_GSTS_REG_OFF, !gsts.bits.ires, (void *)&gsts.value, "	Could not disable IR. Halting!");
    }
    printf("Done.\n");

    // 8. enable device
    printf("	Enabling device...");
    {
        // enable translation
        _vtd_drhd_issue_gcmd(drhd, VTD_GCMD_BIT_TE, 1);

        // wait for translation enabled status to go green...
        IOMMU_WAIT_OP(drhd, VTD_GSTS_REG_OFF, gsts.bits.tes, (void *)&gsts.value, "	DMA translation cannot be enabled. Halting!");
    }
    printf("Done.\n");

    // 9. disable protected memory regions (PMR) if available
    printf("	Checking and disabling PMR...");
    {
        VTD_PMEN_REG pmen;

        // PMR caps present, so disable it as we dont support that
        if (vtd_cap_plmr(drhd) || vtd_cap_phmr(drhd))
        {
            _vtd_reg(drhd, VTD_REG_READ, VTD_PMEN_REG_OFF, (void *)&pmen.value);
            pmen.bits.epm = 0; // disable PMR
            _vtd_reg(drhd, VTD_REG_WRITE, VTD_PMEN_REG_OFF, (void *)&pmen.value);
#ifndef __XMHF_VERIFICATION__
            // wait for PMR disabled...
            IOMMU_WAIT_OP(drhd, VTD_PMEN_REG_OFF, !pmen.bits.prs, (void *)&pmen.value, "	PMR cannot be disabled. Halting!");
#endif
        }
    }
    printf("Done.\n");
}

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

/********* Debug functions *********/
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
    VTD_GSTS_REG gsts;

    // disable translation
    _vtd_drhd_issue_gcmd(drhd, VTD_GCMD_BIT_TE, 0); // te
    // wait for translation enabled status to go red...
    IOMMU_WAIT_OP(drhd, VTD_GSTS_REG_OFF, !gsts.bits.tes, (void *)&gsts.value, "	DMA translation cannot be disabled. Halting!");

    // enable translation
    _vtd_drhd_issue_gcmd(drhd, VTD_GCMD_BIT_TE, 1); // te
    // wait for translation enabled status to go green...
    IOMMU_WAIT_OP(drhd, VTD_GSTS_REG_OFF, gsts.bits.tes, (void *)&gsts.value, "	    DMA translation cannot be enabled. Halting!");
}

void _vtd_disable_dma_iommu(VTD_DRHD *drhd)
{
    VTD_GSTS_REG gsts;

    // disable translation
    _vtd_drhd_issue_gcmd(drhd, VTD_GCMD_BIT_TE, 0);
    // wait for translation enabled status to go red...
    IOMMU_WAIT_OP(drhd, VTD_GSTS_REG_OFF, !gsts.bits.tes, (void *)&gsts.value, "	DMA translation cannot be disabled. Halting!");
}