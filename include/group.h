#ifndef	__FIBERNET_GROUP_H__
#define __FIBERNET_GROUP_H__

#include "skynet_group.h"
#include "skynet_multicast.h"
#include "skynet_server.h"
#include "skynet.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

#define HASH_SIZE 1024

namespace fibernet
{
	class Group	
	{
	private:
		struct group_node {
			int handle;					// group handle
			Context *ctx;				// context
			struct group_node * next;
		};

		int lock;
		struct group_node * node[HASH_SIZE];
	public:

	private:
		inline void lock() { while (__sync_lock_test_and_set(&g->lock,1)) {} }
		inline void unlock(struct group *g) { __sync_lock_release(&g->lock); }


	}
}

#endif //
