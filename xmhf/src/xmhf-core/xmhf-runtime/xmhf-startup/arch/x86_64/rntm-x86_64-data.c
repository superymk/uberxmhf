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

/**
 * rntm-x86-data.c
 * EMHF runtime data definitions - x86 specific
 * author: amit vasudevan (amitvasudevan@acm.org)
 */

#include <xmhf.h>

//runtime GDT descriptor: not needed in x86_64
u64 x_gdt_start[4] __attribute__(( section(".data"), aligned(16) ));
arch_x86_gdtdesc_t x_gdt __attribute__(( section(".data"), aligned(16) ));

//runtime PAE page tables: not needed in x86_64
u8 x_3level_pdpt[PAGE_SIZE_4K] __attribute__(( section(".palign_data") ));
u8 x_3level_pdt[PAE_PTRS_PER_PDPT * PAGE_SIZE_4K] __attribute__(( section(".palign_data") ));

//runtime stack
u8 x_init_stack[RUNTIME_STACK_SIZE] __attribute__(( section(".stack") ));


RPB arch_rpb __attribute__(( section(".s_rpb") )) = {
	.magic= RUNTIME_PARAMETER_BLOCK_MAGIC,
	.XtVmmEntryPoint= (hva_t)xmhf_runtime_entry,
	.XtVmmPdptBase= 0, /* Not used */
	.XtVmmPdtsBase= 0, /* Not used */
	.XtGuestOSBootModuleBase= 0,
	.XtGuestOSBootModuleSize= 0,
	.runtime_appmodule_base= 0,
	.runtime_appmodule_size= 0,
	.XtVmmStackBase= (hva_t)x_init_stack,
	.XtVmmStackSize= 8192,
	.XtVmmGdt= 0,	/* Not used */
	.XtVmmIdt= (hva_t)xmhf_xcphandler_idt,
	.XtVmmIdtFunctionPointers= (hva_t)xmhf_xcphandler_exceptionstubs,
	.XtVmmIdtEntries= 32,
	.XtVmmRuntimePhysBase= 0,
	.XtVmmRuntimeVirtBase= 0,
	.XtVmmRuntimeSize= 0,
	.XtVmmE820Buffer= (hva_t)g_e820map,
	.XtVmmE820NumEntries= 0,
	.XtVmmMPCpuinfoBuffer= (hva_t)g_cpumap,
	.XtVmmMPCpuinfoNumEntries= 0,
	.XtVmmTSSBase= (hva_t)g_runtime_TSS,
	.RtmUartConfig = {0, 0, 0, 0, 0, 0, 0},
	.isEarlyInit=1,					//1 for an "early init" else 0 (late-init)
};
 
