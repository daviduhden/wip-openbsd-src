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

/*
 * JBD2 journal replay for ext4fs.
 *
 * Implements the standard three-pass replay algorithm:
 *   1. SCAN   - find the last complete transaction.
 *   2. REVOKE - collect revocations from the committed range.
 *   3. REPLAY - write surviving data blocks in transaction order.
 *
 * The separate revoke pass lets a later transaction suppress writes from
 * an earlier one. The scan excludes an uncommitted tail from both passes.
 *
 * All JBD2 on-disk fields are big-endian.
 */

// clang-format off
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/vnode.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ext4fs/ext4fs.h>
#include <ufs/ext4fs/ext4fs_journal.h>
// clang-format on

/*
 * Build the journal block map from sb_jnl_blocks[0..14].
 *
 * These little-endian words back up the internal journal inode's i_block.
 * Replay uses the backup to locate the journal before ordinary vnode
 * lookup is available. The map supports inline extents or a root index
 * pointing directly to leaf blocks.
 *
 * sb_jnl_blocks[15] = i_size_lo, sb_jnl_blocks[16] = i_size_hi.
 */
static int
jbd2_build_blockmap(struct jbd2_replay_ctx *ctx)
{
	struct m_ext4fs		    *fs = ctx->rc_fs;
	struct ext4fs		    *sble = &fs->m_sble;
	u_int32_t		    *iblock = sble->sb_jnl_blocks;
	struct ext4fs_extent_header *eh;
	struct ext4fs_extent	    *ext;
	struct ext4fs_extent_idx    *idx;
	u_int16_t		     depth, entries, i;
	u_int32_t		     jblock, maxblocks;
	u_int64_t		     pblock;
	u_int32_t		     len;

	maxblocks = ctx->rc_maxlen;
	ctx->rc_blockmap = mallocarray(maxblocks,
			   sizeof(struct jbd2_blockmap_entry), M_TEMP,
			   M_WAITOK | M_CANFAIL | M_ZERO);
	if (ctx->rc_blockmap == NULL)
		return (ENOMEM);
	ctx->rc_blockmap_count = maxblocks;

	/* Parse extent header from i_block[0..2] (first 12 bytes) */
	eh = (struct ext4fs_extent_header *)iblock;
	if (letoh16(eh->eh_magic) != EXT4FS_EXTENT_HEADER_MAGIC) {
		printf("ext4fs: journal inode has bad extent magic 0x%x\n",
		    letoh16(eh->eh_magic));
		return (EINVAL);
	}

	depth = letoh16(eh->eh_depth);
	entries = letoh16(eh->eh_entries);

	/* The in-inode header cannot hold more than four index entries. */
	if (entries > (sizeof(sble->sb_jnl_blocks) -
			  sizeof(struct ext4fs_extent_header)) /
		sizeof(struct ext4fs_extent_idx)) {
		printf("ext4fs: journal inode has bad extent entry count\n");
		return (EINVAL);
	}

	if (depth == 0) {
		/* Leaf extents follow the header directly */
		ext = (struct ext4fs_extent *)(eh + 1);
		for (i = 0; i < entries; i++) {
			u_int32_t lblk = letoh32(ext[i].e_block);
			len = letoh16(ext[i].e_len);
			/* High bit of e_len marks uninitialized extents. */
			if (len > 32768)
				len -= 32768;
			pblock = (u_int64_t)letoh16(ext[i].e_start_hi) << 32 |
			    letoh32(ext[i].e_start_lo);

			for (jblock = 0; jblock < len; jblock++) {
				u_int32_t j = lblk + jblock;
				if (j < maxblocks)
					ctx->rc_blockmap[j].jb_fsblock =
					    pblock + jblock;
			}
		}
	} else {
		/* Depth > 0: index nodes, need to read leaf blocks */
		idx = (struct ext4fs_extent_idx *)(eh + 1);
		for (i = 0; i < entries; i++) {
			struct buf		    *bp;
			struct ext4fs_extent_header *leh;
			struct ext4fs_extent	    *lext;
			u_int16_t		     lentries, j;
			u_int64_t		     leaf_block;
			int			     error;

			leaf_block = (u_int64_t)letoh16(idx[i].ei_leaf_hi)
				<< 32 |
			    letoh32(idx[i].ei_leaf_lo);
			if (leaf_block < fs->m_first_data_block ||
			    leaf_block >= fs->m_blocks_count)
				return (EINVAL);

			error = bread(ctx->rc_devvp,
			    (daddr_t)EXT4FS_FSBTODB(fs, leaf_block),
			    fs->m_block_size, &bp);
			if (error) {
				brelse(bp);
				printf("ext4fs: journal blockmap: "
				       "can't read index block\n");
				return (error);
			}

			leh = (struct ext4fs_extent_header *)bp->b_data;
			if (letoh16(leh->eh_magic) !=
			    EXT4FS_EXTENT_HEADER_MAGIC) {
				brelse(bp);
				printf("ext4fs: journal blockmap: "
				       "bad leaf magic\n");
				return (EINVAL);
			}
			if (letoh16(leh->eh_depth) != 0) {
				brelse(bp);
				printf("ext4fs: journal blockmap: "
				       "depth > 1 not supported\n");
				return (EINVAL);
			}

			lentries = letoh16(leh->eh_entries);
			if (lentries >
			    (fs->m_block_size -
				sizeof(struct ext4fs_extent_header)) /
				sizeof(struct ext4fs_extent)) {
				brelse(bp);
				printf("ext4fs: journal blockmap: "
				       "bad leaf entry count\n");
				return (EINVAL);
			}
			lext = (struct ext4fs_extent *)(leh + 1);

			for (j = 0; j < lentries; j++) {
				u_int32_t lblk = letoh32(lext[j].e_block);
				len = letoh16(lext[j].e_len);
				/* High bit marks uninitialized extents. */
				if (len > 32768)
					len -= 32768;
				pblock = (u_int64_t)letoh16(lext[j].e_start_hi)
					<< 32 |
				    letoh32(lext[j].e_start_lo);

				for (jblock = 0; jblock < len; jblock++) {
					u_int32_t k = lblk + jblock;
					if (k < maxblocks)
						ctx->rc_blockmap[k].jb_fsblock =
						    pblock + jblock;
				}
			}
			brelse(bp);
		}
	}

	return (0);
}

/*
 * Read a journal block by journal-relative block number.
 */
static int
jbd2_read_block(struct jbd2_replay_ctx *ctx, u_int32_t jblock, struct buf **bpp)
{
	struct m_ext4fs *fs = ctx->rc_fs;
	u_int64_t	 fsblock;

	/*
	 * Leave *bpp defined on every path: callers brelse() the
	 * out-parameter after an error, and bread() may still return a
	 * buffer on I/O errors.
	 */
	*bpp = NULL;

	if (jblock >= ctx->rc_blockmap_count) {
		printf("ext4fs: journal block %u out of range (%u)\n", jblock,
		    ctx->rc_blockmap_count);
		return (EIO);
	}

	fsblock = ctx->rc_blockmap[jblock].jb_fsblock;
	if (fsblock == 0 || fsblock < fs->m_first_data_block ||
	    fsblock >= fs->m_blocks_count) {
		printf("ext4fs: invalid mapping for journal block %u\n",
		    jblock);
		return (EIO);
	}

	return bread(ctx->rc_devvp, (daddr_t)EXT4FS_FSBTODB(fs, fsblock),
	    fs->m_block_size, bpp);
}

/*
 * Wrap journal block number circularly.
 */
static u_int32_t
jbd2_next_block(struct jbd2_replay_ctx *ctx, u_int32_t block)
{
	block++;
	if (block >= ctx->rc_maxlen)
		block = ctx->rc_first;
	return block;
}

/*
 * Size of one on-disk descriptor tag for the journal feature set.
 *
 * Mirrors Linux journal_tag_bytes():
 *   CSUM_V3:          16 bytes (blocknr, flags, high, checksum)
 *   CSUM_V2 + 64BIT:  14 bytes (blocknr, csum, flags, high, 2 pad)
 *   CSUM_V2:          10 bytes (blocknr, csum, flags, 2 pad)
 *   plain + 64BIT:    12 bytes (blocknr, flags(32), high)
 *   plain:             8 bytes (blocknr, flags(32))
 */
u_int32_t
jbd2_tag_bytes(const struct jbd2_replay_ctx *ctx)
{
	if (ctx->rc_csum_mode == JBD2_CSUM_V3)
		return (sizeof(struct jbd2_block_tag3));

	if (ctx->rc_csum_mode == JBD2_CSUM_V2) {
		if (ctx->rc_features_incompat & JBD2_FEATURE_INCOMPAT_64BIT)
			return (14);
		return (10);
	}

	if (ctx->rc_features_incompat & JBD2_FEATURE_INCOMPAT_64BIT)
		return (12);
	return (8);
}

/*
 * Validate the journal feature set and derive the checksum mode.
 *
 * Unknown incompat features, the fast-commit layout, and async
 * commit journals (whose on-disk ordering differs from the simple
 * descriptor/data/commit layout assumed by the three passes) are
 * rejected: replaying them without understanding the format would
 * misinterpret journal data and could write it to the filesystem.
 */
static int
jbd2_feature_check(struct jbd2_replay_ctx *ctx)
{
	u_int32_t incompat = ctx->rc_features_incompat;

	if (incompat & ~JBD2_FEATURE_INCOMPAT_SUPPORTED) {
		printf("ext4fs: journal has unsupported incompat features "
		       "0x%x\n",
		    incompat & ~JBD2_FEATURE_INCOMPAT_SUPPORTED);
		return (EOPNOTSUPP);
	}

	if (incompat & JBD2_FEATURE_INCOMPAT_FAST_COMMIT) {
		printf("ext4fs: journal uses fast commits, not supported\n");
		return (EOPNOTSUPP);
	}

	if (incompat & JBD2_FEATURE_INCOMPAT_ASYNC_COMMIT) {
		printf("ext4fs: journal uses async commits, not supported\n");
		return (EOPNOTSUPP);
	}

	if ((incompat & JBD2_FEATURE_INCOMPAT_CSUM_V2) &&
	    (incompat & JBD2_FEATURE_INCOMPAT_CSUM_V3)) {
		printf("ext4fs: journal sets both checksum v2 and v3\n");
		return (EINVAL);
	}

	/* Checksum v1 (compat CHECKSUM) is mutually exclusive with
	 * v2/v3; jbd2 clears it when v3 is enabled. */
	if ((ctx->rc_features_compat & JBD2_FEATURE_COMPAT_CHECKSUM) &&
	    (incompat &
		(JBD2_FEATURE_INCOMPAT_CSUM_V2 |
		    JBD2_FEATURE_INCOMPAT_CSUM_V3))) {
		printf("ext4fs: journal sets checksum v1 with v2/v3\n");
		return (EINVAL);
	}

	if (incompat & JBD2_FEATURE_INCOMPAT_CSUM_V3)
		ctx->rc_csum_mode = JBD2_CSUM_V3;
	else if (incompat & JBD2_FEATURE_INCOMPAT_CSUM_V2)
		ctx->rc_csum_mode = JBD2_CSUM_V2;
	else
		ctx->rc_csum_mode = JBD2_CSUM_NONE;

	return (0);
}

/*
 * Precompute the checksum seed from the journal UUID.
 *
 * jbd2 defines the seed as crc32c(~0, uuid).  ext4fs_crc32c() works
 * on inverted intermediate values, so store the inverted seed here.
 */
static void
jbd2_seed_init(struct jbd2_replay_ctx *ctx)
{
	ctx->rc_csum_seed =
	    ext4fs_crc32c(0, ctx->rc_uuid, sizeof(ctx->rc_uuid));
}

/*
 * Compute the checksum of one journaled data block: the same
 * crc32c(seed, be32 sequence || block bytes) chain jbd2 stores in
 * the descriptor tags, returned as the raw (non-inverted) value.
 */
static u_int32_t
jbd2_tag_csum(struct jbd2_replay_ctx *ctx, u_int32_t sequence, const void *data,
    size_t len)
{
	u_int32_t csum;
	u_int32_t seq_be = htobe32(sequence);

	csum = ext4fs_crc32c(ctx->rc_csum_seed, &seq_be, sizeof(seq_be));
	csum = ext4fs_crc32c(csum, data, len);
	return (~csum);
}

/*
 * Verify the checksum of a journaled data block against the value
 * recorded in its descriptor tag.  CSUM_V2 stores the low 16 bits,
 * CSUM_V3 the full 32 bits, both big-endian on disk.
 *
 * Returns 0 on success or EIO on mismatch.
 */
static int
jbd2_tag_csum_verify(struct jbd2_replay_ctx *ctx, u_int32_t sequence,
    const void *data, size_t len, u_int32_t tag_csum)
{
	u_int32_t calculated = jbd2_tag_csum(ctx, sequence, data, len);

	if (ctx->rc_csum_mode == JBD2_CSUM_V2)
		calculated &= 0xFFFF;

	if (calculated != tag_csum)
		return (EIO);

	return (0);
}

/*
 * Verify the 4-byte checksum tail of a descriptor or revoke block
 * (CSUM_V2/V3 only).  Returns 0 on success or EIO on mismatch.
 */
static int
jbd2_descr_tail_verify(
    struct jbd2_replay_ctx *ctx, void *buf, u_int32_t blocksize)
{
	struct jbd2_journal_block_tail *tail;
	u_int32_t			provided, calculated;

	if (ctx->rc_csum_mode == JBD2_CSUM_NONE)
		return (0);

	if (blocksize < sizeof(*tail))
		return (EIO);

	tail = (struct jbd2_journal_block_tail *)((char *)buf + blocksize -
	    sizeof(*tail));
	provided = betoh32(tail->t_checksum);
	tail->t_checksum = 0;
	calculated = ~ext4fs_crc32c(ctx->rc_csum_seed, buf, blocksize);
	tail->t_checksum = htobe32(provided);

	if (provided != calculated) {
		printf("ext4fs: journal descriptor block checksum mismatch: "
		       "stored=0x%08x calculated=0x%08x\n",
		    provided, calculated);
		return (EIO);
	}

	return (0);
}

/*
 * Parse one descriptor tag from a descriptor block.
 *
 * The tag format is selected from the active journal feature flags;
 * nothing is trusted from the tag itself until the whole fixed-size
 * portion (plus the UUID that follows unless SAME_UUID is set) has
 * been proven to fit inside the descriptor block.  bufsize is the
 * usable byte count (the checksum tail, if any, is already excluded).
 *
 * Returns 0 on success, setting *target (filesystem block number),
 * *flags (tag flags), *tag_csum (stored checksum value, 0 when the
 * journal has no per-tag checksums) and advancing *offset past the
 * tag and its optional UUID.  Returns EINVAL on a truncated or
 * otherwise malformed tag; on error *offset is unchanged.
 */
static int
jbd2_parse_tag(struct jbd2_replay_ctx *ctx, const char *buf, u_int32_t bufsize,
    u_int32_t *offset, u_int64_t *target, u_int32_t *flags, u_int32_t *tag_csum)
{
	const struct jbd2_block_tag  *tag;
	const struct jbd2_block_tag3 *tag3;
	u_int32_t		      o = *offset;
	u_int32_t		      tag_size = jbd2_tag_bytes(ctx);

	/*
	 * Prove the complete fixed-size tag fits before touching
	 * anything; the UUID (when present) is checked after the
	 * flags are known.
	 */
	if (o > bufsize || tag_size > bufsize - o)
		return (EINVAL);

	*tag_csum = 0;

	if (ctx->rc_csum_mode == JBD2_CSUM_V3) {
		tag3 = (const struct jbd2_block_tag3 *)(buf + o);
		*target = betoh32(tag3->t_blocknr);
		if (ctx->rc_features_incompat & JBD2_FEATURE_INCOMPAT_64BIT)
			*target |= (u_int64_t)betoh32(tag3->t_blocknr_high)
			    << 32;
		else if (betoh32(tag3->t_blocknr_high) != 0) {
			printf("ext4fs: journal tag3 has nonzero high "
			       "block without 64BIT\n");
			return (EINVAL);
		}
		/* Flags occupy the high half of the nominal 32-bit
		 * field; jbd2 writers only ever use the low bits. */
		*flags = betoh32(tag3->t_flags) >> 16;
		*tag_csum = betoh32(tag3->t_checksum);
	} else if (ctx->rc_csum_mode == JBD2_CSUM_V2) {
		tag = (const struct jbd2_block_tag *)(buf + o);
		*target = betoh32(tag->t_blocknr);
		if (ctx->rc_features_incompat & JBD2_FEATURE_INCOMPAT_64BIT)
			*target |= (u_int64_t)betoh32(tag->t_blocknr_high)
			    << 32;
		*flags = betoh16(tag->t_flags);
		*tag_csum = betoh16(tag->t_checksum);
	} else {
		/*
		 * Plain jbd2 tag: blocknr followed by a full 32-bit
		 * flags word; only the low 16 bits are meaningful.
		 */
		tag = (const struct jbd2_block_tag *)(buf + o);
		*target = betoh32(tag->t_blocknr);
		if (ctx->rc_features_incompat & JBD2_FEATURE_INCOMPAT_64BIT)
			*target |= (u_int64_t)betoh32(tag->t_blocknr_high)
			    << 32;
		*flags = (u_int32_t)betoh16(*(const u_int16_t *)(buf + o + 6));
	}

	if (*flags &
	    ~(JBD2_FLAG_ESCAPE | JBD2_FLAG_SAME_UUID | JBD2_FLAG_DELETED |
		JBD2_FLAG_LAST_TAG)) {
		printf("ext4fs: journal tag has unknown flags 0x%x\n", *flags);
		return (EINVAL);
	}

	o += tag_size;

	/* The UUID follows unless SAME_UUID is set. */
	if (!(*flags & JBD2_FLAG_SAME_UUID)) {
		if (o > bufsize || 16 > bufsize - o)
			return (EINVAL);
		o += 16;
	}

	*offset = o;
	return (0);
}

struct jbd2_revoke_entry {
	RB_ENTRY(jbd2_revoke_entry) re_node;
	u_int64_t re_block;
	u_int32_t re_sequence;
};

static int
jbd2_revoke_cmp(struct jbd2_revoke_entry *a, struct jbd2_revoke_entry *b)
{
	if (a->re_block < b->re_block)
		return (-1);
	if (a->re_block > b->re_block)
		return (1);
	return (0);
}

RB_GENERATE_STATIC(jbd2_revoke_tree, jbd2_revoke_entry, re_node,
    jbd2_revoke_cmp)

/*
 * Keep one entry per block. Compare sequence distances from the start of
 * recovery so the newest revocation survives across sequence wraparound.
 */
static int
jbd2_revoke_add(
    struct jbd2_replay_ctx *ctx, u_int64_t block, u_int32_t sequence)
{
	struct jbd2_revoke_entry key = { .re_block = block };
	struct jbd2_revoke_entry *entry;

	entry = RB_FIND(jbd2_revoke_tree, &ctx->rc_revoke, &key);
	if (entry != NULL) {
		if (sequence - ctx->rc_sequence >
		    entry->re_sequence - ctx->rc_sequence)
			entry->re_sequence = sequence;
		return (0);
	}

	entry = mallocarray(1, sizeof(*entry), M_TEMP, M_WAITOK | M_CANFAIL);
	if (entry == NULL)
		return (ENOMEM);
	entry->re_block = block;
	entry->re_sequence = sequence;
	RB_INSERT(jbd2_revoke_tree, &ctx->rc_revoke, entry);
	ctx->rc_revoke_count++;
	return (0);
}

/*
 * Check if a block is revoked at or after the given sequence.
 */
static int
jbd2_revoke_check(
    struct jbd2_replay_ctx *ctx, u_int64_t block, u_int32_t sequence)
{
	struct jbd2_revoke_entry key = { .re_block = block };
	struct jbd2_revoke_entry *entry;

	entry = RB_FIND(jbd2_revoke_tree, &ctx->rc_revoke, &key);
	return (entry != NULL &&
	    entry->re_sequence - ctx->rc_sequence >= sequence - ctx->rc_sequence);
}

static void
jbd2_revoke_clear(struct jbd2_replay_ctx *ctx)
{
	struct jbd2_revoke_entry *entry;

	while ((entry = RB_ROOT(&ctx->rc_revoke)) != NULL) {
		RB_REMOVE(jbd2_revoke_tree, &ctx->rc_revoke, entry);
		free(entry, M_TEMP, sizeof(*entry));
	}
	ctx->rc_revoke_count = 0;
}

/* CRC32C is stored complemented relative to ext4fs_crc32c(). */
static u_int32_t
jbd2_superblock_csum(struct jbd2_superblock *sb)
{
	u_int32_t saved, crc;

	saved = sb->s_checksum;
	sb->s_checksum = 0;
	crc = ~ext4fs_crc32c(0, sb, sizeof(*sb));
	sb->s_checksum = saved;
	return (htobe32(crc));
}

/* Checksum v1 uses the non-reflected IEEE polynomial. */
static u_int32_t
jbd2_v1_crc(u_int32_t crc, const void *data, size_t len)
{
	const u_int8_t *p = data;
	unsigned int bit;

	while (len-- != 0) {
		crc ^= (u_int32_t)*p++ << 24;
		for (bit = 0; bit < 8; bit++)
			crc = (crc << 1) ^ ((crc & 0x80000000U)
			      ? 0x04c11db7U : 0);
	}
	return (crc);
}

static int
jbd2_commit_valid(struct jbd2_replay_ctx *ctx, void *data, u_int32_t v1_crc)
{
	struct jbd2_commit_prefix *commit = data;
	u_int32_t saved, crc;

	if (ctx->rc_csum_mode == JBD2_CSUM_NONE) {
		if (!(ctx->rc_features_compat & JBD2_FEATURE_COMPAT_CHECKSUM))
			return (1);
		/* Linux accepts old commits with no checksum recorded. */
		if (commit->h_chksum_type == 0 && commit->h_chksum_size == 0 &&
		    commit->h_chksum == 0)
			return (1);
		return (commit->h_chksum_type == JBD2_CRC32_CHKSUM &&
			commit->h_chksum_size == sizeof(commit->h_chksum) &&
			commit->h_chksum == htobe32(v1_crc));
	}

	saved = commit->h_chksum;
	commit->h_chksum = 0;
	crc = ~ext4fs_crc32c(ctx->rc_csum_seed, data, ctx->rc_blocksize);
	commit->h_chksum = saved;
	return (saved == htobe32(crc));
}

/*
 * Count data blocks described by a descriptor block's tags.
 *
 * Returns 0 with *count set, or an error when the tag stream is
 * malformed (truncated tag, unknown flags, or a descriptor without
 * a LAST_TAG marker).  The descriptor checksum tail, if any, is
 * excluded from the usable area.
 */
static int
jbd2_count_tags(struct jbd2_replay_ctx *ctx, struct buf *bp, u_int32_t *count)
{
	char	 *buf;
	u_int32_t offset, bufsize, flags, tag_csum;
	u_int64_t target;
	int	  error;

	buf = (char *)bp->b_data;
	bufsize = ctx->rc_blocksize;
	if (ctx->rc_csum_mode != JBD2_CSUM_NONE) {
		if (bufsize < sizeof(struct jbd2_journal_block_tail))
			return (EINVAL);
		bufsize -= sizeof(struct jbd2_journal_block_tail);
	}
	offset = sizeof(struct jbd2_header);
	*count = 0;

	while (offset < bufsize) {
		error = jbd2_parse_tag(
		    ctx, buf, bufsize, &offset, &target, &flags, &tag_csum);
		if (error)
			return (error);
		(*count)++;
		if (flags & JBD2_FLAG_LAST_TAG)
			return (0);
	}

	printf("ext4fs: journal descriptor block has no LAST_TAG\n");
	return (EINVAL);
}

/*
 * Pass 1: SCAN
 *
 * Walk from s_start/s_sequence. A transaction may span several descriptor
 * and revoke blocks before its commit. Only a validated commit advances
 * rc_end_sequence, the exclusive end of the range for the later passes.
 */
static int
jbd2_pass_scan(struct jbd2_replay_ctx *ctx)
{
	u_int32_t		 block, seq;
	u_int32_t		 tag_count;
	u_int32_t		 scanned, v1_crc = UINT32_MAX;
	struct buf		*bp;
	struct jbd2_header	*hdr;
	int			 error;
	int			 v1 =
	    (ctx->rc_features_compat & JBD2_FEATURE_COMPAT_CHECKSUM) != 0;

	block = ctx->rc_start;
	seq = ctx->rc_sequence;
	ctx->rc_end_sequence = seq;

	printf(
	    "ext4fs: journal scan: start block %u sequence %u\n", block, seq);

	/*
	 * Walk the ring at most once: a journal made of nothing but
	 * well-formed headers (e.g. only revoke blocks) must not
	 * loop forever at mount time.
	 */
	for (scanned = 0; scanned < ctx->rc_maxlen - ctx->rc_first; scanned++) {
		error = jbd2_read_block(ctx, block, &bp);
		if (error) {
			printf("ext4fs: journal scan: read error at "
			       "block %u\n",
			    block);
			if (bp != NULL)
				brelse(bp);
			return (error);
		}

		hdr = (struct jbd2_header *)bp->b_data;
		if (betoh32(hdr->h_magic) != JBD2_MAGIC) {
			brelse(bp);
			break;
		}
		if (betoh32(hdr->h_sequence) != seq) {
			brelse(bp);
			break;
		}

		switch (betoh32(hdr->h_blocktype)) {
		case JBD2_DESCRIPTOR_BLOCK:
			/*
			 * Verify the descriptor block checksum before
			 * trusting its tag stream: a corrupt tag list
			 * would otherwise drive the scan over the
			 * wrong blocks.
			 */
			if (jbd2_descr_tail_verify(
				ctx, bp->b_data, ctx->rc_blocksize) != 0) {
				brelse(bp);
				printf("ext4fs: journal scan: descriptor "
				       "block checksum failed at block %u\n",
				    block);
				return (EIO);
			}

			/* Count tags to know how many data blocks follow */
			error = jbd2_count_tags(ctx, bp, &tag_count);
			if (!error && v1)
				v1_crc = jbd2_v1_crc(v1_crc, bp->b_data,
					 ctx->rc_blocksize);
			brelse(bp);
			if (error) {
				printf("ext4fs: journal scan: malformed "
				       "descriptor block at block %u\n",
				    block);
				return (error);
			}

			if (tag_count >= ctx->rc_maxlen - ctx->rc_first -
					 scanned)
				return (EIO);
			scanned += tag_count;
			while (tag_count-- != 0) {
				block = jbd2_next_block(ctx, block);
				if (!v1)
					continue;
				error = jbd2_read_block(ctx, block, &bp);
				if (error) {
					if (bp != NULL)
						brelse(bp);
					return (error);
				}
				v1_crc = jbd2_v1_crc(v1_crc, bp->b_data,
					 ctx->rc_blocksize);
				brelse(bp);
			}
			block = jbd2_next_block(ctx, block);
			break;

		case JBD2_REVOKE_BLOCK:
			/* Revoke blocks carry the same 4-byte tail. */
			if (jbd2_descr_tail_verify(
				ctx, bp->b_data, ctx->rc_blocksize) != 0) {
				brelse(bp);
				printf("ext4fs: journal scan: revoke "
				       "block checksum failed at block %u\n",
				    block);
				return (EIO);
			}
			/*
			 * A revoke block by itself is part of a transaction.
			 * There may be multiple revoke blocks before the
			 * commit.
			 */
			brelse(bp);
			block = jbd2_next_block(ctx, block);
			break;

		case JBD2_COMMIT_BLOCK:
			if (!jbd2_commit_valid(ctx, bp->b_data, v1_crc)) {
				brelse(bp);
				return (EIO);
			}
			brelse(bp);
			v1_crc = UINT32_MAX;
			ctx->rc_end_sequence = ++seq;
			block = jbd2_next_block(ctx, block);
			break;

		default:
			brelse(bp);
			goto done;
		}
	}

done:
	printf("ext4fs: journal scan: end sequence %u (%u transactions)\n",
	    ctx->rc_end_sequence, ctx->rc_end_sequence - ctx->rc_sequence);
	return (0);
}

/*
 * Pass 2: REVOKE
 *
 * Walk the journal again, collecting revoked blocks.
 */
static int
jbd2_pass_revoke(struct jbd2_replay_ctx *ctx)
{
	u_int32_t		   block, seq;
	u_int32_t		   tag_count;
	u_int32_t		   scanned;
	struct buf		  *bp;
	struct jbd2_header	  *hdr;
	struct jbd2_revoke_header *rh;
	int			   has_64bit;
	int			   error;

	has_64bit = ctx->rc_features_incompat & JBD2_FEATURE_INCOMPAT_64BIT;

	block = ctx->rc_start;
	seq = ctx->rc_sequence;

	/* Count metadata and journal data against one traversal of the ring. */
	for (scanned = 0;
	     seq != ctx->rc_end_sequence &&
	     scanned < ctx->rc_maxlen - ctx->rc_first;
	     scanned++) {
		error = jbd2_read_block(ctx, block, &bp);
		if (error) {
			if (bp != NULL)
				brelse(bp);
			return (error);
		}

		hdr = (struct jbd2_header *)bp->b_data;
		if (betoh32(hdr->h_magic) != JBD2_MAGIC ||
		    betoh32(hdr->h_sequence) != seq) {
			brelse(bp);
			return (EIO);
		}

		switch (betoh32(hdr->h_blocktype)) {
		case JBD2_DESCRIPTOR_BLOCK:
			error = jbd2_count_tags(ctx, bp, &tag_count);
			brelse(bp);
			if (error)
				return (error);
			if (tag_count >= ctx->rc_maxlen - ctx->rc_first -
					 scanned)
				return (EIO);
			scanned += tag_count;
			while (tag_count-- != 0)
				block = jbd2_next_block(ctx, block);
			block = jbd2_next_block(ctx, block);
			break;

		case JBD2_REVOKE_BLOCK:
			if (jbd2_descr_tail_verify(
				ctx, bp->b_data, ctx->rc_blocksize) != 0) {
				brelse(bp);
				printf("ext4fs: journal revoke: block "
				       "checksum failed at block %u\n",
				    block);
				return (EIO);
			}
			rh = (struct jbd2_revoke_header *)bp->b_data;
			{
				u_int32_t rcount, usable, off;
				rcount = betoh32(rh->r_count);
				/*
				 * The recorded length must fit inside
				 * the block (checksum tail included);
				 * anything larger is a corrupt journal,
				 * not a reason to read past the buffer.
				 */
				usable = ctx->rc_blocksize;
				if (ctx->rc_csum_mode != JBD2_CSUM_NONE) {
					usable -= sizeof(
					    struct jbd2_journal_block_tail);
				}
				off = sizeof(struct jbd2_revoke_header);
				if (rcount < off || rcount > usable ||
				    (rcount - off) % (has_64bit ? 8 : 4) != 0) {
					brelse(bp);
					return (EIO);
				}
				while (off < rcount) {
					u_int64_t revblk;
					if (has_64bit) {
						revblk =
						    (u_int64_t)betoh32(*(
							u_int32_t
							    *)((char *)
								   bp->b_data +
							off))
							<< 32 |
						    betoh32(*(u_int32_t
							    *)((char *)
								   bp->b_data +
							off + 4));
						off += 8;
					} else {
						revblk = betoh32(*(u_int32_t
							*)((char *)bp->b_data +
						    off));
						off += 4;
					}
					error = jbd2_revoke_add(ctx, revblk,
						seq);
					if (error) {
						brelse(bp);
						return (error);
					}
				}
			}
			brelse(bp);
			block = jbd2_next_block(ctx, block);
			break;

		case JBD2_COMMIT_BLOCK:
			brelse(bp);
			block = jbd2_next_block(ctx, block);
			seq++;
			break;

		default:
			brelse(bp);
			return (EIO);
		}
	}

	if (ctx->rc_revoke_count > 0)
		printf("ext4fs: journal revoke: %zu blocks revoked\n",
		    ctx->rc_revoke_count);

	return (seq == ctx->rc_end_sequence ? 0 : EIO);
}

/*
 * Pass 3: REPLAY
 *
 * Walk the committed range a third time. Verify each surviving data block
 * as required by the checksum profile, then write it to its filesystem
 * destination. An error may leave earlier block writes on disk.
 */
static int
jbd2_pass_replay(struct jbd2_replay_ctx *ctx)
{
	struct m_ext4fs	   *fs = ctx->rc_fs;
	u_int32_t	    block, seq;
	u_int32_t	    scanned;
	u_int32_t	    replayed = 0;
	struct buf	   *bp, *dbp, *wbp;
	struct jbd2_header *hdr;
	char		   *buf;
	u_int32_t	    offset, bufsize, flags;
	u_int64_t	    target;
	int		    error;

	block = ctx->rc_start;
	seq = ctx->rc_sequence;

	/* Count metadata and journal data against one traversal of the ring. */
	for (scanned = 0;
	     seq != ctx->rc_end_sequence &&
	     scanned < ctx->rc_maxlen - ctx->rc_first;
	     scanned++) {
		error = jbd2_read_block(ctx, block, &bp);
		if (error) {
			if (bp != NULL)
				brelse(bp);
			return (error);
		}

		hdr = (struct jbd2_header *)bp->b_data;
		if (betoh32(hdr->h_magic) != JBD2_MAGIC ||
		    betoh32(hdr->h_sequence) != seq) {
			brelse(bp);
			return (EIO);
		}

		switch (betoh32(hdr->h_blocktype)) {
		case JBD2_DESCRIPTOR_BLOCK:
			buf = (char *)bp->b_data;
			bufsize = ctx->rc_blocksize;
			if (ctx->rc_csum_mode != JBD2_CSUM_NONE)
				bufsize -=
				    sizeof(struct jbd2_journal_block_tail);
			offset = sizeof(struct jbd2_header);

			if (jbd2_descr_tail_verify(
				ctx, bp->b_data, ctx->rc_blocksize) != 0) {
				brelse(bp);
				printf("ext4fs: journal replay: descriptor "
				       "block checksum failed at block %u\n",
				    block);
				return (EIO);
			}

			/* Iterate over tags, each followed by a data block */
			while (offset < bufsize) {
				u_int32_t tag_csum;

				error = jbd2_parse_tag(ctx, buf, bufsize,
				    &offset, &target, &flags, &tag_csum);
				if (error) {
					printf("ext4fs: journal replay: "
					       "malformed tag at block %u\n",
					    block);
					brelse(bp);
					return (EIO);
				}

				if (++scanned >=
				    ctx->rc_maxlen - ctx->rc_first) {
					brelse(bp);
					return (EIO);
				}
				/* Advance to data block */
				block = jbd2_next_block(ctx, block);

				/*
				 * The journal is attacker-controlled
				 * input: never allow a replay write to a
				 * block outside the filesystem (that would
				 * scribble over unrelated device data).
				 */
				if (target < fs->m_first_data_block ||
				    target >= fs->m_blocks_count) {
					printf("ext4fs: journal replay: "
					       "target block %llu out of "
					       "range\n",
					    (unsigned long long)target);
					brelse(bp);
					return (EIO);
				}

				/* Skip if revoked */
				if (jbd2_revoke_check(ctx, target, seq)) {
					if (flags & JBD2_FLAG_LAST_TAG)
						break;
					continue;
				}

				/* Read the journal data block */
				error = jbd2_read_block(ctx, block, &dbp);
				if (error) {
					printf("ext4fs: journal replay: "
					       "read error block %u\n",
					    block);
					if (dbp != NULL)
						brelse(dbp);
					brelse(bp);
					return (error);
				}

				/*
				 * Verify the tag checksum over the
				 * stored (possibly escaped) data block
				 * before anything is written back.  A
				 * mismatch means the journal payload
				 * was corrupted: abort the whole
				 * recovery rather than replaying
				 * untrusted data into the filesystem.
				 */
				if (jbd2_tag_csum_verify(ctx, seq, dbp->b_data,
					fs->m_block_size, tag_csum) != 0) {
					printf("ext4fs: journal replay: "
					       "tag checksum failed for "
					       "target %llu (journal "
					       "block %u, sequence %u)\n",
					    (unsigned long long)target, block,
					    seq);
					brelse(dbp);
					brelse(bp);
					return (EIO);
				}

				/* Read the target filesystem block */
				error = bread(ctx->rc_devvp,
				    (daddr_t)EXT4FS_FSBTODB(fs, target),
				    fs->m_block_size, &wbp);
				if (error) {
					brelse(dbp);
					printf("ext4fs: journal replay: "
					       "can't read target %llu\n",
					    (unsigned long long)target);
					brelse(wbp);
					brelse(bp);
					return (error);
				}

				/* Copy data */
				memcpy(
				    wbp->b_data, dbp->b_data, fs->m_block_size);
				brelse(dbp);

				/* Un-escape: restore JBD2 magic if needed */
				if (flags & JBD2_FLAG_ESCAPE) {
					u_int32_t magic = htobe32(JBD2_MAGIC);
					memcpy(wbp->b_data, &magic, 4);
				}

				/* Write to filesystem */
				error = bwrite(wbp);
				if (error) {
					printf("ext4fs: journal replay: "
					       "write error target %llu\n",
					    (unsigned long long)target);
					brelse(bp);
					return (error);
				}
				replayed++;

				if (flags & JBD2_FLAG_LAST_TAG)
					break;
			}

			brelse(bp);

			block = jbd2_next_block(ctx, block);
			break;

		case JBD2_REVOKE_BLOCK:
			brelse(bp);
			block = jbd2_next_block(ctx, block);
			break;

		case JBD2_COMMIT_BLOCK:
			brelse(bp);
			block = jbd2_next_block(ctx, block);
			seq++;
			break;

		default:
			brelse(bp);
			return (EIO);
		}
	}

	ctx->rc_replay_count = replayed;
	printf("ext4fs: journal replay: %u blocks replayed\n", replayed);

	return (seq == ctx->rc_end_sequence ? 0 : EIO);
}

/*
 * Main entry point: replay the ext4 journal.
 *
 * Called during mount when the RECOVER incompat flag is set.
 * Use the journal inode backup in the ext4 superblock to locate the JBD2
 * superblock. The caller supplies a device opened for writes, even for a
 * requested read-only mount. After replay, mark the journal clean before
 * clearing the filesystem recovery flag. The caller reloads filesystem
 * metadata from disk on success.
 */
int
ext4fs_journal_replay(struct vnode *devvp, struct m_ext4fs *fs)
{
	struct jbd2_replay_ctx	ctx;
	struct jbd2_superblock *jsb;
	struct buf	       *bp;
	u_int64_t		jblock0;
	int			error;

	memset(&ctx, 0, sizeof(ctx));
	ctx.rc_devvp = devvp;
	ctx.rc_fs = fs;

	/*
	 * Bootstrap the map using the byte size in the inode backup. Only
	 * after reading journal block zero can we know the JBD2 ring length.
	 */
	{
		u_int64_t jsize;
		jsize = letoh32(fs->m_sble.sb_jnl_blocks[15]) |
		    (u_int64_t)letoh32(fs->m_sble.sb_jnl_blocks[16]) << 32;

		/*
		 * The journal lives inside the filesystem: a size that
		 * exceeds the filesystem is a corrupt on-disk field and
		 * would otherwise drive an attacker-sized allocation
		 * below.
		 */
		if (jsize / fs->m_block_size > fs->m_blocks_count ||
		    jsize / fs->m_block_size > UINT32_MAX) {
			printf("ext4fs: journal size exceeds filesystem\n");
			return (EINVAL);
		}
		ctx.rc_maxlen = jsize / fs->m_block_size;
	}

	if (ctx.rc_maxlen == 0) {
		printf("ext4fs: journal has zero size\n");
		return (EINVAL);
	}

	/* Build journal block → filesystem block mapping */
	error = jbd2_build_blockmap(&ctx);
	if (error)
		goto out;

	/* Read journal superblock (journal block 0) */
	jblock0 = ctx.rc_blockmap[0].jb_fsblock;
	error = jbd2_read_block(&ctx, 0, &bp);
	if (error) {
		printf("ext4fs: can't read journal superblock\n");
		if (bp != NULL)
			brelse(bp);
		goto out;
	}

	jsb = (struct jbd2_superblock *)bp->b_data;

	/* Validate journal superblock */
	if (betoh32(jsb->s_header.h_magic) != JBD2_MAGIC) {
		printf("ext4fs: bad journal magic 0x%x\n",
		    betoh32(jsb->s_header.h_magic));
		brelse(bp);
		error = EINVAL;
		goto out;
	}
	{
		u_int32_t btype = betoh32(jsb->s_header.h_blocktype);
		if (btype != JBD2_SUPERBLOCK_V1 &&
		    btype != JBD2_SUPERBLOCK_V2) {
			printf("ext4fs: bad journal superblock version %u\n",
			    btype);
			brelse(bp);
			error = EINVAL;
			goto out;
		}
	}

	ctx.rc_blocksize = betoh32(jsb->s_blocksize);
	ctx.rc_maxlen = betoh32(jsb->s_maxlen);
	ctx.rc_first = betoh32(jsb->s_first);
	ctx.rc_sequence = betoh32(jsb->s_sequence);
	ctx.rc_start = betoh32(jsb->s_start);

	/*
	 * The on-disk journal parameters must fit inside the journal
	 * area derived from the filesystem superblock; anything
	 * larger would walk or allocate beyond the journal.
	 */
	{
		u_int64_t jsize = letoh32(fs->m_sble.sb_jnl_blocks[15]) |
		    (u_int64_t)letoh32(fs->m_sble.sb_jnl_blocks[16]) << 32;

		if (ctx.rc_maxlen > jsize / fs->m_block_size) {
			printf("ext4fs: journal maxlen exceeds journal "
			       "size\n");
			brelse(bp);
			error = EINVAL;
			goto out;
		}
	}

	if (betoh32(jsb->s_header.h_blocktype) == JBD2_SUPERBLOCK_V2) {
		ctx.rc_features_compat = betoh32(jsb->s_feature_compat);
		ctx.rc_features_incompat = betoh32(jsb->s_feature_incompat);
		memcpy(ctx.rc_uuid, jsb->s_uuid, sizeof(ctx.rc_uuid));
	} else {
		ctx.rc_features_compat = 0;
		ctx.rc_features_incompat = 0;
		memset(ctx.rc_uuid, 0, sizeof(ctx.rc_uuid));
	}

	/*
	 * Derive the descriptor tag format and checksum mode from the
	 * journal features; anything this driver does not understand
	 * must abort the mount instead of mis-replaying the journal.
	 */
	error = jbd2_feature_check(&ctx);
	if (error) {
		brelse(bp);
		goto out;
	}
	if (ctx.rc_csum_mode != JBD2_CSUM_NONE) {
		if (fs->m_block_size < sizeof(*jsb) ||
		    jsb->s_checksum_type != JBD2_CRC32C_CHKSUM ||
		    jsb->s_checksum != jbd2_superblock_csum(jsb)) {
			brelse(bp);
			error = EINVAL;
			goto out;
		}
		jbd2_seed_init(&ctx);
	}

	brelse(bp);

	if (ctx.rc_blocksize != fs->m_block_size) {
		printf("ext4fs: journal blocksize %u != fs blocksize %llu\n",
		    ctx.rc_blocksize, (unsigned long long)fs->m_block_size);
		error = EINVAL;
		goto out;
	}

	/* If s_start == 0, journal is clean — nothing to replay */
	if (ctx.rc_start == 0) {
		printf("ext4fs: journal is clean, no replay needed\n");
		goto clear_recover;
	}
	if (ctx.rc_first == 0 || ctx.rc_first >= ctx.rc_maxlen ||
	    ctx.rc_start < ctx.rc_first || ctx.rc_start >= ctx.rc_maxlen) {
		error = EINVAL;
		goto out;
	}

	printf("ext4fs: replaying journal (sequence %u, start block %u, "
	       "%u journal blocks)\n",
	    ctx.rc_sequence, ctx.rc_start, ctx.rc_maxlen);

	/* Pass 1: SCAN */
	error = jbd2_pass_scan(&ctx);
	if (error)
		goto out;

	if (ctx.rc_end_sequence == ctx.rc_sequence) {
		printf("ext4fs: journal has no valid transactions\n");
		goto clear;
	}

	/* Pass 2: REVOKE */
	error = jbd2_pass_revoke(&ctx);
	if (error)
		goto out;

	/* Pass 3: REPLAY */
	error = jbd2_pass_replay(&ctx);
	if (error)
		goto out;

clear:
	/*
	 * Publish a clean journal first. If the filesystem superblock write
	 * then fails, a subsequent mount can see s_start == 0 and retry
	 * clearing RECOVER without replaying the log again.
	 */
	error = bread(
	    devvp, (daddr_t)EXT4FS_FSBTODB(fs, jblock0), fs->m_block_size, &bp);
	if (error) {
		printf("ext4fs: can't reread journal superblock\n");
		if (bp != NULL)
			brelse(bp);
		goto out;
	}

	jsb = (struct jbd2_superblock *)bp->b_data;
	jsb->s_start = htobe32(0);
	/* Advance sequence past what we replayed */
	jsb->s_sequence = htobe32(ctx.rc_end_sequence);
	if (ctx.rc_csum_mode != JBD2_CSUM_NONE)
		jsb->s_checksum = jbd2_superblock_csum(jsb);
	error = bwrite(bp);
	if (error) {
		printf("ext4fs: can't write journal superblock\n");
		goto out;
	}

clear_recover:
	/* Preserve any superblock changes made by replay. */
	error = bread(devvp, (daddr_t)(EXT4FS_SUPER_BLOCK_OFFSET / DEV_BSIZE),
		EXT4FS_SUPER_BLOCK_SIZE, &bp);
	if (error) {
		if (bp != NULL)
			brelse(bp);
		goto out;
	}
	{
		struct ext4fs *sble = (struct ext4fs *)bp->b_data;
		u_int32_t incompat = letoh32(sble->sb_feature_incompat);

		incompat &= ~EXT4FS_FEATURE_INCOMPAT_RECOVER;
		sble->sb_feature_incompat = htole32(incompat);
		sble->sb_state = htole16(EXT4FS_STATE_VALID);
		sble->sb_checksum = htole32(ext4fs_sb_csum(sble));
	}
	error = bwrite(bp);
	if (error)
		goto out;

	printf("ext4fs: journal replay complete\n");

out:
	if (ctx.rc_blockmap != NULL)
		free(ctx.rc_blockmap, M_TEMP,
		    ctx.rc_blockmap_count * sizeof(struct jbd2_blockmap_entry));
	jbd2_revoke_clear(&ctx);

	return (error);
}
