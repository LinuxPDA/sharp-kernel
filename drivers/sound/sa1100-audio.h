/*
 * Common audio handling for the SA11x0
 *
 * Copyright (c) 2000 Nicolas Pitre <nico@cam.org>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License.
 */


/* 
 * Buffer Management
 */

typedef struct {
	int size;		/* buffer size */
	char *start;		/* points to actual buffer */
	dma_addr_t dma_addr;	/* physical buffer address */
	struct semaphore sem;	/* down before touching the buffer */
	int master;		/* owner for buffer allocation, contain size when true */
} audio_buf_t;

typedef struct {
	audio_buf_t *buffers;	/* pointer to audio buffer structures */
	audio_buf_t *buf;	/* current buffer used by read/write */
	u_int buf_idx;		/* index for the pointer above... */
	u_int fragsize;		/* fragment i.e. buffer size */
	u_int nbfrags;		/* nbr of fragments i.e. buffers */
	dmach_t dma_ch;		/* DMA channel ID */
} audio_stream_t;

/*
 * State structure for one instance
 */

typedef struct {
	audio_stream_t *output_stream;
	audio_stream_t *input_stream;
	int rd_refcount;	/* nbr of concurrent open() for recording */
	int wr_refcount;	/* nbr of concurrent open() for playback */
	int need_tx_for_rx;	/* true if data must be sent while receiving */
	void (*hw_init)(void);
	void (*hw_shutdown)(void);
	int (*client_ioctl)(struct inode *, struct file *, uint, ulong);
	struct pm_dev *pm_dev;
} audio_state_t;

/*
 * Functions exported by this module
 */
extern int sa1100_audio_instance(struct inode *inode, struct file *file,
				 audio_state_t *state);

