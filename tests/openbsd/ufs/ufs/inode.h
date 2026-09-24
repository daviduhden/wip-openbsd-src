/*
 * ufs/ufs/inode.h -- host-test shim.  NOT part of the production
 * build.  The ext4fs test code only needs the struct inode shape
 * referenced by the headers, never a real instance.
 */
#ifndef EXT4FS_TEST_UFS_INODE_H
#define EXT4FS_TEST_UFS_INODE_H

struct ext4fs_dinode_large;
struct m_ext4fs;

struct inode {
	struct ext4fs_dinode_large *i_e4din;
	struct m_ext4fs		   *i_e4fs;
	unsigned long long	    i_number;
	int			    i_flag;
	int			    i_dummy;
};

#endif /* EXT4FS_TEST_UFS_INODE_H */
