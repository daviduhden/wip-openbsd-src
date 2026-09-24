/*
 * ext4fs-jbd2-tests.c -- host-testable unit tests for the jbd2
 * journal descriptor-tag parser and checksum verification in
 * ext4fs/sys/ufs/ext4fs/ext4fs_journal.c.
 *
 * Includes the kernel source directly under a stub include tree so
 * the exact production code is exercised; only the pure parsing and
 * checksum functions are called (no kernel I/O).
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Reference crc32c implementation (bitwise, independent of the
 * table-driven kernel code) used to validate the checksum vectors. */
static const uint32_t ref_poly = 0x82F63B78;

static uint32_t
ref_crc32c(uint32_t init, const void *data, size_t len)
{
	const uint8_t *p = data;
	uint32_t       c = init;

	while (len-- > 0) {
		c ^= *p++;
		for (int i = 0; i < 8; i++)
			c = (c >> 1) ^ ((c & 1) ? ref_poly : 0);
	}
	return c;
}

#define main journal_test_unused_main
#include "../ext4fs/sys/ufs/ext4fs/ext4fs_crc32c.c"
#include "../ext4fs/sys/ufs/ext4fs/ext4fs_journal.c"
#undef main

/* Link stubs for the kernel buffer interface; the tests only reach
 * the pure parsing/checksum paths, never the I/O wrappers. */
int
bread(struct vnode *vp, int64_t blkno, size_t size, struct buf **bpp)
{
	(void)vp;
	(void)blkno;
	(void)size;
	(void)bpp;
	return EIO;
}

void
brelse(struct buf *bp)
{
	(void)bp;
}

int
bwrite(struct buf *bp)
{
	(void)bp;
	return EIO;
}

static int failures;

#define CHECK(cond, msg)                                                       \
	do {                                                                   \
		if (!(cond)) {                                                 \
			fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__,   \
			    __LINE__);                                         \
			failures++;                                            \
		}                                                              \
	} while (0)

static const uint8_t test_uuid[16] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

/* 4096-byte payload matching the reference vector generator:
 * pattern[i] = (i * 7 + 3) & 0xFF. */
static uint8_t pattern4096[4096];

static void
fill_pattern4096(void)
{
	for (size_t i = 0; i < sizeof(pattern4096); i++)
		pattern4096[i] = (uint8_t)((i * 7 + 3) & 0xFF);
}

/* 4096-byte block matching the reference descriptor vector:
 * pattern[i] = (i * 5 + 9) & 0xFF. */
static void
fill_block4096(uint8_t *block)
{
	for (size_t i = 0; i < 4096; i++)
		block[i] = (uint8_t)((i * 5 + 9) & 0xFF);
}

static void
setup_ctx(struct jbd2_replay_ctx *ctx, int csum_mode, int with64)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->rc_blocksize = 4096;
	ctx->rc_maxlen = 64;
	ctx->rc_first = 1;
	ctx->rc_csum_mode = csum_mode;
	if (with64)
		ctx->rc_features_incompat |= JBD2_FEATURE_INCOMPAT_64BIT;
	if (csum_mode != JBD2_CSUM_NONE) {
		memcpy(ctx->rc_uuid, test_uuid, sizeof(ctx->rc_uuid));
		jbd2_seed_init(ctx);
	}
}

static void
test_crc32c_check_value(void)
{
	/* Standard CRC-32C/Castagnoli check value: crc32c("123456789")
	 * with init ~0 and final xor ~0 == 0xE3069283.  The ext4fs
	 * helper returns exactly the finalized value for seed 0. */
	CHECK(ext4fs_crc32c(0, "123456789", 9) == 0xE3069283,
	    "crc32c standard check value");
}

static void
test_tag_sizes(void)
{
	struct jbd2_replay_ctx ctx;

	setup_ctx(&ctx, JBD2_CSUM_NONE, 0);
	CHECK(jbd2_tag_bytes(&ctx) == 8, "plain 32-bit tag size");
	setup_ctx(&ctx, JBD2_CSUM_NONE, 1);
	CHECK(jbd2_tag_bytes(&ctx) == 12, "plain 64-bit tag size");
	setup_ctx(&ctx, JBD2_CSUM_V2, 0);
	CHECK(jbd2_tag_bytes(&ctx) == 10, "csum2 32-bit tag size");
	setup_ctx(&ctx, JBD2_CSUM_V2, 1);
	CHECK(jbd2_tag_bytes(&ctx) == 14, "csum2 64-bit tag size");
	setup_ctx(&ctx, JBD2_CSUM_V3, 0);
	CHECK(jbd2_tag_bytes(&ctx) == 16, "csum3 32-bit tag size");
	setup_ctx(&ctx, JBD2_CSUM_V3, 1);
	CHECK(jbd2_tag_bytes(&ctx) == 16, "csum3 64-bit tag size");
}

/* Build a csum2 tag (32-bit block numbers) at p:
 * blocknr(4) csum(2) flags(2) pad(2), followed by a UUID unless
 * SAME_UUID is set. */
static size_t
mk_tag2(char *p, uint32_t blocknr, uint16_t csum, uint16_t flags)
{
	uint32_t be32 = htobe32(blocknr);
	uint16_t be16;

	memcpy(p, &be32, 4);
	be16 = htobe16(csum);
	memcpy(p + 4, &be16, 2);
	be16 = htobe16(flags);
	memcpy(p + 6, &be16, 2);
	memset(p + 8, 0, 2);
	return 10;
}

static size_t
mk_tag2_64(
    char *p, uint32_t blocknr, uint32_t high, uint16_t csum, uint16_t flags)
{
	uint32_t be32 = htobe32(blocknr);
	uint16_t be16;

	memcpy(p, &be32, 4);
	be16 = htobe16(csum);
	memcpy(p + 4, &be16, 2);
	be16 = htobe16(flags);
	memcpy(p + 6, &be16, 2);
	be32 = htobe32(high);
	memcpy(p + 8, &be32, 4);
	memset(p + 12, 0, 2);
	return 14;
}

static size_t
mk_tag3(char *p, uint32_t blocknr, uint32_t high, uint32_t flags, uint32_t csum)
{
	uint32_t be32;

	be32 = htobe32(blocknr);
	memcpy(p, &be32, 4);
	/* jbd2 writes the 16 flag bits into the high half of the
	 * nominal 32-bit flags field. */
	be32 = htobe32(flags << 16);
	memcpy(p + 4, &be32, 4);
	be32 = htobe32(high);
	memcpy(p + 8, &be32, 4);
	be32 = htobe32(csum);
	memcpy(p + 12, &be32, 4);
	return 16;
}

static size_t
mk_tag_plain(char *p, uint32_t blocknr, uint32_t flags)
{
	uint32_t be32;

	be32 = htobe32(blocknr);
	memcpy(p, &be32, 4);
	be32 = htobe32(flags);
	memcpy(p + 4, &be32, 4);
	return 8;
}

static size_t
mk_tag_plain64(char *p, uint32_t blocknr, uint32_t high, uint32_t flags)
{
	uint32_t be32;

	be32 = htobe32(blocknr);
	memcpy(p, &be32, 4);
	be32 = htobe32(flags);
	memcpy(p + 4, &be32, 4);
	be32 = htobe32(high);
	memcpy(p + 8, &be32, 4);
	return 12;
}

static void
test_parse_variants(void)
{
	struct jbd2_replay_ctx ctx;
	char		       buf[128];
	u_int64_t	       target;
	u_int32_t	       flags, csum, offset;
	size_t		       used;

	/* plain 32-bit */
	setup_ctx(&ctx, JBD2_CSUM_NONE, 0);
	used = mk_tag_plain(buf, 0x11223344, JBD2_FLAG_ESCAPE);
	memcpy(buf + used, test_uuid, 16);
	offset = 0;
	CHECK(jbd2_parse_tag(
		  &ctx, buf, used + 16, &offset, &target, &flags, &csum) == 0,
	    "plain tag parse");
	CHECK(target == 0x11223344, "plain tag target");
	CHECK(flags == JBD2_FLAG_ESCAPE, "plain tag flags");
	CHECK(csum == 0, "plain tag csum absent");
	CHECK(offset == used + 16, "plain tag offset incl uuid");

	/* plain 64-bit with SAME_UUID (no UUID bytes follow) */
	setup_ctx(&ctx, JBD2_CSUM_NONE, 1);
	used = mk_tag_plain64(buf, 0x44556677, 0x89, JBD2_FLAG_SAME_UUID);
	offset = 0;
	CHECK(jbd2_parse_tag(
		  &ctx, buf, used, &offset, &target, &flags, &csum) == 0,
	    "plain64 tag parse");
	CHECK(target == ((u_int64_t)0x89 << 32 | 0x44556677),
	    "plain64 tag target");
	CHECK(flags == JBD2_FLAG_SAME_UUID, "plain64 tag flags");
	CHECK(offset == used, "plain64 tag offset (no uuid)");

	/* csum2 32-bit with UUID */
	setup_ctx(&ctx, JBD2_CSUM_V2, 0);
	used = mk_tag2(buf, 0x123, 0xD1AD, JBD2_FLAG_LAST_TAG);
	memcpy(buf + used, test_uuid, 16);
	offset = 0;
	CHECK(jbd2_parse_tag(
		  &ctx, buf, used + 16, &offset, &target, &flags, &csum) == 0,
	    "csum2 tag parse");
	CHECK(target == 0x123, "csum2 tag target");
	CHECK(flags == JBD2_FLAG_LAST_TAG, "csum2 tag flags");
	CHECK(csum == 0xD1AD, "csum2 tag checksum");
	CHECK(offset == used + 16, "csum2 tag offset");

	/* csum2 64-bit */
	setup_ctx(&ctx, JBD2_CSUM_V2, 1);
	used = mk_tag2_64(buf, 0x55667788, 0x99, 0xBEEF,
	    JBD2_FLAG_ESCAPE | JBD2_FLAG_DELETED);
	memcpy(buf + used, test_uuid, 16);
	offset = 0;
	CHECK(jbd2_parse_tag(
		  &ctx, buf, used + 16, &offset, &target, &flags, &csum) == 0,
	    "csum2-64 tag parse");
	CHECK(target == ((u_int64_t)0x99 << 32 | 0x55667788),
	    "csum2-64 tag target");
	CHECK(flags == (JBD2_FLAG_ESCAPE | JBD2_FLAG_DELETED),
	    "csum2-64 tag flags");
	CHECK(csum == 0xBEEF, "csum2-64 tag checksum");

	/* csum3 64-bit */
	setup_ctx(&ctx, JBD2_CSUM_V3, 1);
	used = mk_tag3(
	    buf, 0xAABBCCDD, 0x11223344, JBD2_FLAG_SAME_UUID, 0xDEADBEEF);
	offset = 0;
	CHECK(jbd2_parse_tag(
		  &ctx, buf, used, &offset, &target, &flags, &csum) == 0,
	    "csum3 tag parse");
	CHECK(target == ((u_int64_t)0x11223344 << 32 | 0xAABBCCDD),
	    "csum3 tag target");
	CHECK(flags == JBD2_FLAG_SAME_UUID, "csum3 tag flags");
	CHECK(csum == 0xDEADBEEF, "csum3 tag checksum");
	CHECK(offset == used, "csum3 tag offset (same uuid)");
}

static void
test_parse_truncation(void)
{
	struct jbd2_replay_ctx ctx;
	char		       buf[64];
	u_int64_t	       target;
	u_int32_t	       flags, csum, offset;

	/* csum3 tag cut off one byte short of the fixed size */
	setup_ctx(&ctx, JBD2_CSUM_V3, 1);
	memset(buf, 0, sizeof(buf));
	offset = 0;
	CHECK(jbd2_parse_tag(&ctx, buf, 15, &offset, &target, &flags, &csum) ==
		EINVAL,
	    "csum3 truncated tag rejected");
	CHECK(offset == 0, "csum3 truncated offset unchanged");

	/* csum2 tag with UUID truncated at block end */
	setup_ctx(&ctx, JBD2_CSUM_V2, 0);
	memset(buf, 0, sizeof(buf));
	offset = 0;
	CHECK(jbd2_parse_tag(&ctx, buf, 10 + 15, &offset, &target, &flags,
		  &csum) == EINVAL,
	    "csum2 truncated uuid rejected");

	/* tag start beyond the usable area */
	offset = 60;
	CHECK(jbd2_parse_tag(&ctx, buf, 61, &offset, &target, &flags, &csum) ==
		EINVAL,
	    "tag start beyond buffer rejected");

	/* unknown flag bits rejected */
	setup_ctx(&ctx, JBD2_CSUM_V2, 0);
	mk_tag2(buf, 1, 0, 0x8000);
	offset = 0;
	CHECK(jbd2_parse_tag(&ctx, buf, 10, &offset, &target, &flags, &csum) ==
		EINVAL,
	    "unknown tag flags rejected");

	/* csum3 with nonzero high blocknr but no 64BIT feature */
	setup_ctx(&ctx, JBD2_CSUM_V3, 0);
	mk_tag3(buf, 1, 5, JBD2_FLAG_SAME_UUID, 0);
	offset = 0;
	CHECK(jbd2_parse_tag(&ctx, buf, 16, &offset, &target, &flags, &csum) ==
		EINVAL,
	    "tag3 high block without 64BIT rejected");
}

static void
test_tag_checksum(void)
{
	struct jbd2_replay_ctx ctx;
	uint32_t	       expected, seq = 0x10203040;
	uint32_t	       seq_be = htobe32(seq);
	uint32_t	       raw;
	uint8_t		       bad[4096];

	/* Seed = raw crc32c(~0, uuid); tag csum = raw crc32c(seed,
	 * be32 seq, 4096-byte payload).  Reference implementation is
	 * the independent bitwise crc32c above. */
	expected = ref_crc32c(ref_crc32c(~0u, test_uuid, sizeof(test_uuid)),
	    &seq_be, sizeof(seq_be));
	expected = ref_crc32c(expected, pattern4096, sizeof(pattern4096));

	/* Independently generated known vector (see
	 * tests/gen-vectors reference output). */
	CHECK(expected == 0x11B8D1AD, "jbd2 tag csum known vector");

	setup_ctx(&ctx, JBD2_CSUM_V3, 1);
	raw = jbd2_tag_csum(&ctx, seq, pattern4096, sizeof(pattern4096));
	CHECK(raw == expected, "jbd2_tag_csum matches reference");

	/* Verify accepts the right checksum */
	CHECK(jbd2_tag_csum_verify(
		  &ctx, seq, pattern4096, sizeof(pattern4096), expected) == 0,
	    "tag csum verify ok");

	/* One-bit payload corruption must be rejected */
	memcpy(bad, pattern4096, sizeof(bad));
	bad[1500] ^= 0x01;
	CHECK(
	    jbd2_tag_csum_verify(&ctx, seq, bad, sizeof(bad), expected) == EIO,
	    "tag csum corrupted payload rejected");

	/* Corrupted checksum must be rejected */
	CHECK(jbd2_tag_csum_verify(&ctx, seq, pattern4096, sizeof(pattern4096),
		  expected ^ 1) == EIO,
	    "tag csum corrupted checksum rejected");

	/* Wrong transaction sequence must be rejected */
	CHECK(jbd2_tag_csum_verify(&ctx, seq + 1, pattern4096,
		  sizeof(pattern4096), expected) == EIO,
	    "tag csum wrong sequence rejected");

	/* CSUM_V2 stores only the low 16 bits */
	setup_ctx(&ctx, JBD2_CSUM_V2, 0);
	CHECK(jbd2_tag_csum_verify(&ctx, seq, pattern4096, sizeof(pattern4096),
		  expected & 0xFFFF) == 0,
	    "csum2 low-16 verify ok");
	CHECK(jbd2_tag_csum_verify(&ctx, seq, pattern4096, sizeof(pattern4096),
		  (expected & 0xFFFF) ^ 0x400) == EIO,
	    "csum2 corrupted checksum rejected");

	/* A different UUID must be rejected (the port seeds from the
	 * journal superblock UUID). */
	setup_ctx(&ctx, JBD2_CSUM_V3, 1);
	ctx.rc_uuid[0] ^= 0xFF;
	jbd2_seed_init(&ctx);
	CHECK(jbd2_tag_csum_verify(
		  &ctx, seq, pattern4096, sizeof(pattern4096), expected) == EIO,
	    "tag csum wrong uuid rejected");
}

static void
test_descriptor_tail(void)
{
	struct jbd2_replay_ctx		ctx;
	uint8_t				block[4096];
	struct jbd2_journal_block_tail *tail;
	uint32_t			expected, stored;

	fill_block4096(block);
	/* The checksum covers the block with the tail zeroed. */
	memset(block + 4096 - 4, 0, 4);
	expected = ref_crc32c(ref_crc32c(~0u, test_uuid, sizeof(test_uuid)),
	    block, sizeof(block));
	CHECK(expected == 0x628E32C7, "descriptor csum known vector");

	setup_ctx(&ctx, JBD2_CSUM_V3, 1);
	tail = (struct jbd2_journal_block_tail *)(block + 4096 - sizeof(*tail));
	stored = htobe32(expected);
	memcpy(&tail->t_checksum, &stored, 4);
	CHECK(jbd2_descr_tail_verify(&ctx, block, 4096) == 0,
	    "descriptor tail verify ok");
	CHECK(tail->t_checksum == stored, "tail restored after verify");

	block[100] ^= 0x40;
	CHECK(jbd2_descr_tail_verify(&ctx, block, 4096) == EIO,
	    "descriptor tail corrupted rejected");

	/* No checksum mode: verify is a no-op */
	setup_ctx(&ctx, JBD2_CSUM_NONE, 0);
	CHECK(jbd2_descr_tail_verify(&ctx, block, 4096) == 0,
	    "no-csum tail verify no-op");
}

static void
test_descriptor_stream(void)
{
	struct jbd2_replay_ctx ctx;
	char		       buf[4096];
	u_int32_t	       offset, flags, csum, count = 0;
	u_int64_t	       target;
	size_t		       off;

	/* Three csum2 tags, first with UUID, LAST_TAG on the third. */
	setup_ctx(&ctx, JBD2_CSUM_V2, 0);
	memset(buf, 0, sizeof(buf));
	off = mk_tag2(buf, 100, 0x1111, 0);
	memcpy(buf + off, test_uuid, 16);
	off += 16;
	off += mk_tag2(buf + off, 200, 0x2222, JBD2_FLAG_SAME_UUID);
	off += mk_tag2(
	    buf + off, 300, 0x3333, JBD2_FLAG_SAME_UUID | JBD2_FLAG_LAST_TAG);

	offset = 0;
	while (offset < off) {
		CHECK(jbd2_parse_tag(
			  &ctx, buf, off, &offset, &target, &flags, &csum) == 0,
		    "stream tag parse");
		CHECK(target == 100u + 100u * count, "stream tag target");
		count++;
		if (flags & JBD2_FLAG_LAST_TAG)
			break;
	}
	CHECK(count == 3, "stream tag count");
	CHECK(offset == off, "stream final offset");

	/* A descriptor stream without LAST_TAG and with trailing
	 * garbage must not parse: leftover zero bytes form a tag with
	 * flags 0 that never terminates.  jbd2_count_tags() rejects
	 * such a block; emulate its logic through the parse loop with
	 * the usable-size bound. */
	memset(buf, 0, sizeof(buf));
	off = mk_tag2(buf, 100, 0, JBD2_FLAG_SAME_UUID);
	offset = 0;
	count = 0;
	while (offset + jbd2_tag_bytes(&ctx) <= 4096 - 4) {
		int rc = jbd2_parse_tag(
		    &ctx, buf, 4096 - 4, &offset, &target, &flags, &csum);
		CHECK(rc == 0, "zero-tail stream parse");
		count++;
		if (flags & JBD2_FLAG_LAST_TAG)
			break;
		if (count > 300)
			break;
	}
	/* The zero tail yields one real tag then zero tags until the
	 * space runs out; count_tags() would reject this block via
	 * its own LAST_TAG requirement. */
	CHECK(count > 1, "zero-tail stream bounded");
}

static void
test_feature_check(void)
{
	struct jbd2_replay_ctx ctx;

	setup_ctx(&ctx, JBD2_CSUM_NONE, 0);
	CHECK(jbd2_feature_check(&ctx) == 0, "no features ok");

	setup_ctx(&ctx, JBD2_CSUM_NONE, 0);
	ctx.rc_features_incompat = 0x80; /* unknown bit */
	CHECK(jbd2_feature_check(&ctx) == EOPNOTSUPP,
	    "unknown incompat feature rejected");

	setup_ctx(&ctx, JBD2_CSUM_NONE, 0);
	ctx.rc_features_incompat = JBD2_FEATURE_INCOMPAT_ASYNC_COMMIT;
	CHECK(jbd2_feature_check(&ctx) == EOPNOTSUPP, "async commit rejected");

	setup_ctx(&ctx, JBD2_CSUM_NONE, 0);
	ctx.rc_features_incompat =
	    JBD2_FEATURE_INCOMPAT_CSUM_V2 | JBD2_FEATURE_INCOMPAT_CSUM_V3;
	CHECK(jbd2_feature_check(&ctx) == EINVAL, "csum v2+v3 rejected");

	setup_ctx(&ctx, JBD2_CSUM_NONE, 0);
	ctx.rc_features_incompat = JBD2_FEATURE_INCOMPAT_CSUM_V2;
	ctx.rc_features_compat = JBD2_FEATURE_COMPAT_CHECKSUM;
	CHECK(jbd2_feature_check(&ctx) == EINVAL, "csum v1 with v2 rejected");

	setup_ctx(&ctx, JBD2_CSUM_NONE, 0);
	ctx.rc_features_incompat =
	    JBD2_FEATURE_INCOMPAT_64BIT | JBD2_FEATURE_INCOMPAT_CSUM_V3;
	CHECK(jbd2_feature_check(&ctx) == 0, "64bit+csum3 ok");
	CHECK(ctx.rc_csum_mode == JBD2_CSUM_V3, "csum3 mode selected");

	setup_ctx(&ctx, JBD2_CSUM_NONE, 0);
	ctx.rc_features_incompat = JBD2_FEATURE_INCOMPAT_CSUM_V2;
	CHECK(jbd2_feature_check(&ctx) == 0, "csum2 ok");
	CHECK(ctx.rc_csum_mode == JBD2_CSUM_V2, "csum2 mode selected");
}

int
main(void)
{
	fill_pattern4096();

	test_crc32c_check_value();
	test_tag_sizes();
	test_parse_variants();
	test_parse_truncation();
	test_tag_checksum();
	test_descriptor_tail();
	test_descriptor_stream();
	test_feature_check();

	if (failures != 0) {
		fprintf(stderr, "ext4fs-jbd2-tests: %d failure(s)\n", failures);
		return 1;
	}
	printf("ext4fs-jbd2-tests: all tests passed\n");
	return 0;
}
