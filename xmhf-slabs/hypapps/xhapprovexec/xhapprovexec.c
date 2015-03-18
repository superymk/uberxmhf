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

// approvexec hypapp main module
// author: amit vasudevan (amitvasudevan@acm.org)

#include <xmhf.h>
#include <xmhfhicslab.h>
#include <xmhf-debug.h>

#include <xc.h>
#include <xhapprovexec.h>


//////
//XMHF_SLABNEW(xhapprovexec)


#define APPROVEXEC_LOCK     			0xD0
#define APPROVEXEC_UNLOCK   			0xD1

static u8 _ae_page_buffer[PAGE_SIZE_4K];

static u8 _ae_database[][SHA_DIGEST_LENGTH] = {
  {0xd1, 0x4e, 0x30, 0x25,  0x8e,  0x16, 0x85, 0x9b, 0x21, 0x81, 0x74, 0x78, 0xbb, 0x1b, 0x5d, 0x99, 0xb5, 0x48, 0x60, 0xca},
  {0xa1, 0x4e, 0x30, 0x25,  0x8e,  0x16, 0x85, 0x9b, 0x21, 0x71, 0x74, 0x78, 0xbb, 0x1b, 0x5d, 0x99, 0xb5, 0x48, 0x60, 0xca},
  {0xf1, 0x4e, 0x30, 0x25,  0x8e,  0x16, 0x85, 0x9b, 0x21, 0x81, 0x54, 0x78, 0xbb, 0x1b, 0x5d, 0x99, 0xb5, 0x48, 0x60, 0xca},
  {0xe1, 0x4e, 0x30, 0x25,  0x9e,  0x16, 0x85, 0x9b, 0x21, 0x81, 0x74, 0x78, 0x6b, 0x1b, 0x5d, 0x99, 0xb5, 0x48, 0x60, 0xca},
};

#define NUMENTRIES_AE_DATABASE  (sizeof(_ae_database)/sizeof(_ae_database[0]))


//approve and lock a page (at gpa)
static void ae_lock(u32 cpuindex, u32 guest_slab_index, u64 gpa){
    slab_params_t spl;
    xmhf_hic_uapi_physmem_desc_t *pdesc = (xmhf_hic_uapi_physmem_desc_t *)&spl.in_out_params[2];
    u8 digest[SHA_DIGEST_LENGTH];
    bool found_in_database=false;
    u32 i;

    _XDPRINTF_("%s[%u]: starting...\n", __func__, (u16)cpuindex);
    spl.src_slabid = XMHF_HYP_SLAB_XHAPPROVEXEC;
    spl.cpuid = cpuindex;

if(!ae_activated){
    //grab page contents at gpa into local page buffer
    pdesc->guest_slab_index = guest_slab_index;
    pdesc->addr_to = &_ae_page_buffer;
    pdesc->addr_from = gpa;
    pdesc->numbytes = PAGE_SIZE_4K;
    //XMHF_HIC_SLAB_UAPI_PHYSMEM(XMHF_HIC_UAPI_PHYSMEM_PEEK, &pdesc, NULL);
    spl.in_out_params[0] = XMHF_HIC_UAPI_PHYSMEM;
    spl.in_out_params[1] = XMHF_HIC_UAPI_PHYSMEM_PEEK;
    XMHF_SLAB_UAPI(&spl);

    _XDPRINTF_("%s[%u]: grabbed page contents at gpa=%016x\n",
                __func__, (u16)cpuindex, gpa);

    //compute SHA-1 of the local page buffer
    sha1_buffer(&_ae_page_buffer, PAGE_SIZE_4K, digest);

    _XDPRINTF_("%s[%u]: computed SHA-1: %*D\n",
               __func__, (u16)cpuindex, SHA_DIGEST_LENGTH, digest, " ");

    //compare computed SHA-1 to the database
    for(i=0; i < NUMENTRIES_AE_DATABASE; i++){
        if(!memcmp(&digest, &_ae_database[i], SHA_DIGEST_LENGTH)){
            found_in_database=true;
            break;
        }
    }

    //if not approved then just return
    if(!found_in_database){
        _XDPRINTF_("%s[%u]: could not find entry in database. returning\n",
               __func__, (u16)cpuindex);
        return;
    }

    _XDPRINTF_("%s[%u]: entry matched in database, proceeding to lock page...\n",
               __func__, (u16)cpuindex);

    {
        //lock the code page so no one can write to it
        xmhf_hic_uapi_mempgtbl_desc_t *mdesc = (xmhf_hic_uapi_mempgtbl_desc_t *)&spl.in_out_params[2];

        mdesc->guest_slab_index = guest_slab_index;
        mdesc->gpa = gpa;
        spl.in_out_params[0] = XMHF_HIC_UAPI_MEMPGTBL;

        //XMHF_HIC_SLAB_UAPI_MEMPGTBL(XMHF_HIC_UAPI_MEMPGTBL_GETENTRY, &mdesc, &mdesc);
        spl.in_out_params[1] = XMHF_HIC_UAPI_MEMPGTBL_GETENTRY;
        XMHF_SLAB_UAPI(&spl);
        _XDPRINTF_("%s[%u]: original entry for gpa=%016llx is %016llx\n",
                   __func__, (u16)cpuindex, gpa, mdesc->entry);

        mdesc->entry &= ~(0x7);
        mdesc->entry |= 0x5; // execute, read-only

        spl.in_out_params[1] = XMHF_HIC_UAPI_MEMPGTBL_SETENTRY;
        //XMHF_HIC_SLAB_UAPI_MEMPGTBL(XMHF_HIC_UAPI_MEMPGTBL_SETENTRY, &mdesc, NULL);
        XMHF_SLAB_UAPI(&spl);

        ae_activated = true;

        _XDPRINTF_("%s[%u]: approved and locked page at gpa %x\n",
                   __func__, (u16)cpuindex, gpa);
    }
}

}


//unlock a page (at gpa)
static void ae_unlock(u32 cpuindex, u32 guest_slab_index, u64 gpa){
     slab_params_t spl;
     xmhf_hic_uapi_mempgtbl_desc_t *mdesc = (xmhf_hic_uapi_mempgtbl_desc_t *)&spl.in_out_params[2];

     spl.src_slabid = XMHF_HYP_SLAB_XHAPPROVEXEC;
     spl.cpuid = cpuindex;
     spl.in_out_params[0] = XMHF_HIC_UAPI_MEMPGTBL;

    _XDPRINTF_("%s[%u]: starting...\n", __func__, (u16)cpuindex);

    if(ae_activated){
         //unlock the code page
         mdesc->guest_slab_index = guest_slab_index;
         mdesc->gpa = gpa;

         //XMHF_HIC_SLAB_UAPI_MEMPGTBL(XMHF_HIC_UAPI_MEMPGTBL_GETENTRY, &mdesc, &mdesc);
         spl.in_out_params[1] = XMHF_HIC_UAPI_MEMPGTBL_GETENTRY;
         XMHF_SLAB_UAPI(&spl);

         _XDPRINTF_("%s[%u]: original entry for gpa=%016llx is %016llx\n",  __func__, (u16)cpuindex, gpa, mdesc->entry);

        mdesc->entry &= ~(0x7);
        mdesc->entry |= 0x7; // execute, read-write

        //XMHF_HIC_SLAB_UAPI_MEMPGTBL(XMHF_HIC_UAPI_MEMPGTBL_SETENTRY, &mdesc, NULL);
        spl.in_out_params[1] = XMHF_HIC_UAPI_MEMPGTBL_GETENTRY;
        XMHF_SLAB_UAPI(&spl);

        ae_activated=false;

        _XDPRINTF_("%s[%u]: restored permissions for page at %016llx\n", __func__, (u16)cpuindex, gpa);
    }
}



//////
// hypapp callbacks


//initialization
static void _hcb_initialize(u32 cpuindex){
	_XDPRINTF_("%s[%u]: approvexec initializing...\n", __func__, (u16)cpuindex);
}

//hypercall
static void _hcb_hypercall(u64 cpuindex, u64 guest_slab_index){
    slab_params_t spl;
    x86regs_t *gprs = (x86regs_t *)&spl.in_out_params[2];
	u32 call_id;
	u64 gpa;

    spl.src_slabid = XMHF_HYP_SLAB_XHAPPROVEXEC;
    spl.cpuid = cpuindex;
    spl.in_out_params[0] = XMHF_HIC_UAPI_CPUSTATE;

    //XMHF_HIC_SLAB_UAPI_CPUSTATE(XMHF_HIC_UAPI_CPUSTATE_GUESTGPRSREAD, NULL, &gprs);
    spl.in_out_params[1] = XMHF_HIC_UAPI_CPUSTATE_GUESTGPRSREAD;
    XMHF_SLAB_UAPI(&spl);

    call_id = gprs->eax;
    gpa = ((u64)gprs->edx << 32) | gprs->ebx;

	_XDPRINTF_("%s[%u]: call_id=%x, gpa=%016llx\n", __func__, (u16)cpuindex, call_id, gpa);


	switch(call_id){

		case APPROVEXEC_LOCK:{
			ae_lock(cpuindex, guest_slab_index, gpa);
		}
		break;

		case APPROVEXEC_UNLOCK:{
			ae_unlock(cpuindex, guest_slab_index, gpa);
		}
		break;

		default:
            _XDPRINTF_("%s[%u]: unsupported hypercall %x. Ignoring\n",
                       __func__, (u16)cpuindex, call_id);
			break;
	}

}


//memory fault
static void _hcb_memoryfault(u32 cpuindex, u32 guest_slab_index, u64 gpa, u64 gva, u64 errorcode){

	_XDPRINTF_("%s[%u]: memory fault in guest slab %u; gpa=%016llx, gva=%016llx, errorcode=%016llx, write error to approved code?\n",
            __func__, (u16)cpuindex, guest_slab_index, gpa, gva, errorcode);

}

// shutdown
static void _hcb_shutdown(u32 cpuindex, u32 guest_slab_index){
	_XDPRINTF_("%s[%u]: guest slab %u shutdown...\n", __func__, (u16)cpuindex, guest_slab_index);
}









//////
// slab interface

//void slab_interface(slab_input_params_t *iparams, u64 iparams_size, slab_output_params_t *oparams, u64 oparams_size, u64 src_slabid, u64 cpuindex){
void slab_main(slab_params_t *sp){
    //xc_hypappcb_inputparams_t *hcb_iparams = (xc_hypappcb_inputparams_t *)iparams;
    //xc_hypappcb_outputparams_t *hcb_oparams = (xc_hypappcb_outputparams_t *)oparams;
    xc_hypappcb_params_t *hcbp = (xc_hypappcb_params_t *)&sp->in_out_params[0];
    hcbp->cbresult=XC_HYPAPPCB_CHAIN;

	_XDPRINTF_("%s[%u]: Got control, cbtype=%x: ESP=%08x\n",
                __func__, (u16)sp->cpuid, hcbp->cbtype, read_esp());


    switch(hcbp->cbtype){
        case XC_HYPAPPCB_INITIALIZE:{
            _hcb_initialize(sp->cpuid);
        }
        break;

        case XC_HYPAPPCB_HYPERCALL:{
            _hcb_hypercall(sp->cpuid, hcbp->guest_slab_index);
        }
        break;

        case XC_HYPAPPCB_MEMORYFAULT:{
         	u64 errorcode;
         	u64 gpa;
         	u64 gva;
         	slab_params_t spl;

         	spl.src_slabid = XMHF_HYP_SLAB_XHAPPROVEXEC;
         	spl.cpuid = sp->cpuid;
            spl.in_out_params[0] = XMHF_HIC_UAPI_CPUSTATE;
            spl.in_out_params[1] = XMHF_HIC_UAPI_CPUSTATE_VMREAD;

         	//XMHF_HIC_SLAB_UAPI_CPUSTATE(XMHF_HIC_UAPI_CPUSTATE_VMREAD, VMCS_INFO_EXIT_QUALIFICATION, &errorcode);
            spl.in_out_params[2] = VMCS_INFO_EXIT_QUALIFICATION;
            XMHF_SLAB_UAPI(&spl);
            errorcode = spl.in_out_params[4];

         	//XMHF_HIC_SLAB_UAPI_CPUSTATE(XMHF_HIC_UAPI_CPUSTATE_VMREAD, VMCS_INFO_GUEST_PADDR_FULL, &gpa);
            spl.in_out_params[2] = VMCS_INFO_GUEST_PADDR_FULL;
            XMHF_SLAB_UAPI(&spl);
            gpa = spl.in_out_params[4];

         	//XMHF_HIC_SLAB_UAPI_CPUSTATE(XMHF_HIC_UAPI_CPUSTATE_VMREAD, VMCS_INFO_GUEST_LINEAR_ADDRESS, &gva);
            spl.in_out_params[2] = VMCS_INFO_GUEST_LINEAR_ADDRESS;
            XMHF_SLAB_UAPI(&spl);
            gva = spl.in_out_params[4];

            _hcb_memoryfault(sp->cpuid, hcbp->guest_slab_index, gpa, gva, errorcode);
        }
        break;

        case XC_HYPAPPCB_SHUTDOWN:{
            _hcb_shutdown(sp->cpuid, hcbp->guest_slab_index);
        }
        break;

        //case XC_HYPAPPCB_TRAP_IO:{
        //
        //
        //}
        //break;

        //case XC_HYPAPPCB_TRAP_INSTRUCTION:{
        //
        //
        //}
        //break;

        //case XC_HYPAPPCB_TRAP_EXCEPTION:{
        //
        //
        //}
        //break;


        default:{
            _XDPRINTF_("%s[%u]: Unknown cbtype. Halting!\n",
                __func__, (u16)sp->cpuid);
            //HALT();
        }
    }

}