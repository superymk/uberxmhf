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
 * This file is part of the EMHF historical reference
 * codebase, and is released under the terms of the
 * GNU General Public License (GPL) version 2.
 * Please see the LICENSE file for details.
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

#include <emhf.h> /* FIXME: narrow this down so this can be compiled
                     and tested independently */

void hpt_walk_set_prot(hpt_walk_ctx_t *walk_ctx, hpt_pm_t pm, int pm_lvl, gpa_t gpa, hpt_prot_t prot)
{
  hpt_pme_t pme;
  int end_pm_lvl=1;

  pm = hpt_walk_get_pm(walk_ctx, pm_lvl, pm, &end_pm_lvl, gpa);
  ASSERT(pm != NULL);
  ASSERT(end_pm_lvl==1); /* FIXME we don't handle large pages */
  pme = hpt_pm_get_pme_by_va(walk_ctx->t, end_pm_lvl, pm, gpa);
  pme = hpt_pme_setprot(walk_ctx->t, end_pm_lvl, pme, prot);
  hpt_pm_set_pme_by_va(walk_ctx->t, end_pm_lvl, pm, gpa, pme);
}

void hpt_walk_set_prots(hpt_walk_ctx_t *walk_ctx,
                        hpt_pm_t pm,
                        int pm_lvl,
                        gpa_t gpas[],
                        size_t num_gpas,
                        hpt_prot_t prot)
{
  size_t i;
  for(i=0; i<num_gpas; i++) {
    hpt_walk_set_prot(walk_ctx, pm, pm_lvl, gpas[i], prot);
  }
}

/* inserts pme into the page map of level tgt_lvl containing va.
 * fails if tgt_lvl is not allocated.
 * XXX move into hpt.h once it's available again.
 */
static inline
int hpt_walk_insert_pme(const hpt_walk_ctx_t *ctx, int lvl, hpt_pm_t pm, int tgt_lvl, hpt_va_t va, hpt_pme_t pme)
{
  int end_lvl=tgt_lvl;
  dprintf(LOG_TRACE, "hpt_walk_insert_pme_alloc: lvl:%d pm:%x tgt_lvl:%d va:%Lx pme:%Lx\n",
          lvl, (u32)pm, tgt_lvl, va, pme);
  pm = hpt_walk_get_pm(ctx, lvl, pm, &end_lvl, va);

  dprintf(LOG_TRACE, "hpt_walk_insert_pme: got pm:%x end_lvl:%d\n",
          (u32)pm, end_lvl);

  if(pm == NULL || tgt_lvl != end_lvl) {
    return 1;
  }

  hpt_pm_set_pme_by_va(ctx->t, tgt_lvl, pm, va, pme);
  return 0;
}

typedef struct {
  hpt_va_t reg_gva;
  hpt_va_t pal_gva;
  size_t size;
  hpt_prot_t prot;
} section_t;

typedef struct {
  hpt_pm_t pm;
  hpt_type_t t;
  int lvl;
} hpt_pmo_t;

typedef struct {
  hpt_pme_t pme;
  hpt_type_t t;
  int lvl;
} hpt_pmeo_t;

int hpt_walk_insert_pmeo(const hpt_walk_ctx_t *ctx,
                         hpt_pmo_t *pmo,
                         const hpt_pmeo_t *pmeo,
                         hpt_va_t va)
{
  return hpt_walk_insert_pme(ctx,
                             pmo->lvl,
                             pmo->pm,
                             pmeo->lvl,
                             va,
                             pmeo->pme);
}

int hpt_walk_insert_pmeo_alloc(const hpt_walk_ctx_t *ctx,
                               hpt_pmo_t *pmo,
                               const hpt_pmeo_t *pmeo,
                               hpt_va_t va)
{
  return hpt_walk_insert_pme_alloc(ctx, pmo->lvl, pmo->pm, pmeo->lvl, va, pmeo->pme);
}

void hpt_walk_get_pmeo(hpt_pmeo_t *pmeo,
                       const hpt_walk_ctx_t *ctx,
                       const hpt_pmo_t *pmo,
                       int end_lvl,
                       hpt_va_t va)
{
  pmeo->lvl = end_lvl;
  pmeo->t = pmo->t;
  pmeo->pme = hpt_walk_get_pme(ctx, pmo->lvl, pmo->pm, &pmeo->lvl, va);
}

hpt_pa_t hpt_pmeo_get_address(const hpt_pmeo_t *pmeo)
{
  return hpt_pme_get_address(pmeo->t, pmeo->lvl, pmeo->pme);
}
void hpt_pmeo_set_address(hpt_pmeo_t *pmeo, hpt_pa_t addr)
{
  pmeo->pme = hpt_pme_set_address(pmeo->t, pmeo->lvl, pmeo->pme, addr);
}

bool hpt_pmeo_is_present(const hpt_pmeo_t *pmeo)
{
  return hpt_pme_is_present(pmeo->t, pmeo->lvl, pmeo->pme);
}

void hpt_pmeo_setprot(hpt_pmeo_t *pmeo, hpt_prot_t perms)
{
  pmeo->pme = hpt_pme_setprot(pmeo->t, pmeo->lvl, pmeo->pme, perms);
}

/* XXX TEMP */
#define CHK(x) ASSERT(x)
#define CHK_RV(x) ASSERT(!(x))
#define hpt_walk_check_prot(x, y) HPT_PROTS_RWX

void scode_add_section(hpt_pmo_t* reg_npmo_root, hpt_walk_ctx_t *reg_npm_ctx,
                       hpt_pmo_t* reg_gpmo_root, hpt_walk_ctx_t *reg_gpm_ctx,
                       hpt_pmo_t* pal_npmo_root, hpt_walk_ctx_t *pal_npm_ctx,
                       hpt_pmo_t* pal_gpmo_root, hpt_walk_ctx_t *pal_gpm_ctx,
                       const section_t *section)
{
  size_t offset;
  int hpt_err;
  
  /* XXX don't hard-code page size here. */
  /* XXX fail gracefully */
  ASSERT((section->size % PAGE_SIZE_4K) == 0); 

  for (offset=0; offset < section->size; offset += PAGE_SIZE_4K) {
    hpt_va_t page_reg_gva = section->reg_gva + offset;
    hpt_va_t page_pal_gva = section->pal_gva + offset;

    /* XXX we don't use hpt_va_t or hpt_pa_t for gpa's because they
       get used as both */
    u64 page_reg_gpa, page_pal_gpa; /* guest-physical-addresses */


    hpt_pmeo_t page_reg_gpmeo; /* reg's guest page-map-entry and lvl */
    hpt_pmeo_t page_pal_gpmeo; /* pal's guest page-map-entry and lvl */

    hpt_pmeo_t page_reg_npmeo; /* reg's nested page-map-entry and lvl */
    hpt_pmeo_t page_pal_npmeo; /* pal's nested page-map-entry and lvl */

    /* lock? quiesce? */

    hpt_walk_get_pmeo(&page_reg_gpmeo,
                      reg_gpm_ctx,
                      reg_gpmo_root,
                      1,
                      page_reg_gva);
    ASSERT(page_reg_gpmeo.lvl==1); /* we don't handle large pages */
    page_reg_gpa = hpt_pmeo_get_address(&page_reg_gpmeo);

    hpt_walk_get_pmeo(&page_reg_npmeo,
                      reg_npm_ctx,
                      reg_npmo_root,
                      1,
                      page_reg_gpa);
    ASSERT(page_reg_npmeo.lvl==1); /* we don't handle large pages */
    /* page_spa = hpt_pme_get_address(reg_npt_ctx->t, page_reg_npme_lvl, page_reg_npme); */

    /* no reason to go with a different mapping */
    page_pal_gpa = page_reg_gpa;

    /* check that this VM is allowed to access this system-physical mem */
    /* XXX CHK(HPT_PROTS_RWX == hpt_walk_check_prot(reg_npt_root, page_reg_gpa)); */

    /* check that this guest process is allowed to access this guest-physical mem */
    /* XXX CHK(HPT_PROTS_RWX == hpt_walk_check_prot(guest_reg_root, page_reg_gva)); */

    /* check that the requested virtual address isn't already mapped
       into PAL's address space */
    {
      hpt_pmeo_t existing_pmeo;
      hpt_walk_get_pmeo(&existing_pmeo,
                        pal_gpm_ctx,
                        pal_gpmo_root,
                        1,
                        page_pal_gva);
      CHK(!hpt_pmeo_is_present(&existing_pmeo));
    }

    /* revoke access from 'reg' VM */
    hpt_pmeo_setprot(&page_reg_npmeo, HPT_PROTS_NONE);
    hpt_err = hpt_walk_insert_pmeo(reg_npm_ctx,
                                   reg_npmo_root,
                                   &page_reg_npmeo,
                                   page_reg_gpa);
    CHK_RV(hpt_err);

    /* for simplicity, we don't bother removing from guest page
       tables. removing from nested page tables is sufficient */

    /* add access to pal guest page tables */
    page_pal_gpmeo = page_reg_gpmeo; /* XXX SECURITY should build from scratch */
    hpt_pmeo_set_address(&page_pal_gpmeo, page_pal_gva);
    hpt_pmeo_setprot    (&page_pal_gpmeo, HPT_PROTS_RWX);
    hpt_err = hpt_walk_insert_pmeo_alloc(pal_gpm_ctx,
                                         pal_gpmo_root,
                                         &page_pal_gpmeo,
                                         page_pal_gva);
    CHK_RV(hpt_err);

    /* add access to pal nested page tables */
    page_pal_npmeo = page_reg_npmeo; /* XXX SECURITY should build from scratch */
    hpt_pmeo_setprot(&page_pal_npmeo, section->prot);
    hpt_err = hpt_walk_insert_pmeo_alloc(pal_npm_ctx,
                                         pal_npmo_root,
                                         &page_pal_npmeo,
                                         page_pal_gpa);
    CHK_RV(hpt_err);

    /* unlock? unquiesce? */
  }
  /* add pal guest page tables to pal nested page tables */
}

/* for a given virtual address range, return an array of page-map-entry-objects */
/* XXX specify behavior if pmeos don't fit exactly. e.g., if requested
 * section size is 4K of a 2MB page */
/* XXX need to think through concurrency issues. e.g., should caller
   hold a lock? */
void pmeos_of_range(hpt_pmeo_t pmeos[], size_t *pmeos_num,
                    hpt_pmo_t* pmo_root, hpt_walk_ctx_t *walk_ctx,
                    hpt_va_t base, size_t size)
{
  size_t offset;
  size_t pmeos_maxnum = *pmeos_num;
  
  *pmeos_num = 0;

  while (offset < size) {
    hpt_va_t va = base + offset;
    size_t page_size;

    ASSERT(*pmeos_num < pmeos_maxnum);

    hpt_walk_get_pmeo(&pmeos[*pmeos_num],
                      walk_ctx,
                      pmo_root,
                      1,
                      va);

    /* XXX need to add support to hpt to get size of memory mapped by
       a page */
    ASSERT(pmeos[*pmeos_num].lvl == 1);
    page_size = PAGE_SIZE_4K;

    offset += page_size;
    (*pmeos_num)++;
  }

  ASSERT(offset == size);
}

/* Local Variables: */
/* mode:c           */
/* indent-tabs-mode:'f */
/* tab-width:2      */
/* End:             */
