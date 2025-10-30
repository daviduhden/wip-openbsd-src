/*
 * FvwmRearrange: fvwm module to tile or cascade windows in a region.
 *
 * Copyright (c) 2025 David Uhden Collado <david@uhden.dev>
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

#include <sys/time.h>
#include <sys/types.h>

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"

#ifdef HAVE_SYS_BSDTYPES_H
#include <sys/bsdtypes.h>
#endif

#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <X11/Xlib.h>

#include "../../fvwm/fvwm.h"
#include "../../fvwm/module.h"
#include "fvwmlib.h"

void DeadPipe(int sig);

typedef struct ClientNode {
	Window frame;
	int title_height;
	int border_width;
	unsigned long width;
	unsigned long height;
	struct ClientNode *prev;
	struct ClientNode *next;
} ClientNode;

typedef struct ModuleState {
	Display *display;
	int screen_width;
	int screen_height;
	char *program_name;
	int pipe_fd[2];
	int fd_width;
	ClientNode *head;
	ClientNode *tail;
	int client_count;
	FILE *log;
	int offset_x;
	int offset_y;
	int limit_width;
	int limit_height;
	int bound_x;
	int bound_y;
	int include_untitled;
	int include_transients;
	int include_maximized;
	int include_sticky;
	int include_all;
	int entire_desk;
	int reverse_order;
	int raise_clients;
	int resize_clients;
	int avoid_stretch;
	int flat_x;
	int flat_y;
	int step_x;
	int step_y;
	int tile_horizontal;
	int tile_limit;
	char run_tile;
	char run_cascade;
} ModuleState;

static ModuleState g_state = {.raise_clients = 1};

static void
prepend_client(ModuleState *state, ClientNode *node)
{
	node->prev = NULL;
	node->next = state->head;
	if (state->head) {
		state->head->prev = node;
	} else {
		state->tail = node;
	}
	state->head = node;
	++state->client_count;
}

static void
release_clients(ModuleState *state)
{
	ClientNode *cursor = state->head;
	while (cursor) {
		ClientNode *next = cursor->next;
		free(cursor);
		cursor = next;
	}
	state->head = NULL;
	state->tail = NULL;
	state->client_count = 0;
}

static ClientNode *
find_client(ModuleState *state, Window frame)
{
	for (ClientNode *cursor = state->head; cursor; cursor = cursor->next) {
		if (cursor->frame == frame) {
			return cursor;
		}
	}
	return NULL;
}

static void
detach_client(ModuleState *state, ClientNode *node)
{
	if (!node) {
		return;
	}
	if (node->prev) {
		node->prev->next = node->next;
	} else {
		state->head = node->next;
	}
	if (node->next) {
		node->next->prev = node->prev;
	} else {
		state->tail = node->prev;
	}
	free(node);
	--state->client_count;
}

static int
window_matches(ModuleState *state, unsigned long *body)
{
	unsigned long flags = body[8];
	XWindowAttributes xwa;

	if ((flags & WINDOWLISTSKIP) && !state->include_all) {
		return 0;
	}
	if ((flags & MAXIMIZED) && !state->include_maximized) {
		return 0;
	}
	if ((flags & STICKY) && !state->include_sticky) {
		return 0;
	}
	if (!XGetWindowAttributes(state->display, (Window)body[1], &xwa)) {
		return 0;
	}
	if (xwa.map_state != IsViewable) {
		return 0;
	}
	if (!(flags & MAPPED)) {
		return 0;
	}
	if (flags & ICONIFIED) {
		return 0;
	}
	if (!state->entire_desk) {
		int x = (int)body[3];
		int y = (int)body[4];
		int w = (int)body[5];
		int h = (int)body[6];
		if (!((x < state->screen_width) && (y < state->screen_height) &&
		        (x + w > 0) && (y + h > 0))) {
			return 0;
		}
	}
	if (!(flags & TITLE) && !state->include_untitled) {
		return 0;
	}
	if ((flags & TRANSIENT) && !state->include_transients) {
		return 0;
	}
	return 1;
}

static int
collect_client(ModuleState *state)
{
	unsigned long header[HEADER_SIZE];
	unsigned long *body;
	fd_set infds;
	int keep_running = 1;

	FD_ZERO(&infds);
	FD_SET(state->pipe_fd[1], &infds);
	select(state->fd_width, &infds, NULL, NULL, NULL);

	if (ReadFvwmPacket(state->pipe_fd[1], header, &body) > 0) {
		switch (header[1]) {
		case M_CONFIGURE_WINDOW:
			if (window_matches(state, body)) {
				ClientNode *node = (ClientNode *)safemalloc(
				    sizeof(ClientNode));
				node->frame = (Window)body[1];
				node->title_height = (int)body[9];
				node->border_width = (int)body[10];
				node->width = body[5];
				node->height = body[6];
				prepend_client(state, node);
			}
			break;
		case M_DESTROY_WINDOW:
			if (body) {
				ClientNode *node =
				    find_client(state, (Window)body[1]);
				if (node) {
					detach_client(state, node);
				}
			}
			break;
		case M_END_WINDOWLIST:
			keep_running = 0;
			break;
		default:
			fprintf(state->log,
			    "%s: internal inconsistency: unknown message\n",
			    state->program_name);
			break;
		}
		free(body);
	} else {
		keep_running = 0;
	}

	return keep_running;
}

static int
await_configure(ModuleState *state, ClientNode *node)
{
	for (;;) {
		unsigned long header[HEADER_SIZE];
		unsigned long *body;
		fd_set infds;

		FD_ZERO(&infds);
		FD_SET(state->pipe_fd[1], &infds);
		select(state->fd_width, &infds, NULL, NULL, NULL);

		if (ReadFvwmPacket(state->pipe_fd[1], header, &body) > 0) {
			switch (header[1]) {
			case M_CONFIGURE_WINDOW:
				if (body && (Window)body[1] == node->frame) {
					free(body);
					return 1;
				}
				break;
			case M_DESTROY_WINDOW:
				if (body) {
					Window frame = (Window)body[1];
					if (frame == node->frame) {
						free(body);
						return 0;
					}
					ClientNode *other =
					    find_client(state, frame);
					if (other) {
						detach_client(state, other);
					}
				}
				break;
			case M_END_WINDOWLIST:
				break;
			default:
				break;
			}
			free(body);
		} else {
			return 0;
		}
	}
}

static int
parse_metric(const char *token, unsigned long reference)
{
	char *endptr;
	long value;

	if (!token || !*token) {
		return 0;
	}

	value = strtol(token, &endptr, 10);
	if (endptr && *endptr && isalpha((unsigned char)*endptr)) {
		return (int)value;
	}
	return (int)((value * (long)reference) / 100);
}

static void
send_resize(ModuleState *state, const ClientNode *node, unsigned long width,
    unsigned long height)
{
	char command[128];

	snprintf(command, sizeof(command), "Resize %lup %lup", width, height);
	SendInfo(state->pipe_fd, command, node->frame);
}

static void
send_move(ModuleState *state, const ClientNode *node, int x, int y)
{
	char command[128];

	snprintf(command, sizeof(command), "Move %up %up", x, y);
	SendInfo(state->pipe_fd, command, node->frame);
}

static void
tile_clients(ModuleState *state)
{
	ClientNode *cursor = state->reverse_order ? state->tail : state->head;
	int stripes = 1;
	int slots_per_stripe;
	int wdiv;
	int hdiv;
	int current_x = state->offset_x;
	int current_y = state->offset_y;
	int limit = state->tile_limit;

	if (state->tile_horizontal) {
		if ((limit > 0) && (limit < state->client_count)) {
			stripes = state->client_count / limit;
			if (state->client_count % limit) {
				++stripes;
			}
			hdiv = (state->bound_y - state->offset_y + 1) / limit;
		} else {
			limit = state->client_count;
			state->tile_limit = limit;
			hdiv = (state->bound_y - state->offset_y + 1) /
			       state->client_count;
		}
		slots_per_stripe = limit;
		wdiv = (state->bound_x - state->offset_x + 1) / stripes;

		for (int s = 0; cursor && (s < stripes); ++s) {
			for (int slot = 0; cursor && (slot < slots_per_stripe);
			    ++slot) {
				int new_width = wdiv - cursor->border_width * 2;
				int new_height = hdiv -
				                 cursor->border_width * 2 -
				                 cursor->title_height;

				if (state->resize_clients) {
					if (state->avoid_stretch) {
						if (new_width >
						    (int)cursor->width) {
							new_width =
							    (int)cursor->width;
						}
						if (new_height >
						    (int)cursor->height) {
							new_height =
							    (int)cursor->height;
						}
					}
					send_resize(state, cursor,
					    (new_width > 0)
					        ? (unsigned long)new_width
					        : cursor->width,
					    (new_height > 0)
					        ? (unsigned long)new_height
					        : cursor->height);
				}

				send_move(state, cursor, current_x, current_y);
				if (state->raise_clients) {
					SendInfo(state->pipe_fd, "Raise",
					    cursor->frame);
				}

				current_y += hdiv;
				{
					int alive =
					    await_configure(state, cursor);
					ClientNode *next = state->reverse_order
					                       ? cursor->prev
					                       : cursor->next;
					if (!alive) {
						detach_client(state, cursor);
					}
					cursor = next;
				}
			}
			current_x += wdiv;
			current_y = state->offset_y;
		}
	} else {
		if ((limit > 0) && (limit < state->client_count)) {
			stripes = state->client_count / limit;
			if (state->client_count % limit) {
				++stripes;
			}
			wdiv = (state->bound_x - state->offset_x + 1) / limit;
		} else {
			limit = state->client_count;
			state->tile_limit = limit;
			wdiv = (state->bound_x - state->offset_x + 1) /
			       state->client_count;
		}
		slots_per_stripe = limit;
		hdiv = (state->bound_y - state->offset_y + 1) / stripes;

		for (int s = 0; cursor && (s < stripes); ++s) {
			for (int slot = 0; cursor && (slot < slots_per_stripe);
			    ++slot) {
				int new_width = wdiv - cursor->border_width * 2;
				int new_height = hdiv -
				                 cursor->border_width * 2 -
				                 cursor->title_height;

				if (state->resize_clients) {
					if (state->avoid_stretch) {
						if (new_width >
						    (int)cursor->width) {
							new_width =
							    (int)cursor->width;
						}
						if (new_height >
						    (int)cursor->height) {
							new_height =
							    (int)cursor->height;
						}
					}
					send_resize(state, cursor,
					    (new_width > 0)
					        ? (unsigned long)new_width
					        : cursor->width,
					    (new_height > 0)
					        ? (unsigned long)new_height
					        : cursor->height);
				}

				send_move(state, cursor, current_x, current_y);
				if (state->raise_clients) {
					SendInfo(state->pipe_fd, "Raise",
					    cursor->frame);
				}

				current_x += wdiv;
				{
					int alive =
					    await_configure(state, cursor);
					ClientNode *next = state->reverse_order
					                       ? cursor->prev
					                       : cursor->next;
					if (!alive) {
						detach_client(state, cursor);
					}
					cursor = next;
				}
			}
			current_x = state->offset_x;
			current_y += hdiv;
		}
	}
}

static void
cascade_clients(ModuleState *state)
{
	ClientNode *cursor = state->reverse_order ? state->tail : state->head;
	int current_x = state->offset_x;
	int current_y = state->offset_y;

	while (cursor) {
		unsigned long target_width = 0;
		unsigned long target_height = 0;
		int advance_x = state->step_x;
		int advance_y = state->step_y;

		if (state->raise_clients) {
			SendInfo(state->pipe_fd, "Raise", cursor->frame);
		}

		send_move(state, cursor, current_x, current_y);

		if (state->resize_clients) {
			if (state->avoid_stretch) {
				if (state->limit_width &&
				    cursor->width >
				        (unsigned long)state->limit_width) {
					target_width =
					    (unsigned long)state->limit_width;
				}
				if (state->limit_height &&
				    cursor->height >
				        (unsigned long)state->limit_height) {
					target_height =
					    (unsigned long)state->limit_height;
				}
			} else {
				target_width = state->limit_width;
				target_height = state->limit_height;
			}

			if (target_width || target_height) {
				send_resize(state, cursor,
				    target_width ? target_width : cursor->width,
				    target_height ? target_height
				                  : cursor->height);
			}
		}

		if (!state->flat_x) {
			advance_x += cursor->border_width;
		}
		if (!state->flat_y) {
			advance_y +=
			    cursor->border_width + cursor->title_height;
		}

		{
			int alive = await_configure(state, cursor);
			ClientNode *next =
			    state->reverse_order ? cursor->prev : cursor->next;
			if (!alive) {
				detach_client(state, cursor);
			}
			cursor = next;
		}

		current_x += advance_x;
		current_y += advance_y;
	}
}

static void
parse_arguments(ModuleState *state, const char *source, int argc, char *argv[],
    int start_index)
{
	int positional = 0;

	for (int i = start_index; i < argc; ++i) {
		const char *arg = argv[i];

		if (!strcmp(arg, "-tile") || !strcmp(arg, "-cascade")) {
			continue;
		} else if (!strcmp(arg, "-u")) {
			state->include_untitled = 1;
		} else if (!strcmp(arg, "-t")) {
			state->include_transients = 1;
		} else if (!strcmp(arg, "-a")) {
			state->include_all = 1;
			state->include_untitled = 1;
			state->include_transients = 1;
			state->include_maximized = 1;
			if (state->run_cascade) {
				state->include_sticky = 1;
			}
		} else if (!strcmp(arg, "-r")) {
			state->reverse_order = 1;
		} else if (!strcmp(arg, "-noraise")) {
			state->raise_clients = 0;
		} else if (!strcmp(arg, "-noresize")) {
			state->resize_clients = 0;
		} else if (!strcmp(arg, "-nostretch")) {
			state->avoid_stretch = 1;
		} else if (!strcmp(arg, "-desk")) {
			state->entire_desk = 1;
		} else if (!strcmp(arg, "-flatx")) {
			state->flat_x = 1;
		} else if (!strcmp(arg, "-flaty")) {
			state->flat_y = 1;
		} else if (!strcmp(arg, "-h")) {
			state->tile_horizontal = 1;
		} else if (!strcmp(arg, "-m")) {
			state->include_maximized = 1;
		} else if (!strcmp(arg, "-s")) {
			state->include_sticky = 1;
		} else if (!strcmp(arg, "-mn") && ((i + 1) < argc)) {
			state->tile_limit = atoi(argv[++i]);
		} else if (!strcmp(arg, "-resize")) {
			state->resize_clients = 1;
		} else if (!strcmp(arg, "-incx") && ((i + 1) < argc)) {
			state->step_x =
			    parse_metric(argv[++i], state->screen_width);
		} else if (!strcmp(arg, "-incy") && ((i + 1) < argc)) {
			state->step_y =
			    parse_metric(argv[++i], state->screen_height);
		} else {
			++positional;
			if (positional > 4) {
				fprintf(state->log,
				    "%s: %s: ignoring unknown arg %s\n",
				    state->program_name, source, arg);
				continue;
			}

			if (positional == 1) {
				state->offset_x =
				    parse_metric(arg, state->screen_width);
			} else if (positional == 2) {
				state->offset_y =
				    parse_metric(arg, state->screen_height);
			} else if (positional == 3) {
				if (state->run_cascade) {
					state->limit_width = parse_metric(
					    arg, state->screen_width);
				} else {
					state->bound_x = parse_metric(
					    arg, state->screen_width);
				}
			} else if (positional == 4) {
				if (state->run_cascade) {
					state->limit_height = parse_metric(
					    arg, state->screen_height);
				} else {
					state->bound_y = parse_metric(
					    arg, state->screen_height);
				}
			}
		}
	}
}

#ifdef USERC
static int
tokenise_config(char *line, char ***argv_out)
{
	char *tokens[48];
	int count = 0;
	char *cursor = strtok(line, " \t");

	while (cursor && count < 48) {
		cursor = strtok(NULL, " \t");
		if (!cursor) {
			break;
		}
		tokens[count++] = cursor;
	}

	if (count > 0) {
		*argv_out = (char **)safemalloc(sizeof(char *) * count);
		for (int i = 0; i < count; ++i) {
			(*argv_out)[i] = tokens[i];
		}
	} else {
		*argv_out = NULL;
	}

	return count;
}

#ifdef FVWM1
static char *
LoadConfigLine(const char *filename, const char *match)
{
	FILE *f = fopen(filename, "r");
	if (f) {
		char line[256];
		size_t match_len = strlen(match);

		while (fgets(line, sizeof(line), f)) {
			if (strncmp(line, match, match_len) == 0) {
				size_t len = strlen(line);
				char *copy = (char *)safemalloc(len + 1);

				strcpy(copy, line);
				if (len && copy[len - 1] == '\n') {
					copy[len - 1] = '\0';
				}
				fclose(f);
				return copy;
			}
		}
		fclose(f);
	}
	return NULL;
}
#endif /* FVWM1 */
#endif /* USERC */

static void
handle_sigpipe(int sig)
{
	(void)sig;
	exit(0);
}

int
main(int argc, char *argv[])
{
	ModuleState *state = &g_state;

#ifdef USERC
	char match[128];
	char *config_line;
#endif

	state->log = fopen("/dev/console", "w");
	if (!state->log) {
		state->log = stderr;
	}

	state->program_name = strrchr(argv[0], '/');
	state->program_name =
	    state->program_name ? state->program_name + 1 : argv[0];

	if (argc < 6) {
#ifdef FVWM1
		fprintf(stderr, "%s: module should be executed by fvwm only\n",
		    state->program_name);
#else
		fprintf(stderr, "%s: module should be executed by fvwm2 only\n",
		    state->program_name);
#endif
		exit(-1);
	}

	state->pipe_fd[0] = atoi(argv[1]);
	state->pipe_fd[1] = atoi(argv[2]);

	state->display = XOpenDisplay(NULL);
	if (!state->display) {
		fprintf(state->log, "%s: couldn't open display %s\n",
		    state->program_name, XDisplayName(NULL));
		exit(-1);
	}

	signal(SIGPIPE, handle_sigpipe);

	{
		int screen = DefaultScreen(state->display);
		state->screen_width = DisplayWidth(state->display, screen);
		state->screen_height = DisplayHeight(state->display, screen);
	}

	state->fd_width = GetFdWidth();

#ifdef USERC
	strcpy(match, "*");
	strcat(match, state->program_name);

#ifdef FVWM1
	config_line = LoadConfigLine(argv[3], match);
	if (config_line) {
		char **args = NULL;
		int arg_count = tokenise_config(config_line, &args);

		parse_arguments(state, "config args", arg_count, args, 0);
		free(args);
		free(config_line);
	}
#else
	GetConfigLine(state->pipe_fd, &config_line);
	while (config_line) {
		if (strncmp(match, config_line, strlen(match)) == 0) {
			char **args = NULL;
			int len = strlen(config_line);
			if (len && config_line[len - 1] == '\n') {
				config_line[len - 1] = '\0';
			}
			{
				int arg_count =
				    tokenise_config(config_line, &args);
				parse_arguments(
				    state, "config args", arg_count, args, 0);
				free(args);
			}
		}
		GetConfigLine(state->pipe_fd, &config_line);
	}
#endif /* FVWM1 */
#endif /* USERC */

	if (strcmp(state->program_name, "FvwmCascade") &&
	    (!strcmp(state->program_name, "FvwmTile") ||
	        (argc >= 7 && !strcmp(argv[6], "-tile")))) {
		state->run_tile = 1;
		state->run_cascade = 0;
		state->resize_clients = 1;
	} else {
		state->run_cascade = 1;
		state->run_tile = 0;
		state->resize_clients = 0;
	}

	parse_arguments(state, "module args", argc, argv, 6);

#ifdef FVWM1
	{
		char msg[256];
		snprintf(msg, sizeof(msg), "SET_MASK %lu\n",
		    (unsigned long)(M_CONFIGURE_WINDOW | M_DESTROY_WINDOW |
		                    M_END_WINDOWLIST));
		SendInfo(state->pipe_fd, msg, 0);

#ifdef FVWM1_MOVENULL
		if (!state->offset_x) {
			++state->offset_x;
		}
		if (!state->offset_y) {
			++state->offset_y;
		}
#endif
	}
#else
	SetMessageMask(state->pipe_fd,
	    M_CONFIGURE_WINDOW | M_DESTROY_WINDOW | M_END_WINDOWLIST);
#endif

	if (state->run_tile) {
		if (!state->bound_x) {
			state->bound_x = state->screen_width;
		}
		if (!state->bound_y) {
			state->bound_y = state->screen_height;
		}
	}

	SendInfo(state->pipe_fd, "Send_WindowList", 0);
	while (collect_client(state)) {
		/* keep reading until the end marker arrives */
	}

	if (state->client_count) {
		if (state->run_cascade) {
			cascade_clients(state);
		} else {
			tile_clients(state);
		}
	}

	release_clients(state);

	if (state->log != stderr) {
		fclose(state->log);
	}

	return 0;
}

void
DeadPipe(int sig)
{
	(void)sig;
	exit(0);
}
