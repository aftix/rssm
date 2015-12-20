#ifndef _CONTROL_H_
#define _CONTROL_H_

#include <unistd.h>

pid_t makeChild(int *in, int *out, int *err, int v);

#endif //_CONTROL_H_
