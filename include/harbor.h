#ifndef __FIBERNET_HARBOR_H__
#define __FIBERNET_HARBOR_H__

#include <stdint.h>
#include <stdlib.h>

#include "fibernet.h"

namespace fibernet
{
	struct remote_name {
		char name[GLOBALNAME_LENGTH];
		uint32_t handle;
	};

	struct remote_message {
		struct remote_name destination;
		const void * message;
		size_t sz;
	};

	class Context;
	class Harbor 
	{
	private:
		Context * REMOTE;
		uint32_t HARBOR;
		Harbor(int harbor): HARBOR(harbor<<HANDLE_REMOTE_SHIFT) {}

		static Harbor * m_instance;

	public:

		static void create_instance(int harbor)
		{
			if (!m_instance) m_instance = new Harbor(harbor);
		}

		static Harbor * instance() { return m_instance;}

		/**
		 * start harbor. 
		 * check result before other ops.
		 */
		bool start(const char * master, const char *local);
		int isremote(uint32_t handle) { return (handle & ~HANDLE_MASK) != HARBOR; }

		/**
		 * send remote message.
		 */
		void send(struct remote_message *rmsg, uint32_t source, int session); 

		/**
		 * register a remote name
		 */
		void reg(struct remote_name *rname);
	};

}

#endif //
