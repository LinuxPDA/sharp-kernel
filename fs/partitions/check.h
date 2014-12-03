/*
 * add_partition adds a partitions details to the devices partition
 * description.
 */
void add_gd_partition(struct gendisk *hd, int minor, int start, int size);

/*
 * Get the default block size for this device
 */
unsigned int get_ptable_blocksize(kdev_t dev);

typedef struct {struct buffer_head *v;} Sector;

unsigned char *read_dev_sector(struct block_device *, unsigned long, Sector *);

static inline void put_dev_sector(Sector p)
{
	brelse(p.v);
}

extern int warn_no_part;
