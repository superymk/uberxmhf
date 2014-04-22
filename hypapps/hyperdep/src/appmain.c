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

// hyperdep hypapp main module
// author: amit vasudevan (amitvasudevan@acm.org)

#include <xmhf.h>
#include <xmhf-core.h>

#define HYPERDEP_ACTIVATEDEP			0xC0
#define HYPERDEP_DEACTIVATEDEP			0xC1
#define HYPERDEP_INITIALIZE				0xC2

u32 hd_runtimephysbase=0;
u32 hd_runtimesize=0;

// hypapp initialization
u32 xmhf_hypapp_initialization(context_desc_t context_desc, hypapp_env_block_t hypappenvb){	
	printf("\nCPU %u: hyperDEP initializing", context_desc.cpu_desc.cpuid);
		
	//store runtime base and size
	hd_runtimephysbase = hypappenvb.runtimephysmembase;
	hd_runtimesize = hypappenvb.runtimesize;
	printf("\n%s: XMHF runtime base=%08x, size=%08x", __FUNCTION__, hd_runtimephysbase, hd_runtimesize);

	//apb->runtimephysmembase=0;	//write to core parameter area, should trigger a #PF
	//*((u32 *)0x10000000) = 0; 	//write to core memory, should trigger a #PF
	//{
	//		typedef void (*testfun)(void);
	//		testfun fun = (testfun)0x10000000;
	//		fun();	//execute arbitrary code in core memory region, should trigger a #pf
	//}

	printf("\nCPU %u: hyperDEP initialized!", context_desc.cpu_desc.cpuid);
	
	return APP_INIT_SUCCESS;  //successful
}

//----------------------------------------------------------------------
// RUNTIME

static void hd_activatedep(context_desc_t context_desc, u32 gpa){
	xmhfcore_setmemprot(context_desc, gpa, (MEMP_PROT_PRESENT | MEMP_PROT_READWRITE | MEMP_PROT_NOEXECUTE) );	   
	xmhfcore_memprot_flushmappings(context_desc);
	printf("\nCPU(%02x): %s removed EXECUTE permission for page at gpa %08x", context_desc.cpu_desc.cpuid, __FUNCTION__, gpa);
}

//de-activate DEP protection
static void hd_deactivatedep(context_desc_t context_desc, u32 gpa){
	xmhfcore_setmemprot(context_desc, gpa, (MEMP_PROT_PRESENT | MEMP_PROT_READWRITE | MEMP_PROT_EXECUTE) );	   
	xmhfcore_memprot_flushmappings(context_desc);
	printf("\nCPU(%02x): %s added EXECUTE permission for page at gpa %08x", context_desc.cpu_desc.cpuid, __FUNCTION__, gpa);
}

static void hd_initialize(context_desc_t context_desc){
	printf("\n%s: nothing to do", __FUNCTION__);
}


u32 xmhf_hypapp_handlehypercall(context_desc_t context_desc, u64 hypercall_id, u64 hypercall_param){
	u32 status=APP_SUCCESS;
	u32 call_id;
	u32 gva, gpa;
	
	call_id= hypercall_id;

	gva=(u32)hypercall_param;
	
	printf("\nCPU(%02x): %s starting: call number=%x, gva=%x", context_desc.cpu_desc.cpuid, __FUNCTION__, call_id, gva);

	switch(call_id){
		case HYPERDEP_INITIALIZE:{
			hd_initialize(context_desc);
		}
		break;

		case HYPERDEP_ACTIVATEDEP:{
			gpa=(u32)xmhfcore_smpguest_walk_pagetables(context_desc, gva);
			if(gpa == 0xFFFFFFFFUL){
				printf("\nCPU(%02x): WARNING: unable to get translation for gva=%x, just returning", context_desc.cpu_desc.cpuid, gva);
				return status;
			}		
			printf("\nCPU(%02x): translated gva=%x to gpa=%x", context_desc.cpu_desc.cpuid, gva, gpa);
			hd_activatedep(context_desc, gpa);
		}
		break;
		
		case HYPERDEP_DEACTIVATEDEP:{
			gpa=(u32)xmhfcore_smpguest_walk_pagetables(context_desc, gva);
			if(gpa == 0xFFFFFFFFUL){
				printf("\nCPU(%02x): WARNING: unable to get translation for gva=%x, just returning", context_desc.cpu_desc.cpuid, gva);
				return status;
			}		
			printf("\nCPU(%02x): translated gva=%x to gpa=%x", context_desc.cpu_desc.cpuid, gva, gpa);
			hd_deactivatedep(context_desc, gpa);
		}
		break;
		
		default:
			printf("\nCPU(0x%02x): unsupported hypercall (0x%08x)!!", 
			  context_desc.cpu_desc.cpuid, call_id);
			status=APP_ERROR;
			break;
	}

	return status;			
}


//handles XMHF shutdown callback
//note: should not return
void xmhf_hypapp_handleshutdown(context_desc_t context_desc){
	printf("\n%s:%u: rebooting now", __FUNCTION__, context_desc.cpu_desc.cpuid);
	xmhfcore_reboot(context_desc);				
}

//handles h/w pagetable violations
//for now this always returns APP_SUCCESS
u32 xmhf_hypapp_handleintercept_hwpgtblviolation(context_desc_t context_desc, u64 gpa, u64 gva, u64 error_code){
	u32 status = APP_SUCCESS;

	printf("\nCPU(%02x): FATAL HWPGTBL violation (gva=%x, gpa=%x, code=%x): app tried to execute data page??", context_desc.cpu_desc.cpuid, (u32)gva, (u32)gpa, (u32)error_code);
	HALT();
	
	return status;
}


//handles i/o port intercepts
//returns either APP_IOINTERCEPT_SKIP or APP_IOINTERCEPT_CHAIN
u32 xmhf_hypapp_handleintercept_portaccess(context_desc_t context_desc, u32 portnum, u32 access_type, u32 access_size){
	(void)portnum; //unused
	(void)access_type; //unused
	(void)access_size; //unused

 	return APP_IOINTERCEPT_CHAIN;
}

