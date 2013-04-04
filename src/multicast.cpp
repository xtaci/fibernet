#include "mutlicast.h"

namespace fibernet 
{
	int MulticastGroup::cast(Context * from, MulticastMessage *msg)
	{
		combine_queue(from);
		int release = 0;
		if (group->number > 0) {
			uint32_t source = from->handle();
			msg->copy(number);
			int i;
			for (i=0;i<number;i++) {
				uint32_t p = data[i];
				Context * ctx = Handle::instance()->grab(p);
				if (ctx) {
					ctx->send(msg, 0 , source, PTYPE_MULTICAST , 0);
					ctx->release();
				} else {
					leave(p);
					++release;
				}
			}
		}
		
		msg->copy(msg, -release);
		
		return number - release;
	}

	void MulticastGroup::combine_queue(Context * from) 
	{
		qsort(this->enter_queue.data, this->enter_queue.number, sizeof(uint32_t), compar_uint);
		qsort(this->leave_queue.data, this->leave_queue.number, sizeof(uint32_t), compar_uint);
		int i;
		int enter = this->enter_queue.number;
		uint32_t last = 0;

		int new_size = this->number + enter;
		if (new_size > this->cap) {
			this->data = (uint32_t *)realloc(this->data, new_size * sizeof(uint32_t));
			this->cap = new_size;
		}

		// combine enter queue
		int old_index = this->number - 1;
		int new_index = new_size - 1;
		for (i= enter - 1;i >=0 ; i--) {
			uint32_t handle = this->enter_queue.data[i];
			if (handle == last)
				continue;
			last = handle;
			if (old_index < 0) {
				this->data[new_index] = handle;
			} else {
				uint32_t p = this->data[old_index];
				if (handle == p)
					continue;
				if (handle > p) {
					this->data[new_index] = handle;
				} else {
					this->data[new_index] = this->data[old_index];
					--old_index;
					last = 0;
					++i;
				}
			}
			--new_index;
		}
		while (old_index >= 0) {
			this->data[new_index] = this->data[old_index];
			--old_index;
			--new_index;
		}
		this->enter_queue.number = 0;

		// remove leave queue
		old_index = new_index + 1;
		new_index = 0;

		int count = new_size - old_index;

		int leave = this->leave_queue.number;
		for (i=0;i<leave;i++) {
			if (old_index >= new_size) {
				count = 0;
				break;
			}
			uint32_t handle = this->leave_queue.data[i];
			uint32_t p = this->data[old_index];
			if (handle == p) {
				--count;
				++old_index;
			} else if ( handle > p) {
				this->data[new_index] = this->data[old_index];
				++new_index;
				++old_index;
				--i;
			} else {
				skynet_error(from, "Try to remove a none exist handle : %x", handle);
			}
		}
		while (new_index < count) {
			this->data[new_index] = this->data[old_index];
			++new_index;
			++old_index;
		}

		this->leave_queue.number = 0;

		this->number = new_index;
	}

}
