/*
 * ext4fs-inode-tests.c -- host-testable unit tests for the ext4fs
 * variable inode-size handling: superblock inode-size validation and
 * the metadata_csum inode checksum across inode sizes.
 *
 * Includes the checksum source directly under the stub include tree;
 * expected values come from the independent reference generator
 * (see the vectors documented below).
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Independent bitwise reference crc32c. */
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

#define main inode_test_unused_main
#include "../ext4fs/sys/ufs/ext4fs/ext4fs_crc32c.c"
#undef main

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

static void
setup_fs(struct m_ext4fs *fs, uint32_t inode_size)
{
	memset(fs, 0, sizeof(*fs));
	memcpy(fs->m_sble.sb_uuid, test_uuid, sizeof(test_uuid));
	fs->m_feature_ro_compat = EXT4FS_FEATURE_RO_COMPAT_METADATA_CSUM;
	fs->m_inode_size = inode_size;
}

/* Fill a serialized inode with the reference pattern
 * raw[i] = (i * 37 + 11) & 0xFF, then clear the checksum fields and
 * set i_extra_isize.  Returns the expected checksum computed with the
 * independent reference implementation; the generation bytes are
 * read from the buffer itself (offset 0x64), as the kernel does. */
static uint32_t
mk_inode(uint8_t *raw, uint32_t inode_size, uint16_t extra_isize)
{
	uint32_t csum, ino = 0x12345678;
	uint32_t ino_le = htole32(ino);
	uint16_t extra_le = htole16(extra_isize);
	uint8_t	 zero[2] = {0, 0};
	uint32_t seed;
	size_t	 off;

	for (size_t i = 0; i < inode_size; i++)
		raw[i] = (uint8_t)((i * 37 + 11) & 0xFF);
	memset(raw + 0x7C, 0, 2);
	if (inode_size > 128) {
		memcpy(raw + 0x80, &extra_le, 2);
		memset(raw + 0x82, 0, 2);
	}

	seed = ref_crc32c(~0u, test_uuid, sizeof(test_uuid));
	csum = ref_crc32c(seed, &ino_le, sizeof(ino_le));
	csum = ref_crc32c(
	    csum, raw + offsetof(struct ext4fs_dinode, i_nfs_generation), 4);
	csum = ref_crc32c(csum, raw, 0x7C);
	csum = ref_crc32c(csum, zero, sizeof(zero));
	csum = ref_crc32c(csum, raw + 0x7E, 2);
	if (inode_size > 128) {
		off = 0x82;
		csum = ref_crc32c(csum, raw + 0x80, 2);
		if (off + 2 <= 128u + extra_isize) {
			csum = ref_crc32c(csum, zero, sizeof(zero));
			off += 2;
		}
		csum = ref_crc32c(csum, raw + off, inode_size - off);
	}
	return csum;
}

static void
test_known_vectors(void)
{
	struct m_ext4fs fs;
	uint8_t	       *raw;
	uint32_t	csum, want;
	static const struct {
		uint32_t size;
		uint16_t extra;
		uint32_t want;
	} vectors[] = {
	    {128, 0, 0xBDEA1E8C},
	    {256, 32, 0x058A881A},
	    {256, 4, 0xA94A0A9E},
	    {512, 32, 0xE63EB1D8},
	    {512, 156, 0xA8BC1A0C},
	    {1024, 32, 0x4F2E74FE},
	};

	for (size_t v = 0; v < sizeof(vectors) / sizeof(vectors[0]); v++) {
		uint32_t size = vectors[v].size;

		raw = calloc(1, size);
		setup_fs(&fs, size);
		/* The reference fill also validates the independent
		 * implementation against the embedded vector. */
		want = mk_inode(raw, size, vectors[v].extra);
		CHECK(want == vectors[v].want, "reference matches vector");
		csum = ext4fs_inode_csum(
		    &fs, (struct ext4fs_dinode_large *)raw, 0x12345678);
		CHECK(csum == vectors[v].want, "inode csum known vector");
		if (csum != vectors[v].want)
			fprintf(stderr,
			    "  size=%u extra=%u got=0x%08x "
			    "want=0x%08x\n",
			    size, vectors[v].extra, csum, vectors[v].want);
		free(raw);
	}
}

static void
test_csum_roundtrip(void)
{
	struct m_ext4fs		    fs;
	uint8_t			   *raw;
	uint32_t		    csum, ino = 42;
	struct ext4fs_dinode_large *dp;
	uint32_t		    sizes[] = {128, 256, 512, 1024, 2048};
	uint16_t		    extras[] = {0, 32, 32, 156, 32};

	for (size_t v = 0; v < sizeof(sizes) / sizeof(sizes[0]); v++) {
		uint32_t size = sizes[v];
		uint16_t extra = extras[v];

		raw = calloc(1, size);
		dp = (struct ext4fs_dinode_large *)raw;
		setup_fs(&fs, size);
		mk_inode(raw, size, extra);

		/* Store the checksum and verify it round-trips. */
		csum = ext4fs_inode_csum(&fs, dp, ino);
		dp->dinode.i_checksum_lo = htole16(csum & 0xFFFF);
		if (size > 128 && 0x84 <= 128 + extra)
			dp->dinode.i_checksum_hi =
			    htole16((csum >> 16) & 0xFFFF);
		CHECK(ext4fs_inode_csum_verify(&fs, dp, ino) == 0,
		    "inode csum roundtrip");

		/* One-bit corruption in the prefix must fail. */
		raw[37] ^= 0x01;
		CHECK(ext4fs_inode_csum_verify(&fs, dp, ino) == EINVAL,
		    "inode csum corrupted prefix rejected");
		raw[37] ^= 0x01;
		CHECK(ext4fs_inode_csum_verify(&fs, dp, ino) == 0,
		    "inode csum restored");

		/* Corruption in the tail (past the struct) must fail. */
		if (size > 160) {
			raw[160] ^= 0x01;
			CHECK(ext4fs_inode_csum_verify(&fs, dp, ino) == EINVAL,
			    "inode csum corrupted tail rejected");
		}

		/* A different inode number must fail. */
		CHECK(ext4fs_inode_csum_verify(&fs, dp, ino + 1) == EINVAL,
		    "inode csum wrong ino rejected");

		free(raw);
	}
}

static void
test_sb_inode_size_check(void)
{
	struct ext4fs sb;

	memset(&sb, 0, sizeof(sb));
	sb.sb_log_block_size = htole32(2); /* 4096-byte blocks */

	sb.sb_inode_size = htole16(128);
	CHECK(ext4fs_sb_inode_size_check(&sb) == 0, "128 valid");
	sb.sb_inode_size = htole16(256);
	CHECK(ext4fs_sb_inode_size_check(&sb) == 0, "256 valid");
	sb.sb_inode_size = htole16(512);
	CHECK(ext4fs_sb_inode_size_check(&sb) == 0, "512 valid");
	sb.sb_inode_size = htole16(1024);
	CHECK(ext4fs_sb_inode_size_check(&sb) == 0, "1024 valid");
	sb.sb_inode_size = htole16(2048);
	CHECK(ext4fs_sb_inode_size_check(&sb) == 0, "2048 valid");
	sb.sb_inode_size = htole16(4096);
	CHECK(ext4fs_sb_inode_size_check(&sb) == 0, "4096 valid");

	sb.sb_inode_size = htole16(64);
	CHECK(ext4fs_sb_inode_size_check(&sb) == EINVAL, "64 too small");
	sb.sb_inode_size = htole16(100);
	CHECK(ext4fs_sb_inode_size_check(&sb) == EINVAL, "100 not pow2");
	sb.sb_inode_size = htole16(384);
	CHECK(ext4fs_sb_inode_size_check(&sb) == EINVAL, "384 not pow2");
	sb.sb_inode_size = htole16(8192);
	CHECK(ext4fs_sb_inode_size_check(&sb) == EINVAL,
	    "8192 larger than blocksize");
	sb.sb_inode_size = htole16(0);
	CHECK(ext4fs_sb_inode_size_check(&sb) == EINVAL, "zero invalid");

	/* 2048-byte blocks (logblk 1): 4096-byte inodes invalid. */
	sb.sb_log_block_size = htole32(1);
	sb.sb_inode_size = htole16(4096);
	CHECK(ext4fs_sb_inode_size_check(&sb) == EINVAL,
	    "4096 larger than 2048 blocksize");
	sb.sb_inode_size = htole16(2048);
	CHECK(ext4fs_sb_inode_size_check(&sb) == 0,
	    "2048 valid for 2048 blocksize");

	/* Extra-isize window checks. */
	sb.sb_log_block_size = htole32(2);
	sb.sb_inode_size = htole16(256);
	sb.sb_inode_size_extra_min = htole16(0);
	sb.sb_inode_size_extra_want = htole16(0);
	CHECK(ext4fs_sb_inode_size_check(&sb) == 0, "extra 0/0 valid");
	sb.sb_inode_size_extra_want = htole16(64);
	CHECK(ext4fs_sb_inode_size_check(&sb) == 0, "extra want 64 valid");
	sb.sb_inode_size_extra_want = htole16(3);
	CHECK(ext4fs_sb_inode_size_check(&sb) == EINVAL, "want 3 invalid");
	sb.sb_inode_size_extra_want = htole16(33);
	CHECK(ext4fs_sb_inode_size_check(&sb) == EINVAL, "want 33 misaligned");
	sb.sb_inode_size_extra_want = htole16(129);
	CHECK(ext4fs_sb_inode_size_check(&sb) == EINVAL,
	    "want 129 exceeds inode");
	sb.sb_inode_size_extra_want = htole16(0);
	sb.sb_inode_size_extra_min = htole16(300);
	CHECK(
	    ext4fs_sb_inode_size_check(&sb) == EINVAL, "min 300 exceeds inode");
}

static void
test_extra_isize_validation(void)
{
	struct m_ext4fs		    fs;
	uint8_t			    raw[256];
	struct ext4fs_dinode_large *dp = (struct ext4fs_dinode_large *)raw;

	memset(raw, 0, sizeof(raw));
	setup_fs(&fs, 256);

	dp->dinode.i_extra_isize = htole16(32);
	CHECK(ext4fs_inode_extra_isize_valid(&fs, dp) == 1, "extra 32 ok");
	CHECK(ext4fs_inode_extra_isize(&fs, dp) == 32, "extra 32 read");

	dp->dinode.i_extra_isize = htole16(33);
	CHECK(ext4fs_inode_extra_isize_valid(&fs, dp) == 0,
	    "extra 33 misaligned");

	dp->dinode.i_extra_isize = htole16(300);
	CHECK(ext4fs_inode_extra_isize_valid(&fs, dp) == 0,
	    "extra 300 exceeds inode");

	/* 128-byte inodes have no extra_isize field. */
	setup_fs(&fs, 128);
	CHECK(ext4fs_inode_extra_isize_valid(&fs, dp) == 1,
	    "128-byte inode trivially valid");
	CHECK(ext4fs_inode_extra_isize(&fs, dp) == 0, "128-byte inode extra 0");

	/* Field fits checks at the boundary. */
	setup_fs(&fs, 256);
	dp->dinode.i_extra_isize = htole16(32);
	CHECK(ext4fs_din_fits(&fs, dp,
		  offsetof(struct ext4fs_dinode, i_atime_extra), 4) == 1,
	    "i_atime_extra fits with extra 32");
	CHECK(ext4fs_din_fits(&fs, dp,
		  offsetof(struct ext4fs_dinode, i_project_id), 4) == 1,
	    "i_projid fits with extra 32");
	setup_fs(&fs, 128);
	CHECK(ext4fs_din_fits(&fs, dp,
		  offsetof(struct ext4fs_dinode, i_atime_extra), 4) == 0,
	    "i_atime_extra absent in 128-byte inode");
	dp->dinode.i_extra_isize = htole16(4);
	setup_fs(&fs, 256);
	CHECK(ext4fs_din_fits(&fs, dp,
		  offsetof(struct ext4fs_dinode, i_atime_extra), 4) == 0,
	    "i_atime_extra absent with extra 4");
	CHECK(ext4fs_din_fits(&fs, dp,
		  offsetof(struct ext4fs_dinode, i_checksum_hi), 2) == 1,
	    "i_checksum_hi fits with extra 4");
}

static void
test_inode_geometry(void)
{
	/* The inode-table arithmetic the kernel uses must hold for
	 * every validated inode size: whole inodes fit per block and
	 * offsets never cross block boundaries. */
	uint32_t blocksize = 4096;
	uint32_t sizes[] = {128, 256, 512, 1024, 2048, 4096};

	for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
		uint32_t per_block = blocksize / sizes[i];
		uint64_t max_inodes = 1024;

		CHECK(per_block >= 1 && per_block * sizes[i] == blocksize,
		    "inodes per block integral");
		for (uint32_t idx = 0; idx < max_inodes; idx++) {
			uint32_t offset = (idx % per_block) * sizes[i];
			CHECK(offset + sizes[i] <= blocksize,
			    "inode never crosses block boundary");
		}
	}
}

int
main(void)
{
	test_known_vectors();
	test_csum_roundtrip();
	test_sb_inode_size_check();
	test_extra_isize_validation();
	test_inode_geometry();

	if (failures != 0) {
		fprintf(
		    stderr, "ext4fs-inode-tests: %d failure(s)\n", failures);
		return 1;
	}
	printf("ext4fs-inode-tests: all tests passed\n");
	return 0;
}
