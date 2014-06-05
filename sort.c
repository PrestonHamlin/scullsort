/*
 * pipe.c -- fifo driver for scull
 *
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 */
 
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>    /* printk(), min() */
#include <linux/slab.h>        /* kzalloc() */
#include <linux/sched.h>
#include <linux/fs.h>        /* everything... */
#include <linux/proc_fs.h>
#include <linux/errno.h>    /* error codes */
#include <linux/types.h>    /* size_t */
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>

#include "scull.h"        /* local definitions */

struct scull_sort {
        wait_queue_head_t inq, outq;        /* read and write queues */
        char *buffer, *end;                 /* begin of buf, end of buf */
        int buffersize;                     /* used in pointer arithmetic */
        char *rp, *wp;                      /* not a circular queue */
        int nreaders, nwriters;             /* number of openings for r/w */
        struct fasync_struct *async_queue;  /* asynchronous readers */
        struct mutex mutex;                 /* mutual exclusion semaphore */
        struct cdev cdev;                   /* Char device structure */
};

/* parameters */
int sort_buffer =  SCULL_SORT_BUFFER;   // size of buffer
dev_t scull_sort_devno;                 // device number
static bool sort_initialized = false;   // first-time operations

static struct scull_sort my_dev;        // device data

static int scull_sort_fasync(int fd, struct file *filp, int mode);
static int spacefree(void);
void print_stuff(void);
void scull_shift_buffer(void);



//=============================================================================
//                          Open/Close & Read/Write
//=============================================================================

// open scullsort device
static int scull_sort_open(struct inode *inode, struct file *filp) {
    printk("Opening scullsort\n");
    
    return 0;
}

// close scullsort device
static int scull_sort_release(struct inode *inode, struct file *filp) {
    printk("Releasing scullsort\n");
    
    return 0;
}



/* Wait for space for writing; caller must hold device semaphore.  On
 * error the semaphore will be released before returning. */
static int scull_getwritespace(struct scull_sort *dev,
                               struct file *filp)
{
    return 0;
}    
/* How much space is free? */
static int spacefree(void) {
    if (my_dev.wp == my_dev.rp) return my_dev.buffersize -1;
    
    return ((my_dev.buffersize + my_dev.rp) - my_dev.wp) -1;
}



// read stuff
// The read pointer will be incremented so as to prevent unneeded shifting
// If more than a quarter of the buffer space is unusable, then the data will
//  be shifted and the pointers moved.
static ssize_t scull_sort_read (struct file *filp, char __user *buf,
                                size_t count,      loff_t *f_pos)
{
    printk("Attempting read from scullsort\n");
    print_stuff();
    
    // sleep (retry call) until lock acquired
    if (mutex_lock_interruptible(&my_dev.mutex))
        return -ERESTARTSYS;
    printk("Reading from scullsort\n");
    
        
    while (my_dev.rp == my_dev.wp) {    // while there is nothing to read
        mutex_unlock(&my_dev.mutex);        //  free the lock
        
        // exit if non-blocking
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;
        
        // sleep (retry call) until there is something to read
        if (wait_event_interruptible(my_dev.inq, (my_dev.rp != my_dev.wp)))
            return -ERESTARTSYS;
        
        // re-acquire mutex
        if (mutex_lock_interruptible(&my_dev.mutex))
            return -ERESTARTSYS;
    }
    
    // there is now data to be read, and it is safe to read the data
    count = min(count, (size_t)(my_dev.wp - my_dev.rp));
    if ( copy_to_user(buf, my_dev.rp, count) ) {
        mutex_unlock(&my_dev.mutex);
        return -EFAULT;
    }
    my_dev.rp += count;
    
    // if lots of space is wasted, clean it up
    if ((my_dev.rp - my_dev.buffer) > (sort_buffer >>2)) {
        scull_shift_buffer();
    }
    
    mutex_unlock(&my_dev.mutex);
    
    return count;
    wake_up_interruptible(&my_dev.outq);
}

// perform write operations
static ssize_t scull_sort_write(struct file *filp, const char __user *buf,
                                size_t count,      loff_t *f_pos)
{
    printk("Writing to scullsort\n");
    
    return 0;
}



//=============================================================================
//                              Poll & Async
//=============================================================================

static unsigned int scull_sort_poll(struct file *filp, poll_table *wait) {
    printk("Polling scullsort...\n");
    
    return 0;
}

static int scull_sort_fasync(int fd, struct file *filp, int mode) {
    printk("Async scullsort...\n");
    
    return 0;
}



//=============================================================================
//                              File Operations
//=============================================================================

//The file operations for the pipe device
//  (some are overlayed with bare scull)
struct file_operations scull_sort_fops = {
    .owner          = THIS_MODULE,
    .llseek         = no_llseek,
    .read           = scull_sort_read,
    .write          = scull_sort_write,
    .poll           = scull_sort_poll,
    .unlocked_ioctl = scull_ioctl,
    .open           = scull_sort_open,
    .release        = scull_sort_release,
    .fasync         = scull_sort_fasync,
};



int scull_sort_init(dev_t firstdev) {
    int result;
    printk("Initializing scullsort\n");

    if (sort_initialized) {
        printk(KERN_ALERT "ERROR: Already initialized!\n");
    }

    // register device    
    result = register_chrdev_region(firstdev, 1, "sculls");
    if (result < 0) {
        printk(KERN_NOTICE "Unable to register sculls region: %d\n", result);
        return 0;
    }
    
    // initialize queues for readers and writers
    init_waitqueue_head(&(my_dev.inq));
    init_waitqueue_head(&(my_dev.outq));
    
    // initialize the per-device mutex (only one since reads modify)
    mutex_init(&(my_dev.mutex));
    
    // configure cdev entry
    scull_sort_devno = firstdev;
    cdev_init(&(my_dev.cdev), &scull_sort_fops);
    my_dev.cdev.owner = THIS_MODULE;
    result = cdev_add(&(my_dev.cdev), scull_sort_devno, 1);
    
    // scull_sort member data
    my_dev.buffersize   = sort_buffer;
    my_dev.buffer       = kzalloc(sort_buffer, GFP_KERNEL);
    my_dev.end = my_dev.buffer + my_dev.buffersize;
    my_dev.wp = my_dev.rp = my_dev.buffer;
    my_dev.nreaders = my_dev.nwriters = 0;
    
    sort_initialized = true;
    
    return 1;
}



//This is called by cleanup_module or on failure.
//  It is required to never fail, even if nothing was initialized first
void scull_sort_cleanup(void) {
    printk("Cleaning up scullsort\n");
    
    // remove cdev entry
    cdev_del(&my_dev.cdev);
    
    // free buffer
    kfree(my_dev.buffer);
    
    // unregister device
    unregister_chrdev_region(scull_sort_devno, 1);
}




// prints some info to help with debugging
//  acquires the lock itself, so do not call inside locked section
void print_stuff(void) {
    mutex_lock(&my_dev.mutex);

    printk( "\nBuffer size: %d"
            "\nFilled:      %ld"
            "\nHidden:      %ld"
            "\nReaders:     %d"
            "\nWriters:     %d\n",
            my_dev.buffersize,
            my_dev.wp - my_dev.rp,
            my_dev.rp - my_dev.buffer,
            my_dev.nreaders,
            my_dev.nwriters
    );
    
    mutex_unlock(&my_dev.mutex);
}

// cleans up wasted space in the buffer
//  does not take a lock, assumes caller is holding one 
void scull_shift_buffer(void) {
    int dist, count, i;

    if (my_dev.rp > my_dev.wp) {
        printk("Something went horribly wrong!\n");
        return;
    }
    if (my_dev.rp == my_dev.buffer) {
        printk("No shifting neede\nd");
        return;
    }
    
    dist    = my_dev.rp - my_dev.buffer;
    count   = my_dev.wp - my_dev.rp;
    printk("Shifting %d buffer elements by %d spaces\n", count, dist);

    for (i=0; i<count; i++) {
        *(my_dev.buffer + i) = *(my_dev.rp + i);
    }
    
    my_dev.rp = my_dev.buffer;
    my_dev.wp -= dist;
}








