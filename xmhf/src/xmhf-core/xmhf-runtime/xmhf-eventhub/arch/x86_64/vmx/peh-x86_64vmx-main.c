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

// peh-x86vmx-main.c
// EMHF partition event-hub for Intel x86 vmx
// author: amit vasudevan (amitvasudevan@acm.org)
#include <xmhf.h>


//---VMX decode assist----------------------------------------------------------
//map a CPU register index into appropriate VCPU *vcpu or struct regs *r field
//and return the address of the field
static u64 * _vmx_decode_reg(u32 gpr, VCPU *vcpu, struct regs *r){
    switch(gpr){
        case  0: return &r->rax;
        case  1: return &r->rcx;
        case  2: return &r->rdx;
        case  3: return &r->rbx;
        case  4: return (u64*)&vcpu->vmcs.guest_RSP;
        case  5: return &r->rbp;
        case  6: return &r->rsi;
        case  7: return &r->rdi;
        case  8: return &r->r8 ;
        case  9: return &r->r9 ;
        case 10: return &r->r10;
        case 11: return &r->r11;
        case 12: return &r->r12;
        case 13: return &r->r13;
        case 14: return &r->r14;
        case 15: return &r->r15;
        default: {
            printf("\n[%02x]%s: invalid gpr value (%u). halting!", vcpu->id,
                   __FUNCTION__, gpr);
            HALT();
            //we will never get here, appease the compiler
            return (u64 *)&r->rax;
        }
    }
}


//---intercept handler (CPUID)--------------------------------------------------
static void _vmx_handle_intercept_cpuid(VCPU *vcpu, struct regs *r){
	//printf("\nCPU(0x%02x): CPUID", vcpu->id);
	u32 old_eax = r->eax;
	asm volatile ("cpuid\r\n"
          :"=a"(r->eax), "=b"(r->ebx), "=c"(r->ecx), "=d"(r->edx)
          :"a"(r->eax), "c" (r->ecx));
	if (old_eax == 0x1) {
		/* Clear VMX capability */
		r->ecx &= ~(1U << 5);
		/* Clear x2APIC capability */
		r->ecx &= ~(1U << 21);
	}
	vcpu->vmcs.guest_RIP += vcpu->vmcs.info_vmexit_instruction_length;
}


//---vmx int 15 intercept handler-----------------------------------------------
static void _vmx_int15_handleintercept(VCPU *vcpu, struct regs *r){
	u16 cs, ip;
	u8 *bdamemory = (u8 *)0x4AC;

	//if in V86 mode translate the virtual address to physical address
	if( (vcpu->vmcs.guest_CR0 & CR0_PE) && (vcpu->vmcs.guest_CR0 & CR0_PG) &&
			(vcpu->vmcs.guest_RFLAGS & EFLAGS_VM) ){
		u8 *bdamemoryphysical;
		#ifdef __XMHF_VERIFICATION__
			bdamemoryphysical = (u8 *)nondet_u32();
		#else
			bdamemoryphysical = (u8 *)xmhf_smpguest_arch_x86_64vmx_walk_pagetables(vcpu, (hva_t)bdamemory);
		#endif
		if((sla_t)bdamemoryphysical < rpb->XtVmmRuntimePhysBase){
			printf("\nINT15 (E820): V86 mode, bdamemory translated from %08lx to %08lx",
				(hva_t)bdamemory, (sla_t)bdamemoryphysical);
			bdamemory = bdamemoryphysical; 
		}else{
			printf("\nCPU(0x%02x): INT15 (E820) V86 mode, translated bdamemory points beyond \
				guest physical memory space. Halting!", vcpu->id);
			HALT();
		}
	}


	//if E820 service then...
	if((u16)r->eax == 0xE820){
		//AX=0xE820, EBX=continuation value, 0 for first call
		//ES:DI pointer to buffer, ECX=buffer size, EDX='SMAP'
		//return value, CF=0 indicated no error, EAX='SMAP'
		//ES:DI left untouched, ECX=size returned, EBX=next continuation value
		//EBX=0 if last descriptor
		printf("\nCPU(0x%02x): INT 15(e820): AX=0x%04x, EDX=0x%08x, EBX=0x%08x, ECX=0x%08x, ES=0x%04x, DI=0x%04x", vcpu->id,
		(u16)r->eax, r->edx, r->ebx, r->ecx, (u16)vcpu->vmcs.guest_ES_selector, (u16)r->edi);

		//HALT_ON_ERRORCOND(r->edx == 0x534D4150UL);  //'SMAP' should be specified by guest
		//HALT_ON_ERRORCOND(r->ebx < rpb->XtVmmE820NumEntries); //invalid continuation value specified by guest!
		if( (r->edx == 0x534D4150UL) && (r->ebx < rpb->XtVmmE820NumEntries) ){

			//copy the e820 descriptor and return its size in ECX
			{

				if( ((sla_t)(vcpu->vmcs.guest_ES_base+(u16)r->edi)) < rpb->XtVmmRuntimePhysBase){
					GRUBE820 *pe820entry;
					#ifdef __XMHF_VERIFICATION__
						GRUBE820 e820entry;
						pe820entry = &e820entry;
					#else
						pe820entry = (GRUBE820 *)((sla_t)(vcpu->vmcs.guest_ES_base+(u16)r->edi));
					#endif //__XMHF_VERIFICATION__
					pe820entry->baseaddr_low = g_e820map[r->ebx].baseaddr_low;
					pe820entry->baseaddr_high = g_e820map[r->ebx].baseaddr_high;
					pe820entry->length_low = g_e820map[r->ebx].length_low;
					pe820entry->length_high = g_e820map[r->ebx].length_high;
					pe820entry->type = g_e820map[r->ebx].type;
					/* Check whether exceed supported memory */
					{
						u64 baseaddr = (((u64)pe820entry->baseaddr_high) << 32) |
										pe820entry->baseaddr_low;
						u64 length = (((u64)pe820entry->length_high) << 32) |
									pe820entry->length_low;
						HALT_ON_ERRORCOND(baseaddr + length <= MAX_PHYS_ADDR);
					}
				}else{
					printf("\nCPU(0x%02x): INT15 E820. Guest buffer is beyond guest "
							"physical memory bounds. Halting!", vcpu->id);
					HALT();
				}

			}
			r->ecx=20;

			//set EAX to 'SMAP' as required by the service call
			r->eax=r->edx;

			//we need to update carry flag in the guest EFLAGS register
			//however since INT 15 would have pushed the guest FLAGS on stack
			//we cannot simply reflect the change by modifying vmcb->rflags
			//instead we need to make the change to the pushed FLAGS register on
			//the guest stack. the real-mode IRET frame looks like the following
			//when viewed at top of stack
			//guest_ip		(16-bits)
			//guest_cs		(16-bits)
			//guest_flags (16-bits)
			//...

			{
				//u16 guest_cs, guest_ip, guest_flags;
				u16 guest_cs __attribute__((unused)), guest_ip __attribute__((unused)), guest_flags;
				/* Truncate RSP to 16 bits, (higher bits not used in real mode) */
				u16 *gueststackregion = (u16 *)( (hva_t)vcpu->vmcs.guest_SS_base + (u16)vcpu->vmcs.guest_RSP );


				//if V86 mode translate the virtual address to physical address
				if( (vcpu->vmcs.guest_CR0 & CR0_PE) && (vcpu->vmcs.guest_CR0 & CR0_PG) &&
					(vcpu->vmcs.guest_RFLAGS & EFLAGS_VM) ){
					#ifdef __XMHF_VERIFICATION__
						u8 *gueststackregionphysical = (u8 *)nondet_u32();
					#else
						u8 *gueststackregionphysical = (u8 *)xmhf_smpguest_arch_x86_64vmx_walk_pagetables(vcpu, (hva_t)gueststackregion);
					#endif
					if((sla_t)gueststackregionphysical < rpb->XtVmmRuntimePhysBase){
						printf("\nINT15 (E820): V86 mode, gueststackregion translated from %08x to %08x",
							(hva_t)gueststackregion, (sla_t)gueststackregionphysical);
						gueststackregion = (u16 *)gueststackregionphysical; 
					}else{
						printf("\nCPU(0x%02x): INT15 (E820) V86 mode, translated gueststackregion points beyond \
							guest physical memory space. Halting!", vcpu->id);
						HALT();
					}
				}


				//get guest IP, CS and FLAGS from the IRET frame
				#ifdef __XMHF_VERIFICATION__
					guest_ip = nondet_u16();
					guest_cs = nondet_u16();
					guest_flags = nondet_u16();
				#else
					guest_ip = gueststackregion[0];
					guest_cs = gueststackregion[1];
					guest_flags = gueststackregion[2];
				#endif	//__XMHF_VERIFICATION__


				//increment e820 descriptor continuation value
				r->ebx=r->ebx+1;
				if (r->ebx >= rpb->XtVmmE820NumEntries) {
					//we have reached the last record, so make EBX=0
					r->ebx=0;
				}

				// clear CF when success
				guest_flags &= ~(u16)EFLAGS_CF;
				#ifndef __XMHF_VERIFICATION__
					gueststackregion[2] = guest_flags;
				#endif

			}

		}else{	//invalid state specified during INT 15 E820, fail by
				//setting carry flag
				printf("\nCPU(0x%02x): INT15 (E820), invalid state specified by guest \
						Halting!", vcpu->id);
				HALT();
		}

		//update RIP to execute the IRET following the VMCALL instruction
		//effectively returning from the INT 15 call made by the guest
		vcpu->vmcs.guest_RIP += vcpu->vmcs.info_vmexit_instruction_length;

		return;
	} //E820 service

	//ok, this is some other INT 15h service, so simply chain to the original
	//INT 15h handler

#ifdef __XMHF_VERIFICATION__
	//get IP and CS of the original INT 15h handler
	ip = nondet_u16();
	cs = nondet_u16();
#else
	//get IP and CS of the original INT 15h handler
	ip = *((u16 *)((hva_t)bdamemory + 4));
	cs = *((u16 *)((hva_t)bdamemory + 6));
#endif

	//update VMCS with the CS and IP and let go
	vcpu->vmcs.guest_RIP = ip;
	vcpu->vmcs.guest_CS_base = cs * 16;
	vcpu->vmcs.guest_CS_selector = cs;
}

static void _vmx_int1a_handleintercept(VCPU *vcpu, struct regs *r, uintptr_t OLD_AC){
	u16 cs, ip;
	u8 *bdamemory = (u8 *)(0x400 + OLD_AC);
	(void)r;

	//if in V86 mode translate the virtual address to physical address
	if( (vcpu->vmcs.guest_CR0 & CR0_PE) && (vcpu->vmcs.guest_CR0 & CR0_PG) &&
			(vcpu->vmcs.guest_RFLAGS & EFLAGS_VM) ){
		HALT_ON_ERRORCOND(0);
	}

	if ((r->eax & 0xff00) == 0xbb00) {
		if ((r->eax & 0xff) == 0x00) {
			// TCG_StatusCheck, to hide TPM return 0x23 (TCG_PC_TPM_NOT_PRESENT)
			if (0) {
				printf("\nTCG_StatusCheck, return 0x23");
				r->rax = 0;
				r->eax = 0x23U;
				r->rbx = 0;
				r->ebx = 0x41504354U;
				r->rcx = 0;
				r->rdx = 0;
				r->rsi = 0;
				r->rdi = 0;
			} else {
				printf("\nTCG_StatusCheck, no modify");
			}
		} else {
//			u16 *gueststackregion = (u16 *)( (hva_t)vcpu->vmcs.guest_SS_base + (u16)vcpu->vmcs.guest_RSP );
//			gueststackregion[2] |= (u16)EFLAGS_CF;
			printf("\nOther TCG BIOS calls");
		}
//		vcpu->vmcs.guest_RIP += vcpu->vmcs.info_vmexit_instruction_length;
//		return;
	}

	//get IP and CS of the original INT 15h handler
	ip = *((u16 *)((hva_t)bdamemory + 4));
	cs = *((u16 *)((hva_t)bdamemory + 6));

	//update VMCS with the CS and IP and let go
	vcpu->vmcs.guest_RIP = ip;
	vcpu->vmcs.guest_CS_base = cs * 16;
	vcpu->vmcs.guest_CS_selector = cs;
}

/*
static void _vmx_int_handleintercept(VCPU *vcpu, struct regs *r, uintptr_t OLD_AC){
	u16 cs, ip;
	u8 *bdamemory = (u8 *)(0x400 + OLD_AC);
	(void)r;

	//if in V86 mode translate the virtual address to physical address
	if( (vcpu->vmcs.guest_CR0 & CR0_PE) && (vcpu->vmcs.guest_CR0 & CR0_PG) &&
			(vcpu->vmcs.guest_RFLAGS & EFLAGS_VM) ){
		HALT_ON_ERRORCOND(0);
	}

	//get IP and CS of the original INT 15h handler
	ip = *((u16 *)((hva_t)bdamemory + 4));
	cs = *((u16 *)((hva_t)bdamemory + 6));

	//update VMCS with the CS and IP and let go
	vcpu->vmcs.guest_RIP = ip;
	vcpu->vmcs.guest_CS_base = cs * 16;
	vcpu->vmcs.guest_CS_selector = cs;
}
*/

//------------------------------------------------------------------------------
// guest MSR r/w intercept handling
// HAL invokes NT kernel via SYSENTER if CPU supports it. However,
// regular apps using NTDLL will still use INT 2E if registry entry is not
// tweaked. So, we HAVE to emulate SYSENTER_CS/EIP/ESP to ensure that
// NT kernel doesnt panic with SESSION5_INITIALIZATION_FAILED!
//
// This took me nearly a month of disassembly into the HAL,
// NTKERNEL and debugging to figure out..eh?
//
// AMD SVM is neater, only
// when you ask for these MSR intercepts do they get stored and read from
// the VMCB. However, for Intel regardless they get stored and read from VMCS
// for the guest. So we need to have these intercepts bare minimum!!
// A line to this effect would have been much appreciated in the Intel manuals
// doh!!!
//------------------------------------------------------------------------------

//---intercept handler (WRMSR)--------------------------------------------------
static void _vmx_handle_intercept_wrmsr(VCPU *vcpu, struct regs *r){
	//printf("\nCPU(0x%02x): WRMSR 0x%08x 0x%08x%08x @ %p", vcpu->id, r->ecx, r->edx, r->eax, vcpu->vmcs.guest_RIP);

	u64 write_data = ((u64)r->edx << 32) | (u64)r->eax;

	/* Disallow x2APIC MSRs */
	HALT_ON_ERRORCOND((r->ecx & 0xffffff00U) != 0x800);

	switch(r->ecx){
		case IA32_SYSENTER_CS_MSR:
			vcpu->vmcs.guest_SYSENTER_CS = (u32)write_data;
			break;
		case IA32_SYSENTER_EIP_MSR:
			vcpu->vmcs.guest_SYSENTER_EIP = (u64)write_data;
			break;
		case IA32_SYSENTER_ESP_MSR:
			vcpu->vmcs.guest_SYSENTER_ESP = (u64)write_data;
			break;
		case IA32_MSR_FS_BASE:
			vcpu->vmcs.guest_FS_base = (u64)write_data;
			break;
		case IA32_MSR_GS_BASE:
			vcpu->vmcs.guest_GS_base = (u64)write_data;
			break;
		case MSR_EFER: /* fallthrough */
		case MSR_IA32_PAT: /* fallthrough */
		case MSR_K6_STAR: {
			u32 found = 0;
			for (u32 i = 0; i < vcpu->vmcs.control_VM_entry_MSR_load_count; i++) {
				msr_entry_t *entry = &((msr_entry_t *)vcpu->vmx_vaddr_msr_area_guest)[i];
				if (entry->index == r->ecx) {
					entry->data = write_data;
					found = 1;
					break;
				}
			}
			HALT_ON_ERRORCOND(found != 0);
			break;
		}
		case IA32_MTRR_FIX64K_00000: /* fallthrough */
		case IA32_MTRR_FIX16K_80000: /* fallthrough */
		case IA32_MTRR_FIX16K_A0000: /* fallthrough */
		case IA32_MTRR_FIX4K_C0000: /* fallthrough */
		case IA32_MTRR_FIX4K_C8000: /* fallthrough */
		case IA32_MTRR_FIX4K_D0000: /* fallthrough */
		case IA32_MTRR_FIX4K_D8000: /* fallthrough */
		case IA32_MTRR_FIX4K_E0000: /* fallthrough */
		case IA32_MTRR_FIX4K_E8000: /* fallthrough */
		case IA32_MTRR_FIX4K_F0000: /* fallthrough */
		case IA32_MTRR_FIX4K_F8000: /* fallthrough */
		case IA32_MTRR_PHYSBASE0: /* fallthrough */
		case IA32_MTRR_PHYSMASK0: /* fallthrough */
		case IA32_MTRR_PHYSBASE1: /* fallthrough */
		case IA32_MTRR_PHYSMASK1: /* fallthrough */
		case IA32_MTRR_PHYSBASE2: /* fallthrough */
		case IA32_MTRR_PHYSMASK2: /* fallthrough */
		case IA32_MTRR_PHYSBASE3: /* fallthrough */
		case IA32_MTRR_PHYSMASK3: /* fallthrough */
		case IA32_MTRR_PHYSBASE4: /* fallthrough */
		case IA32_MTRR_PHYSMASK4: /* fallthrough */
		case IA32_MTRR_PHYSBASE5: /* fallthrough */
		case IA32_MTRR_PHYSMASK5: /* fallthrough */
		case IA32_MTRR_PHYSBASE6: /* fallthrough */
		case IA32_MTRR_PHYSMASK6: /* fallthrough */
		case IA32_MTRR_PHYSBASE7: /* fallthrough */
		case IA32_MTRR_PHYSMASK7: /* fallthrough */
		case IA32_MTRR_PHYSBASE8: /* fallthrough */
		case IA32_MTRR_PHYSMASK8: /* fallthrough */
		case IA32_MTRR_PHYSBASE9: /* fallthrough */
		case IA32_MTRR_PHYSMASK9:
			/*
			 * Need to change EPT to reflect MTRR changes, because host MTRRs
			 * are not used when EPT is used.
			 */
			{
				/*
				 * As a workaround, if writing the MSR will not cause change,
				 * do not halt. For example Windows 10 will do this.
				 */
				u32 eax, edx;
				rdmsr(r->ecx, &eax, &edx);
				if (eax == r->eax && edx == r->edx) {
					break;
				}
				printf("\nCPU(0x%02x): Old       MTRR 0x%08x is 0x %08x %08x",
						vcpu->id, r->ecx, edx, eax);
			}
			printf("\nCPU(0x%02x): Modifying MTRR 0x%08x to 0x %08x %08x",
					vcpu->id, r->ecx, r->edx, r->eax);
			printf("\nCPU(0x%02x): Modifying MTRR not yet supported. Halt!",
					vcpu->id);
			HALT();
			break;
		default:{
			asm volatile ("wrmsr\r\n"
          : //no outputs
          :"a"(r->eax), "c" (r->ecx), "d" (r->edx));
			break;
		}
	}

	vcpu->vmcs.guest_RIP += vcpu->vmcs.info_vmexit_instruction_length;
	//printf("\nCPU(0x%02x): WRMSR end", vcpu->id);
}

//---intercept handler (RDMSR)--------------------------------------------------
static void _vmx_handle_intercept_rdmsr(VCPU *vcpu, struct regs *r){
	//printf("\nCPU(0x%02x): RDMSR 0x%08x @ %p", vcpu->id, r->ecx, vcpu->vmcs.guest_RIP);

	/* After switch statement, will assign this value to r->eax and r->edx */
	u64 read_result = 0;

	/* Disallow x2APIC MSRs */
	HALT_ON_ERRORCOND((r->ecx & 0xffffff00U) != 0x800);

	switch(r->ecx){
		case IA32_SYSENTER_CS_MSR:
			read_result = (u64)vcpu->vmcs.guest_SYSENTER_CS;
			break;
		case IA32_SYSENTER_EIP_MSR:
			read_result = (u64)vcpu->vmcs.guest_SYSENTER_EIP;
			break;
		case IA32_SYSENTER_ESP_MSR:
			read_result = (u64)vcpu->vmcs.guest_SYSENTER_ESP;
			break;
		case IA32_MSR_FS_BASE:
			read_result = (u64)vcpu->vmcs.guest_FS_base;
			break;
		case IA32_MSR_GS_BASE:
			read_result = (u64)vcpu->vmcs.guest_GS_base;
			break;
		case MSR_EFER: /* fallthrough */
		case MSR_IA32_PAT: /* fallthrough */
		case MSR_K6_STAR: {
			u32 found = 0;
			for (u32 i = 0; i < vcpu->vmcs.control_VM_exit_MSR_store_count; i++) {
				msr_entry_t *entry = &((msr_entry_t *)vcpu->vmx_vaddr_msr_area_guest)[i];
				if (entry->index == r->ecx) {
					read_result = entry->data;
					found = 1;
					break;
				}
			}
			HALT_ON_ERRORCOND(found != 0);
			break;
		}
		default:{
			if (rdmsr_safe(r) != 0) {
				/* Inject a Hardware exception #GP */
				union {
					struct _vmx_event_injection st;
					uint32_t ui;
				} injection_info;
				injection_info.ui = 0;
				injection_info.st.vector = 0xd;     /* #GP */
				injection_info.st.type = 0x3;       /* Hardware Exception */
				injection_info.st.errorcode = 1;    /* Deliver error code */
				injection_info.st.valid = 1;
				vcpu->vmcs.control_VM_entry_interruption_information = injection_info.ui;
				vcpu->vmcs.control_VM_entry_exception_errorcode = 0;

				/* Do not increase guest RIP */
				return;
			}
			goto no_assign_read_result;
		}
	}

	/* Assign read_result to r->eax and r->edx */
	{
		r->rax = 0;	/* Clear upper 32-bits of RAX */
		r->rdx = 0;	/* Clear upper 32-bits of RDX */
		r->eax = (u32)(read_result);
		r->edx = (u32)(read_result >> 32);
	}

no_assign_read_result:
	vcpu->vmcs.guest_RIP += vcpu->vmcs.info_vmexit_instruction_length;

	//printf("\nCPU(0x%02x): RDMSR (0x%08x)=0x%08x%08x", vcpu->id, r->ecx, r->edx, r->eax);
}


//---intercept handler (EPT voilation)----------------------------------
static void _vmx_handle_intercept_eptviolation(VCPU *vcpu, struct regs *r){
	u32 errorcode;
	u64 gpa, gva;
	errorcode = (u32)vcpu->vmcs.info_exit_qualification;
	gpa = vcpu->vmcs.guest_paddr;
	gva = vcpu->vmcs.info_guest_linear_address;

	//check if EPT violation is due to LAPIC interception
	if(vcpu->isbsp && (gpa >= g_vmx_lapic_base) && (gpa < (g_vmx_lapic_base + PAGE_SIZE_4K)) ){
		xmhf_smpguest_arch_x86_64_eventhandler_hwpgtblviolation(vcpu, (u32)gpa, errorcode);
	}else{ //no, pass it to hypapp
		xmhf_smpguest_arch_x86_64vmx_quiesce(vcpu);
		xmhf_app_handleintercept_hwpgtblviolation(vcpu, r, gpa, gva,
				(errorcode & 7));
		xmhf_smpguest_arch_x86_64vmx_endquiesce(vcpu);
	}
}


//---intercept handler (I/O port access)----------------------------------------
static void _vmx_handle_intercept_ioportaccess(VCPU *vcpu, struct regs *r){
  u32 access_size, access_type, portnum, stringio;
	u32 app_ret_status = APP_IOINTERCEPT_CHAIN;

  access_size = (u32)vcpu->vmcs.info_exit_qualification & 0x00000007UL;
	access_type = ((u32)vcpu->vmcs.info_exit_qualification & 0x00000008UL) >> 3;
	portnum =  ((u32)vcpu->vmcs.info_exit_qualification & 0xFFFF0000UL) >> 16;
	stringio = ((u32)vcpu->vmcs.info_exit_qualification & 0x00000010UL) >> 4;

  HALT_ON_ERRORCOND(!stringio);	//we dont handle string IO intercepts

  //call our app handler, TODO: it should be possible for an app to
  //NOT want a callback by setting up some parameters during appmain
	xmhf_smpguest_arch_x86_64vmx_quiesce(vcpu);
	app_ret_status=xmhf_app_handleintercept_portaccess(vcpu, r, portnum, access_type,
          access_size);
    xmhf_smpguest_arch_x86_64vmx_endquiesce(vcpu);

  if(app_ret_status == APP_IOINTERCEPT_CHAIN){
   	if(access_type == IO_TYPE_OUT){
  		if( access_size== IO_SIZE_BYTE)
  				outb((u8)r->eax, portnum);
  		else if (access_size == IO_SIZE_WORD)
  				outw((u16)r->eax, portnum);
  		else if (access_size == IO_SIZE_DWORD)
  				outl((u32)r->eax, portnum);
  	}else{
  		if( access_size== IO_SIZE_BYTE){
  				r->eax &= 0xFFFFFF00UL;	//clear lower 8 bits
  				r->eax |= (u8)inb(portnum);
  		}else if (access_size == IO_SIZE_WORD){
  				r->eax &= 0xFFFF0000UL;	//clear lower 16 bits
  				r->eax |= (u16)inw(portnum);
  		}else if (access_size == IO_SIZE_DWORD){
  				r->eax = (u32)inl(portnum);
  		}
  	}
  	vcpu->vmcs.guest_RIP += vcpu->vmcs.info_vmexit_instruction_length;

  }else{
    //skip the IO instruction, app has taken care of it
  	vcpu->vmcs.guest_RIP += vcpu->vmcs.info_vmexit_instruction_length;
  }

	return;
}


//---CR0 access handler-------------------------------------------------
static void vmx_handle_intercept_cr0access_ug(VCPU *vcpu, struct regs *r, u32 gpr, u32 tofrom){
	u64 cr0_value, old_cr0;
	u64 fixed_1_fields;

	HALT_ON_ERRORCOND(tofrom == VMX_CRX_ACCESS_TO);

	cr0_value = *((u64 *)_vmx_decode_reg(gpr, vcpu, r));
	old_cr0 = vcpu->vmcs.guest_CR0;

	//printf("\n[cr0-%02x] MOV TO, old=0x%08llx, new=0x%08llx, shadow=0x%08llx",
	//	vcpu->id, old_cr0, cr0_value, vcpu->vmcs.control_CR0_shadow);

	/*
	 * Make the guest think that move to CR0 succeeds (by changing shadow).
	 *
	 * control_CR0_mask is set in vmx_initunrestrictedguestVMCS(). The mask
	 * is the IA32_VMX_CR0_FIXED0 MSR, with PE and PG unset, CD and NW set.
	 *
	 * Intel manual says that exceptions to fixed CR0 are CD NW PG PE.
	 *
	 * So for all set bits in control_CR0_mask, we force CD and NW to be not
	 * set, and other bits to be set.
	 */

	fixed_1_fields = vcpu->vmcs.control_CR0_mask;
	vcpu->vmcs.control_CR0_shadow = cr0_value;
	vcpu->vmcs.guest_CR0 = (cr0_value | fixed_1_fields) & ~(CR0_CD | CR0_NW);

	/*
	 * If CR0.PG bit changes, need to update guest_PDPTE0 - guest_PDPTE3 if
	 * PAE is enabled.
	 *
	 * To support x86-64 guests, also need to update bit 9 of VM-Entry Controls
	 * (IA-32e mode guest). This bit should always equal to EFER.LME && CR0.PG
	 */
	if ((old_cr0 ^ cr0_value) & CR0_PG) {
		u32 value = vcpu->vmcs.control_VM_entry_controls;
		u32 lme, pae;
		msr_entry_t *efer = &((msr_entry_t *)vcpu->vmx_vaddr_msr_area_guest)[0];
		HALT_ON_ERRORCOND(efer->index == MSR_EFER);
		lme = (cr0_value & CR0_PG) && (efer->data & (0x1U << EFER_LME));
		value &= ~(1U << 9);
		value |= lme << 9;
		vcpu->vmcs.control_VM_entry_controls = value;

		pae = (cr0_value & CR0_PG) && (!lme) && (vcpu->vmcs.guest_CR4 & CR4_PAE);
		/* TODO: Need to walk EPT and retrieve values for guest_PDPTE* */
		//HALT_ON_ERRORCOND(!pae);
		/*
		 * XXX: SECURITY: should walk EPT, otherwise a malicious guest OS can
		 * use this to access memory it do not have access to.
		 */
		if (pae) {
			u64 *pdptes = (u64 *)(uintptr_t)(vcpu->vmcs.guest_CR3 & ~0x1FUL);
			vcpu->vmcs.guest_PDPTE0 = pdptes[0];
			vcpu->vmcs.guest_PDPTE1 = pdptes[1];
			vcpu->vmcs.guest_PDPTE2 = pdptes[2];
			vcpu->vmcs.guest_PDPTE3 = pdptes[3];
		}
	}

	//flush mappings
	xmhf_memprot_arch_x86_64vmx_flushmappings(vcpu);
}

//---CR4 access handler---------------------------------------------------------
static void vmx_handle_intercept_cr4access_ug(VCPU *vcpu, struct regs *r, u32 gpr, u32 tofrom){
  if(tofrom == VMX_CRX_ACCESS_TO){
	u64 cr4_proposed_value;

	cr4_proposed_value = *((u64 *)_vmx_decode_reg(gpr, vcpu, r));

	printf("\nCPU(0x%02x): CS:EIP=0x%04x:0x%08x MOV CR4, xx", vcpu->id,
		(u16)vcpu->vmcs.guest_CS_selector, (u32)vcpu->vmcs.guest_RIP);

	printf("\nMOV TO CR4 (flush TLB?), current=0x%08x, proposed=0x%08x",
			(u32)vcpu->vmcs.guest_CR4, cr4_proposed_value);

	/*
	 * CR4 mask is the IA32_VMX_CR4_FIXED0 MSR. Modify CR4 shadow to let the
	 * guest think MOV CR4 succeeds.
	 */
	vcpu->vmcs.control_CR4_shadow = cr4_proposed_value;
	vcpu->vmcs.guest_CR4 = (cr4_proposed_value | vcpu->vmcs.control_CR4_mask);

	#if defined (__NESTED_PAGING__)
	//we need to flush EPT mappings as we emulated CR4 load above
	__vmx_invvpid(VMX_INVVPID_SINGLECONTEXT, 1, 0);
	#endif
  }

}

//---XSETBV intercept handler-------------------------------------------
static void _vmx_handle_intercept_xsetbv(VCPU *vcpu, struct regs *r){
	u64 xcr_value;

	xcr_value = ((u64)r->edx << 32) + (u64)r->eax;

	if(r->ecx != XCR_XFEATURE_ENABLED_MASK){
			printf("\n%s: unhandled XCR register %u", __FUNCTION__, r->ecx);
			HALT();
	}

	//XXX: TODO: check for invalid states and inject GP accordingly

	printf("\n%s: xcr_value=%llx", __FUNCTION__, xcr_value);

	//XXX: debug: dump CR4 contents
	{
		u32 t_cr4;
		t_cr4 = read_cr4();
		printf("\n%s: host cr4 value=%08x", __FUNCTION__, t_cr4);
	}

	//set XCR with supplied value
	xsetbv(XCR_XFEATURE_ENABLED_MASK, xcr_value);

	//skip the emulated XSETBV instruction
	vcpu->vmcs.guest_RIP += vcpu->vmcs.info_vmexit_instruction_length;
}


#define INT3 ((u8)0xcc)
#define MAX_BP 10
#define ENABLE_MONITOR_TRAP do { \
	vcpu->vmcs.control_VMX_cpu_based |= (1 << 27); \
	} while(0)
#define DISABLE_MONITOR_TRAP do { \
	vcpu->vmcs.control_VMX_cpu_based &= ~(1 << 27); \
	} while(0)

struct bp_info {
	u8 valid;
	u8 old;
	u16 cs;
	u64 rip;
} bps[MAX_BP] = {};

static void set_breakpoint(u16 cs, u64 rip) {
	u64 addr = (u64)cs * 16 + rip;
	u8 *ptr = (u8 *)addr;
	int i;
	if (*ptr == INT3) {
		/* re-set break point, make sure break point exists and do nothing */
		for (i = 0; i < MAX_BP; i++) {
			if (bps[i].valid && bps[i].cs == cs && bps[i].rip == rip) {
				break;
			}
		}
		HALT_ON_ERRORCOND(i < MAX_BP);
	} else {
		for (i = 0; i < MAX_BP; i++) {
			if (!bps[i].valid) {
				break;
			}
		}
		HALT_ON_ERRORCOND(i < MAX_BP);	/* overflow of bps */
		bps[i].valid = 1;
		bps[i].old = *ptr;
		bps[i].cs = cs;
		bps[i].rip = rip;
		*ptr = INT3;
	}
}

static void hit_breakpoint(u16 cs, u64 rip) {
	u64 addr = (u64)cs * 16 + rip;
	u8 *ptr = (u8 *)addr;
	int i;
	HALT_ON_ERRORCOND(*ptr == INT3);
	for (i = 0; i < MAX_BP; i++) {
		if (bps[i].valid && bps[i].cs == cs && bps[i].rip == rip) {
			break;
		}
	}
	HALT_ON_ERRORCOND(i < MAX_BP);	/* overflow of bps */
	bps[i].valid = 0;
	*ptr = bps[i].old;
}

static void xxd(u32 start, u32 end) {
	HALT_ON_ERRORCOND((start & 0xf) == 0);
	HALT_ON_ERRORCOND((end & 0xf) == 0);
	for (u32 i = start; i < end; i += 0x10) {
		printf("\n%08x: ", i);
		for (u32 j = 0; j < 0x10; j++) {
			if (j & 1) {
				printf("%02x", (unsigned)*(unsigned char*)(uintptr_t)(i + j));
			} else {
				printf(" %02x", (unsigned)*(unsigned char*)(uintptr_t)(i + j));
			}
		}
	}
}

static void handle_entry1(VCPU *vcpu, struct regs *r, u16 cs, u64 rip) {
	(void)vcpu;(void)r;(void)cs;(void)rip;
	// set_breakpoint(0x0, 0x7c00);
}

static void handle_entry21(VCPU *vcpu, struct regs *r, u16 cs, u64 rip) {
	(void)vcpu;(void)r;(void)cs;(void)rip;
	set_breakpoint(0x7c0, 0x118);
	set_breakpoint(0x7c0, 0x16a);
	printf("\nWBINVD");
	wbinvd();
}

static void handle_entry22(VCPU *vcpu, struct regs *r, u16 cs, u64 rip) {
	(void)vcpu;(void)r;(void)cs;(void)rip;
	// ENABLE_MONITOR_TRAP;
	// set_breakpoint(0x7c0, 0x1068);
	printf("\nWBINVD");
	wbinvd();
}

static void handle_monitor_trap(VCPU *vcpu, struct regs *r, u16 cs, u64 rip) {
	(void)vcpu;
	(void)r;
	/* Skip timer intercepts */
	if (cs == 0xf000) {
		u16 timer_rips[] = {0xfea8, 0xfeaa, 0xfeab, 0xfeac, 0xfead, 0xfeb0,
							0xfeb2, 0xfeb6, 0xfeb8, 0xfebe, 0xfecb, 0xfecf,
							0xfed4, 0xfed6, 0xfeda, 0xfedc, 0xfede, 0xfee1,
							0xfee2, 0xfee7, 0xfee9, 0xfeeb, 0xfeed, 0xfeee,
							0xfeef, 0xfef0, 0xfef2, 0xff53};
		for (u32 i = 0; i < sizeof(timer_rips) / sizeof(timer_rips[0]); i++) {
			if (rip == timer_rips[i]) {
				return;
			}
		}
	}
	printf("\nMT%x: 0x%04x:0x%04llx ECX=0x%08x EDI=0x%08x ESI=0x%08x *0x20000=0x%016llx *0x30000=0x%016llx",
			vcpu->id, cs, rip, r->ecx, r->edi, r->esi,
			*(u64 *)0x20000, *(u64 *)0x30000);
//	printf("\nMT%x: 0x%04x:0x%04llx ECX=0x%08x EAX=0x%08x EBX=0x%08x",
//			vcpu->id, cs, rip, r->ecx, r->eax, r->ebx);
	switch ((cs << 16) | rip) {
	case 0x07c00e88:
		printf(" DS=0x%04x ES=0x%04x", vcpu->vmcs.guest_DS_selector, vcpu->vmcs.guest_ES_selector);
		DISABLE_MONITOR_TRAP;
		// set_breakpoint(0x7c0, 0xe9d);
		set_breakpoint(0x7c0, 0xea2);
		break;
	case 0x07c00145:
		DISABLE_MONITOR_TRAP;
		set_breakpoint(0x7c0, 0x143);
		set_breakpoint(0x7c0, 0x165);
		printf(" DX=0x%04x DS=0x%04x",
				(u32)(u16)r->edx, (u32)(u16)vcpu->vmcs.guest_DS_selector);
		{
			u64 dssi = (u64)((u16)vcpu->vmcs.guest_DS_selector) * 16 + (u16)r->esi;
			printf(" *0x%05llx=0x%016llx *0x%05llx=0x%016llx",
					dssi, ((u64*)dssi)[0], dssi + 8, ((u64*)dssi)[1]);
		}
		break;
//	case 0x07c00ea0:
//		DISABLE_MONITOR_TRAP;
//		set_breakpoint(0x7c0, 0xe9d);
//		break;
	default:
		/* nop */
		break;
	}
}

static void handle_breakpoint_hit(VCPU *vcpu, struct regs *r, u16 cs, u64 rip) {
	(void)vcpu;
	(void)r;
//	printf("\nBP%x: 0x%04x:0x%04llx ECX=0x%08x EAX=0x%08x EBX=0x%08x",
//			vcpu->id, cs, rip, r->ecx, r->eax, r->ebx);
	printf("\nBP%x: 0x%04x:0x%04llx ECX=0x%08x EDI=0x%08x ESI=0x%08x",
			vcpu->id, cs, rip, r->ecx, r->edi, r->esi);
	switch ((cs << 16) | rip) {
	case 0x07c01068:
		if ("nop ef8") {
			*(char *)(0x7c00 + 0xef8) = 0xc3;	// ret;
			printf("\nNOP 0x07c0:0x0ef8 !");
		}
		if ("dump IVT") {
			for (u32 i = 0; i < 32; i++) {
				printf("\n*0x%04x = 0x%08x", i * 4, *(u32*)(uintptr_t)(i * 4));
			}
		}
		if (!"break IVT") {
			set_breakpoint(0xf000, 0x2aef);
			set_breakpoint(0xf000, 0x2aef);
			set_breakpoint(0xf000, 0xe2c3);
			set_breakpoint(0xf000, 0x2aef);
			set_breakpoint(0xf000, 0x2aef);
			set_breakpoint(0xf000, 0xff54);
			set_breakpoint(0xf000, 0xf002);
			set_breakpoint(0xf000, 0x2aef);
			// set_breakpoint(0xf000, 0xfea8);
			set_breakpoint(0xf000, 0xe987);
			set_breakpoint(0xf000, 0x2aef);
			set_breakpoint(0xf000, 0x2aef);
			set_breakpoint(0xf000, 0x2aef);
			set_breakpoint(0xf000, 0x2aef);
			set_breakpoint(0xf000, 0x2aef);
			set_breakpoint(0xf000, 0x2aef);
			set_breakpoint(0xc000, 0x0014);
			set_breakpoint(0xf000, 0xf84d);
			set_breakpoint(0xf000, 0xf841);
			set_breakpoint(0xf000, 0xe9f3);
			set_breakpoint(0xf000, 0xe739);
			// set_breakpoint(0x0040, 0x00ac);
			set_breakpoint(0xf000, 0xe82e);
			set_breakpoint(0xf000, 0xefd2);
			set_breakpoint(0xf000, 0xffbe);
			set_breakpoint(0xf000, 0xe6f2);
			// set_breakpoint(0x0040, 0x00cc);
			set_breakpoint(0xf000, 0xff53);
			set_breakpoint(0xf000, 0xff53);
			set_breakpoint(0x0000, 0x0000);
			set_breakpoint(0xf000, 0xefc7);
			set_breakpoint(0xc000, 0x9f31);
		}
		if (!"dump BIOS") {
			printf("\nStart dump BIOS");
			xxd(0x0f0000, 0x100000);
			printf("\nEnd dump BIOS");
		}
		if ("wbinvd") {
			printf("\nWBINVD");
			wbinvd();
		}
		if (!"dump bootmgr") {
			printf("\nStart dump bootmgr heads2");
			for (u32 i = 0x20000; i < 0x85000; i += 0x8000) {
				xxd(i + 0x100, i + 0x200);
			}
			printf("\nEnd dump bootmgr heads2");
			printf("\nStart dump bootmgr heads1");
			for (u32 i = 0x20000; i < 0x85000; i += 0x8000) {
				xxd(i, i + 0x100);
			}
			printf("\nEnd dump bootmgr heads1");
			printf("\nStart dump bootmgr");
			xxd(0x20000, 0x20000 + 0x65160);
			printf("\nEnd dump bootmgr");
		}

		ENABLE_MONITOR_TRAP;
		vcpu->vmcs.control_exception_bitmap |= 0xffffffff;
		break;
	case 0x07c0016a:
		HALT_ON_ERRORCOND(0);	/* See strange error */
		break;
	default:
		ENABLE_MONITOR_TRAP;
		break;
	}
}

//---hvm_intercept_handler------------------------------------------------------
u32 xmhf_parteventhub_arch_x86_64vmx_intercept_handler(VCPU *vcpu, struct regs *r){
	//read VMCS from physical CPU/core
#ifndef __XMHF_VERIFICATION__
	xmhf_baseplatform_arch_x86_64vmx_getVMCS(vcpu);
#endif //__XMHF_VERIFICATION__
	//sanity check for VM-entry errors
	if( (u32)vcpu->vmcs.info_vmexit_reason & 0x80000000UL ){
		printf("\nCPU(0x%02x): VM-ENTRY error, reason=0x%08x, qualification=0x%016llx",
			vcpu->id, (u32)vcpu->vmcs.info_vmexit_reason,
			(u64)vcpu->vmcs.info_exit_qualification);
		xmhf_baseplatform_arch_x86_64vmx_dumpVMCS(vcpu);
		HALT();
	}

	if (vcpu->vmcs.info_vmexit_reason == 37) {
		// monitor trap
		handle_monitor_trap(vcpu, r, vcpu->vmcs.guest_CS_selector, vcpu->vmcs.guest_RIP);
	} else if (vcpu->vmcs.info_vmexit_reason == 0 && ((u32)vcpu->vmcs.info_vmexit_interrupt_information & INTR_INFO_VECTOR_MASK) == 3) {
		/* Do nothing */;
	} else {
		if (vcpu->vmcs.info_vmexit_reason == VMX_VMEXIT_INIT) {
			wbinvd();
		}
		printf("\nCPU(0x%02x): Intercept %d @ 0x%04x:0x%08llx", vcpu->id, vcpu->vmcs.info_vmexit_reason, vcpu->vmcs.guest_CS_selector, vcpu->vmcs.guest_RIP);
		printf(" *0x20000=0x%016llx *0x30000=0x%016llx",
				*(u64 *)0x20000, *(u64 *)0x30000);
	}
	if (vcpu->vmcs.info_vmexit_reason == 10) {
		/* CPUID */
		printf(" CPUID 0x%08lx", r->eax);
	}
	if (vcpu->vmcs.info_vmexit_reason == 18) {
		u16 *rsp = (u16 *)( (hva_t)vcpu->vmcs.guest_SS_base + (u16)vcpu->vmcs.guest_RSP );
		printf(" VMCALL CS:IP=0x%04x:0x%04x EFLAGS=0x%04x",
				(unsigned)rsp[1], (unsigned)rsp[0], (unsigned)rsp[2]);
		printf(" EAX=0x%08x EBX=0x%08x ECX=0x%08x EDX=0x%08x",
				r->eax, r->ebx, r->ecx, r->edx);
		if ((r->eax & 0xffffU) == 0x2400U) {
			handle_entry1(vcpu, r, vcpu->vmcs.guest_CS_selector, vcpu->vmcs.guest_RIP);
		}
		if ((r->eax & 0xffffU) == 0xbb00U) {
			static int count = 0;
			count++;
			if (count == 1) {
				handle_entry21(vcpu, r, vcpu->vmcs.guest_CS_selector, vcpu->vmcs.guest_RIP);
			}
			if (count == 2) {
				handle_entry22(vcpu, r, vcpu->vmcs.guest_CS_selector, vcpu->vmcs.guest_RIP);
			}
		}
	}

	/*
	 * Cannot print anything before event handler returns if this intercept
	 * is for quiescing (vcpu->vmcs.info_vmexit_reason == VMX_VMEXIT_EXCEPTION),
	 * otherwise will deadlock. See xmhf_smpguest_arch_x86_64vmx_quiesce().
	 */

	//handle intercepts
	switch((u32)vcpu->vmcs.info_vmexit_reason){
		//--------------------------------------------------------------
		//xmhf-core and hypapp intercepts
		//--------------------------------------------------------------

#define CAPTURE_BIOS1A(OLD_AC, OLD_54) \
	else if (vcpu->vmcs.guest_CS_base == 0x400 && \
			vcpu->vmcs.guest_RIP == OLD_AC) { \
		printf(" INT=0x%02x", (unsigned)OLD_54 / 4); \
		_vmx_int1a_handleintercept(vcpu, r, OLD_AC); \
	}

#define CAPTURE_BIOS(OLD_AC, OLD_54) \
	else if (vcpu->vmcs.guest_CS_base == 0x400 && \
			vcpu->vmcs.guest_RIP == OLD_AC) { \
		printf(" INT=0x%02x", (unsigned)OLD_54 / 4); \
		_vmx_int_handleintercept(vcpu, r, OLD_AC); \
	}

		case VMX_VMEXIT_VMCALL:{
			//if INT 15h E820 hypercall, then let the xmhf-core handle it
			if(vcpu->vmcs.guest_CS_base == (VMX_UG_E820HOOK_CS << 4) &&
				vcpu->vmcs.guest_RIP == VMX_UG_E820HOOK_IP){
				//we need to be either in real-mode or in protected
				//mode with paging and EFLAGS.VM bit set (virtual-8086 mode)
				HALT_ON_ERRORCOND( !(vcpu->vmcs.guest_CR0 & CR0_PE)  ||
					( (vcpu->vmcs.guest_CR0 & CR0_PE) && (vcpu->vmcs.guest_CR0 & CR0_PG) &&
						(vcpu->vmcs.guest_RFLAGS & EFLAGS_VM)  ) );
				_vmx_int15_handleintercept(vcpu, r);
			}
			CAPTURE_BIOS1A(0xcc, 0x68)
//			CAPTURE_BIOS(0xbc, 0x50)
//			CAPTURE_BIOS(0xc4, 0x58)
//			CAPTURE_BIOS(0xcc, 0x5c)
//			CAPTURE_BIOS(0xd4, 0x60)
//			CAPTURE_BIOS(0xdc, 0x64)
//			CAPTURE_BIOS(0xe4, 0x6c)
//			CAPTURE_BIOS(0xec, 0x70)
//			CAPTURE_BIOS(0xf4, 0x74)
//			CAPTURE_BIOS(0xfc, 0x78)
			else{	//if not E820 hook, give hypapp a chance to handle the hypercall
				// Simulate handler for KVM_HC_VAPIC_POLL_IRQ
				HALT_ON_ERRORCOND(r->eax == 1);
				r->eax = 0;
if (0) {
				xmhf_smpguest_arch_x86_64vmx_quiesce(vcpu);
				if( xmhf_app_handlehypercall(vcpu, r) != APP_SUCCESS){
					printf("\nCPU(0x%02x): error(halt), unhandled hypercall 0x%08x!", vcpu->id, r->eax);
					HALT();
				}
				xmhf_smpguest_arch_x86_64vmx_endquiesce(vcpu);
}
				vcpu->vmcs.guest_RIP += vcpu->vmcs.info_vmexit_instruction_length;
			}
		}
		break;

		case VMX_VMEXIT_IOIO:{
			_vmx_handle_intercept_ioportaccess(vcpu, r);
		}
		break;

		case VMX_VMEXIT_EPT_VIOLATION:{
			_vmx_handle_intercept_eptviolation(vcpu, r);
		}
		break;

		case VMX_VMEXIT_INIT:{
			printf("\n***** VMEXIT_INIT xmhf_app_handleshutdown\n");
			xmhf_app_handleshutdown(vcpu, r);
			printf("\nCPU(0x%02x): Fatal, xmhf_app_handleshutdown returned. Halting!", vcpu->id);
			HALT();
		}
		break;

		//--------------------------------------------------------------
		//xmhf-core only intercepts
		//--------------------------------------------------------------
		case VMX_VMEXIT_HLT:
			if(!vcpu->vmx_guest_unrestricted){
				printf("\nCPU(0x%02x): V86 monitor based real-mode exec. unsupported!", vcpu->id);
				HALT();
			}else{
				printf("\nCPU(0x%02x): Unexpected HLT intercept in UG, Halting!", vcpu->id);
				HALT();
			}
		break;

 		case VMX_VMEXIT_EXCEPTION:{
			switch( ((u32)vcpu->vmcs.info_vmexit_interrupt_information & INTR_INFO_VECTOR_MASK) ){
				case 0x01:
					xmhf_smpguest_arch_x86_64_eventhandler_dbexception(vcpu, r);
					break;

				case 0x02:	//NMI
					#ifndef __XMHF_VERIFICATION__
					//we currently discharge quiescing via manual inspection
					xmhf_smpguest_arch_x86_64vmx_eventhandler_nmiexception(vcpu, r, 1);
					#endif // __XMHF_VERIFICATION__
					break;

				case 0x03:	// INT3
					/* vector = 3, software interrupt, valid */
					HALT_ON_ERRORCOND(vcpu->vmcs.info_vmexit_interrupt_information == 0x80000603);
					hit_breakpoint(vcpu->vmcs.guest_CS_selector, vcpu->vmcs.guest_RIP);
					handle_breakpoint_hit(vcpu, r, vcpu->vmcs.guest_CS_selector, vcpu->vmcs.guest_RIP);
					break;

				default:
					printf("\nVMEXIT-EXCEPTION:");
					printf("\ncontrol_exception_bitmap=0x%08lx",
						(unsigned long)vcpu->vmcs.control_exception_bitmap);
					printf("\ninterruption information=0x%08lx",
						(unsigned long)vcpu->vmcs.info_vmexit_interrupt_information);
					printf("\nerrorcode=0x%08lx",
						(unsigned long)vcpu->vmcs.info_vmexit_interrupt_error_code);
					HALT();
			}
		}
		break;

		case VMX_VMEXIT_NMI_WINDOW: {
			/* Clear NMI windowing */
			vcpu->vmcs.control_VMX_cpu_based &= ~(1U << 22);
			/* Check whether the CPU can handle NMI */
			if (vcpu->vmcs.control_exception_bitmap & CPU_EXCEPTION_NMI) {
				/*
				 * TODO: hypapp has chosen to intercept NMI so callback.
				 * Currently not implemented, so drop the NMI exception.
				 */
				printf("\nCPU(0x%02x): drop NMI", vcpu->id);
			} else {
				/* Inject NMI to guest */
				vcpu->vmcs.control_VM_entry_exception_errorcode = 0;
				vcpu->vmcs.control_VM_entry_interruption_information = NMI_VECTOR |
					INTR_TYPE_NMI |
					INTR_INFO_VALID_MASK;
				printf("\nCPU(0x%02x): inject NMI", vcpu->id);
			}
		}
		break;

 		case VMX_VMEXIT_CRX_ACCESS:{
			u32 tofrom, gpr, crx;
			//printf("\nVMEXIT_CRX_ACCESS:");
			//printf("\ninstruction length: %u", info_vmexit_instruction_length);
			crx=(u32) ((u64)vcpu->vmcs.info_exit_qualification & 0x000000000000000FULL);
			gpr=
			 (u32) (((u64)vcpu->vmcs.info_exit_qualification & 0x0000000000000F00ULL) >> (u64)8);
			tofrom =
			(u32) (((u64)vcpu->vmcs.info_exit_qualification & 0x0000000000000030ULL) >> (u64)4);
			//printf("\ncrx=%u, gpr=%u, tofrom=%u", crx, gpr, tofrom);

			if ( ((int)gpr >=0) && ((int)gpr <= 15) ){
				switch(crx){
					case 0x0: //CR0 access
						vmx_handle_intercept_cr0access_ug(vcpu, r, gpr, tofrom);
						break;

					case 0x4: //CR4 access
						if(!vcpu->vmx_guest_unrestricted){
							printf("\nHALT: v86 monitor based real-mode exec. unsupported!");
							HALT();
						}else{
							vmx_handle_intercept_cr4access_ug(vcpu, r, gpr, tofrom);
						}
						break;

					default:
						printf("\nunhandled crx, halting!");
						HALT();
				}
				vcpu->vmcs.guest_RIP += vcpu->vmcs.info_vmexit_instruction_length;
			}else{
				printf("\n[%02x]%s: invalid gpr value (%u). halting!", vcpu->id,
					__FUNCTION__, gpr);
				HALT();
			}
		}
		break;

 		case VMX_VMEXIT_RDMSR:
			_vmx_handle_intercept_rdmsr(vcpu, r);
			break;

		case VMX_VMEXIT_WRMSR:
			_vmx_handle_intercept_wrmsr(vcpu, r);
			break;

		case VMX_VMEXIT_CPUID:
			_vmx_handle_intercept_cpuid(vcpu, r);
			break;

		case VMX_VMEXIT_TASKSWITCH:{
			u32 idt_v = vcpu->vmcs.info_IDT_vectoring_information & VECTORING_INFO_VALID_MASK;
			u32 type = vcpu->vmcs.info_IDT_vectoring_information & VECTORING_INFO_TYPE_MASK;
			u32 reason = vcpu->vmcs.info_exit_qualification >> 30;
			u16 tss_selector = (u16)vcpu->vmcs.info_exit_qualification;

			if(reason == TASK_SWITCH_GATE && type == INTR_TYPE_NMI){
				printf("\nCPU(0x%02x): NMI received (MP guest shutdown?)", vcpu->id);
					xmhf_app_handleshutdown(vcpu, r);
				printf("\nCPU(0x%02x): warning, xmhf_app_handleshutdown returned!", vcpu->id);
				printf("\nCPU(0x%02x): HALTING!", vcpu->id);
				HALT();
			}else{
				printf("\nCPU(0x%02x): Unhandled Task Switch. Halt!", vcpu->id);
				printf("\n	idt_v=0x%08x, type=0x%08x, reason=0x%08x, tsssel=0x%04x",
					idt_v, type, reason, tss_selector);
			}
			HALT();
		}
		break;

		case VMX_VMEXIT_XSETBV:{
			_vmx_handle_intercept_xsetbv(vcpu, r);
		}
		break;

		case 37: {
			// monitor trap flag
			// printf(" monitor trap");
		}
		break;

		case 52: {
			// preemption timer
			printf(" preemption timer 0x%08x",
					(u32)vcpu->vmcs.preemption_timer_value);
		}
		break;

		default:{
			if (vcpu->vmcs.control_VM_entry_controls & (1U << 9)) {
				/* x86-64 mode */
				printf("\nCPU(0x%02x): Unhandled intercept in long mode: %d (0x%08x)",
						vcpu->id, (u32)vcpu->vmcs.info_vmexit_reason,
						(u32)vcpu->vmcs.info_vmexit_reason);
				printf("\n	CPU(0x%02x): RFLAGS=0x%016llx",
						vcpu->id, vcpu->vmcs.guest_RFLAGS);
				printf("\n	SS:RSP =0x%04x:0x%016llx",
						(u16)vcpu->vmcs.guest_SS_selector,
						vcpu->vmcs.guest_RSP);
				printf("\n	CS:RIP =0x%04x:0x%016llx",
						(u16)vcpu->vmcs.guest_CS_selector,
						vcpu->vmcs.guest_RIP);
				printf("\n	IDTR base:limit=0x%016llx:0x%04x",
						vcpu->vmcs.guest_IDTR_base,
						(u16)vcpu->vmcs.guest_IDTR_limit);
				printf("\n	GDTR base:limit=0x%016llx:0x%04x",
						vcpu->vmcs.guest_GDTR_base,
						(u16)vcpu->vmcs.guest_GDTR_limit);
				if(vcpu->vmcs.info_IDT_vectoring_information & 0x80000000){
					printf("\nCPU(0x%02x): HALT; Nested events unhandled 0x%08x",
						vcpu->id, vcpu->vmcs.info_IDT_vectoring_information);
				}
			} else {
				/* x86 mode */
				printf("\nCPU(0x%02x): Unhandled intercept: %d (0x%08x)",
						vcpu->id, (u32)vcpu->vmcs.info_vmexit_reason,
						(u32)vcpu->vmcs.info_vmexit_reason);
				printf("\n	CPU(0x%02x): EFLAGS=0x%08x",
						vcpu->id, (u32)vcpu->vmcs.guest_RFLAGS);
				printf("\n	SS:ESP =0x%04x:0x%08x",
						(u16)vcpu->vmcs.guest_SS_selector,
						(u32)vcpu->vmcs.guest_RSP);
				printf("\n	CS:EIP =0x%04x:0x%08x",
						(u16)vcpu->vmcs.guest_CS_selector,
						(u32)vcpu->vmcs.guest_RIP);
				printf("\n	IDTR base:limit=0x%08x:0x%04x",
						(u32)vcpu->vmcs.guest_IDTR_base,
						(u16)vcpu->vmcs.guest_IDTR_limit);
				printf("\n	GDTR base:limit=0x%08x:0x%04x",
						(u32)vcpu->vmcs.guest_GDTR_base,
						(u16)vcpu->vmcs.guest_GDTR_limit);
				if(vcpu->vmcs.info_IDT_vectoring_information & 0x80000000){
					printf("\nCPU(0x%02x): HALT; Nested events unhandled 0x%08x",
						vcpu->id, vcpu->vmcs.info_IDT_vectoring_information);
				}
			}
			HALT();
		}
	} //end switch((u32)vcpu->vmcs.info_vmexit_reason)


	/*
	 * Check and clear guest interruptibility state.
	 * However, ignore bit 3, because it is for virtual NMI.
	 */
	if ((vcpu->vmcs.guest_interruptibility & ~(1U << 3)) != 0){
		vcpu->vmcs.guest_interruptibility &= (1U << 3);
	}

	//make sure we have no nested events
	if(vcpu->vmcs.info_IDT_vectoring_information & 0x80000000){
		printf("\nCPU(0x%02x): HALT; Nested events unhandled with hwp:0x%08x",
			vcpu->id, vcpu->vmcs.info_IDT_vectoring_information);
		HALT();
	}

	//write updated VMCS back to CPU
#ifndef __XMHF_VERIFICATION__
	xmhf_baseplatform_arch_x86_64vmx_putVMCS(vcpu);
#endif // __XMHF_VERIFICATION__


#ifdef __XMHF_VERIFICATION_DRIVEASSERTS__
	//ensure that whenever a partition is resumed on a vcpu, we have extended paging
	//enabled and that the base points to the extended page tables we have initialized
	assert( (vcpu->vmcs.control_VMX_seccpu_based & 0x2) );
	assert( (vcpu->vmcs.control_EPT_pointer == (hva2spa((void*)vcpu->vmx_vaddr_ept_pml4_table) | 0x1E)) )
#endif

	return 1;
}
