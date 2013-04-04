namespace fibernet
{
	uint32_t GroupManager::query(int group_handle);
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
			Dispatcher::send(ctx, cmd, n+1, 0 , PTYPE_SYSTEM, 0);
			m_hash.delete_key(group_handle);
		}

		unlock();
	}

	Context * GroupManager::create_group(int handle) 
	{
		Context * ctx = ContextFactory::create("multicast", NULL);
		assert(ctx);

		m_hash.insert(handle, ctx);
		return ctx;
	}


	void GroupManager::send_command(Context *ctx, const char * cmd, uint32_t node) 
	{
		char * tmp = malloc(16);
		int n = sprintf(tmp, "%s %x", cmd, node);
		Dispatcher::send(ctx, tmp, n+1 , 0, PTYPE_SYSTEM, 0);
	}

}
