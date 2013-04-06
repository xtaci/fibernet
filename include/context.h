#ifndef __FIBERNET_CONTEXT_H__
#define __FIBERNET_CONTEXT_H__
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "module.h"
#include "mq.h"

#ifdef CALLING_CHECK

#define CHECKCALLING_BEGIN(ctx) assert(__sync_lock_test_and_set(&ctx->calling,1) == 0);
#define CHECKCALLING_END(ctx) __sync_lock_release(&ctx->calling);
#define CHECKCALLING_INIT(ctx) ctx->calling = 0;
#define CHECKCALLING_DECL int calling;

#else

#define CHECKCALLING_BEGIN(ctx)
#define CHECKCALLING_END(ctx)
#define CHECKCALLING_INIT(ctx)
#define CHECKCALLING_DECL

#endif

namespace fibernet 
{
	class Context
	{
	private:
		void * instance;		// result of xxx_create
		MOdule * mod;			// the module
		uint32_t handle;		// handle
		int ref;				// ref count
		char result[32];		// non integer result
		void * cb_ud;			// 
		skynet_cb cb;
		int session_id;
		uint32_t forward;				// forward to another ctx
		struct message_queue *queue;	// mq
		bool init;
		bool endless;

		static int g_total_context = 0;

		CHECKCALLING_DECL

		friend class ContextFactory;	
		friend class Dispatcher;
		friend class Command;

	public:
		void init(uint32_t _handle) { this.handle = _handle; }

		int newsession()
		{
			int session = ++session_id;
			return session;
		}

		void grab() 
		{
			__sync_add_and_fetch(&ctx->ref,1);
		}

		Context * release() 
		{
			if (__sync_sub_and_fetch(&ctx->ref,1) == 0) {
				_delete_context(ctx);
				return NULL;
			}
			return ctx;
		}

	private:
		Context();
		Context(const Context&);
		Context& operator= (const Context&);

		void _delete_context() 
		{
			mod->call_release();
			queue->mark_release();	
		
			delete context;	
			context_dec();
		}

	public:
		/**
		 * push a message to a context by handle.
		 */
		static int push(uint32_t handle, Message *message) 
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
		static void endless(uint32_t handle) 
		{
			Context * ctx = Handle::instance()->grab(handle);
			if (ctx == NULL) {
				return;
			}
			ctx->endless = true;
			ctx->release();
		}

		/**
		 * forward the message of to another context.
		 */
		static void forward(uint32_t destination) 
		{
			assert(context->forward == 0);
			context->forward = destination;
		}
	
	public:
		static int context_total() { return g_total_context; }
		static void context_inc() { __sync_fetch_and_add(&g_total_context,1); }
		static void context_dec() { __sync_fetch_and_sub(&g_total_context,1); }

	};

	class ContextFactory {
	public:
		static Context * create(const char * name, const char *param) 
		{
			Module * mod = GlobalModule::instance()->query(name);

			if (mod == NULL)
				return NULL;

			void *inst = mod->call_create();

			if (inst == NULL)
				return NULL;

			Context * ctx = new Context;
			CHECKCALLING_INIT(ctx)

			ctx->mod = mod;
			ctx->instance = inst;
			ctx->ref = 2;
			ctx->cb = NULL;
			ctx->cb_ud = NULL;
			ctx->session_id = 0;

			ctx->forward = 0;
			ctx->init = false;
			ctx->endless = false;
			ctx->handle = Handle::instance()->reg(ctx);

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

		
	};
}


#endif //
