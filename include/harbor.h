#ifndef __FIBERNET_HARBOR_H__
#define __FIBERNET_HARBOR_H__

#include <stdint.h>
#include <stdlib.h>

#define GLOBALNAME_LENGTH 16
#define REMOTE_MAX 256

// reserve high 8 bits for remote id
#define HANDLE_MASK 0xffffff
#define HANDLE_REMOTE_SHIFT 24

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

	public:
		Harbor(int harbor): HARBOR(harbor<<HANDLE_REMOTE_SHIFT) {}

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
