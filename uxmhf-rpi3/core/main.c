#include <types.h>
#include <arm8-32.h>
#include <bcm2837.h>
#include <miniuart.h>
#include <atags.h>
#include <fdt.h>
#include <debug.h>
#include <dmaprot.h>

extern void chainload_os(u32 r0, u32 id, struct atag *at, u32 address);
extern void chainload_os_svc(u32 start_address);

//extern void cpumodeswitch_hyp2svc(u32 address);
extern void cpumodeswitch_hyp2svc(u32 r0, u32 id, struct atag *at, u32 address, u32 cpuid);

extern void entry_svc(void);
extern void secondary_cpu_entry_svc(void);


extern void hypvtable_reserved_handler();
extern void hypvtable_hyphvc_handler();
extern void hypvtable_hypsvc_handler();

extern u32 g_hypvtable[BCM2837_MAXCPUS][8];

//u32 guestos_boot_r0=0;
//u32 guestos_boot_r1=0;
//u32 guestos_boot_r2=0;


void hyp_rsvhandler(void){
	_XDPRINTF_("%s: unhandled exception\n", __func__);
	_XDPRINTF_("%s: Halting!\n", __func__);
	HALT();
}


void hyphvc_handler(void){
	_XDPRINTF_("%s: [IN]\n", __func__);
	_XDPRINTF_("%s: Hello world from hypercall\n", __func__);
	_XDPRINTF_("%s: [OUT]\n", __func__);
}


//////
// guest register and memory read/write helpers
//////

void guest_regwrite(arm8_32_regs_t *r, u32 regnum, u32 value){
	switch(regnum){
		case 0:		r->r0 = value; break;
		case 1:		r->r1 = value; break;
		case 2:		r->r2 = value; break;
		case 3:		r->r3 = value; break;
		case 4:		r->r4 = value; break;
		case 5:		r->r5 = value; break;
		case 6:		r->r6 = value; break;
		case 7:		r->r7 = value; break;
		case 8:		r->r8 = value; break;
		case 9:		r->r9 = value; break;
		case 10:	r->r10 = value; break;
		case 11:	r->r11 = value; break;
		case 12:	r->r12 = value; break;
		default:
			_XDPRINTFSMP_("%s: Invalid regnum=%u. Halting!\n", __func__, regnum);
			HALT();
	}
}


u32 guest_regread(arm8_32_regs_t *r, u32 regnum){

	if(regnum == 0)
		return(r->r0);
	else if (regnum == 1)
		return(r->r1);
	else if (regnum == 2)
		return(r->r2);
	else if (regnum == 3)
		return(r->r3);
	else if (regnum == 4)
		return(r->r4);
	else if (regnum == 5)
		return(r->r5);
	else if (regnum == 6)
		return(r->r6);
	else if (regnum == 7)
		return(r->r7);
	else if (regnum == 8)
		return(r->r8);
	else if (regnum == 9)
		return(r->r9);
	else if (regnum == 10)
		return(r->r10);
	else if (regnum == 11)
		return(r->r11);
	else if (regnum == 12)
		return(r->r12);
	else{
			_XDPRINTFSMP_("%s: Invalid regnum=%u. Halting!\n", __func__, regnum);
			HALT();
	}

	return 0;	//never reached
}


void guest_hypercall_handler(arm8_32_regs_t *r, u32 hsr){
	u32 hvc_iss;
	u32 hvc_imm16;

	hvc_imm16 = hvc_iss & 0x0000FFFFUL;
	hvc_iss = ((hsr & HSR_ISS_MASK) >> HSR_ISS_SHIFT);


	if( hvc_imm16 ==  1 ){
		//_XDPRINTFSMP_("%s: r0=0x%08x, r1=0x%08x, r2=0x%08x\n", __func__,
		//		r->r0, r->r1, r->r2);

		//r->r0 = 0x21;
		//r->r1 = 0x22;
		//r->r2 = 0x23;

	}else if (hvc_imm16 == 2){
		//u64 attrs_noaccess = (LDESC_S2_MC_OUTER_WRITE_BACK_CACHEABLE_INNER_WRITE_BACK_CACHEABLE << LDESC_S2_MEMATTR_MC_SHIFT) |
		//	(LDESC_S2_S2AP_NO_ACCESS << LDESC_S2_MEMATTR_S2AP_SHIFT) |
		//	(MEM_INNER_SHAREABLE << LDESC_S2_MEMATTR_SH_SHIFT) |
		//	LDESC_S2_MEMATTR_AF_MASK;

		//_XDPRINTFSMP_("%s: setprot_noaccess r0=0x%08x\n", __func__,
		//		r->r0);

		//uapi_s2pgtbl_setprot(r->r0, attrs_noaccess);
		//sysreg_tlbiallis();

	}else if (hvc_imm16 == 3){
		//u64 attrs = (LDESC_S2_MC_OUTER_WRITE_BACK_CACHEABLE_INNER_WRITE_BACK_CACHEABLE << LDESC_S2_MEMATTR_MC_SHIFT) |
		//	(LDESC_S2_S2AP_READ_WRITE << LDESC_S2_MEMATTR_S2AP_SHIFT) |
		//	(MEM_INNER_SHAREABLE << LDESC_S2_MEMATTR_SH_SHIFT) |
		//	LDESC_S2_MEMATTR_AF_MASK;

		//_XDPRINTFSMP_("%s: setprot_restore-access r0=0x%08x\n", __func__,
		//		r->r0);

		//uapi_s2pgtbl_setprot(r->r0, attrs);
		//sysreg_tlbiallis();

	}else if (hvc_imm16 == 5) {
		//_XDPRINTFSMP_("%s: HVC: reg=0x%08x, val=0x%08x\n",
		//		__func__, r->r0, r->r1);

		//mmio_write32(r->r0, r->r1);

	}else{
		_XDPRINTFSMP_("%s: unknown HVC instruction imm16=0x%08x. Halting!\n", __func__,
				hvc_imm16);
		HALT();
	}

}


void guest_data_abort_handler(arm8_32_regs_t *r, u32 hsr){
	u32 elr_hyp;
	u32 fault_va;
	u32 fault_pa_page;
	u32 fault_pa;
	u32 fault_va_page_offset;
	u32 fault_iss;
	u32 fault_iss_isv;
	u32 guest_regnum;
	u32 fault_il;
	info_intercept_data_abort_t ida;

	//get faulting iss
	fault_iss = (hsr & HSR_ISS_MASK) >> HSR_ISS_SHIFT;

	//compute validity bit of additional information
	fault_iss_isv = (fault_iss & 0x01000000UL) >> 24;

	if(!fault_iss_isv){
		_XDPRINTFSMP_("%s: s2pgtbl DATA ABORT: invalid isv. Halting!\n", __func__);
		HALT();
	}

	//compute fault instruction length
	fault_il = ((hsr & HSR_IL_MASK) >> HSR_IL_SHIFT);

	//fix return address
	elr_hyp = sysreg_read_elrhyp();
	if(fault_il)
		elr_hyp += sizeof(u32);
	else
		elr_hyp += sizeof(u16);
	sysreg_write_elrhyp(elr_hyp);

	//get faulting va
	fault_va= sysreg_read_hdfar();

	//compute faulting va page_offset
	fault_va_page_offset = fault_va & 0x00000FFFUL;

	//get faulting pa page
	fault_pa_page = sysreg_read_hpfar();
	fault_pa_page = (fault_pa_page & 0xFFFFFFF0UL) << 8;

	//compute faulting pa
	fault_pa = 	fault_pa_page | fault_va_page_offset;

	//fill out 	info_intercept_data_abort_t ida
	ida.il = fault_il;
	ida.sas = (fault_iss & 0x00C00000UL) >> 22;
	ida.srt = (fault_iss & 0x000F0000UL) >> 16;
	ida.wnr = (fault_iss & 0x00000040UL) >> 6;
	ida.va = fault_va;
	ida.pa = fault_pa;
	ida.r = r;

	//handle data abort fault by passing it to appropriate module
	if ( fault_pa_page == DWC_REGS_BASE ){
		dmaprot_handle_usbdmac_access(&ida);

	}else{
		_XDPRINTFSMP_("%s: unknown s2pgtbl DATA ABORT. Halting! (va=0x%08x, pa=0x%08x)\n",
			__func__, ida.va, ida.pa);
		HALT();
	}

	/*if( (da_pa_page == BCM2837_DMA0_REGS_BASE) ||
		(da_pa_page == BCM2837_DMA15_REGS_BASE) ){
		dmaprot_handle_dmacontroller_access(&ida);
	*/

}




__attribute__(( section(".data") )) u32 hypsvc_handler_lock=1;

void hypsvc_handler(arm8_32_regs_t *r){
	u32 hsr;
	u32 hsr_ec;
	//_XDPRINTFSMP_("%s: ENTER\n", __func__);

	//acquire lock
	spin_lock(&hypsvc_handler_lock);

	//read hsr to determine the cause of the intercept
	hsr = sysreg_read_hsr();
	hsr_ec = ((hsr & HSR_EC_MASK) >> HSR_EC_SHIFT);
	//bcm2837_miniuart_puts(" HSR= ");
	//debug_hexdumpu32(hsr);

	//switch ( ((hsr & HSR_EC_MASK) >> HSR_EC_SHIFT) ){
	if(hsr_ec == HSR_EC_HVC){
		guest_hypercall_handler(r, hsr);

	}else if (hsr_ec == HSR_EC_DATA_ABORT_ELCHANGE){
		guest_data_abort_handler(r, hsr);

	}else{
		_XDPRINTFSMP_("uXMHF-rpi3: core: UNHANDLED INTERCEPT HALTING! hsr=0x%08x\n", hsr);
		HALT();
	}

	//_XDPRINTFSMP_("%s: EXIT\n", __func__);

	//release lock
	spin_unlock(&hypsvc_handler_lock);
}


void main_svc(u32 r0, u32 id, struct atag *at){
	u32 cpsr;

	_XDPRINTF_("%s: now in SVC mode\n", __func__);
	_XDPRINTF_("%s: r0=0x%08x, id=0x%08x, ATAGS=0x%08x\n", __func__, r0, id, at);

	_XDPRINTF_("%s: CPSR[mode]=0x%08x\n", __func__, (sysreg_read_cpsr() & 0xF));

	_XDPRINTF_("%s: proceeding to test hypercall (HVC) in SVC mode...\n", __func__);
	hypcall();
	_XDPRINTF_("%s: successful return after hypercall test\n", __func__);


	//_XDPRINTF_("%s: chainloading OS kernel...\n", __func__);
	//_XDPRINTF_("%s: r0=0x%08x, id=0x%08x, ATAGS=0x%08x\n", __func__, r0, id, at);
	//chainload_os(r0, id, at);

	_XDPRINTF_("%s: should not be here. Halting!\n", __func__);
	HALT();
}


void core_fixresmemmap(u32 fdt_address){
	struct fdt_header *fdth = (struct fdt_header *)fdt_address;
	struct fdt_reserve_entry *fdtrsvmmapentryp;
	u32 newtotalsize, padding;

	bcm2837_miniuart_puts("uxmhf-rpi3: core: core_fixresmemmap [IN]\n");

	bcm2837_miniuart_puts(" fdt_address=0x");
	debug_hexdumpu32(fdt_address);

	bcm2837_miniuart_puts(" totalsize=0x");
	debug_hexdumpu32(cpu_be2le_u32(fdth->totalsize));

	bcm2837_miniuart_puts(" off_dt_struct=0x");
	debug_hexdumpu32(cpu_be2le_u32(fdth->off_dt_struct));

	bcm2837_miniuart_puts(" size_dt_struct=0x");
	debug_hexdumpu32(cpu_be2le_u32(fdth->size_dt_struct));

	bcm2837_miniuart_puts(" off_dt_strings=0x");
	debug_hexdumpu32(cpu_be2le_u32(fdth->off_dt_strings));

	bcm2837_miniuart_puts(" size_dt_strings=0x");
	debug_hexdumpu32(cpu_be2le_u32(fdth->size_dt_strings));

	bcm2837_miniuart_puts(" off_mem_rsvmap=0x");
	debug_hexdumpu32(cpu_be2le_u32(fdth->off_mem_rsvmap));

	bcm2837_miniuart_puts(" version=0x");
	debug_hexdumpu32(cpu_be2le_u32(fdth->version));

	bcm2837_miniuart_puts(" last_comp_version=0x");
	debug_hexdumpu32(cpu_be2le_u32(fdth->last_comp_version));

	//pad totalsize to a page-boundary
	padding = PAGE_SIZE_4K - (cpu_be2le_u32(fdth->totalsize) % PAGE_SIZE_4K);
	bcm2837_miniuart_puts("padding=0x");
	debug_hexdumpu32(padding);

	//take totalsize and compute var = size + 8 * 2
	newtotalsize = cpu_be2le_u32(fdth->totalsize);
	newtotalsize += padding;
	newtotalsize += (2 * sizeof(struct fdt_reserve_entry));

	//put rsv_mem_off to size
	fdth->off_mem_rsvmap = cpu_le2be_u32(cpu_be2le_u32(fdth->totalsize) + padding);

	//set totalsize to var
	fdth->totalsize = cpu_le2be_u32(newtotalsize);

	//populate fdtrsvmmapentryp to rsv_mem_off
	fdtrsvmmapentryp = (struct fdt_reserve_entry *)(fdt_address + cpu_be2le_u32(fdth->off_mem_rsvmap));

	bcm2837_miniuart_puts("fdtrsvmmapentryp=0x");
	debug_hexdumpu32((u32)fdtrsvmmapentryp);
	bcm2837_miniuart_puts("sizeof(fdtrsvmmapentryp)=0x");
	debug_hexdumpu32(sizeof(struct fdt_reserve_entry));

	//write the guestos extent as first entry
	fdtrsvmmapentryp->address = cpu_le2be_u64(0x0000000028000000ULL);
	fdtrsvmmapentryp->size = cpu_le2be_u64(0x0000000000C00000ULL);
	//fdtrsvmmapentryp->address = 0ULL;
	//fdtrsvmmapentryp->size = 0ULL;

	//terminate the list with 0sadd 16 bytes
	fdtrsvmmapentryp++;
	bcm2837_miniuart_puts("fdtrsvmmapentryp=0x");
	debug_hexdumpu32((u32)fdtrsvmmapentryp);

	fdtrsvmmapentryp->address = 0ULL;
	fdtrsvmmapentryp->size = 0ULL;

	//debug
	bcm2837_miniuart_puts("uxmhf-rpi3: core: dumping reserved memmap...\n");
	fdtrsvmmapentryp = (struct fdt_reserve_entry *)(fdt_address + cpu_be2le_u32(fdth->off_mem_rsvmap));
	bcm2837_miniuart_puts("fdtrsvmmapentryp=0x");
	debug_hexdumpu32((u32)fdtrsvmmapentryp);


	while(1){
		u64 addr = cpu_be2le_u64(fdtrsvmmapentryp->address);
		u64 size = cpu_be2le_u64(fdtrsvmmapentryp->size);
		if( addr == 0ULL &&  size == 0ULL){
			break;
		}
		bcm2837_miniuart_puts(" address:0x");
		debug_hexdumpu32(addr >> 32);
		debug_hexdumpu32((u32)addr);
		bcm2837_miniuart_puts(" size:0x");
		debug_hexdumpu32(size >> 32);
		debug_hexdumpu32((u32)size);
		fdtrsvmmapentryp++;
		bcm2837_miniuart_puts("fdtrsvmmapentryp=0x");
		debug_hexdumpu32((u32)fdtrsvmmapentryp);
	}

	bcm2837_miniuart_puts("uxmhf-rpi3: core: dumped reserved memmap...\n");

	bcm2837_miniuart_puts("uxmhf-rpi3: core: core_fixresmemmap [OUT]\n");
}


volatile __attribute__((aligned(32))) u32 my_lock=1;
extern u32 cpu_smpready[];
extern u8 cpu_stacks[];
extern u8 cpu_stacks_svc[];


void main(u32 r0, u32 id, struct atag *at, u32 cpuid){
	u32 hvbar, hcr, spsr_hyp;

	_XDPRINTF_("%s[%u]: ENTER: sp=0x%08x (cpu_stacks=0x%08x)\n", __func__, cpuid,
			cpu_read_sp(), &cpu_stacks);
	_XDPRINTF_("%s[%u]: r0=0x%08x, id=0x%08x, ATAGS=0x%08x\n", __func__, cpuid, r0, id, at);

	//sanity check ATAGS pointer
	if(!(at->size == FDT_MAGIC)){
		_XDPRINTF_("%s[%u]: Error: require ATAGS to be FDT blob. Halting!\n", __func__, cpuid);
		HALT();
	}
	_XDPRINTF_("%s[%u]: ATAGS pointer is a FDT blob so no worries\n", __func__, cpuid);

	//fix reserved memory map
	core_fixresmemmap((u32)at);

	//initialize base hardware platform
	bcm2837_platform_initialize();
	_XDPRINTF_("%s[%u]: initialized base hardware platform\n", __func__, cpuid);

	hyppgtbl_populate_tables();
	_XDPRINTF_("%s[%u]: page-tables populated\n", __func__, cpuid);

	hyppgtbl_activate();
	_XDPRINTF_("%s[%u]: hyp page-tables activated\n", __func__, cpuid);

	// populate stage-2 page tables
	s2pgtbl_populate_tables();
	_XDPRINTF_("%s[%u]: stage-2 pts populated.\n", __func__, cpuid);

	//activate DMA protection mechanism via stage-2 pts
	dmaprot_activate();
	_XDPRINTF_("%s[%u]: DMA protection mechanism activated via stage-2 pts\n", __func__, cpuid);


	//dump hyp registers and load hvbar
	_XDPRINTF_("%s[%u]: HCR=0x%08x\n", __func__, cpuid, sysreg_read_hcr());
	_XDPRINTF_("%s[%u]: HSTR=0x%08x\n", __func__, cpuid, sysreg_read_hstr());
	_XDPRINTF_("%s[%u]: HCPTR=0x%08x\n", __func__, cpuid, sysreg_read_hcptr());
	_XDPRINTF_("%s[%u]: HDCR=0x%08x\n", __func__, cpuid, sysreg_read_hdcr());
	_XDPRINTF_("%s[%u]: HVBAR[before]=0x%08x\n", __func__, cpuid, sysreg_read_hvbar());

/*	//debug
	_XDPRINTF_("%s[%u]: proceeding to dump ghypvtable...\n", __func__, cpuid);
	_XDPRINTF_("%s[%u]: rsv handler at 0x%08x\n", __func__, cpuid, &hypvtable_reserved_handler);
	_XDPRINTF_("%s[%u]: hvc handler at 0x%08x\n", __func__, cpuid, &hypvtable_hyphvc_handler);
	_XDPRINTF_("%s[%u]: hvc handler at 0x%08x\n", __func__, cpuid, &hypvtable_hypsvc_handler);

	{
		u32 i, j;
		for(i=0; i < BCM2837_MAXCPUS; i++){
			_XDPRINTF_("  ghypvtable[%u] at 0x%08x\n", i, (u32)&g_hypvtable[i]);
			for(j=0; j < 8; j++){
				_XDPRINTF_("  ghypvtable[%u][%u]=0x%08x\n", i, j, g_hypvtable[i][j]);
			}
		}
	}
	_XDPRINTF_("%s[%u]: dumped g_hypvtable. Halting\n", __func__, cpuid);
	HALT();
*/

	_XDPRINTF_("%s[%u]: ghypvtable at 0x%08x\n", __func__, cpuid, (u32)&g_hypvtable[cpuid]);
	sysreg_write_hvbar((u32)&g_hypvtable[cpuid]);
	_XDPRINTF_("%s[%u]: HVBAR[after]=0x%08x\n", __func__, cpuid, sysreg_read_hvbar());

	//test hyp mode hvc
	//_XDPRINTF_("%s[%u]: proceeding to test hypercall (HVC) in HYP mode...\n", __func__, cpuid);
	//hypcall();
	//_XDPRINTF_("%s[%u]: successful return from hypercall\n", __func__, cpuid);

	// initialize cpu support for second stage page table translations
	s2pgtbl_initialize();
	_XDPRINTF_("%s[%u]: cpu ready for stage-2 pts...\n", __func__, cpuid);

	// load page table base
	s2pgtbl_loadpgtblbase();
	_XDPRINTF_("%s[%u]: loaded stage-2 page-table base register\n", __func__, cpuid);

	// activate translation
	s2pgtbl_activatetranslation();
	_XDPRINTF_("%s[%u]: activated stage-2 translation\n", __func__, cpuid);

	// boot secondary cores
	_XDPRINTF_("%s[%u]: proceeding to initialize SMP...\n", __func__, cpuid);
	bcm2837_platform_smpinitialize();
	_XDPRINTF_("%s[%u]: secondary cores booted, moving on...\n", __func__, cpuid);

	//brief delay to allow secondary cores to start spinning on mailboxes
/*	_XDPRINTF_("%s[%u]: waiting for secondary cores to spin into mailbox wait...\n", __func__, cpuid);
	{
		u32 i,j;
		for(i=0; i < 1024*256; i++){
			for(j=0; j < 1024; j++){
				cpu_dsb();
			}
		}
	}
*/

	_XDPRINTF_("%s[%u]: booting guest in SVC mode\n", __func__, cpuid);
	_XDPRINTF_("%s[%u]: r0=0x%08x, id=0x%08x, at=0x%08x\n", __func__, cpuid, r0, id, at);

	//cpumodeswitch_hyp2svc(r0, id, at, &entry_svc);
	//cpumodeswitch_hyp2svc(r0, id, at, 0x8000, 0);
	chainload_os(r0,id,at,0x8000);

	_XDPRINTF_("%s[%u]: Halting\n", __func__, cpuid);
	HALT();
}



void secondary_main(u32 cpuid){
	armlocalregisters_mailboxwrite_t *armlocalregisters_mailboxwrite = (armlocalregisters_mailboxwrite_t *)(ARMLOCALREGISTERS_MAILBOXWRITE_BASE + (0 * sizeof(armlocalregisters_mailboxwrite_t)));

	_XDPRINTF_("%s[%u]: ENTER: sp=0x%08x (cpu_stacks=0x%08x)\n", __func__, cpuid,
			cpu_read_sp(), &cpu_stacks);

	hyppgtbl_activate();
	_XDPRINTF_("%s[%u]: hyp page-tables activated\n", __func__, cpuid);

	//dump hyp registers and load hvbar
	_XDPRINTF_("%s[%u]: HCR=0x%08x\n", __func__, cpuid, sysreg_read_hcr());
	_XDPRINTF_("%s[%u]: HSTR=0x%08x\n", __func__, cpuid, sysreg_read_hstr());
	_XDPRINTF_("%s[%u]: HCPTR=0x%08x\n", __func__, cpuid, sysreg_read_hcptr());
	_XDPRINTF_("%s[%u]: HDCR=0x%08x\n", __func__, cpuid, sysreg_read_hdcr());
	_XDPRINTF_("%s[%u]: HVBAR[before]=0x%08x\n", __func__, cpuid, sysreg_read_hvbar());
	_XDPRINTF_("%s[%u]: ghypvtable at 0x%08x\n", __func__, cpuid, (u32)&g_hypvtable[cpuid]);
	sysreg_write_hvbar((u32)&g_hypvtable[cpuid]);
	_XDPRINTF_("%s[%u]: HVBAR[after]=0x%08x\n", __func__, cpuid, sysreg_read_hvbar());

	//test hyp mode hvc
	//_XDPRINTF_("%s[%u]: proceeding to test hypercall (HVC) in HYP mode...\n", __func__, cpuid);
	//hypcall();
	//_XDPRINTF_("%s[%u]: successful return from hypercall\n", __func__, cpuid);

	// initialize cpu support for second stage page table translations
	s2pgtbl_initialize();
	_XDPRINTF_("%s[%u]: cpu ready for stage-2 pts...\n", __func__, cpuid);

	// load page table base
	s2pgtbl_loadpgtblbase();
	_XDPRINTF_("%s[%u]: loaded stage-2 page-table base register\n", __func__, cpuid);

	// activate translation
	s2pgtbl_activatetranslation();
	_XDPRINTF_("%s[%u]: activated stage-2 translation\n", __func__, cpuid);


/*
	_XDPRINTF_("%s[%u]: Moving into SVC mode...\n", __func__, cpuid);

	//_XDPRINTF_("%s[%u]: Signalling SMP readiness and moving into SVC mode...\n", __func__, cpuid);
	//armlocalregisters_mailboxwrite->mailbox3write = 1;
	//cpu_smpready[cpuid]=1;

	cpumodeswitch_hyp2svc(0, 0, 0, &secondary_cpu_entry_svc, cpuid);

	_XDPRINTF_("%s[%u]: should never get here. halting!\n", __func__, cpuid);
	HALT();
*/
	{
		u32 start_address;
		armlocalregisters_mailboxwrite_t *armlocalregisters_mailboxwrite;
		armlocalregisters_mailboxwrite = (armlocalregisters_mailboxwrite_t *)(ARMLOCALREGISTERS_MAILBOXWRITE_BASE + (0 * sizeof(armlocalregisters_mailboxwrite_t)));

		_XDPRINTF_("%s[%u]: Signalling SMP readiness and entering SMP boot wait loop...\n", __func__, cpuid);
		armlocalregisters_mailboxwrite->mailbox3write = 1;
		cpu_dsb();

		start_address=bcm2837_platform_waitforstartup(cpuid);

		_XDPRINTFSMP_("%s[%u]: Got startup signal, address=0x%08x\n", __func__, cpuid, start_address);

		chainload_os(0, 0, 0, start_address);

		HALT();
	}


}


//all secondary CPUs get here in SVC mode and enter the wait-for-startup loop
void secondary_main_svc(u32 cpuid){
	u32 start_address;
	armlocalregisters_mailboxwrite_t *armlocalregisters_mailboxwrite;

	armlocalregisters_mailboxwrite = (armlocalregisters_mailboxwrite_t *)(ARMLOCALREGISTERS_MAILBOXWRITE_BASE + (0 * sizeof(armlocalregisters_mailboxwrite_t)));

	_XDPRINTF_("%s[%u]: ENTER: sp=0x%08x (cpu_stacks_svc=0x%08x)\n", __func__, cpuid,
			cpu_read_sp(), &cpu_stacks_svc);

	//_XDPRINTF_("%s[%u]: cpu_smpready[%u]=%u\n", __func__, cpuid, cpuid, cpu_smpready[cpuid]);
	_XDPRINTF_("%s[%u]: Signalling SMP readiness and halting!\n", __func__, cpuid);
	armlocalregisters_mailboxwrite->mailbox3write = 1;
	//cpu_smpready[cpuid]=1;
	cpu_dsb();

	start_address=bcm2837_platform_waitforstartup(cpuid);

	//if(cpuid == 1){
		chainload_os_svc(start_address);
	//}

	//_XDPRINTF_("%s[%u]: We should never be here. Halting!\n", __func__, cpuid);
	HALT();
}


/*
	_XDPRINTFSMP_("%s: lock variable at address=0x%08x\n", __func__, &my_lock);
	_XDPRINTFSMP_("%s: acquiring lock [current value=0x%08x]...\n", __func__, (u32)my_lock);
	spin_lock(&my_lock);
	_XDPRINTFSMP_("%s: lock acquired\n", __func__);
	_XDPRINTFSMP_("%s: lock current value=0x%08x\n", __func__, my_lock);


	_XDPRINTFSMP_("%s: going to release lock...\n", __func__);
	spin_unlock(&my_lock);
	_XDPRINTFSMP_("%s: lock released [cirrent value=0x%08x]\n", __func__, my_lock);
*/

