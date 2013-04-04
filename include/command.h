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

		static void _id_to_hex(char * str, uint32_t id) 
		{
			int i;
			static char hex[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };
			str[0] = ':';
			for (i=0;i<8;i++) {
				str[i+1] = hex[(id >> ((7-i) * 4))&0xf];
			}
			str[9] = '\0';
		}

		static void _copy_name(char name[GLOBALNAME_LENGTH], const char * addr) 
		{
			int i;
			for (i=0;i<GLOBALNAME_LENGTH && addr[i];i++) {
				name[i] = addr[i];
			}
			for (;i<GLOBALNAME_LENGTH;i++) {
				name[i] = '\0';
			}
		}

	};
}

#endif //
