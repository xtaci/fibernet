#ifndef __FIBERNET_MULTICAST_H__
#define __FIBERNET_MULTICAST_H__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace fibernet 
{
	extern "C" {
		int compar_uint(const void *a, const void *b) {
			const uint32_t * aa = (const uint32_t *)a;
			const uint32_t * bb = (const uint32_t *)b;
			return (int)(*aa - *bb);
		}
	}

	typedef void (*skynet_multicast_func)(void *ud, uint32_t source, const void * msg, size_t sz);
	class MulticastMessage
	{
	private:
		int m_ref;
		const void * m_msg;
		size_t m_sz;
		uint32_t m_source;
	public:
		/**
		 * create a piece of multicast message.
		 * the msg is malloc-ed by caller, released by this.
		 */
		MulticastMessage(const void * msg, size_t sz, uint32_t source):m_ref(0),m_msg(msg),m_sz(sz), m_source(source) {}

		~MulticastMessage() { free((void*)m_msg); }

		/**
		 * copy msg
		 */
		void copy(int copy) 
		{
			int r = __sync_add_and_fetch(&m_ref, copy);
			if (r == 0) { delete this; }
		}

		void dispatch(void * ud, skynet_multicast_func func) {
			if (func) {
				func(ud, m_source, m_msg, m_sz);
			}
			int ref = __sync_sub_and_fetch(&m_ref, 1);
			if (ref == 0) {
				delete this;
			}
		}
	};

	class Context;
	class MulticastGroup
	{
	private:
		struct _mc_array {
			int cap;
			int number;
			uint32_t *data;
		};

		_mc_array enter_queue;
		_mc_array leave_queue;
		int cap;
		int number;
		uint32_t * data;

	public:
		MulticastGroup():cap(0), number(0),data(NULL) {};

		void enter(uint32_t handle) { push_array(&enter_queue, handle); }
		void leave(uint32_t handle) { push_array(&leave_queue, handle); }

		/**
		 * cast a message to this group
		 */
		int cast(Context * from, MulticastMessage *msg);

	private:
		void push_array(_mc_array * a, uint32_t v) 
		{
			if (a->number >= a->cap) {
				a->cap *= 2;
				if (a->cap == 0) {
					a->cap = 4;
				}
				a->data = (uint32_t *)realloc(a->data, a->cap * sizeof(uint32_t));
			}
			a->data[a->number++] = v;
		}

		void combine_queue(Context * from);

	};
}

#endif //
