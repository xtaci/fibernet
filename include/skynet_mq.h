#ifndef SKYNET_MESSAGE_QUEUE_H
#define SKYNET_MESSAGE_QUEUE_H

#include <stdlib.h>
#include <stdint.h>

#define DEFAULT_QUEUE_SIZE 64;
#define MAX_GLOBAL_MQ 0x10000

// 0 means mq is not in global mq.
// 1 means mq is in global mq , or the message is dispatching.
// 2 means message is dispatching with locked session set.
// 3 means mq is not in global mq, and locked session has been set.

#define MQ_IN_GLOBAL 1
#define MQ_DISPATCHING 2
#define MQ_LOCKED 3

#define LOCK(q) while (__sync_lock_test_and_set(&(q)->lock,1)) {}
#define UNLOCK(q) __sync_lock_release(&(q)->lock);

#define GP(p) ((p) % MAX_GLOBAL_MQ)

namespace fibernet 
{
	/**
	 * Global Message Queue
	 */
	class GlobalMQ {
	private:
		uint32_t head;
		uint32_t tail;
		MQ ** queue;
		bool * flag;
	public:
		GlobalMQ():head(0), tail(0)
		{
			queue = new MQ*[MAX_GLOBAL_MQ];
			flag = new bool[MAX_GLOBAL_MQ];
			memset(flag, 0, sizeof(bool) * MAX_GLOBAL_MQ);
		}

		~GlobalMQ()
		{
			delete [] queue;
			delete [] flag;
		}

		MQ * pop(void)
		{
			uint32_t head = head;
			uint32_t head_ptr = GP(head);

			if (head_ptr == GP(tail)) {	//empty queue
				return NULL;
			}

			if(!flag[head_ptr]) {
				return NULL;
			}

			MQ * mq = queue[head_ptr];
			if (!__sync_bool_compare_and_swap(&head, head, head+1)) {
				return NULL;
			}
			q->flag[head_ptr] = false;
			__sync_synchronize();

			return mq;
		}

		void push(MQ * queue)
		{
			uint32_t tail = GP(__sync_fetch_and_add(&tail,1));
			queue[tail] = queue;
			__sync_synchronize();
			flag[tail] = true;
			__sync_synchronize();
		}

		void force_push(MQ * queue)
		{
			assert(queue->in_global);
			push(queue);
		}
	}

	/**
	 * Message Queue
	 */
	class MQ 
	{
	public:
		struct Message 
		{
			uint32_t source;
			int session;
			void * data;
			size_t sz;
		};

	private:
		uint32_t handle;
		int cap;
		int head;
		int tail;
		int lock;
		int release;
		int lock_session;
		int in_global;
		Message *queue;
	public:

		MQ(uint32_t _handle):handle(_handle),
			cap(DEFAULT_QUEUE_SIZE),
			head(0),
			tail(0),
			lock(0),
			in_global(MQ_IN_GLOBAL),
			release(0),
			lock_session(0)
		{
			queue = new Message[cap];
		}

		~MQ()
		{
			delete [] queue;
		}
		
		void mark_release()
		{
		}
		
		int release()
		{
		}
		
		inline uint32_t handle() { return handle;}

		int pop(Message * message)
		{
			int ret = 1;
			lock()

			if (q->head != q->tail) {
				*message = q->queue[q->head];
				ret = 0;
				if ( ++ q->head >= q->cap) {
					q->head = 0;
				}
			}

			if (ret) {
				q->in_global = 0;
			}
			
			unlock()

			return ret;
		}

		void push(Message * message)
		{
		}	

		void lock(int session)
		{
		}

		void force_push()
		{
		}	
	private:
		void lock() { while (__sync_lock_test_and_set(&lock,1)) {} }
		void unlock() { __sync_lock_release(&lock); }

		/** 
		 * queue expanding
		 */
		void expand() 
		{
			Message * new_queue = new Message[cap*2];
			int i;
			for (i=0;i<cap;i++) {
				new_queue[i] = queue[(head + i) % cap];
			}
			head = 0;
			tail = q->cap;
			cap *= 2;
			
			delete [] queue;	
			queue = new_queue;
		}
	}
}

#endif
