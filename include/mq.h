#ifndef __FIBERNET_MQ_H__
#define __FIBERNET_MQ_H__

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "fibernet.h"
#include "harbor.h"

#define DEFAULT_QUEUE_SIZE 64
#define MAX_GLOBAL_MQ 0x10000

// 0 means mq is not in global mq.
// 1 means mq is in global mq , or the message is dispatching.
// 2 means message is dispatching with locked session set.
// 3 means mq is not in global mq, and locked session has been set.

#define MQ_IN_GLOBAL 1
#define MQ_DISPATCHING 2
#define MQ_LOCKED 3

#define GP(p) ((p) % MAX_GLOBAL_MQ)

namespace fibernet 
{
	class MQ;
	/**
	 * Global Message Queue
	 */
	class GlobalMQ {
	private:
		uint32_t m_head;
		uint32_t m_tail;
		MQ ** queue;
		bool * flag;

		static GlobalMQ * _instance;

	private:
		GlobalMQ():m_head(0), m_tail(0)
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
	
	public:
		static GlobalMQ * instance()
		{
			if (!_instance) {
				_instance = new GlobalMQ();
			}

			return _instance;
		}

		static void release()
		{
			if (_instance) { delete _instance; }
		}

		MQ * pop(void)
		{
			uint32_t head = m_head;
			uint32_t head_ptr = GP(head);

			if (head_ptr == GP(m_tail)) {	//empty queue
				return NULL;
			}

			if(!flag[head_ptr]) {
				return NULL;
			}

			MQ * mq = queue[head_ptr];
			if (!__sync_bool_compare_and_swap(&m_head, head, head+1)) {
				return NULL;
			}

			flag[head_ptr] = false;
			__sync_synchronize();

			return mq;
		}

		void push(MQ * mq)
		{
			uint32_t tail = GP(__sync_fetch_and_add(&m_tail,1));
			queue[tail] = mq;
			__sync_synchronize();
			flag[tail] = true;
			__sync_synchronize();
		}
	};

	/**
	 * Message definition
	 */
	struct Message 
	{
		uint32_t source;
		int session;
		void * data;
		size_t sz;
	};

	/**
	 * Message Queue
	 */
	class MQ 
	{
	public:

	private:

		uint32_t m_handle;
		int m_cap;
		int m_head;
		int m_tail;
		int m_lock;
		int m_release;
		int lock_session;
		int in_global;
		Message *queue;

	public:

		MQ(uint32_t handle) : m_handle(handle),
			m_cap(DEFAULT_QUEUE_SIZE),
			m_head(0),
			m_tail(0),
			m_lock(0),
			m_release(0),
			lock_session(0),
			in_global(MQ_IN_GLOBAL)
		{
			queue = new Message[m_cap];
		}

	
		inline uint32_t handle() { return m_handle;}

		/**
		 * enter global queue
		 */
		void pushglobal() {
			LOCK();
			assert(in_global);
			if (in_global == MQ_DISPATCHING) {
				// lock message queue just now.
				in_global = MQ_LOCKED;
			}
			if (lock_session == 0) {
				GlobalMQ::instance()->push(this);
				in_global = MQ_IN_GLOBAL;
			}
			UNLOCK();
		}

		/**
		 * pop out a message
		 */
		bool pop(Message * message)
		{
			bool ret = false;
			LOCK();

			if (m_head != m_tail) {
				*message = queue[m_head];
				ret = true;
				if ( ++m_head >= m_cap) {
					m_head = 0;
				}
			}

			if (ret) {
				in_global = 0;
			}
			
			UNLOCK();

			return ret;
		}

		/**
		 * push a message
		 */
		void push(const Message * message)
		{
			assert(message);
			LOCK();
	
			// detect whether the msg sender is the locker
			// if it's the locker's msg, put it in front of the queue.
			if (lock_session !=0 && message->session == lock_session) {
				pushhead(message);
			} else {
				// queue the msg
				queue[m_tail] = *message;
				if (++m_tail >= m_cap) {
					m_tail = 0;
				}

				if (m_head == m_tail) {
					expand();
				}

				// if mq is locked, just leave
				if (lock_session == 0) {
					if (in_global == 0) {
						in_global = MQ_IN_GLOBAL;
						GlobalMQ::instance()->push(this);
					}
				}
			}
			
			UNLOCK();
		}	

		/**
		 * lock the queue by session
		 */
		void lock(int session)
		{
			LOCK();
			assert(lock_session == 0);
			assert(in_global == MQ_IN_GLOBAL);
			in_global = MQ_DISPATCHING;
			lock_session = session;
			UNLOCK();
		}

		/**
		 * release procedure, mark->(dispatcher release)
		 * mark first, later schedule.
		 */
		void mark_release()
		{
			assert(m_release == 0);
			m_release = 1;
		}
		
		int release() 
		{
			int ret = 0;
			LOCK();
			
			if (m_release) {
				UNLOCK();
				ret = drop_queue();
			} else {
				GlobalMQ::instance()->push(this);
				UNLOCK();
			}
		
			return ret;
		}

	private:

		~MQ()
		{
			delete [] queue;
		}

		inline void LOCK() { while (__sync_lock_test_and_set(&m_lock,1)) {} }
		inline void UNLOCK() { __sync_lock_release(&m_lock); }

		/** 
		 * queue expanding
		 */
		void expand() 
		{
			Message * new_queue = new Message[m_cap*2];
			int i;
			for (i=0;i<m_cap;i++) {
				new_queue[i] = queue[(m_head + i) % m_cap];
			}
			m_head = 0;
			m_tail = m_cap;
			m_cap *= 2;
			
			delete [] queue;	
			queue = new_queue;
		}

		/**
	     * push the msg to the head of the queue.
		 */
		void pushhead(const Message *message) {
			int head = m_head - 1;
			if (head < 0) {
				head = m_cap - 1;
			}
			if (head == m_tail) {
				expand();
				--m_tail;
				head = m_cap - 1;
			}

			queue[head] = *message;
			m_head = head;

			// this api use in push a unlock message, so the in_global flags must not be 0 , 
			// but the q is not exist in global queue.
			if (in_global == MQ_LOCKED) {
				GlobalMQ::instance()->push(this);
				in_global = MQ_IN_GLOBAL;
			} else {
				assert(in_global == MQ_DISPATCHING);
			}
			lock_session = 0;
		}

		/**
		 * queue drop 
		 */
		int drop_queue();
	};
}

#endif
