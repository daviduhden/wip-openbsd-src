/*
 * sys/vnode.h -- host-test shim.  NOT part of the production build.
 */
#ifndef EXT4FS_TEST_VNODE_H
#define EXT4FS_TEST_VNODE_H

// clang-format off
#include <sys/buf.h>
// clang-format on

struct vnode {
	int v_dummy;
};

#endif /* EXT4FS_TEST_VNODE_H */
