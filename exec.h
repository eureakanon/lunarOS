#ifndef _EXEC_H
#define _EXEC_H
#include"types.h"
#include"trapframe.h"

#define MAXARGS 16
#define MAXARG_LEN 64

int do_exec(struct trapframe* tf,const char* upath,char** uargv);
uint32 build_user_stack(uint32* pgd,int argc,char args[MAXARGS][MAXARG_LEN]);

#endif
