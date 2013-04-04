#ifndef __FIBERNET_COMMAND_H__
#define __FIBERNET_COMMAND_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "fibernet.h"

namespace fibernet 
{
	/**
	 * command sub-system
	 */
	class Context;
	class Command 
	{
	public:
		/**
		 * execute a command under context.
		 */
		const char * exec(Context * ctx, const char * cmd,const char * param);

	private:
		/**
		 * group command, start with GROUP command params
		 */
		static const char * _group_command(Context * ctx, const char * cmd, int group_handle, uint32_t addr);
	};
}

#endif //
