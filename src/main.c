#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include "utils.h"
#include "macro.h"
#include <linux/const.h>
#include <linux/errno.h>
#include <linux/fs.h>   /* Needed for KERN_INFO */
#include <linux/types.h>
#include <linux/fcntl.h>
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
#include <linux/smp.h>


static int vmx(void)
{
    printk(KERN_INFO "[+] Entering the kernel...\n");
    if(vmxSupport())
    {
        printk(KERN_INFO "[+] VMX supported...\n");
    }
    else
    {
        printk(KERN_INFO "[+] VMX not supported...\n");
    }

    if(eptSupport())
    {
        printk(KERN_INFO "[+] EPT supported...\n");
    }
    else
    {
        printk(KERN_INFO "[+] EPT not supported...\n");
        return 0;
    }

    if(getVmxOperation())
    {
         printk(KERN_INFO "[+] VMX enabled...\n");

    }
    else
    {
        printk(KERN_INFO "VMX cannot be  enabled !! EXITING\n");
        return 0;
    }

	if (!vmcsOperations()) {
		printk(KERN_INFO "VMCS Allocation failed! EXITING");
		return 0;
	}
	else {
		printk(KERN_INFO "[+] VMCS Allocation succeeded! CONTINUING");
	}
    return 0;
}

static int entry_func(void)
{   
    unsigned int i, cpu;
    cpumask_t affinity_mask;

    for_each_online_cpu(cpu) {
        cpumask_clear(&affinity_mask);
        cpumask_set_cpu(cpu, &affinity_mask);
        set_cpus_allowed_ptr(current, &affinity_mask);
        printk(KERN_INFO "=====================================================\n");
        printk(KERN_INFO "Current thread is executing in %d th logical processor.\n", cpu);
        printk(KERN_INFO "=====================================================\n");
        vmx();
        vmxoffOperation();
    }
   

    
    return  0;

}
   
static void exit_func(void)
{  
      
    

    printk(KERN_INFO "[+]===================Exiting the kernel=================\n");
    return;
}
   
module_init(entry_func);
module_exit(exit_func);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Deepika");
MODULE_DESCRIPTION("Testing");
