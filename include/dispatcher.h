#ifndef __FIBERNET_DISPATCHER_H__
#define __FIBERNET_DISPATCHER_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "fibernet.h"

namespace fibernet
{
	class Context;
	class Monitor;
	class Message;
	class Dispatcher {
	public:
		/**
		 * dispatcher for scheduler
		 */
		static int dispatch(Monitor *sm);

		/**
		 * send by handle
		 */
		static int send(Context * context, uint32_t source, uint32_t destination , int type, int session, void * data, size_t sz);

		/**
		 * send by name
		 */
		static int send(Context * context, const char * addr , int type, int session, void * data, size_t sz);

	private:
		/**
		 * dispatch message
		 */
		static void _dispatch_message(Context *ctx, Message *msg);

		/**
		 * pre-work before sending
		 * msg encapsulation.
		 */
		static void _filter_args(Context * context, int type, int *session, void ** data, size_t * sz);

		/**
		 * forwarding message
		 */
		static int _forwarding(Context *ctx, Message *msg);

		/**
		 * internal message re-send proc
		 */
		static void _send_message(uint32_t des, Message *msg);

		/**
		 * callback function for MULTICAST.
		 */
		static void _mc(void *ud, uint32_t source, const void * msg, size_t sz);
	};
}
#endif //
