#include "mq.h"

namespace fibernet 
{
	GlobalMQ * GlobalMQ::_instance = NULL;

	int MQ::drop_queue() {
		// todo: send message back to message source
		struct Message msg;
		int s = 0;
		while(!pop(&msg)) {
			++s;
			int type = msg.sz >> HANDLE_REMOTE_SHIFT;
			if (type == PTYPE_MULTICAST) {
				assert((msg.sz & HANDLE_MASK) == 0);
				((MulticastMessage *)msg.data)->dispatch(NULL, NULL);
			} else {
				// make sure data is freed
				free(msg.data);
			}
		}
		delete this;
		return s;
	}
}
