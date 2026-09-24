/*
 * Copyright (c) 2026 David Uhden Collado <david@uhden.dev>
 * Copyright (c) 2025 kmx.io.
 * Copyright (c) 1997 Manuel Bouyer.
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Modified for ext4fs by kmx.io.
 */

#include <sys/stat.h>

#ifndef _EXT4FS_DINODE_H_
#define _EXT4FS_DINODE_H_

#define EXT4FS_EXTENT_HEADER_MAGIC  0xF30A

/*
 * Maximum extent tree depth defined by the ext4 on-disk format
 * (root + four index levels + leaf).
 */
#define EXT4FS_MAX_EXTENT_DEPTH	5

struct ext4fs_extent_header {
	u_int16_t eh_magic;
	u_int16_t eh_entries;
	u_int16_t eh_max;
	u_int16_t eh_depth;
	u_int32_t eh_generation;
} __attribute__((packed));

struct ext4fs_extent {
	u_int32_t e_block;
	u_int16_t e_len;
	u_int16_t e_start_hi;
	u_int32_t e_start_lo;
} __attribute__((packed));

struct ext4fs_extent_idx {
	u_int32_t ei_block;
	u_int32_t ei_leaf_lo;
	u_int16_t ei_leaf_hi;
	u_int16_t ei_unused;
} __attribute__((packed));

struct ext4fs_dinode {
	u_int16_t i_mode;
	u_int16_t i_uid_lo;
	u_int32_t i_size_lo;
	u_int32_t i_atime;
	u_int32_t i_ctime;
  /* 0x10 */
	u_int32_t i_mtime;
	u_int32_t i_dtime;
	u_int16_t i_gid_lo;
	u_int16_t i_links_count;
	u_int32_t i_blocks_lo;
  /* 0x20 */
	u_int32_t i_flags;
	u_int32_t i_version;
	union {
		u_int32_t i_block[15];
		struct {
			struct ext4fs_extent_header i_extent_header;
			union {
				struct ext4fs_extent i_extent[4];
				struct ext4fs_extent_idx i_extent_idx[4];
			};
		};
	};
	u_int32_t i_nfs_generation;
	u_int32_t i_extended_attributes_lo;
	u_int32_t i_size_hi;
  /* 0x70 */
	u_int32_t i_fragment_address;
	u_int16_t i_blocks_hi;
	u_int16_t i_extended_attributes_hi;
	u_int16_t i_uid_hi;
	u_int16_t i_gid_hi;
	u_int16_t i_checksum_lo;
	u_int16_t i_reserved_7e;
  /* 0x80 */
	u_int16_t i_extra_isize;
	u_int16_t i_checksum_hi;
	u_int32_t i_ctime_extra;
	u_int32_t i_mtime_extra;
	u_int32_t i_atime_extra;
  /* 0x90 */
	u_int32_t i_crtime;
	u_int32_t i_crtime_extra;
	u_int32_t i_version_hi;
	u_int32_t i_project_id;
  /* 0xA0 */
} __attribute__((packed));

/*
 * Fixed size of the ext4 inode core, matching the Linux struct
 * ext4_inode.  The on-disk inode may be larger: the region between
 * offset 128 and i_extra_isize holds extended fields, and everything
 * after it up to the superblock's s_inode_size is padding.  The
 * trailing bytes are not part of this struct; in-memory inodes are
 * allocated with the full filesystem s_inode_size and this struct
 * only overlays the fixed prefix.
 */
#define EXT4FS_DINODE_SIZE	sizeof(struct ext4fs_dinode)
#define EXT4FS_GOOD_OLD_INODE_SIZE	128

/*
 * These structures are an on-disk ABI.  The packed attribute fixes the
 * layout (no padding), and the assertions below pin the sizes and the
 * inline extent area so a field edit cannot silently change what is
 * read from or written to disk.
 */
_Static_assert(sizeof(struct ext4fs_extent_header) == (size_t)12,
    "ext4 extent header must be 12 bytes");
_Static_assert(sizeof(struct ext4fs_extent) == (size_t)12,
    "ext4 extent must be 12 bytes");
_Static_assert(sizeof(struct ext4fs_extent_idx) == (size_t)12,
    "ext4 extent index must be 12 bytes");
_Static_assert(sizeof(((struct ext4fs_dinode *)0)->i_block) == (size_t)60,
    "ext4 i_block area must be 60 bytes");
_Static_assert(EXT4FS_DINODE_SIZE == (size_t)160,
    "ext4 inode core must be 160 bytes");
_Static_assert(EXT4FS_GOOD_OLD_INODE_SIZE == 128,
    "ext4 good-old inode size must be 128 bytes");

struct ext4fs_dinode_large {
	struct ext4fs_dinode dinode;
	/*
	 * Variable-length tail: i_extra_isize region (when the inode
	 * size exceeds 128 bytes) plus padding, in total
	 * fs->m_inode_size - EXT4FS_DINODE_SIZE bytes.  Never index
	 * this region with a fixed structure; use byte offsets and
	 * the ext4fs_inode_fits()/ext4fs_din_fits() guards.
	 */
};

#endif /* _EXT4FS_DINODE_H_ */
