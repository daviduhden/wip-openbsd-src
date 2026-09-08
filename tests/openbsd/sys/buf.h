/*
 * sys/buf.h -- host-test shim.  NOT part of the production build.
 */
#ifndef EXT4FS_TEST_BUF_H
#define EXT4FS_TEST_BUF_H

struct buf {
	void	*b_data;
	size_t	 b_bufsize;
};

struct vnode;

int	bread(struct vnode *, int64_t, size_t, struct buf **);
void	brelse(struct buf *);
int	bwrite(struct buf *);

#endif /* EXT4FS_TEST_BUF_H */
