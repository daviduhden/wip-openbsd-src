#ifndef READCONFIG_H
#define READCONFIG_H

#include "FvwmIconMan.h"

typedef enum {
	READ_LINE = 1,
	READ_OPTION = 2,
	READ_ARG = 4,
	READ_REST_OF_LINE = 12
} ReadOption;

extern void read_in_resources(char *file);
extern void print_bindings(Binding *list);
extern void print_args(int numargs, BuiltinArg *args);
extern Binding *ParseMouseEntry(char *tline);

extern void run_function_list(Function *func);
extern void run_binding(WinManager *man, Action action);

/* builtin command handlers, defined in functions.c */
extern int builtin_gotobutton(int numargs, BuiltinArg *args);
extern int builtin_gotomanager(int numargs, BuiltinArg *args);
extern int builtin_refresh(int numargs, BuiltinArg *args);
extern int builtin_select(int numargs, BuiltinArg *args);
extern int builtin_sendcommand(int numargs, BuiltinArg *args);
extern int builtin_bif(int numargs, BuiltinArg *args);
extern int builtin_bifn(int numargs, BuiltinArg *args);
extern int builtin_print(int numargs, BuiltinArg *args);
extern int builtin_jmp(int numargs, BuiltinArg *args);
extern int builtin_ret(int numargs, BuiltinArg *args);
extern int builtin_searchforward(int numargs, BuiltinArg *args);
extern int builtin_searchback(int numargs, BuiltinArg *args);
extern int builtin_warp(int numargs, BuiltinArg *args);
extern int builtin_printdebug(int numargs, BuiltinArg *args);
extern int builtin_quit(int numargs, BuiltinArg *args);
extern void execute_function(char *string);

#define MODS_USED							\
	(ShiftMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask |\
	    Mod5Mask)

#endif /* READCONFIG_H */
