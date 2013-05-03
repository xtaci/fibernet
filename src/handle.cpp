#include "handle.h"

namespace fibernet
{
	uint32_t Handle::reg(Context *ctx)
	{
		uint32_t handle;	

		lock.wlock();
		handle = handle_index++;
		handle |= m_harbor;
		h2c.insert(handle, ctx);
		lock.wunlock();

		ctx->init(handle);	

		return handle;
	}


	void Handle::retire(uint32_t handle) 
	{
		Context * ctx = NULL;

		lock.wlock();

		// delete from h2c
		if (h2c.contains(handle)) { 
			ctx = h2c[handle];
			h2c.delete_key(handle);
		}

		// delete from h2n, n2h
		std::string name;
		if (h2n.contains(handle)) {
			name = h2n[handle];
			h2n.delete_key(handle);
			n2h.delete_key(name);
		}

		lock.wunlock();

		if(ctx) ctx->release();
	}

	Context * Handle::grab(uint32_t handle)
	{
		Context * ctx = NULL;
		lock.rlock();

		if(h2c.contains(handle)) {
			ctx = h2c[handle];
			ctx->grab();
		}

		lock.runlock();

		return ctx;
	}
}
