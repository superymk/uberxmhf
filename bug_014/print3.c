	{
		static u32 global_halt = 0;

		if (vcpu->id) {
			printf("\nIntercept: 0x%02x 0x%08x 0x%016llx 0x%04x:0x%016llx ;", vcpu->id, vcpu->vmcs.info_vmexit_reason, vcpu->vmcs.info_exit_qualification, (u16)vcpu->vmcs.guest_CS_selector, vcpu->vmcs.guest_RIP);
			printf("\n\trax = 0x%016llx", r->rax);
			printf("\n\trbx = 0x%016llx", r->rbx);
			printf("\n\trcx = 0x%016llx", r->rcx);
			printf("\n\trdx = 0x%016llx", r->rdx);
			printf("\n\tcr0 = 0x%016llx", vcpu->vmcs.guest_CR0);
			printf("\n\tcr3 = 0x%016llx", vcpu->vmcs.guest_CR3);
			printf("\n\tcr4 = 0x%016llx", vcpu->vmcs.guest_CR4);
			printf("\n\texc = 0x%016llx", vcpu->vmcs.control_VM_exit_controls);
			printf("\n\tenc = 0x%016llx", vcpu->vmcs.control_VM_entry_controls);
			{
				msr_entry_t *efer = &((msr_entry_t *)vcpu->vmx_vaddr_msr_area_guest)[0];
				HALT_ON_ERRORCOND(efer->index == MSR_EFER);
				printf("\n\tefer= 0x%016llx", efer->data);
			}
			if (vcpu->vmcs.guest_RIP == 0x000000000009ae79) {
#define LXY_DUMP_MEMORY(x) ({uintptr_t xx = (uintptr_t)(x); u64 ans = *(u64*)xx; printf("\n\tmem *%08x = 0x%016llx", (u32)xx, ans); ans;})
				u64 pte = 0x9c000;
				pte = LXY_DUMP_MEMORY(pte & ~0xfff);
				pte = LXY_DUMP_MEMORY(pte & ~0xfff);
				pte = LXY_DUMP_MEMORY(pte & ~0xfff);
				pte = LXY_DUMP_MEMORY((pte & ~0xfff) | (0x9a * 8));
				LXY_DUMP_MEMORY(0x9d000);
				LXY_DUMP_MEMORY(0x9d008);
				LXY_DUMP_MEMORY(0x9d010);
				for (int i = 0; i < IA32_VMX_MSRCOUNT; i++) {
					printf("\n\tvmx_msrs[%d] = 0x%016llx", i, vcpu->vmx_msrs[i]);
				}
				printf("\n\tvmx_guest_unrestricted = 0x%08x", vcpu->vmx_guest_unrestricted);
				printf("\n\tvmx_msr_efer = 0x%016llx", vcpu->vmx_msr_efer);
				printf("\n\tvmx_msr_efcr = 0x%016llx", vcpu->vmx_msr_efcr);

				xmhf_baseplatform_arch_x86_64vmx_dumpVMCS(vcpu);
				printf("\n\tNOP Loop");
				asm volatile("1: nop; jmp 1b; nop; nop; nop; nop; nop; nop; nop; nop");
				// for (u64 i = 0x000000000009ae00; i < 0x000000000009af00; i += 4) {
				// 	printf("\nmem: *0x%016lx = 0x%08lx", i, *(u32*)i);
				// }
			}
		}

		if (global_halt) {
			printf("\n0x%02x halt due to global_halt", vcpu->id);
			HALT();
		}
		if (vcpu->vmcs.info_vmexit_reason == 0x2) {
			printf("\n0x%02x set global_halt", vcpu->id);
			global_halt = 1;
		}
	}

