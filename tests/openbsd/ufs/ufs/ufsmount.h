/*
 * ufs/ufs/ufsmount.h -- host-test shim.  NOT part of the production
 * build.
 */
#ifndef EXT4FS_TEST_UFSMOUNT_H
#define EXT4FS_TEST_UFSMOUNT_H

struct m_ext4fs;

struct ufsmount {
	struct m_ext4fs *um_e4fs;
	struct vnode	*um_devvp;
	int		 um_dummy;
};

#define VFSTOUFS(mp) ((struct ufsmount *)(mp))

#endif /* EXT4FS_TEST_UFSMOUNT_H */
