/*
 * Common audio handling for the SA11x0 processor
 *
 * Copyright (c) 2000 Nicolas Pitre <nico@cam.org>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License.
 * 
 * 
 * This module handles the generic buffering/DMA/mmap audio interface for
 * codecs connected to the SA1100 chip.  All features depending on specific
 * hardware implementations like supported audio formats or samplerates are
 * relegated to separate specific modules.
 * 
 * 
 * History:
 * 
 * 2000-05-21	Nicolas Pitre	Initial release.
 * 
 * 2000-06-10	Erik Bunce	Add initial poll support.
 * 
 * 2000-08-22	Nicolas Pitre	Removed all DMA stuff. Now using the
 * 				generic SA1100 DMA interface.
 * 
 * 2000-11-30	Nicolas Pitre	- Validation of opened instances;
 * 				- Power handling at open/release time instead
 * 				  of driver load/unload;
 * 
 * 2001-06-03	Nicolas Pitre	Made this file a separate module, based on
 * 				the former sa1100-uda1341.c driver.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/pm.h>
#include <linux/errno.h>
#include <linux/sound.h>
#include <linux/soundcard.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/semaphore.h>
#include <asm/dma.h>

#include "sa1100-audio.h"


#undef DEBUG
#ifdef DEBUG
#define DPRINTK( x... )  printk( ##x )
#else
#define DPRINTK( x... )
#endif


#define AUDIO_NAME		"sa1100-audio"
#define AUDIO_NBFRAGS_DEFAULT	8
#define AUDIO_FRAGSIZE_DEFAULT	8192

#define NEXT_BUF(_s_,_b_) { \
	(_s_)->_b_##_idx++; \
	(_s_)->_b_##_idx %= (_s_)->nbfrags; \
	(_s_)->_b_ = (_s_)->buffers + (_s_)->_b_##_idx; }

#define AUDIO_ACTIVE(state)	((state)->rd_refcount || (state)->wr_refcount)

/* 
 * This function frees all buffers 
 */

static void audio_clear_buf(audio_stream_t * s)
{
	DPRINTK("audio_clear_buf\n");

	/* ensure DMA won't run anymore */
	sa1100_dma_flush_all(s->dma_ch);

	if (s->buffers) {
		int frag;
		for (frag = 0; frag < s->nbfrags; frag++) {
			if (!s->buffers[frag].master)
				continue;
			consistent_free(s->buffers[frag].start,
					s->buffers[frag].master,
					s->buffers[frag].dma_addr);
		}
		kfree(s->buffers);
		s->buffers = NULL;
	}

	s->buf_idx = 0;
	s->buf = NULL;
}


/* 
 * This function allocates the buffer structure array and buffer data space
 * according to the current number of fragments and fragment size.
 */

static int audio_setup_buf(audio_stream_t * s)
{
	int frag;
	int dmasize = 0;
	char *dmabuf = NULL;
	dma_addr_t dmaphys = 0;

	if (s->buffers)
		return -EBUSY;

	s->buffers = (audio_buf_t *)
		kmalloc(sizeof(audio_buf_t) * s->nbfrags, GFP_KERNEL);
	if (!s->buffers)
		goto err;
	memset(s->buffers, 0, sizeof(audio_buf_t) * s->nbfrags);

	for (frag = 0; frag < s->nbfrags; frag++) {
		audio_buf_t *b = &s->buffers[frag];

		/*
		 * Let's allocate non-cached memory for DMA buffers.
		 * We try to allocate all memory at once.
		 * If this fails (a common reason is memory fragmentation), 
		 * then we allocate more smaller buffers.
		 */
		if (!dmasize) {
			dmasize = (s->nbfrags - frag) * s->fragsize;
			do {
			  	dmabuf = consistent_alloc(GFP_KERNEL|GFP_DMA,
							  dmasize,
							  &dmaphys);
				if (!dmabuf)
					dmasize -= s->fragsize;
			} while (!dmabuf && dmasize);
			if (!dmabuf)
				goto err;
			b->master = dmasize;
		}

		b->start = dmabuf;
		b->dma_addr = dmaphys;
		sema_init(&b->sem, 1);
		DPRINTK("buf %d: start %p dma %p\n", frag, b->start,
			b->dma_addr);

		dmabuf += s->fragsize;
		dmaphys += s->fragsize;
		dmasize -= s->fragsize;
	}

	s->buf_idx = 0;
	s->buf = &s->buffers[0];

	return 0;

err:
	printk(AUDIO_NAME ": unable to allocate audio memory\n ");
	audio_clear_buf(s);
	return -ENOMEM;
}


/*
 * DMA callback functions
 */

static void audio_dmaout_done_callback(void *buf_id, int size)
{
	audio_buf_t *b = (audio_buf_t *) buf_id;
	/* 
	 * Current buffer is sent: wake up any process waiting for it.
	 */
	up(&b->sem);
	/* And any process polling on write. */
	wake_up(&b->sem.wait);
}

static void audio_dmain_done_callback(void *buf_id, int size)
{
	audio_buf_t *b = (audio_buf_t *) buf_id;
	/* 
	 * Current buffer is full: set its size and wake up any 
	 * process waiting for it.
	 */
	b->size = size;
	up(&b->sem);
	/* And any process polling on read. */
	wake_up(&b->sem.wait);
}


static int audio_sync(struct file *file)
{
	audio_state_t *state = (audio_state_t *)file->private_data;
	audio_stream_t *s = state->output_stream;
	audio_buf_t *b = s->buf;

	DPRINTK("audio_sync\n");

	if (!s->buffers)
		return 0;

	/* Send half-full buffers */
	if (b->size != 0) {
		down(&b->sem);
		sa1100_dma_queue_buffer(s->dma_ch, (void *) b,
					b->dma_addr, b->size);
		b->size = 0;
		NEXT_BUF(s, buf);
	}

	/*
	 * Let's wait for the last buffer we sent i.e. the one before the
	 * current buf_idx.  When we acquire the semaphore, this means either:
	 * - DMA on the buffer completed or 
	 * - the buffer was already free thus nothing else to sync.
	 */
	b = s->buffers + ((s->nbfrags + s->buf_idx - 1) % s->nbfrags);
	if (down_interruptible(&b->sem))
		return -EINTR;
	up(&b->sem);
	return 0;
}


static int audio_write(struct file *file, const char *buffer,
		       size_t count, loff_t * ppos)
{
	const char *buffer0 = buffer;
	audio_state_t *state = (audio_state_t *)file->private_data;
	audio_stream_t *s = state->output_stream;
	int chunksize, ret = 0;

	DPRINTK("audio_write: count=%d\n", count);

	switch (file->f_flags & O_ACCMODE) {
	case O_WRONLY:
	case O_RDWR:
		break;
	default:
		return -EPERM;
	}

	if (!s->buffers && audio_setup_buf(s))
		return -ENOMEM;

	/* be sure to have a full sample byte count */
	count &= ~0x03;

	while (count > 0) {
		audio_buf_t *b = s->buf;

		/* Wait for a buffer to become free */
		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			if (down_trylock(&b->sem))
				break;
		} else {
			ret = -ERESTARTSYS;
			if (down_interruptible(&b->sem))
				break;
		}

		/* Feed the current buffer */
		chunksize = s->fragsize - b->size;
		if (chunksize > count)
			chunksize = count;
		DPRINTK("write %d to %d\n", chunksize, s->buf_idx);
		if (copy_from_user(b->start + b->size, buffer, chunksize)) {
			up(&b->sem);
			return -EFAULT;
		}
		b->size += chunksize;
		buffer += chunksize;
		count -= chunksize;
		if (b->size < s->fragsize) {
			up(&b->sem);
			break;
		}

		/* Send current buffer to dma */
		sa1100_dma_queue_buffer(s->dma_ch, (void *) b,
					b->dma_addr, b->size);
		b->size = 0;	/* indicate that the buffer has been sent */
		NEXT_BUF(s, buf);
	}

	if ((buffer - buffer0))
		ret = buffer - buffer0;
	DPRINTK("audio_write: return=%d\n", ret);
	return ret;
}


static int audio_recording(audio_state_t *state)
{
	audio_stream_t *s = state->input_stream;
	int i;

	if (!s->buffers) {
		if (audio_setup_buf(s))
			return -ENOMEM;

		/*
		 * With some chips like the UDA1341 we must ensure there is
		 * an output stream at any time while recording since this is
		 * how the UDA1341 gets its clock from the SA1100.
		 * So while there is no playback data to send, the output DMA
		 * will spin with all zeroes.  We use the cache flush special
		 * area for that.
		 */
		if (state->need_tx_for_rx)
		  	sa1100_dma_set_spin(state->output_stream->dma_ch,
					    (dma_addr_t)FLUSH_BASE_PHYS, 2048);

		/* 
		 * Since we just allocated all buffers, we must send them to
		 * the DMA code before receiving data.
		 */
		for (i = 0; i < s->nbfrags; i++) {
			audio_buf_t *b = s->buf;
			down(&b->sem);
			sa1100_dma_queue_buffer(s->dma_ch, (void *) b,
						b->dma_addr, s->fragsize);
			NEXT_BUF(s, buf);
		}
	}
	return 0;
}


static int audio_read(struct file *file, char *buffer,
		      size_t count, loff_t * ppos)
{
	char *buffer0 = buffer;
	audio_state_t *state = (audio_state_t *)file->private_data;
	audio_stream_t *s = state->input_stream;
	int chunksize, ret = 0;

	DPRINTK("audio_read: count=%d\n", count);

	switch (file->f_flags & O_ACCMODE) {
	case O_RDONLY:
	case O_RDWR:
		break;
	default:
		return -EPERM;
	}

	ret = audio_recording(state);
	if (ret)
		return ret;

	/* be sure to have a full sample byte count */
	count &= ~0x03;

	while (count > 0) {
		audio_buf_t *b = s->buf;

		/* Wait for a buffer to become full */
		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			if (down_trylock(&b->sem))
				break;
		} else {
			ret = -ERESTARTSYS;
			if (down_interruptible(&b->sem))
				break;
		}

		/* Grab data from the current buffer */
		chunksize = b->size;
		if (chunksize > count)
			chunksize = count;
		DPRINTK("read %d from %d\n", chunksize, s->buf_idx);
		if (copy_to_user(buffer,
				 b->start + s->fragsize - b->size,
				 chunksize)) {
			up(&b->sem);
			return -EFAULT;
		}
		b->size -= chunksize;
		buffer += chunksize;
		count -= chunksize;
		if (b->size > 0) {
			up(&b->sem);
			break;
		}

		/* Make current buffer available for DMA again */
		sa1100_dma_queue_buffer(s->dma_ch, (void *) b,
					b->dma_addr, s->fragsize);
		NEXT_BUF(s, buf);
	}

	if ((buffer - buffer0))
		ret = buffer - buffer0;
	DPRINTK("audio_read: return=%d\n", ret);
	return ret;
}


static unsigned int audio_poll(struct file *file,
			       struct poll_table_struct *wait)
{
	audio_state_t *state = (audio_state_t *)file->private_data;
	audio_stream_t *is = state->input_stream;
	audio_stream_t *os = state->output_stream;
	unsigned int mask = 0;
	int i;
	int ret;

	DPRINTK("audio_poll(): mode=%s%s\n",
		(file->f_mode & FMODE_READ) ? "r" : "",
		(file->f_mode & FMODE_WRITE) ? "w" : "");

	if (file->f_mode & FMODE_READ) {
		/* Start audio input if not already active */
		ret = audio_recording(state);
		if (ret < 0)
			return ret;
		poll_wait(file, &is->buf->sem.wait, wait);
	}

	if (file->f_mode & FMODE_WRITE) {
		if (!os->buffers && audio_setup_buf(os)) 
			return -ENOMEM;
		poll_wait(file, &os->buf->sem.wait, wait);
	}

	if (file->f_mode & FMODE_READ) {
		for (i = 0; i < is->nbfrags; i++) {
			if (atomic_read(&is->buffers[i].sem.count) > 0) {
				mask |= POLLIN | POLLRDNORM;
				break;
			}
		}
	}
	if (file->f_mode & FMODE_WRITE) {
		for (i = 0; i < os->nbfrags; i++) {
			if (atomic_read(&os->buffers[i].sem.count) > 0) {
				mask |= POLLOUT | POLLWRNORM;
				break;
			}
		}
	}

	DPRINTK("audio_poll() returned mask of %s%s\n",
		(mask & POLLIN) ? "r" : "", 
		(mask & POLLOUT) ? "w" : "");

	return mask;
}


static loff_t audio_llseek(struct file *file, loff_t offset, int origin)
{
	return -ESPIPE;
}


static int audio_set_fragments(audio_stream_t *s, int val)
{
	if (s->buffers)
		return -EBUSY;
	s->fragsize = 1 << (val & 0xFFFF);
	if (s->fragsize < 16)
		s->fragsize = 16;
	if (s->fragsize > 16384)
		s->fragsize = 16384;
	s->nbfrags = (val >> 16) & 0x7FFF;
	if (s->nbfrags < 2)
		s->nbfrags = 2;
	if (s->nbfrags * s->fragsize > 128 * 1024)
		s->nbfrags = 128 * 1024 / s->fragsize;
	return audio_setup_buf(s);
}

static int audio_ioctl(struct inode *inode, struct file *file,
		       uint cmd, ulong arg)
{
	audio_state_t *state = (audio_state_t *)file->private_data;
	audio_stream_t *os = state->output_stream;
	audio_stream_t *is = state->input_stream;
	long val;

	/* dispatch based on command */
	switch (cmd) {
	case SNDCTL_DSP_GETBLKSIZE:
		return put_user(os->fragsize, (long *) arg);
		break;

	case SNDCTL_DSP_SETFRAGMENT:
		if (get_user(val, (long *) arg))
			return -EFAULT;
		return audio_set_fragments(os, val);
		break;

	case SNDCTL_DSP_SYNC:
		return audio_sync(file);

	case SNDCTL_DSP_GETOSPACE:
	    {
		audio_buf_info *inf = (audio_buf_info *) arg;
		int err = verify_area(VERIFY_WRITE, inf, sizeof(*inf));
		int i;
		int frags = 0, bytes = 0;

		if (err)
			return err;
		for (i = 0; i < os->nbfrags; i++) {
			if (atomic_read(&os->buffers[i].sem.count) > 0) {
				if (os->buffers[i].size == 0) frags++;
				bytes += os->fragsize - os->buffers[i].size;
			}
		}
		put_user(frags, &inf->fragments);
		put_user(os->nbfrags, &inf->fragstotal);
		put_user(os->fragsize, &inf->fragsize);
		put_user(bytes, &inf->bytes);
		break;
	    }

	case SNDCTL_DSP_GETISPACE:
	    {
		audio_buf_info *inf = (audio_buf_info *) arg;
		int err = verify_area(VERIFY_WRITE, inf, sizeof(*inf));
		int i;
		int frags = 0, bytes = 0;

		if (err)
			return err;
		for (i = 0; i < is->nbfrags; i++) {
			if (atomic_read(&is->buffers[i].sem.count) > 0) {
				if (is->buffers[i].size == is->fragsize) frags++;
				bytes += is->buffers[i].size;
			}
		}
		put_user(frags, &inf->fragments);
		put_user(is->nbfrags, &inf->fragstotal);
		put_user(is->fragsize, &inf->fragsize);
		put_user(bytes, &inf->bytes);
		break;
	    }

	case SNDCTL_DSP_RESET:
		switch (file->f_flags & O_ACCMODE) {
		case O_RDONLY:
		case O_RDWR:
			if (state->need_tx_for_rx)
				sa1100_dma_set_spin(os->dma_ch, 0, 0);
			audio_clear_buf(is);
		}
		switch (file->f_flags & O_ACCMODE) {
		case O_WRONLY:
		case O_RDWR:
			audio_clear_buf(os);
		}
		return 0;

	case SNDCTL_DSP_POST:
	case SNDCTL_DSP_SUBDIVIDE:
	case SNDCTL_DSP_NONBLOCK:
	case SNDCTL_DSP_GETCAPS:
	case SNDCTL_DSP_GETTRIGGER:
	case SNDCTL_DSP_SETTRIGGER:
	case SNDCTL_DSP_GETIPTR:
	case SNDCTL_DSP_GETOPTR:
	case SNDCTL_DSP_MAPINBUF:
	case SNDCTL_DSP_MAPOUTBUF:
	case SNDCTL_DSP_SETSYNCRO:
	case SNDCTL_DSP_SETDUPLEX:
		return -ENOSYS;
	default:
		/* 
		 * Let the client of this module handle the 
		 * non generic ioctls
		 */
		return state->client_ioctl(inode, file, cmd, arg);
	}

	return 0;
}


static int audio_release(struct inode *inode, struct file *file)
{
	audio_state_t *state = (audio_state_t *)file->private_data;
	DPRINTK("audio_release\n");

	switch (file->f_flags & O_ACCMODE) {
	case O_RDONLY:
	case O_RDWR:
		if (state->rd_refcount == 1) {
			if (state->need_tx_for_rx)
				sa1100_dma_set_spin(state->output_stream->dma_ch, 0, 0);
			audio_clear_buf(state->input_stream);
			state->rd_refcount = 0;
		}
	}

	switch (file->f_flags & O_ACCMODE) {
	case O_WRONLY:
	case O_RDWR:
		if (state->wr_refcount == 1) {
			audio_sync(file);
			audio_clear_buf(state->output_stream);
			state->wr_refcount = 0;
		}
	}

	if (!AUDIO_ACTIVE(state)) {
	       if (state->hw_shutdown)
		       state->hw_shutdown();
#ifdef CONFIG_PM
	       pm_unregister(state->pm_dev);
#endif
	}

	return 0;
}


#ifdef CONFIG_PM

static int audio_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	audio_state_t *state = (audio_state_t *)pm_dev->data;

	switch (req) {
	case PM_SUSPEND: /* enter D1-D3 */
		if (state->output_stream)
			sa1100_dma_sleep(state->output_stream->dma_ch);
		if (state->input_stream)
			sa1100_dma_sleep(state->input_stream->dma_ch);
		if (AUDIO_ACTIVE(state) && state->hw_shutdown)
			state->hw_shutdown();
		break;
	case PM_RESUME:  /* enter D0 */
		if (AUDIO_ACTIVE(state) && state->hw_init)
			state->hw_init();
		if (state->input_stream)
			sa1100_dma_wakeup(state->input_stream->dma_ch);
		if (state->output_stream)
			sa1100_dma_wakeup(state->output_stream->dma_ch);
		break;
	}
	return 0;
}

#endif


int sa1100_audio_instance(struct inode *inode, struct file *file,
			  audio_state_t *state)
{
	int in = 0, out = 0;

	DPRINTK("audio_open\n");

	if ((file->f_flags & O_ACCMODE) == O_RDONLY) {
		if (!state->input_stream)
			return -ENODEV;
		if (state->rd_refcount)
			return -EBUSY;
		in = 1;
	} else if ((file->f_flags & O_ACCMODE) == O_WRONLY) {
		if (!state->output_stream)
			return -ENODEV;
		if (state->wr_refcount)
			return -EBUSY;
		out = 1;
	} else if ((file->f_flags & O_ACCMODE) == O_RDWR) {
		if (!state->input_stream || !state->output_stream)
			return -EINVAL;
		if (state->rd_refcount || state->wr_refcount)
			return -EBUSY;
		in = out = 1;
	} else
		return -EINVAL;

	if (!AUDIO_ACTIVE(state)) {
		if (state->hw_init)
			state->hw_init();
#ifdef CONFIG_PM
		state->pm_dev = pm_register(PM_SYS_DEV, 0, audio_pm_callback);
		if (state->pm_dev)
			state->pm_dev->data = state;
#endif
	}

	if (in) {
		audio_clear_buf(state->input_stream);
		state->rd_refcount = 1;
		state->input_stream->fragsize = AUDIO_FRAGSIZE_DEFAULT;
		state->input_stream->nbfrags = AUDIO_NBFRAGS_DEFAULT;
		sa1100_dma_set_callback(state->input_stream->dma_ch,
					audio_dmain_done_callback);
	}
	
	if (out) {
		audio_clear_buf(state->output_stream);
		state->wr_refcount = 1;
		state->output_stream->fragsize = AUDIO_FRAGSIZE_DEFAULT;
		state->output_stream->nbfrags = AUDIO_NBFRAGS_DEFAULT;
		sa1100_dma_set_callback(state->output_stream->dma_ch,
					audio_dmaout_done_callback);
	}
	
	file->private_data	= state;
	file->f_op->release	= audio_release;
	file->f_op->write	= audio_write;
	file->f_op->read	= audio_read;
	file->f_op->poll	= audio_poll;
	file->f_op->ioctl	= audio_ioctl;
	file->f_op->llseek	= audio_llseek;
	
	return 0;
}

EXPORT_SYMBOL(sa1100_audio_instance);

MODULE_AUTHOR("Nicolas Pitre");
MODULE_DESCRIPTION("Common audio handling for the SA11x0 processor");
