/*
** Module.c: code for modules to communicate with fvwm
*/
// clang-format off
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../fvwm/module.h"
#include "config.h"
#include "fvwmlib.h"
// clang-format on

extern void DeadPipe(int);

static int
read_full(int fd, void *buf, size_t len)
{
	size_t done = 0;

	while (done < len) {
		ssize_t n = read(fd, (char *)buf + done, len - done);

		if (n > 0) {
			done += (size_t)n;
			continue;
		}
		if (n == -1 && errno == EINTR)
			continue;
		if (n == 0)
			errno = EPIPE;
		return -1;
	}
	return 0;
}

static int
write_full(int fd, const void *buf, size_t len)
{
	size_t done = 0;

	while (done < len) {
		ssize_t n = write(fd, (const char *)buf + done, len - done);

		if (n > 0) {
			done += (size_t)n;
			continue;
		}
		if (n == -1 && errno == EINTR)
			continue;
		if (n == 0)
			errno = EIO;
		return -1;
	}
	return 0;
}

/************************************************************************
 *
 * Reads a single packet of info from fvwm. Prototype is:
 * unsigned long header[HEADER_SIZE];
 * unsigned long *body;
 * int fd[2];
 * void DeadPipe(int nonsense);  * Called if the pipe is no longer open  *
 *
 * ReadFvwmPacket(fd[1],header, &body);
 *
 * Returns:
 *   > 0 everything is OK.
 *   = 0 invalid packet.
 *   < 0 pipe is dead. (Should never occur)
 *   body is a malloc'ed space which needs to be freed
 *
 **************************************************************************/
int
ReadFvwmPacket(int fd, unsigned long *header, unsigned long **body)
{
	int	    count, body_length;
	char	   *cbody;

	errno = 0;
	*body = nullptr;

	/*
	 * A pipe read may return less than the whole header.  Acting on a
	 * partially filled header would leave header[2] undetermined and
	 * turn body_length into garbage, so read the header in full first.
	 */
	if (read_full(fd, header, HEADER_SIZE * sizeof(unsigned long)) == -1) {
		DeadPipe(errno);
		return -1;
	}
	count = HEADER_SIZE * sizeof(unsigned long);

	if (header[0] == START_FLAG) {
		if (header[2] < HEADER_SIZE ||
		    header[2] > HEADER_SIZE + MAX_BODY_SIZE)
			return -1;
		body_length = header[2] - HEADER_SIZE;
		*body = (unsigned long *)xmalloc(
		    body_length * sizeof(unsigned long));
		cbody = (char *)(*body);
		if (body_length > 0 && read_full(fd, cbody,
			body_length * sizeof(unsigned long)) == -1) {
			/* The packet is incomplete; discard its partial body. */
			free(*body);
			*body = nullptr;
			DeadPipe(errno);
			return -1;
		}
	} else
		count = 0;

	if (count <= 0)
		DeadPipe(errno);
	return count;
}

/************************************************************************
 *
 * SendText - Sends arbitrary text/command back to fvwm
 *
 ***********************************************************************/
void
SendText(int *fd, char *message, unsigned long window)
{
	int w;

	if (message != nullptr) {
		if (write_full(fd[0], &window, sizeof(window)) == -1) {
			DeadPipe(errno);
			return;
		}

		w = strlen(message);
		if (write_full(fd[0], &w, sizeof(w)) == -1 ||
		    (w && write_full(fd[0], message, (size_t)w) == -1)) {
			DeadPipe(errno);
			return;
		}

		/* keep going */
		w = 1;
		if (write_full(fd[0], &w, sizeof(w)) == -1) {
			DeadPipe(errno);
			return;
		}
	}
}

/***************************************************************************
 *
 * Sets the which-message-types-do-I-want mask for modules
 *
 **************************************************************************/
void
SetMessageMask(int *fd, unsigned long mask)
{
	char set_mask_mesg[50];

	snprintf(set_mask_mesg, sizeof(set_mask_mesg), "SET_MASK %lu\n", mask);
	SendText(fd, set_mask_mesg, 0);
}

/***************************************************************************
 * Gets a module configuration line from fvwm. Returns nullptr if there are
 * no more lines to be had. "line" is a pointer to a char *.
 *
 * The returned text points into static packet storage. It remains valid only
 * until the next GetConfigLine() call, which frees that packet buffer.
 *
 * Changed 10/19/98 by Dan Espen:
 *
 * - The "isspace"  call was referring to  memory  beyond the end of  the
 * input area.  This could have led to the creation of a core file. Added
 * "body_size" to keep it in bounds.
 **************************************************************************/
void
GetConfigLine(int *fd, char **tline)
{
	static int    first_pass = 1;
	int	      count, done = 0;
	int	      body_size;
	static char  *line = nullptr;
	unsigned long header[HEADER_SIZE];

	if (line != nullptr) {
		free(line);
		line = nullptr;
	}

	if (first_pass) {
		SendInfo(fd, "Send_ConfigInfo", 0);
		first_pass = 0;
	}

	while (!done) {
		/* Drop the body of any packet we are skipping. */
		if (line != nullptr) {
			free(line);
			line = nullptr;
		}
		count = ReadFvwmPacket(fd[1], header, (unsigned long **)&line);
		/* DB(("Packet count is %d", count)); */
		if (count <= 0)
			*tline = nullptr;
		else {
			*tline = &line[3 * sizeof(long)];
			body_size = header[2] - HEADER_SIZE;
			/* DB(("Config line (%d): `%s'", body_size, body_size ?
			 * *tline : "")); */
			while ((body_size > 0) &&
			    isspace((unsigned char)**tline)) {
				(*tline)++;
				--body_size;
			}
		}

		/*   fprintf(stderr,"%x %x\n",header[1],M_END_CONFIG_INFO);*/
		if (header[1] == M_CONFIG_INFO)
			done = 1;
		else if (header[1] == M_END_CONFIG_INFO) {
			done = 1;
			if (line != nullptr)
				free(line);
			line = nullptr;
			*tline = nullptr;
		}
	}
}
