//---CR0 access handler-------------------------------------------------
static void vmx_handle_intercept_cr0access_ug(VCPU *vcpu, struct regs *r, u32 gpr, u32 tofrom){
	u32 cr0_value, old_cr0;
	u64 fixed_1_fields;
	
	HALT_ON_ERRORCOND(tofrom == VMX_CRX_ACCESS_TO);
	
	cr0_value = *((u32 *)_vmx_decode_reg(gpr, vcpu, r));
	old_cr0 = (u32)vcpu->vmcs.guest_CR0;

	//printf("\n[cr0-%02x] MOV TO, current=0x%08x, proposed=0x%08x, shadow=0x%08x",
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
	 * If CR0.PG bit changes, need to update bit 9 of VM-Entry Controls
	 * (IA-32e mode guest). This bit should always equal to EFER.LME && CR0.PG
	 */
	if ((old_cr0 ^ cr0_value) & CR0_PG) {
		u32 value = vcpu->vmcs.control_VM_entry_controls;
		u32 lme;
		msr_entry_t *efer = &((msr_entry_t *)vcpu->vmx_vaddr_msr_area_guest)[0];
		printf("\nSet bit 9 of 'IA-32e mode guest' to EFER.LME && CR0.PG");
		HALT_ON_ERRORCOND(efer->index == MSR_EFER);
		lme = (cr0_value & CR0_PG) && (efer->data & (0x1U << EFER_LME));
		value &= ~(1U << 9);
		value |= lme << 9;
		vcpu->vmcs.control_VM_entry_controls = value;
	}

	//flush mappings
	xmhf_memprot_arch_x86_64vmx_flushmappings(vcpu);
}
