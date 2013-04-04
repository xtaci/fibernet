#include "group_mgr.h"
#include "context.h"
#include "dispatcher.h"
#include "mq.h"

namespace fibernet
{
	GroupManager * GroupManager::m_instance = NULL;

	uint32_t GroupManager::query(int group_handle)
	{
		lock();

		Context * ctx = NULL;
		if (m_hash.contains(group_handle)) {
			ctx = m_hash[group_handle];	
		} else {
			ctx = create_group(group_handle);
		}

		unlock();

		return ctx->handle();
	}

	void GroupManager::clear(int group_handle) 
	{
		lock();

		if (m_hash.contains(group_handle)) {
			Context * ctx = m_hash[group_handle];
			char * cmd = (char *)malloc(8);
			int n = sprintf(cmd, "C");
			ctx->send(cmd, n+1, 0 , PTYPE_SYSTEM, 0);
			m_hash.delete_key(group_handle);
		}

		unlock();
	}

	Context * GroupManager::create_group(int handle) 
	{
		Context * ctx = ContextFactory::create("multicast", NULL);
		assert(ctx);

		m_hash[handle] = ctx;
		return ctx;
	}


	void GroupManager::send_command(Context *ctx, const char * cmd, uint32_t node) 
	{
		char * tmp = (char *)malloc(16);
		int n = sprintf(tmp, "%s %x", cmd, node);
		ctx->send(tmp, n+1 , 0, PTYPE_SYSTEM, 0);
	}

}
