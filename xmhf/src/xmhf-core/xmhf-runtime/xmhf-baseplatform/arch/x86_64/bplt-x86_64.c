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

/*
 * EMHF base platform component interface, x86 common backend
 * author: amit vasudevan (amitvasudevan@acm.org)
 */

#include <xmhf.h>

//get CPU vendor
u32 xmhf_baseplatform_arch_getcpuvendor(void){
	u32 vendor_dword1, vendor_dword2, vendor_dword3;
	u32 cpu_vendor;
	asm(	"xor	%%eax, %%eax \n"
					"cpuid \n"		
					"mov	%%ebx, %0 \n"
					"mov	%%edx, %1 \n"
					"mov	%%ecx, %2 \n"
					 : "=m"(vendor_dword1), "=m"(vendor_dword2), "=m"(vendor_dword3)
					 : //no inputs
					 : "eax", "ebx", "ecx", "edx" );

	if(vendor_dword1 == AMD_STRING_DWORD1 && vendor_dword2 == AMD_STRING_DWORD2
			&& vendor_dword3 == AMD_STRING_DWORD3)
		cpu_vendor = CPU_VENDOR_AMD;
	else if(vendor_dword1 == INTEL_STRING_DWORD1 && vendor_dword2 == INTEL_STRING_DWORD2
			&& vendor_dword3 == INTEL_STRING_DWORD3)
		cpu_vendor = CPU_VENDOR_INTEL;
	else{
		printf("\n%s: unrecognized x86 CPU (0x%08x:0x%08x:0x%08x). HALT!",
			__FUNCTION__, vendor_dword1, vendor_dword2, vendor_dword3);
		HALT();
	}   	 	

	return cpu_vendor;
}

void udelay(u32 usecs){
    u8 val;
    u32 latchregval;  

    //enable 8254 ch-2 counter
    val = inb(0x61);
    val &= 0x0d; //turn PC speaker off
    val |= 0x01; //turn on ch-2
    outb(val, 0x61);
  
    //program ch-2 as one-shot
    outb(0xB0, 0x43);
  
    //compute appropriate latch register value depending on usecs
    latchregval = ((u64)1193182 * usecs) / 1000000;

    //write latch register to ch-2
    val = (u8)latchregval;
    outb(val, 0x42);
    val = (u8)((u32)latchregval >> (u32)8);
    outb(val , 0x42);
  
    //wait for countdown
    while(!(inb(0x61) & 0x20));
  
    //disable ch-2 counter
    val = inb(0x61);
    val &= 0x0c;
    outb(val, 0x61);
}

//initialize basic platform elements
void xmhf_baseplatform_arch_initialize(void){
	//initialize PCI subsystem
	xmhf_baseplatform_arch_x86_64_pci_initialize();
	
	//check ACPI subsystem
	{
		ACPI_RSDP rsdp;
		#ifndef __XMHF_VERIFICATION__
			//TODO: plug in a BIOS data area map/model
			printf("\nFILE:LINE %s:%d", __FILE__, __LINE__);
			printf("\nFILE:LINE %s:%d &rsdp=0x%016lx", __FILE__, __LINE__, (uintptr_t)&rsdp);
			if(!xmhf_baseplatform_arch_x86_64_acpi_getRSDP(&rsdp)){
			    printf("\nFILE:LINE %s:%d", __FILE__, __LINE__);
				printf("\n%s: ACPI RSDP not found, Halting!", __FUNCTION__);
				HALT();
			}
			printf("\nFILE:LINE %s:%d", __FILE__, __LINE__);
		#endif //__XMHF_VERIFICATION__
	}

}


//initialize CPU state
void xmhf_baseplatform_arch_cpuinitialize(void){
	u32 cpu_vendor = xmhf_baseplatform_arch_getcpuvendor();

	//set OSXSAVE bit in CR4 to enable us to pass-thru XSETBV intercepts
	//when the CPU supports XSAVE feature
	if(xmhf_baseplatform_arch_x86_64_cpuhasxsavefeature()){
		u32 t_cr4;
		t_cr4 = read_cr4();
		t_cr4 |= CR4_OSXSAVE;	
		write_cr4(t_cr4);
	}

	if(cpu_vendor == CPU_VENDOR_INTEL)
		xmhf_baseplatform_arch_x86_64vmx_cpuinitialize();
}
