// clang-format off
#include "fvwmlib.h"
// clang-format on

constexpr int ACTION1 = 1;
constexpr int ACTION2 = 2;
constexpr int ACTION3 = 4;

struct list {
	unsigned long id;
	unsigned long last_focus_time;
	unsigned long actions;
	struct list  *next;
};

/*************************************************************************
 *
 * Subroutine Prototypes
 *
 *************************************************************************/
void		  Loop(int *fd);
void		  SendInfo(int *fd, char *message, unsigned long window);
struct list	 *find_window(unsigned long id);
void		  remove_window(unsigned long id);
void		  add_window(unsigned long new_win);
void		  update_focus(struct list *l, unsigned long);
[[noreturn]] void DeadPipe(int nonsense);
void		  find_next_event_time(void);
void		  process_message(unsigned long type, unsigned long *body);
