#include "dispatcher.h"
#include "handle.h"
#include "mq.h"
#include "context.h"
#include "monitor.h"
#include "multicast.h"
#include "errorlog.h"

namespace fibernet
{
	//------------------------------------------------------------------------
	int Dispatcher::dispatch(Monitor *sm) 
	{
		// pop one MQ
		MQ * q = GlobalMQ::instance()->pop();

		if (q==NULL)
			return 1;

		uint32_t handle = q->handle();
		Context * ctx = Handle::instance()->grab(handle);
		// if the context is released, release the MQ
		if (ctx == NULL) {
			int s = q->release();
			if (s>0) {
				errorlog(NULL, "Drop message queue %x (%d messages)", handle, s);
			}
			return 0;
		}

		// pop one piece of message from the MQ.
		Message msg;
		if (!q->pop(&msg)) {
			ctx->release();
			return 0;
		}

		sm->trigger(msg.source, handle);

		// when callback function is not defined.
		if (ctx->m_cb == NULL) {
			free(msg.data);
			errorlog(NULL, "Drop message from %x to %x without callback , size = %d",msg.source, handle, (int)msg.sz);
		} else {
			_dispatch_message(ctx, &msg);
		}

		// push to global queue again.
		assert(q == ctx->queue);
		q->pushglobal();
		ctx->release();
		sm->trigger(0,0);

		return 0;
	}

	//------------------------------------------------------------------------
	int Dispatcher::send(Context * context, uint32_t source, uint32_t destination , int type, int session, void * data, size_t sz) 
	{
		_filter_args(context, type, &session, (void **)&data, &sz);

		if (source == 0) {
			source = context->m_handle;
		}

		if (destination == 0) {
			return session;
		}
		if (Harbor::instance()->isremote(destination)) {
			struct remote_message * rmsg = (struct remote_message*)malloc(sizeof(*rmsg));
			rmsg->destination.handle = destination;
			rmsg->message = data;
			rmsg->sz = sz;
			Harbor::instance()->send(rmsg, source, session);
		} else {
			struct Message smsg;
			smsg.source = source;
			smsg.session = session;
			smsg.data = data;
			smsg.sz = sz;

			if (Context::push(destination, &smsg)) {
				free(data);
				errorlog(NULL, "Drop message from %x to %x (type=%d)(size=%d)", source, destination, type, (int)(sz & HANDLE_MASK));
				return -1;
			}
		}
		return session;
	}

	//------------------------------------------------------------------------
	int Dispatcher::send(Context * context, const char * addr , int type, int session, void * data, size_t sz) 
	{
		uint32_t source = context->m_handle;
		uint32_t des = 0;
		if (addr[0] == ':') {
			des = strtoul(addr+1, NULL, 16);
		} else if (addr[0] == '.') {
			des = Handle::instance()->get_handle(addr+1);
			if (des == 0) {
				free(data);
				errorlog(context, "Drop message to %s", addr);
				return session;
			}
		} else {
			_filter_args(context, type, &session, (void **)&data, &sz);

			struct remote_message * rmsg = (struct remote_message *)malloc(sizeof(*rmsg));
			_copy_name(rmsg->destination.name, addr);
			rmsg->destination.handle = 0;
			rmsg->message = data;
			rmsg->sz = sz;

			Harbor::instance()->send(rmsg, source, session);
			return session;
		}

		return send(context, source, des, type, session, data, sz);
	}
	
	//------------------------------------------------------------------------
	void Dispatcher::_filter_args(Context * context, int type, int *session, void ** data, size_t * sz) 
	{
		int dontcopy = type & PTYPE_TAG_DONTCOPY;
		int allocsession = type & PTYPE_TAG_ALLOCSESSION;
		type &= 0xff;

		if (allocsession) {
			assert(*session == 0);
			*session = context->newsession();
		}

		char * msg;
		if (dontcopy || *data == NULL) {
			msg = (char*)*data;
		} else {
			msg = (char*)malloc(*sz+1);
			memcpy(msg, *data, *sz);
			msg[*sz] = '\0';
		}
		*data = msg;

		assert((*sz & HANDLE_MASK) == *sz);
		*sz |= type << HANDLE_REMOTE_SHIFT;
	}

	//------------------------------------------------------------------------
	void Dispatcher::_dispatch_message(Context *ctx, Message *msg) 
	{
		assert(ctx->m_init);
		CHECKCALLING_BEGIN(ctx)
		int type = msg->sz >> HANDLE_REMOTE_SHIFT;
		size_t sz = msg->sz & HANDLE_MASK;
		if (type == PTYPE_MULTICAST) {
			((MulticastMessage*)msg->data)->dispatch(ctx,_mc);
		} else {
			int reserve = ctx->m_cb(ctx, ctx->m_ud, type, msg->session, msg->source, msg->data, sz);
			reserve |= _forwarding(ctx, msg);
			if (!reserve) {
				free(msg->data);
			}
		}
		CHECKCALLING_END(ctx)
	}

	//------------------------------------------------------------------------
	int Dispatcher::_forwarding(Context *ctx, Message *msg) 
	{
		if (ctx->m_forward) {
			uint32_t des = ctx->m_forward;
			ctx->m_forward = 0;
			_send_message(des, msg);
			return 1;
		}
		return 0;
	}
	
	//------------------------------------------------------------------------
	void Dispatcher::_send_message(uint32_t des, Message *msg) 
	{
		if (Harbor::instance()->isremote(des)) {
				struct remote_message * rmsg = (struct remote_message *)malloc(sizeof(*rmsg));
				rmsg->destination.handle = des;
				rmsg->message = msg->data;
				rmsg->sz = msg->sz;
				Harbor::instance()->send(rmsg, msg->source, msg->session);
		} else {
			if (Context::push(des, msg)) {
				free(msg->data);
				errorlog(NULL, "Drop message from %x forward to %x (size=%d)", msg->source, des, (int)msg->sz);
			}
		}
	}

	//------------------------------------------------------------------------
	void Dispatcher::_mc(void *ud, uint32_t source, const void * msg, size_t sz) 
	{
		Context * ctx = (Context *) ud;
		int type = sz >> HANDLE_REMOTE_SHIFT;
		sz &= HANDLE_MASK;
		ctx->m_cb(ctx, ctx->m_ud, type, 0, source, msg, sz);
		if (ctx->m_forward) {
			uint32_t des = ctx->m_forward;
			ctx->m_forward = 0;
			Message message;
			message.source = source;
			message.session = 0;
			message.data = malloc(sz);
			memcpy(message.data, msg, sz);
			message.sz = sz  | (type << HANDLE_REMOTE_SHIFT);
			_send_message(des, &message);
		}
	}
}
