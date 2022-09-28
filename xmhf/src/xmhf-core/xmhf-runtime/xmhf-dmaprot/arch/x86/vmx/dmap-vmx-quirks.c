// author: Miao Yu [Superymk]
#ifdef __XMHF_ALLOW_HYPAPP_DISABLE_IGFX_IOMMU__

#include <xmhf.h>
#include "dmap-vmx-internal.h"

static VTD_DRHD* _vtd_find_igfx_drhd(VTD_DRHD* drhds, uint32_t vtd_num_drhd)
{
    // uint32_t i = 0;

    // FOREACH_S(i, vtd_num_drhd, VTD_MAX_DRHD, 0, 1)
    // {
    //     VTD_DRHD* drhd = &drhds[i];

    //     if(drhd->flags & ACPI_DMAR_INCLUDE_ALL == 0)
    //     {
    //         // Find the DRHD for an integrated GPU only
    //         return drhd;
    //     }
    // }

    // [TODO][Urgent] We hardcode the result for HP 2540p currently, a correct implementation should refer to 
    // <acpi_parse_dev_scope> in Xen 4.16.1
    static uint32_t ioh_bus = 0, ioh_dev = 0, ioh_fn = 0;
    uint32_t ioh_id = 0;

    #define IS_ILK(id)    (id == 0x00408086 || id == 0x00448086 || id== 0x00628086 || id == 0x006A8086)


    xmhf_baseplatform_arch_x86_pci_type1_read(ioh_bus, ioh_dev, ioh_fn, 0, sizeof(u32), &ioh_id);
    if(IS_ILK(ioh_id))
        return &drhds[1];

    (void)vtd_num_drhd;
    return NULL;
}

//! \brief Disable the IOMMU servicing the integrated GPU only. Other IOMMUs are not modified.
//!
//! @return Return true on success
bool _vtd_disable_igfx_drhd(VTD_DRHD* drhds, uint32_t vtd_num_drhd)
{
    VTD_DRHD* drhd = NULL;

    drhd = _vtd_find_igfx_drhd(drhds, vtd_num_drhd);
    if(!drhd)
        return false;

    _vtd_disable_dma_iommu(drhd);
    return true;
}

//! \brief Enable the IOMMU servicing the integrated GPU only. Other IOMMUs are not modified.
//!
//! @return Return true on success
bool _vtd_enable_igfx_drhd(VTD_DRHD* drhds, uint32_t vtd_num_drhd)
{
    VTD_DRHD* drhd = NULL;

    drhd = _vtd_find_igfx_drhd(drhds, vtd_num_drhd);
    if(!drhd)
        return false;

    _vtd_enable_dma_iommu(drhd);
    return true;
}


#endif // __XMHF_ALLOW_HYPAPP_DISABLE_IGFX_IOMMU__