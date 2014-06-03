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

#include <linux/kernel.h>	/* printk(), min() */
#include <linux/slab.h>		/* kzalloc() */
#include <linux/sched.h>
#include <linux/fs.h>		/* everything... */
#include <linux/proc_fs.h>
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>

#include "scull.h"		/* local definitions */

struct scull_sort {
        wait_queue_head_t inq, outq;       /* read and write queues */
        char *buffer, *end;                /* begin of buf, end of buf */
        int buffersize;                    /* used in pointer arithmetic */
        char *rp, *wp;                     /* where to read, where to write */
        int nreaders, nwriters;            /* number of openings for r/w */
        struct fasync_struct *async_queue; /* asynchronous readers */
        struct mutex mutex;              /* mutual exclusion semaphore */
        struct cdev cdev;                  /* Char device structure */
};

/* parameters */
static int scull_sort_nr_devs = SCULL_SORT_NR_DEVS;	/* number of pipe devices */
int scull_sort_buffer =  SCULL_SORT_BUFFER;	/* buffer size */
dev_t scull_sort_devno;			/* Our first device number */

module_param(scull_sort_nr_devs, int, 0);	/* FIXME check perms */
module_param(scull_sort_buffer, int, 0);

static struct scull_sort *scull_sort_devices;

static int scull_sort_fasync(int fd, struct file *filp, int mode);
static int spacefree(struct scull_sort *dev);

/*
 * Open and close
 */

static int scull_sort_open(struct inode *inode, struct file *filp)
{
	struct scull_sort *dev;

	dev = container_of(inode->i_cdev, struct scull_sort, cdev);
	filp->private_data = dev;

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;
	if (!dev->buffer) {
		/* allocate the buffer */
		dev->buffer = kzalloc(scull_sort_buffer, GFP_KERNEL);
		if (!dev->buffer) {
			mutex_unlock(&dev->mutex);
			return -ENOMEM;
		}
	}
	dev->buffersize = scull_sort_buffer;
	dev->end = dev->buffer + dev->buffersize;
	dev->rp = dev->wp = dev->buffer; /* rd and wr from the beginning */

	/* use f_mode,not  f_flags: it's cleaner (fs/open.c tells why) */
	if (filp->f_mode & FMODE_READ)
		dev->nreaders++;
	if (filp->f_mode & FMODE_WRITE)
		dev->nwriters++;
	mutex_unlock(&dev->mutex);

	return nonseekable_open(inode, filp);
}

static int scull_sort_release(struct inode *inode, struct file *filp)
{
	struct scull_sort *dev = filp->private_data;

	/* remove this filp from the asynchronously notified filp's */
	scull_sort_fasync(-1, filp, 0);
	mutex_lock(&dev->mutex);
	if (filp->f_mode & FMODE_READ)
		dev->nreaders--;
	if (filp->f_mode & FMODE_WRITE)
		dev->nwriters--;
	if (dev->nreaders + dev->nwriters == 0) {
		kfree(dev->buffer);
		dev->buffer = NULL; /* the other fields are not checked on open */
	}
	mutex_unlock(&dev->mutex);
	return 0;
}

/*
 * Data management: read and write
 */

static ssize_t scull_sort_read (struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct scull_sort *dev = filp->private_data;

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	while (dev->rp == dev->wp) { /* nothing to read */
		mutex_unlock(&dev->mutex); /* release the lock */
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		PDEBUG("\"%s\" reading: going to sleep\n", current->comm);
		if (wait_event_interruptible(dev->inq, (dev->rp != dev->wp)))
			return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
		/* otherwise loop, but first reacquire the lock */
		if (mutex_lock_interruptible(&dev->mutex))
			return -ERESTARTSYS;
	}
	/* ok, data is there, return something */
	if (dev->wp > dev->rp)
		count = min(count, (size_t)(dev->wp - dev->rp));
	else /* the write pointer has wrapped, return data up to dev->end */
		count = min(count, (size_t)(dev->end - dev->rp));
	if (copy_to_user(buf, dev->rp, count)) {
		mutex_unlock (&dev->mutex);
		return -EFAULT;
	}
	dev->rp += count;
	if (dev->rp == dev->end)
		dev->rp = dev->buffer; /* wrapped */
	mutex_unlock (&dev->mutex);

	/* finally, awake any writers and return */
	wake_up_interruptible(&dev->outq);
	PDEBUG("\"%s\" did read %li bytes\n",current->comm, (long)count);
	return count;
}

/* Wait for space for writing; caller must hold device semaphore.  On
 * error the semaphore will be released before returning. */
static int scull_getwritespace(struct scull_sort *dev, struct file *filp)
{
	while (spacefree(dev) == 0) { /* full */
		DEFINE_WAIT(wait);
		
		mutex_unlock(&dev->mutex);
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		PDEBUG("\"%s\" writing: going to sleep\n",current->comm);
		prepare_to_wait(&dev->outq, &wait, TASK_INTERRUPTIBLE);
		if (spacefree(dev) == 0)
			schedule();
		finish_wait(&dev->outq, &wait);
		if (signal_pending(current))
			return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
		if (mutex_lock_interruptible(&dev->mutex))
			return -ERESTARTSYS;
	}
	return 0;
}	

/* How much space is free? */
static int spacefree(struct scull_sort *dev)
{
	if (dev->rp == dev->wp)
		return dev->buffersize - 1;
	return ((dev->rp + dev->buffersize - dev->wp) % dev->buffersize) - 1;
}

static ssize_t scull_sort_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct scull_sort *dev = filp->private_data;
	int result;

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	/* Make sure there's space to write */
	result = scull_getwritespace(dev, filp);
	if (result)
		return result; /* scull_getwritespace called mutex_unlock(&dev->mutex) */

	/* ok, space is there, accept something */
	count = min(count, (size_t)spacefree(dev));
	if (dev->wp >= dev->rp)
		count = min(count, (size_t)(dev->end - dev->wp)); /* to end-of-buf */
	else /* the write pointer has wrapped, fill up to rp-1 */
		count = min(count, (size_t)(dev->rp - dev->wp - 1));
	PDEBUG("Going to accept %li bytes to %p from %p\n", (long)count, dev->wp, buf);
	if (copy_from_user(dev->wp, buf, count)) {
		mutex_unlock (&dev->mutex);
		return -EFAULT;
	}
	dev->wp += count;
	if (dev->wp == dev->end)
		dev->wp = dev->buffer; /* wrapped */
	mutex_unlock(&dev->mutex);

	/* finally, awake any reader */
	wake_up_interruptible(&dev->inq);  /* blocked in read() and select() */

	/* and signal asynchronous readers, explained late in chapter 5 */
	if (dev->async_queue)
		kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
	PDEBUG("\"%s\" did write %li bytes\n",current->comm, (long)count);
	return count;
}

static unsigned int scull_sort_poll(struct file *filp, poll_table *wait)
{
	struct scull_sort *dev = filp->private_data;
	unsigned int mask = 0;

	/*
	 * The buffer is circular; it is considered full
	 * if "wp" is right behind "rp" and empty if the
	 * two are equal.
	 */
	mutex_lock(&dev->mutex);
	poll_wait(filp, &dev->inq,  wait);
	poll_wait(filp, &dev->outq, wait);
	if (dev->rp != dev->wp)
		mask |= POLLIN | POLLRDNORM;	/* readable */
	if (spacefree(dev))
		mask |= POLLOUT | POLLWRNORM;	/* writable */
	mutex_unlock(&dev->mutex);
	return mask;
}

static int scull_sort_fasync(int fd, struct file *filp, int mode)
{
	struct scull_sort *dev = filp->private_data;

	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

#ifdef SCULL_DEBUG
static int scull_read_p_mem_proc_show(struct seq_file *m, void *v)
{
	int i;
	struct scull_sort *p;

#define LIMIT (m->size-200)	/* don't print any more after this size */
	seq_printf(m, "Default buffersize is %i\n", scull_sort_buffer);
	for(i = 0; i<scull_sort_nr_devs && m->count <= LIMIT; i++) {
		p = &scull_sort_devices[i];
		if (mutex_lock_interruptible(&p->mutex))
			return -ERESTARTSYS;
		seq_printf(m, "\nDevice %i: %p\n", i, p);
/*		seq_printf(m, "   Queues: %p %p\n", p->inq, p->outq);*/
		seq_printf(m, "   Buffer: %p to %p (%i bytes)\n", p->buffer, p->end, p->buffersize);
		seq_printf(m, "   rp %p   wp %p\n", p->rp, p->wp);
		seq_printf(m, "   readers %i   writers %i\n", p->nreaders, p->nwriters);
		mutex_unlock(&p->mutex);
	}
	return 0;
}

#define DEFINE_PROC_SEQ_FILE(_name) \
	static int _name##_proc_open(struct inode *inode, struct file *file)\
	{\
		return single_open(file, _name##_proc_show, NULL);\
	}\
	\
	static const struct file_operations _name##_proc_fops = {\
		.open		= _name##_proc_open,\
		.read		= seq_read,\
		.llseek		= seq_lseek,\
		.release	= single_release,\
	};

DEFINE_PROC_SEQ_FILE(scull_read_p_mem)

#endif


/*
 * The file operations for the pipe device
 * (some are overlayed with bare scull)
 */
struct file_operations scull_sort_fops = {
	.owner =	THIS_MODULE,
	.llseek =	no_llseek,
	.read =		scull_sort_read,
	.write =	scull_sort_write,
	.poll =		scull_sort_poll,
	.unlocked_ioctl =	scull_ioctl,
	.open =		scull_sort_open,
	.release =	scull_sort_release,
	.fasync =	scull_sort_fasync,
};

/*
 * Set up a cdev entry.
 */
static void scull_sort_setup_cdev(struct scull_sort *dev, int index)
{
	int err, devno = scull_sort_devno + index;
    
	cdev_init(&dev->cdev, &scull_sort_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add (&dev->cdev, devno, 1);
	if (err)
		/* fail gracefully */
		printk(KERN_NOTICE "Error %d adding scullpipe%d", err, index);
}

/*
 * Initialize the pipe devs; return how many we did.
 */
int scull_sort_init(dev_t firstdev)
{
	int i, result;

	result = register_chrdev_region(firstdev, scull_sort_nr_devs, "scullp");
	if (result < 0) {
		printk(KERN_NOTICE "Unable to get scullp region, error %d\n", result);
		return 0;
	}
	scull_sort_devno = firstdev;
	scull_sort_devices = kzalloc(scull_sort_nr_devs * sizeof(struct scull_sort), GFP_KERNEL);
	if (scull_sort_devices == NULL) {
		unregister_chrdev_region(firstdev, scull_sort_nr_devs);
		return 0;
	}
	for (i = 0; i < scull_sort_nr_devs; i++) {
		init_waitqueue_head(&(scull_sort_devices[i].inq));
		init_waitqueue_head(&(scull_sort_devices[i].outq));
		mutex_init(&scull_sort_devices[i].mutex);
		scull_sort_setup_cdev(scull_sort_devices + i, i);
	}
#ifdef SCULL_DEBUG
	proc_create("scullpipe", 0, NULL, &scull_read_p_mem_proc_fops);
#endif
	return scull_sort_nr_devs;
}

/*
 * This is called by cleanup_module or on failure.
 * It is required to never fail, even if nothing was initialized first
 */
void scull_sort_cleanup(void)
{
	int i;

#ifdef SCULL_DEBUG
	remove_proc_entry("scullpipe", NULL);
#endif

	if (!scull_sort_devices)
		return; /* nothing else to release */

	for (i = 0; i < scull_sort_nr_devs; i++) {
		cdev_del(&scull_sort_devices[i].cdev);
		kfree(scull_sort_devices[i].buffer);
	}
	kfree(scull_sort_devices);
	unregister_chrdev_region(scull_sort_devno, scull_sort_nr_devs);
	scull_sort_devices = NULL; /* pedantic */
}
