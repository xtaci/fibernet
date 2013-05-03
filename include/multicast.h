#ifndef __FIBERNET_MULTICAST_H__
#define __FIBERNET_MULTICAST_H__

namespace fibernet 
{

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
		MulticastMessage(const void * msg, size_t sz, uint32_t source)
		{
			m_ref = 0;
			m_msg = msg;
			m_sz = sz;
			m_source = source;
		}

		~MulticastMessage() { free(m_msg); }

		/**
		 * copy msg
		 */
		void copy(int copy) 
		{
			int r = __sync_add_and_fetch(&m_ref, copy);
			if (r == 0) { delete this; }
		}
	};

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
		MulticastGroup():cap(0), number(0),data(NULL);

		void enter(uint32_t handle) { push_array(&group->enter_queue, handle); }
		void leave(uint32_t handle) { push_array(&group->leave_queue, handle); }

		/**
		 * cast a message to this group
		 */
		int cast(Context * from, MulticastMessage *msg) 
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

	private:
		void push_array(_mc_array * a, uint32_t v) 
		{
			if (a->number >= a->cap) {
				a->cap *= 2;
				if (a->cap == 0) {
					a->cap = 4;
				}
				a->data = realloc(a->data, a->cap * sizeof(uint32_t));
			}
			a->data[a->number++] = v;
		}

		int compar_uint(const void *a, const void *b) {
			const uint32_t * aa = a;
			const uint32_t * bb = b;
			return (int)(*aa - *bb);
		}

		void combine_queue(Context * from) 
		{
			qsort(this->enter_queue.data, this->enter_queue.number, sizeof(uint32_t), compar_uint);
			qsort(this->leave_queue.data, this->leave_queue.number, sizeof(uint32_t), compar_uint);
			int i;
			int enter = this->enter_queue.number;
			uint32_t last = 0;

			int new_size = this->number + enter;
			if (new_size > this->cap) {
				this->data = realloc(this->data, new_size * sizeof(uint32_t));
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

	};
}

#endif //
