/*
 * =====================================================================================
 *
 *       Filename:  customdevice.c
 *
 *    Description:  Implementation of a mock char device driver.  
 *
 *        Version:  1.0
 *        Created:  06/08/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Gasper Stukelj (gst), gasperstukelj@protonmail.com
 *   Organization:  /
 *
 * =====================================================================================
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <asm/current.h>

int open (struct inode *, struct file *);
int release (struct inode *, struct file *);
long ioctl(struct file *, unsigned int, unsigned long);

static struct file_operations fops = {
	.owner          =  THIS_MODULE,
	.open           =  open,
	.release        =  release,
 	.unlocked_ioctl =  ioctl,
};


/* Struct representing different variables associated with the device driver that are needed througout. */
typedef struct {
	struct mutex lock; 
	struct cdev cdev;
	struct class  * class_ptr;
	struct device * device_ptr;
	dev_t dev; 
	unsigned int major; 
} custom_dev_t;


static custom_dev_t custom_dev = 
{
	.class_ptr  = NULL,
	.device_ptr = NULL,
	.dev        = 0, 
	.major      = 0,
};


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  open
 *  Description:  Called when open() is called with the corresponding file descriptor. 
 *                Implementation is such that the file can only be opened once at a time. 
 * =====================================================================================
 */
int open (struct inode * inode, struct file * fileptr)
{
  	unsigned int mj = imajor(inode);	
  	unsigned int mn = iminor(inode);
	
	if (mj != custom_dev.major || mn < 0)
	{
		printk(KERN_ALERT "Error opening device %d:%d. Device doesn't exist.\n", mj, mn);
		return -1;
	}
	
	if (inode->i_cdev != &(custom_dev.cdev))
	{
		printk(KERN_ALERT "Error opening device %d:%d.\n", mj, mn);
		return -1;
	}
	
	
	if (mutex_lock_interruptible(&(custom_dev.lock))) 
	{
		printk(KERN_ALERT "Error opening device %d:%d.\n", mj, mn);
		return -1; 
	}
	
  	printk(KERN_ALERT "[customdevice] File was opened by \"%s\" (pid %i).\n", current->comm, current->pid);
	return 0;
}		/* -----  end of function open  ----- */



/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  release
 *  Description:  Called when the file descriptor associated with the driver is released. 
 *                Releases the mutex so that another process can open the file using the 
 *                same file descriptor.  
 * =====================================================================================
 */
int release(struct inode * inode, struct file * fileptr)
{
	unsigned int mj = imajor(inode);
	unsigned int mn = iminor(inode);
	
	if (mj != custom_dev.major || mn < 0)
	{
		printk(KERN_ALERT "Error releasing device %d:%d. Device doesn't exist.\n", mj, mn);
		return -1; /* No such device */
	}
	
	if (inode->i_cdev != &(custom_dev.cdev))
	{
		printk(KERN_ALERT "Error releasing device %d:%d. Internal error.\n", mj, mn);
		return -1; /* No such device */
	}
	
	if (mutex_is_locked(&(custom_dev.lock))) 
	{
		mutex_unlock(&(custom_dev.lock));
	}

  	printk(KERN_ALERT "[customdevice] File was released.\n"); 
	
	return 0; 
}		/* -----  end of function release  ----- */



/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  ioctl
 *  Description:  Simply prints the calling process' name and ID to the kernel ring  
 *                buffer and returns (always 0). Ignores the argument.  
 * =====================================================================================
 */
long ioctl(struct file * fileptr, unsigned int cmd, unsigned long arg)
{
 	printk(KERN_ALERT "[customdevice] The ioctl() was called by \"%s\" (pid %i).\n", current->comm, current->pid);
	return 0; 
}		/* -----  end of function ioctl  ----- */



/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  cleanup
 *  Description:  Cleans up by in the reverse order of initializations/allocations of 
 *                relevant resources set up by the custom_init().  
 * =====================================================================================
 */
static void cleanup (int err)
{
	/* Make use of the fall-through mechanic of the switch statement. */
	switch (err)
	{
		case 0: 
			/* we should only arrive here from the __exit function! */
			if (mutex_is_locked(&(custom_dev.lock))) 
			{
				mutex_unlock(&(custom_dev.lock));
			}
			
			mutex_destroy(&(custom_dev.lock));
		case 1: 
			if (custom_dev.device_ptr != NULL)
			{
				device_destroy(custom_dev.class_ptr, custom_dev.dev); 
			}
		case 2: 
			cdev_del(&(custom_dev.cdev)); 
		case 3: 
			if (custom_dev.class_ptr != NULL) 
			{
				class_destroy(custom_dev.class_ptr);
			} 
		case 4: 
			if (custom_dev.dev != 0)
			{
				unregister_chrdev_region(custom_dev.dev, 1); 
			}
		default:
			break; 
	}
}		/* -----  end of function cleanup  ----- */



/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  custom_init
 *  Description:  Called when module is loaded into kernel. Performs all the inits and 
 *                registers the device under a custom_class, visible under /sys/class/ 
 *                and /dev/, respectively. Importantly, major number for the device is 
 *                allocated dynamically by the kernel and is thus not constant. 
 * =====================================================================================
 */
int __init custom_init (void)
{
	int err = 0;

	/* use non-repeating do-while + error breaks instead of goto statements */ 
	do 
 	{
		if (alloc_chrdev_region(&(custom_dev.dev), 0, 1, "customdevice") != 0)
		{
			printk(KERN_ALERT "Warning: alloc_chrdev_region() failed.\n");
			err = 4; 
			break; 
		}
        
		custom_dev.major = MAJOR(custom_dev.dev);
    
		if ((custom_dev.class_ptr = class_create(THIS_MODULE, "dummy_class")) == NULL)
		{
			printk(KERN_ALERT "Warning: class_create() failed.\n");
			err = 3; 
			break; 
		}
	
		cdev_init(&(custom_dev.cdev), &fops);
		custom_dev.cdev.owner = THIS_MODULE; 
    	   	
		mutex_init(&(custom_dev.lock));
    	   	
	  	if (cdev_add(&(custom_dev.cdev), custom_dev.dev, 1) == -1)
		{
			printk(KERN_ALERT "Warning: cdev_add() failed.\n");
			err = 2;
			break;  
		}
		
		if ((custom_dev.device_ptr = device_create(custom_dev.class_ptr, NULL, custom_dev.dev, NULL, "customdevice")) == NULL)
		{
			printk(KERN_ALERT "Warning: device_create() failed.\n");
			err = 1;
			break; 
		}
	} while (0); 
        
	if (err != 0) 
  	{
    		cleanup(err); 
    		return err; 
 	}
        
	return 0;
}		/* -----  end of function custom_init  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  custom_exit
 *  Description:  Called when the module is unloaded. Cleans up everything. Deregisters 
 *                both the custom class and custom device from /sys/class and /dev/. 
 * =====================================================================================
 */
void __exit custom_exit (void)
{
	cleanup(0); 
}		/* -----  end of function custom_exit  ----- */



module_init(custom_init);
module_exit(custom_exit); 

MODULE_AUTHOR("Gasper Stukelj"); 
MODULE_LICENSE("GPL");
