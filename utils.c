#include <linux/init.h>
#include <linux/module.h>
#include <linux/const.h>
#include <linux/errno.h>
#include <linux/fs.h>   /* Needed for KERN_INFO */
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/smp.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cpu.h>
#include <linux/notifier.h>
#include <linux/uaccess.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <asm/asm.h>
#include <asm/errno.h>
#include "macro.h"

// vmxon region
uint64_t *vmxonRegion = NULL;
//vmcs region
uint64_t *vmcsRegion = NULL;


static inline unsigned long long notrace __rdmsr1(unsigned int msr)
{
	DECLARE_ARGS(val, low, high);

	asm volatile("1: rdmsr\n"
		     "2:\n"
		     : EAX_EDX_RET(val, low, high) : "c" (msr));

	return EAX_EDX_VAL(val, low, high);
}

static inline int _vmxon(uint64_t phys)
{
	uint8_t ret;

	__asm__ __volatile__ ("vmxon %[pa]; setna %[ret]"
		: [ret]"=rm"(ret)
		: [pa]"m"(phys)
		: "cc", "memory");
	return ret;
}
static inline uint32_t vmcs_revision_id(void)
{
	return __rdmsr1(MSR_IA32_VMX_BASIC);
}

static inline int _vmptrld(uint64_t vmcs_pa)
{
	uint8_t ret;

	__asm__ __volatile__ ("vmptrld %[pa]; setna %[ret]"
		: [ret]"=rm"(ret)
		: [pa]"m"(vmcs_pa)
		: "cc", "memory");
	return ret;
}
// CH 23.7, Vol 3
// Enter in VMX mode
bool allocVmcsRegion(void) {
	vmcsRegion = kzalloc(MYPAGE_SIZE,GFP_KERNEL);
    printk(KERN_INFO "[+] VMCS allocated at ===> %llx", vmcsRegion);
   	if(vmcsRegion==NULL){
		printk(KERN_INFO "Error allocating vmcs region\n");
      	return false;
   	}
	return true;
}

bool deallocate_vmxon_region(void) {
	if(vmxonRegion){
	    kfree(vmxonRegion);
		return true;
   	}
   	return false;
}

/* Dealloc vmcs guest region*/
bool deallocate_vmcs_region(void) {
	if(vmcsRegion){
    	printk(KERN_INFO "Freeing allocated vmcs region!\n");
    	kfree(vmcsRegion);
		return true;
	}
	return false;
}

bool vmxSupport(void)
{

int getVmxSupport, vmxBit;
__asm__ __volatile__ (
    "mov $1, %%rax\n\t"
    "cpuid\n\t"
    "mov %%ecx, %0\n\t"
    : "=r" (getVmxSupport)
    :
    : "rax", "rbx", "rcx", "rdx"
);
    vmxBit = (getVmxSupport >> 21) & 1;
    if (vmxBit == 1){
        return true;
    }
    else {
        return false;
    }
    return false;
}

bool eptSupport(void)
{

    int getVmxSupport, vmxBit;
__asm__ __volatile__ (
    "mov $1, %%rax\n\t"
    "cpuid\n\t"
    "mov %%ecx, %0\n\t"
    : "=r" (getVmxSupport)
    :
    : "rax", "rbx", "rcx", "rdx"
);
    vmxBit = (getVmxSupport >> 21) & 1;
    if (vmxBit == 1){
        return true;
    }
    else {
        return false;
    }
    return false;
}

bool getVmxOperation(void) {
    //unsigned long cr0;
	unsigned long cr4;
	unsigned long cr0;
    uint64_t feature_control;
	uint64_t required;
	long int vmxon_phy_region = 0;
	u32 low1 = 0;
    // setting CR4.VMXE[bit 13] = 1
    __asm__ __volatile__("mov %%cr4, %0" : "=r"(cr4) : : "memory");
    cr4 |= X86_CR4_VMXE;
    __asm__ __volatile__("mov %0, %%cr4" : : "r"(cr4) : "memory");

    /*
	 * Configure IA32_FEATURE_CONTROL MSR to allow VMXON:
	 *  Bit 0: Lock bit. If clear, VMXON causes a #GP.
	 *  Bit 2: Enables VMXON outside of SMX operation. If clear, VMXON
	 *    outside of SMX causes a #GP.
	 */
	required = FEATURE_CONTROL_VMXON_ENABLED_OUTSIDE_SMX;
	required |= FEATURE_CONTROL_LOCKED;
	feature_control = __rdmsr1(MSR_IA32_FEATURE_CONTROL);

	if ((feature_control & required) != required) {
		wrmsr(MSR_IA32_FEATURE_CONTROL, feature_control | required, low1);
	}

	/*
	 * Ensure bits in CR0 and CR4 are valid in VMX operation:
	 * - Bit X is 1 in _FIXED0: bit X is fixed to 1 in CRx.
	 * - Bit X is 0 in _FIXED1: bit X is fixed to 0 in CRx.
	 */
	__asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0) : : "memory");
	cr0 &= __rdmsr1(MSR_IA32_VMX_CR0_FIXED1);
	cr0 |= __rdmsr1(MSR_IA32_VMX_CR0_FIXED0);
	__asm__ __volatile__("mov %0, %%cr0" : : "r"(cr0) : "memory");

	__asm__ __volatile__("mov %%cr4, %0" : "=r"(cr4) : : "memory");
	cr4 &= __rdmsr1(MSR_IA32_VMX_CR4_FIXED1);
	cr4 |= __rdmsr1(MSR_IA32_VMX_CR4_FIXED0);
	__asm__ __volatile__("mov %0, %%cr4" : : "r"(cr4) : "memory");

	// allocating 4kib((4096 bytes) of memory for vmxon region
	vmxonRegion = kzalloc(MYPAGE_SIZE,GFP_KERNEL);
    printk(KERN_INFO "[+] VMXOn allocated at ===> %llx", vmxonRegion);

   	if(vmxonRegion==NULL){
		printk(KERN_INFO "Error allocating vmxon region\n");
      	return false;
   	}
	vmxon_phy_region = __pa(vmxonRegion);
	*(uint32_t *)vmxonRegion = vmcs_revision_id();
	if (_vmxon(vmxon_phy_region))
		return false;
	return true;
}

bool vmcsOperations(void) {
	long int vmcsPhyRegion = 0;
	if (allocVmcsRegion()){
		vmcsPhyRegion = __pa(vmcsRegion);
		*(uint32_t *)vmcsRegion = vmcs_revision_id();
	}
	else {
		return false;
	}

	//making the vmcs active and current
	if (_vmptrld(vmcsPhyRegion))
		return false;
	return true;
}

bool vmxoffOperation(void)
{
	if (deallocate_vmxon_region()) {
		printk(KERN_INFO "Successfully freed allocated vmxon region!\n");
	}
	else {
		printk(KERN_INFO "Error freeing allocated vmxon region!\n");
	}
	if (deallocate_vmcs_region()) {
		printk(KERN_INFO "Successfully freed allocated vmcs region!\n");
	}
	else {
		printk(KERN_INFO "Error freeing allocated vmcs region!\n");
	}
	asm volatile ("vmxoff\n" : : : "cc");
	return true;
	
}