/*
 * Copyright (c) 2026 David Uhden Collado <david@uhden.dev>
 * Copyright (c) 2025 kmx.io.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _EXT4FS_JOURNAL_H_
#define _EXT4FS_JOURNAL_H_

/*
 * JBD2 journal on-disk structures.
 * All JBD2 fields are big-endian.
 */

#define JBD2_MAGIC		0xC03B3998

/* Block types */
#define JBD2_DESCRIPTOR_BLOCK	1
#define JBD2_COMMIT_BLOCK	2
#define JBD2_SUPERBLOCK_V1	3
#define JBD2_SUPERBLOCK_V2	4
#define JBD2_REVOKE_BLOCK	5

/* Descriptor tag flags */
#define JBD2_FLAG_ESCAPE	0x01
#define JBD2_FLAG_SAME_UUID	0x02
#define JBD2_FLAG_DELETED	0x04
#define JBD2_FLAG_LAST_TAG	0x08

/* Journal feature flags (in journal superblock) */
#define JBD2_FEATURE_COMPAT_CHECKSUM	0x01

#define JBD2_FEATURE_INCOMPAT_REVOKE		0x01
#define JBD2_FEATURE_INCOMPAT_64BIT		0x02
#define JBD2_FEATURE_INCOMPAT_ASYNC_COMMIT	0x04
#define JBD2_FEATURE_INCOMPAT_CSUM_V2		0x08
#define JBD2_FEATURE_INCOMPAT_CSUM_V3		0x10
#define JBD2_FEATURE_INCOMPAT_FAST_COMMIT	0x40

/*
 * Incompat features this replay implementation understands.  A journal
 * with any other bit set is rejected at mount time rather than being
 * replayed incorrectly.
 */
#define JBD2_FEATURE_INCOMPAT_SUPPORTED					\
	(JBD2_FEATURE_INCOMPAT_REVOKE |					\
	 JBD2_FEATURE_INCOMPAT_64BIT |					\
	 JBD2_FEATURE_INCOMPAT_ASYNC_COMMIT |				\
	 JBD2_FEATURE_INCOMPAT_CSUM_V2 |				\
	 JBD2_FEATURE_INCOMPAT_CSUM_V3)

/* Common block header (12 bytes) */
struct jbd2_header {
	u_int32_t	h_magic;
	u_int32_t	h_blocktype;
	u_int32_t	h_sequence;
} __attribute__((packed));

/* Journal superblock */
struct jbd2_superblock {
	struct jbd2_header	s_header;
	/* 0x0C */
	u_int32_t		s_blocksize;
	u_int32_t		s_maxlen;
	u_int32_t		s_first;
	/* 0x18 */
	u_int32_t		s_sequence;
	u_int32_t		s_start;
	/* 0x20 */
	u_int32_t		s_errno;
	/* V2+ fields */
	u_int32_t		s_feature_compat;
	u_int32_t		s_feature_incompat;
	u_int32_t		s_feature_ro_compat;
	/* 0x30 */
	u_int8_t		s_uuid[16];
	/* 0x40 */
	u_int32_t		s_nr_users;
	u_int32_t		s_dynsuper;
	/* 0x48 */
	u_int32_t		s_max_transaction;
	u_int32_t		s_max_trans_data;
	/* 0x50 */
	u_int8_t		s_checksum_type;
	u_int8_t		s_padding2[3];
	/* 0x54 */
	u_int8_t		s_padding[168];
	/* 0xFC */
	u_int32_t		s_checksum;
	/* 0x100 */
	u_int8_t		s_users[16 * 48];
} __attribute__((packed));

/* Descriptor block tag v3 (CSUM_V3, 16 bytes without UUID) */
struct jbd2_block_tag3 {
	u_int32_t	t_blocknr;
	u_int32_t	t_flags;
	u_int32_t	t_blocknr_high;
	u_int32_t	t_checksum;
} __attribute__((packed));

/*
 * Descriptor block tag v2 (no CSUM_V3).
 *
 * The on-disk tag never matches sizeof() of this struct.  The size
 * must be computed with jbd2_tag_bytes() from the journal feature
 * flags:
 *
 *   plain jbd2:         blocknr(4) flags(4) [high(4)]         =  8/12
 *   CSUM_V2:            blocknr(4) csum(2) flags(2) pad(2)
 *                       [high(4)]                             = 10/14
 *   CSUM_V3 (tag3):                                            = 16
 *
 * For CSUM_V2 and CSUM_V3 the 16-bit flags live at bytes 6..7.
 */
struct jbd2_block_tag {
	u_int32_t	t_blocknr;
	u_int16_t	t_checksum;
	u_int16_t	t_flags;
	u_int32_t	t_blocknr_high;	/* only if 64BIT */
} __attribute__((packed));

/* 4-byte checksum tail of descriptor/revoke blocks (CSUM_V2/V3) */
struct jbd2_journal_block_tail {
	u_int32_t	t_checksum;
} __attribute__((packed));

/*
 * Descriptor tag checksum modes.  JBD2_CSUM_NONE covers plain jbd2
 * journals as well as checksum-v1 (compat CHECKSUM) journals, which
 * carry no per-tag checksums.
 */
#define JBD2_CSUM_NONE		0
#define JBD2_CSUM_V2		2
#define JBD2_CSUM_V3		3

/* Revoke block header */
struct jbd2_revoke_header {
	struct jbd2_header	r_header;
	u_int32_t		r_count;	/* bytes used in this block */
} __attribute__((packed));

/*
 * The journal structures are an on-disk ABI shared with Linux jbd2.
 * The packed attribute fixes the layout; these assertions make any
 * change to a field type, size or order a compile-time error.
 */
_Static_assert(sizeof(struct jbd2_header) == (size_t)12,
    "jbd2 header must be 12 bytes");
_Static_assert(sizeof(struct jbd2_superblock) == (size_t)1024,
    "jbd2 superblock must be 1024 bytes");
_Static_assert(sizeof(struct jbd2_block_tag3) == (size_t)16,
    "jbd2 block tag v3 must be 16 bytes");
_Static_assert(sizeof(struct jbd2_block_tag) == (size_t)12,
    "jbd2 block tag must be 12 bytes");
_Static_assert(sizeof(struct jbd2_journal_block_tail) == (size_t)4,
    "jbd2 checksum tail must be 4 bytes");
_Static_assert(sizeof(struct jbd2_revoke_header) == (size_t)16,
    "jbd2 revoke header must be 16 bytes");

/* Revocation table entry */
struct jbd2_revoke_entry {
	u_int64_t	re_block;
	u_int32_t	re_sequence;
};

/* Block map entry: journal block -> filesystem block */
struct jbd2_blockmap_entry {
	u_int64_t	jb_fsblock;	/* filesystem block number */
};

/* In-memory replay context */
struct jbd2_replay_ctx {
	struct vnode			*rc_devvp;
	struct m_ext4fs			*rc_fs;

	/* Journal geometry (from journal superblock, host order) */
	u_int32_t			 rc_blocksize;
	u_int32_t			 rc_maxlen;
	u_int32_t			 rc_first;
	u_int32_t			 rc_sequence;	/* starting sequence */
	u_int32_t			 rc_start;	/* starting block */

	/* Journal feature flags */
	u_int32_t			 rc_features_compat;
	u_int32_t			 rc_features_incompat;

	/* Journal UUID and checksum state */
	u_int8_t			 rc_uuid[16];
	u_int8_t			 rc_csum_mode;	/* JBD2_CSUM_* */
	u_int32_t			 rc_csum_seed;	/* ext4fs_crc32c seed */

	/* Block map: journal block number -> filesystem block */
	struct jbd2_blockmap_entry	*rc_blockmap;
	u_int32_t			 rc_blockmap_count;

	/* Revocation table */
	struct jbd2_revoke_entry	*rc_revoke;
	u_int32_t			 rc_revoke_count;
	u_int32_t			 rc_revoke_alloc;

	/* Scan result */
	u_int32_t			 rc_end_sequence;
	u_int32_t			 rc_replay_count;
};

/*
 * Size of one descriptor tag (without the UUID bytes) for the
 * journal feature set recorded in ctx.  Never trust sizeof() of the
 * tag structs; the on-disk layout depends on the feature flags.
 */
u_int32_t jbd2_tag_bytes(const struct jbd2_replay_ctx *);

int ext4fs_journal_replay(struct vnode *, struct m_ext4fs *);

#endif /* _EXT4FS_JOURNAL_H_ */
