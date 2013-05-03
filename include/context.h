#ifndef __FIBERNET_CONTEXT_H__
#define __FIBERNET_CONTEXT_H__
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "module.h"
#include "mq.h"
#include "handle.h"

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
		Module * m_mod;			// the module
		void * m_instance;		// result of xxx_create

		uint32_t m_handle;		// handle
		int m_sess_id;

		char m_result[32];		// storing non-integer result
		void * m_ud;			// user data
		fibernet_cb m_cb;		// call back function

		MQ * queue;				// mq

		uint32_t m_forward;		// forward to another ctx
		bool m_init;
		bool m_endless;
		int m_ref;				// ref count

		static int g_total_context;

		CHECKCALLING_DECL

		friend class ContextFactory;	
		friend class Dispatcher;
		friend class Command;

	public:
		void init(uint32_t handle) { m_handle = handle; }
		uint32_t handle() { return m_handle; }

		void callback(void *ud, fibernet_cb cb) 
		{
			m_cb = cb;
			m_ud = ud;
		}

		int newsession()
		{
			int session = ++m_sess_id;
			return session;
		}

		void grab() { __sync_add_and_fetch(&m_ref,1); }

		void send(void * msg, size_t sz, uint32_t source, int type, int session);
		static int push(uint32_t handle, MQ::Message *message);
		static void endless(uint32_t handle);
		Context * release(); 
	
		/**
		 * forward the message of to another context.
		 */
		void forward(uint32_t destination) 
		{
			assert(m_forward == 0);
			m_forward = destination;
		}
	
		static int context_total() { return g_total_context; }
		static void context_inc() { __sync_fetch_and_add(&g_total_context,1); }
		static void context_dec() { __sync_fetch_and_sub(&g_total_context,1); }

	private:
		Context();
		Context(const Context&);
		Context& operator= (const Context&);
		void _delete_context();
	};

	class ContextFactory {
	public:
		static Context * create(const char * name, const char *param);
	};
}


#endif //
