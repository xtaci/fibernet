#include "timer.h"
#include "mq.h"
#include "context.h"

namespace fibernet
{
	Timer * Timer::m_instance = NULL;

	int Timer::timeout(uint32_t handle, int time, int session) 
	{
		if (time == 0) {
			Message message;
			message.source = 0;
			message.session = session;
			message.data = NULL;
			message.sz = PTYPE_RESPONSE << HANDLE_REMOTE_SHIFT;

			if (Context::push(handle, &message)) {
				return -1;
			}
		} else {
			timer_node * node = new timer_node;
			node->event.handle = handle;
			node->event.session = session;
			
			while (__sync_lock_test_and_set(&m_lock,1)) {};
				node->expire = time + m_time;
				add_node(node);
			__sync_lock_release(&m_lock);
		}

		return session;
	}

	void Timer::execute()
	{
		while (__sync_lock_test_and_set(&m_lock,1)) {};

		// process near events
		int idx = m_time & TIME_NEAR_MASK;
		struct timer_node * node, * safe;
		list_for_each_entry_safe(node, safe, &near[idx], node) {
			Message message;
			message.source = 0;
			message.session = node->event.session;
			message.data = NULL;
			message.sz = PTYPE_RESPONSE << HANDLE_REMOTE_SHIFT;

			Context::push(node->event.handle, &message);

			list_del(&node->node);
			delete node;
		}

		++m_time;		// 10ms has passed
	
		// schedule further events
		int msb = TIME_NEAR;					// most significant bit
		int time = m_time >> TIME_NEAR_SHIFT;	// 24bit part 
		int i=0;
	
		// for each 6-bit part
		while ((m_time & (msb-1))==0) {
			idx=time & TIME_LEVEL_MASK;
			if (idx!=0) { // ignore 0
				--idx;
				list_for_each_entry_safe(node,safe,&t[i][idx], node) {
					list_del(&node->node);
					add_node(node);
				}
				break;				
			}
			msb <<= TIME_LEVEL_SHIFT;
			time >>= TIME_LEVEL_SHIFT;
			++i;
		}
		__sync_lock_release(&m_lock);
	}
}
