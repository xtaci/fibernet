#ifndef __FIBERNET_LOG_H__
#define	__FIBERNET_LOG_H__

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LOG_MESSAGE_SIZE 1024

namespace fibernet
{
	class Context;
	static void errorlog(Context * context, const char *msg, ...);
}

#endif //
