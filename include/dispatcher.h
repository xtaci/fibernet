#ifndef __FIBERNET_DISPATCHER_H__
#define __FIBERNET_DISPATCHER_H__

namespace fibernet
{
	class Dispatcher {
	public:

		/**
		 * core message dispatcher
		 */
		static int dispatch(struct skynet_monitor *sm) 
		{
			// pop one MQ
			MQ * q = GlobalMQ::pop();

			if (q==NULL)
				return 1;

			uint32_t handle = q->handle();
			Context * ctx = Handle::instance()->grab(handle);
			// if the context is released, release the MQ
			if (ctx == NULL) {
				int s = q->release();
				if (s>0) {
					skynet_error(NULL, "Drop message queue %x (%d messages)", handle, s);
				}
				return 0;
			}

			// pop one piece of message from the MQ.
			Message msg;
			if (!q->pop(&msg)) {
				ctx->release();
				return 0;
			}

			skynet_monitor_trigger(sm, msg.source , handle);

			// when callback function is not defined.
			if (ctx->cb == NULL) {
				free(msg.data);
				skynet_error(NULL, "Drop message from %x to %x without callback , size = %d",msg.source, handle, (int)msg.sz);
			} else {
				_dispatch_message(ctx, &msg);
			}

			// push to global queue again.
			assert(q == ctx->queue);
			q->pushglobal();
			ctx->release();
			skynet_monitor_trigger(sm, 0,0);

			return 0;
		}

		/**
		 * send message from context to destination.
		 * source can be faked for purpose.
		 */
		int send(Context * context, uint32_t source, uint32_t destination , int type, int session, void * data, size_t sz) 
		{
			_filter_args(context, type, &session, (void **)&data, &sz);

			if (source == 0) {
				source = context->handle;
			}

			if (destination == 0) {
				return session;
			}
			if (skynet_harbor_message_isremote(destination)) {
				struct remote_message * rmsg = malloc(sizeof(*rmsg));
				rmsg->destination.handle = destination;
				rmsg->message = data;
				rmsg->sz = sz;
				skynet_harbor_send(rmsg, source, session);
			} else {
				struct skynet_message smsg;
				smsg.source = source;
				smsg.session = session;
				smsg.data = data;
				smsg.sz = sz;

				if (Context::push(destination, &smsg)) {
					free(data);
					skynet_error(NULL, "Drop message from %x to %x (type=%d)(size=%d)", source, destination, type, (int)(sz & HANDLE_MASK));
					return -1;
				}
			}
			return session;
		}

		/**
		 * send message from context to addr, with format of:
		 *  :numberhandle	
		 *  .localname
		 *  remote 
		 */
		int send(Context * context, const char * addr , int type, int session, void * data, size_t sz) 
		{
			uint32_t source = context->handle;
			uint32_t des = 0;
			if (addr[0] == ':') {
				des = strtoul(addr+1, NULL, 16);
			} else if (addr[0] == '.') {
				des = (*Handle::instance())[addr+1];
				if (des == 0) {
					free(data);
					skynet_error(context, "Drop message to %s", addr);
					return session;
				}
			} else {
				_filter_args(context, type, &session, (void **)&data, &sz);

				struct remote_message * rmsg = malloc(sizeof(*rmsg));
				_copy_name(rmsg->destination.name, addr);
				rmsg->destination.handle = 0;
				rmsg->message = data;
				rmsg->sz = sz;

				skynet_harbor_send(rmsg, source, session);
				return session;
			}

			return send(context, source, des, type, session, data, sz);
		}

	private:
		/**
		 * dispatch the message, callback function is called with msg.
		 */
		static void _dispatch_message(Context *ctx, Message *msg) 
		{
			assert(ctx->init);
			CHECKCALLING_BEGIN(ctx)
			int type = msg->sz >> HANDLE_REMOTE_SHIFT;
			size_t sz = msg->sz & HANDLE_MASK;
			if (type == PTYPE_MULTICAST) {
				skynet_multicast_dispatch((struct skynet_multicast_message *)msg->data, ctx, 
			} else {
				int reserve = ctx->cb(ctx, ctx->cb_ud, type, msg->session, msg->source, msg->data, sz);
				reserve |= _forwarding(ctx, msg);
				if (!reserve) {
					free(msg->data);
				}
			}
			CHECKCALLING_END(ctx)
		}

		/**
		 * pre-work for sending message.
		 */
		static void _filter_args(Context * context, int type, int *session, void ** data, size_t * sz) 
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
				msg = *data;
			} else {
				msg = malloc(*sz+1);
				memcpy(msg, *data, *sz);
				msg[*sz] = '\0';
			}
			*data = msg;

			assert((*sz & HANDLE_MASK) == *sz);
			*sz |= type << HANDLE_REMOTE_SHIFT;
		}

		/**
		 * forward the message if set.
		 */
		static int _forwarding(Context *ctx, Message *msg) 
		{
			if (ctx->forward) {
				uint32_t des = ctx->forward;
				ctx->forward = 0;
				_send_message(des, msg);
				return 1;
			}
			return 0;
		}

		/**
		 * forward-message sender
		 */
		static void _send_message(uint32_t des, Message *msg) 
		{
			if (skynet_harbor_message_isremote(des)) {
					struct remote_message * rmsg = malloc(sizeof(*rmsg));
					rmsg->destination.handle = des;
					rmsg->message = msg->data;
					rmsg->sz = msg->sz;
					skynet_harbor_send(rmsg, msg->source, msg->session);
			} else {
				if (Context::push(des, msg)) {
					free(msg->data);
					skynet_error(NULL, "Drop message from %x forward to %x (size=%d)", msg->source, des, (int)msg->sz);
				}
			}
		}


		static void _mc(void *ud, uint32_t source, const void * msg, size_t sz) 
		{
			Context * ctx = ud;
			int type = sz >> HANDLE_REMOTE_SHIFT;
			sz &= HANDLE_MASK;
			ctx->cb(ctx, ctx->cb_ud, type, 0, source, msg, sz);
			if (ctx->forward) {
				uint32_t des = ctx->forward;
				ctx->forward = 0;
				struct skynet_message message;
				message.source = source;
				message.session = 0;
				message.data = malloc(sz);
				memcpy(message.data, msg, sz);
				message.sz = sz  | (type << HANDLE_REMOTE_SHIFT);
				_send_message(des, &message);
			}
		}

	}
}
#endif //
