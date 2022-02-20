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

//	acpi.c 
//  advanced configuration and power-management interface subsystem 
//	glue code
//  author: amit vasudevan (amitvasudevan@acm.org)

#include <xmhf.h> 

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
    latchregval = (1193182 * usecs) / 1000000;

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


//------------------------------------------------------------------------------
//compute checksum of ACPI table
static u32 _acpi_computetablechecksum(uintptr_t spaddr, uintptr_t size){
  char *p;
  char checksum=0;
  u32 i;

  p=(char *)spaddr;
  for(i=0; i< size; i++) {
    checksum+= (char)(*(p+i));
  }
  
  printf("\nDONE");
  
  return (u32)checksum;
}


//------------------------------------------------------------------------------
//get the physical address of the root system description pointer (rsdp)
//return 0 in case of error (ACPI RSDP not found) else the absolute physical
//memory address of the RSDP
u32 xmhf_baseplatform_arch_x86_64_acpi_getRSDP(ACPI_RSDP *rsdp){
  u16 ebdaseg;
  uintptr_t ebdaphys;
  uintptr_t i;
  u32 found=0;
  
  //get EBDA segment from 040E:0000h in BIOS data area
  xmhf_baseplatform_arch_flat_copy((u8 *)&ebdaseg, (u8 *)0x0000040E, sizeof(u16));

  //convert it to its 32-bit physical address
  ebdaphys=(uintptr_t)(ebdaseg * 16);

  //search first 1KB of ebda for rsdp signature (8 bytes long)
  for(i=0; i < (1024-8); i+=16){
    xmhf_baseplatform_arch_flat_copy((u8 *)rsdp, (u8 *)(ebdaphys+i), sizeof(ACPI_RSDP));
    if(rsdp->signature == ACPI_RSDP_SIGNATURE){
      if(!_acpi_computetablechecksum((uintptr_t)rsdp, 20)){
        found=1;
        break;
      }
    }
  }
  printf("\nFILE:LINE %s:%d", __FILE__, __LINE__);
	//found RSDP?  
  if(found)
    return (u32)(ebdaphys+i);
  printf("\nFILE:LINE %s:%d", __FILE__, __LINE__);
  //nope, search within BIOS areas 0xE0000 to 0xFFFFF
  for(i=0xE0000; i < (0xFFFFF-8); i+=16){
    xmhf_baseplatform_arch_flat_copy((u8 *)rsdp, (u8 *)i, sizeof(ACPI_RSDP));
    //printf("\nLINE %d 0x%08lx 0x%08lx", __LINE__, (uintptr_t)rsdp, (uintptr_t)i);
    if(rsdp->signature == ACPI_RSDP_SIGNATURE){
      if(!_acpi_computetablechecksum((uintptr_t)rsdp, 20)){
        printf("\nFOUND 2");
        for (int i = 0; i < 1000; i++) { udelay(1000); }
        found=1;
        break;
      }
      printf("\nFILE:LINE %s:%d", __FILE__, __LINE__);
    }
  }
  printf("\nFILE:LINE %s:%d", __FILE__, __LINE__);
  //found RSDP?
  if(found)
    return i;
  printf("\nFILE:LINE %s:%d", __FILE__, __LINE__);
  //no RSDP, system is not ACPI compliant!
  return 0;  
}
//------------------------------------------------------------------------------
