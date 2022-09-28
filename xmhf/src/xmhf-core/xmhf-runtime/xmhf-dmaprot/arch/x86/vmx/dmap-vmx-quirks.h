// author: Miao Yu [Superymk]
#ifdef __XMHF_ALLOW_HYPAPP_DISABLE_IGFX_IOMMU__

#ifndef XMHF_DMAP_VMX_QUIRKS_H
#define XMHF_DMAP_VMX_QUIRKS_H


#ifndef __ASSEMBLY__

//! \brief Enable the IOMMU servicing the integrated GPU only. Other IOMMUs are not modified.
//!
//! @return Return true on success
extern bool _vtd_enable_igfx_drhd(VTD_DRHD* drhds, uint32_t vtd_num_drhd);

//! \brief Disable the IOMMU servicing the integrated GPU only. Other IOMMUs are not modified.
//!
//! @return Return true on success
extern bool _vtd_disable_igfx_drhd(VTD_DRHD* drhds, uint32_t vtd_num_drhd);

#endif	//__ASSEMBLY__
#endif //XMHF_DMAP_VMX_QUIRKS_H
#endif // __XMHF_ALLOW_HYPAPP_DISABLE_IGFX_IOMMU__