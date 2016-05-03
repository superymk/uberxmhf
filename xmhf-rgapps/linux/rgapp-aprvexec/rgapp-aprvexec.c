/*
 * XMHF rich guest app for aprvexec hypapp
 * author: amit vasudevan (amitvasudevan@acm.org)
 */

#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>


//////////////////////////////////////////////////////////////////////////////
// base types

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long int u64;


#define PAGE_SHIFT 12
#define PAGEMAP_LENGTH 8


//////
// vmcall interface
//////
static void __vmcall(u32 eax, u32 ebx, u32 edx){
	asm volatile (
			"movl %0, %%eax \r\n"
			"movl %1, %%ebx \r\n"
			"movl %2, %%edx \r\n"
			"vmcall \r\n"
			: /*no output*/
			: "g" (eax), "g" (ebx), "g" (edx)
			: "%eax", "%ebx", "%edx"
	);
}


//////
// va_to_pa: virtual to physical address mapping
//////
static u64 va_to_pa(void *vaddr) {
	FILE *pagemap;
	unsigned long offset;
	u64 page_frame_number = 0;

	// open the pagemap file for the current process
	pagemap = fopen("/proc/self/pagemap", "rb");
	if(pagemap == NULL){
		printf("\n%s: unable to open pagemap file. exiting!", __FUNCTION__);
		exit(1);
	}

	// seek to the page that vaddr is on
   offset = (unsigned long)vaddr / getpagesize() * PAGEMAP_LENGTH;
   if(fseek(pagemap, (unsigned long)offset, SEEK_SET) != 0) {
      printf("\n%s: Failed to seek pagemap to proper location", __FUNCTION__);
      exit(1);
   }

   // The page frame number is in bits 0-54 so read the first 7 bytes and clear the 55th bit
   fread(&page_frame_number, 1, PAGEMAP_LENGTH-1, pagemap);

   page_frame_number &= 0x7FFFFFFFFFFFFF;

   fclose(pagemap);

   return (page_frame_number << PAGE_SHIFT);
}




//////
// aprvexec test harness
//////

/*
extern void _xcguestslab_do_testxhapprovexec_functoprotect(void);



#define APPROVEXEC_LOCK     			0xD0
#define APPROVEXEC_UNLOCK   			0xD1

void xcguestslab_do_testxhapprovexec(void){
    u32 gpa = &_xcguestslab_do_testxhapprovexec_functoprotect;

    _XDPRINTF_("%s: Going to approve and lock function at %x\n", __func__, gpa);

    _xcguestslab_vmcall(APPROVEXEC_LOCK, 0, gpa);

    _XDPRINTF_("%s: Locked function\n", __func__);

    //{
    //    u8 *pokefun = (u8 *)&_xcguestslab_do_testxhapprovexec_functoprotect;
    //    pokefun[0] = 0xAB;
    //}

    _XDPRINTF_("%s: Going to unlock function on page %x\n", __func__, gpa);

    _xcguestslab_vmcall(APPROVEXEC_UNLOCK, 0, gpa );

    _XDPRINTF_("%s: unlocked function\n", __func__);

}
*/





void main(void){
    printf("\n%s: Proceeding with aprvexec test...", __FUNCTION__);


    printf("\n%s: aprvexec test done", __FUNCTION__);
    printf("\n\n");
}

//////
// building pieces
//////

//__attribute__((aligned(4096))) static u8 testxhhyperdep_page[4096];


//printf("\n%s: DEP page unlocked", __FUNCTION__);

//if(munlock(&testxhhyperdep_page, sizeof(testxhhyperdep_page)) == -1) {
//	  printf("\nFailed to unlock page in memory: %s\n", strerror(errno));
//	  exit(1);
//}

//if(mlock(&testxhhyperdep_page, sizeof(testxhhyperdep_page)) == -1) {
//	  printf("\nFailed to lock page in memory: %s\n", strerror(errno));
//	  exit(1);
//}

//	if(mprotect(&testxhhyperdep_page, sizeof(testxhhyperdep_page), (PROT_READ | PROT_WRITE | PROT_EXEC)) != 0){
//	    printf("\n%s: Could not change page protections: %s\n", __FUNCTION__, strerror(errno));
//	    exit(1);
//	}

