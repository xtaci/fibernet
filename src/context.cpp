#include "context.h"
#include "handle.h"

namespace fibernet
{
	/**
	 * send message directly to the this context.
	 */
	void Context::send(void * msg, size_t sz, uint32_t source, int type, int session) 
	{
		MQ::Message smsg;
		smsg.source = source;
		smsg.session = session;
		smsg.data = msg;
		smsg.sz = sz | type << HANDLE_REMOTE_SHIFT;
		
		queue->push(&smsg);
	}

	/**
	 * push a closed-message to a context by handle.
	 */
	static int Context::push(uint32_t handle, MQ::Message *message) 
	{
		Context * ctx = Handle::instance()->grab(handle);
		if (ctx == NULL) {
			return -1;
		}
		ctx->queue->push(message);
		ctx->release();

		return 0;
	}

	/**
	 * set the endless flag for a context by handle.
	 */
	static void Context::endless(uint32_t handle) 
	{
		Context * ctx = Handle::instance()->grab(handle);
		if (ctx == NULL) {
			return;
		}
		ctx->m_endless = true;
		ctx->release();
	}

	Context * Context::release() 
	{
		if (__sync_sub_and_fetch(&m_ref,1) == 0) {
			_delete_context();
			return NULL;
		}
		return this;
	}

	void Context::_delete_context()
	{
		mod->call_release(this);
		queue->mark_release();	
	
		delete context;	
		context_dec();
	}

	/// context factory
	static Context * ContextFactory::create(const char * name, const char *param) 
	{
		Module * mod = GlobalModule::instance()->query(name);

		if (mod == NULL)
			return NULL;

		void *inst = mod->call_create();

		if (inst == NULL)
			return NULL;

		Context * ctx = new Context;
		CHECKCALLING_INIT(ctx)

		ctx->m_mod = mod;
		ctx->m_instance = inst;
		ctx->m_ref = 2;
		ctx->m_cb = NULL;
		ctx->m_ud = NULL;
		ctx->m_sess_id = 0;

		ctx->m_forward = 0;
		ctx->m_init = false;
		ctx->m_endless = false;
		ctx->m_handle = Handle::instance()->reg(ctx);

		MQ * queue = ctx->queue = new MQ(ctx->handle);
		// init function maybe use ctx->handle, so it must init at last
		_context_inc();

		CHECKCALLING_BEGIN(ctx)
		int r = mod->call_init(inst, ctx, param);
		CHECKCALLING_END(ctx)
		if (r == 0) {
			Context * ret = ctx->release();
			if (ret) {
				ctx->init = true;
			}
			GlobalMQ::instance()->push(queue);
			if (ret) {
				printf("[:%x] launch %s %s\n",ret->handle, name, param ? param : "");
			}
			return ret;
		} else {
			ctx->release();
			Handle::instance()->retire(ctx->handle);
			return NULL;
		}
	}
}
