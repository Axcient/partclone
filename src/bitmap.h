#include <string.h>

#define PART_BYTES_PER_LONG ((int)sizeof(unsigned long))
#define PART_BITS_PER_BYTE  (8)
#define PART_BITS_PER_LONG  (PART_BYTES_PER_LONG*8)
#define BITS_TO_BYTES(bits) (((bits)+PART_BITS_PER_BYTE-1)/PART_BITS_PER_BYTE)
#define BITS_TO_LONGS(bits) (((bits)+PART_BITS_PER_LONG-1)/PART_BITS_PER_LONG)

static inline int
pc_test_bit(unsigned long int nr, unsigned long *bitmap,
	    unsigned long long total)
{
	if (!bitmap)
		return -1;
	if (nr >= total){
	    printf("test block %lu out of boundary(%llu)\n", nr, total);
		exit(1);
	}
	unsigned long offset = nr / PART_BITS_PER_LONG;
	unsigned long bit = nr & (PART_BITS_PER_LONG - 1);
	return (bitmap[offset] >> bit) & 1;
}

static inline void
pc_set_bit(unsigned long int nr, unsigned long *bitmap,
	   unsigned long long total)
{
	if (!bitmap)
		return;
	if (nr >= total){
	    printf("set block %lu out of boundary(%llu)\n", nr, total);
		exit(1);
	}
	unsigned long offset = nr / PART_BITS_PER_LONG;
	unsigned long bit = nr & (PART_BITS_PER_LONG - 1);
	bitmap[offset] |= 1UL << bit;
}

static inline void
pc_clear_bit(unsigned long int nr, unsigned long *bitmap,
	     unsigned long long total)
{
	if (!bitmap)
		return;
	if (nr >= total){
	    printf("clear block %lu out of boundary(%llu)\n", nr, total);
		exit(1);
	}
	unsigned long offset = nr / PART_BITS_PER_LONG;
	unsigned long bit = nr & (PART_BITS_PER_LONG - 1);
	bitmap[offset] &= ~(1UL << bit);
}

static inline unsigned long* pc_alloc_bitmap(unsigned long bits)
{
	return (unsigned long*)calloc(PART_BYTES_PER_LONG, BITS_TO_LONGS(bits));
}

static inline void pc_init_bitmap(unsigned long* bitmap, char value, unsigned long bits)
{
	unsigned long byte_count = PART_BYTES_PER_LONG * BITS_TO_LONGS(bits);

	memset(bitmap, value, byte_count);
}

static inline int pc_test_bit_fast(unsigned long int nr, unsigned long *bitmap)
{
	unsigned long offset = nr / PART_BITS_PER_LONG;
	unsigned long bit = nr & (PART_BITS_PER_LONG - 1);
	return (bitmap[offset] >> bit) & 1;
}

static inline void pc_set_bit_fast(unsigned long int nr, unsigned long *bitmap)
{
	unsigned long offset = nr / PART_BITS_PER_LONG;
	unsigned long bit = nr & (PART_BITS_PER_LONG - 1);
	bitmap[offset] |= 1UL << bit;
}

static inline void pc_clear_bit_fast(unsigned long int nr, unsigned long *bitmap)
{
	unsigned long offset = nr / PART_BITS_PER_LONG;
	unsigned long bit = nr & (PART_BITS_PER_LONG - 1);
	bitmap[offset] &= ~(1UL << bit);
}

static inline void pc_rescale_bitmap_fast_aligned(unsigned long* bitmap, unsigned long long total, unsigned int scale_factor, unsigned long long * used_blocks)
{
	*used_blocks = 0;
	unsigned int count_group = PART_BITS_PER_LONG / scale_factor;
	unsigned long long aligned_part = total / scale_factor;
	unsigned long long tail = aligned_part * scale_factor;

	unsigned long mask = (1ul << scale_factor) - 1;
	unsigned long init_mask = mask;
	unsigned int cur_group = 0;
	unsigned long* cur_bitmap_block = &bitmap[0];
	unsigned long long cur_block;
	for(cur_block = 0; cur_block < aligned_part; ++cur_block)
	{
		if(*cur_bitmap_block & mask)
		{
			pc_set_bit_fast(cur_block, bitmap);
			*used_blocks += 1;
		}
		else
		{
			pc_clear_bit_fast(cur_block, bitmap);
		}

		++cur_group;
		if(cur_group == count_group)
		{
			mask = init_mask;
			++cur_bitmap_block;
			cur_group = 0;
		}
		else
		{
			mask = mask << scale_factor;
		}
	}

	if(tail < total)
	{
		mask = (1 << (total - tail)) - 1;
		unsigned long offset = tail / PART_BITS_PER_LONG;
		unsigned long bit = tail & (PART_BITS_PER_LONG - 1);
		if((bitmap[offset] >> bit) & mask)
		{
			pc_set_bit_fast(aligned_part, bitmap);
			*used_blocks += 1;
		}
		else
		{
			pc_clear_bit_fast(aligned_part, bitmap);
		}
	}
}

static inline void pc_rescale_bitmap_generic(unsigned long* bitmap, unsigned long long total, unsigned int scale_factor, unsigned long long * used_blocks)
{
	*used_blocks = 0;
	unsigned long long aligned_part = total / scale_factor;
	unsigned long long tail = aligned_part * scale_factor;
	unsigned long long cur_block = 0;
	unsigned int cur_bit = 0;
	unsigned long long nr;
	for(nr = 0; nr < tail; ++nr)
	{
		if(pc_test_bit_fast(nr, bitmap))
		{
			pc_set_bit_fast(cur_block, bitmap);
			nr += scale_factor - cur_bit - 1;
			++cur_block;
			cur_bit = 0;
			*used_blocks += 1;
		}
		else
		{
			++cur_bit;
			if(cur_bit == scale_factor)
			{
				pc_clear_bit_fast(cur_block, bitmap);
				++cur_block;
				cur_bit = 0;
			}
		}
	}

	if(tail < total)
	{
		pc_clear_bit_fast(aligned_part, bitmap);
		for(nr = tail; nr < total; ++nr)
		{
			if(pc_test_bit_fast(nr, bitmap))
			{
				pc_set_bit_fast(aligned_part, bitmap);
				*used_blocks += 1;
				break;
			}
		}
	}
}

static inline void pc_rescale_bitmap(unsigned long* bitmap, unsigned long long total, unsigned int scale_factor, unsigned long long * used_blocks)
{
	if(PART_BITS_PER_LONG % scale_factor == 0)
	{
		pc_rescale_bitmap_fast_aligned(bitmap, total, scale_factor, used_blocks);
	}
	else
	{
		pc_rescale_bitmap_generic(bitmap, total, scale_factor, used_blocks);
	}
}
