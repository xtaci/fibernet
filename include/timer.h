#ifndef __FIBERNET_TIMER_H__
#define	__FIBERNET_TIMER_H__

#include <time.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "lib/double_linked_list.h"

#include "fibernet.h"

#if defined(__APPLE__)
#include <sys/time.h>
#endif

#define TIME_NEAR_SHIFT 8
#define TIME_NEAR (1 << TIME_NEAR_SHIFT)
#define TIME_LEVEL_SHIFT 6
#define TIME_LEVEL (1 << TIME_LEVEL_SHIFT)
#define TIME_NEAR_MASK (TIME_NEAR-1)
#define TIME_LEVEL_MASK (TIME_LEVEL-1)

namespace fibernet
{
	class Timer
	{
	private:
		struct timer_event 
		{
			uint32_t handle;
			int session;
		};

		struct timer_node 
		{
			int expire;
			struct timer_event event;
			list_head node;
		};

		/**
		 * time is sliced into 5-parts
		 * |6b|6b|6b|6b|8b|
		 * for each part, time is hashed into a list of size 2^8 or 2^6
		 */
		list_head near[TIME_NEAR];
		list_head t[4][TIME_LEVEL-1];

		int m_lock;
		int m_time;
		uint32_t m_current;
		uint32_t m_starttime;

		static Timer * m_instance;

	public:
		static Timer * instance()
		{
			if (!m_instance) m_instance = new Timer();
			return m_instance;
		}

		inline uint32_t gettime_fixsec(void) { return m_starttime; }
		inline uint32_t gettime(void) { return m_current; }

		/**
		 * call this method periodicity to update time.
		 */
		void updatetime(void) 
		{
			uint32_t ct = _gettime();
			if (ct > m_current) 
			{
				int diff = ct - m_current;	// in 10ms
				m_current = ct;
				int i;
				for (i=0;i<diff;i++) {
					execute();
				}
			}
		}

		int timeout(uint32_t handle, int time, int session);

	private:
		Timer()
		{
			int i,j;
			for (i=0;i<TIME_NEAR;i++) {
				INIT_LIST_HEAD(&near[i]);
			}
			
			for (i=0;i<4;i++) {
				for (j=0;j<TIME_LEVEL-1;j++) {
					INIT_LIST_HEAD(&t[i][j]);
				}
			}

			m_lock = 0;
			m_time = 0;
			m_current = _gettime();

		#if !defined(__APPLE__)
			struct timespec ti;
			clock_gettime(CLOCK_REALTIME, &ti);
			uint32_t sec = (uint32_t)ti.tv_sec;
		#else
			struct timeval tv;
			gettimeofday(&tv, NULL);
			uint32_t sec = (uint32_t)tv.tv_sec;
		#endif
			uint32_t mono = _gettime() / 100;

			m_starttime = sec - mono;
		}

		/**
		 * time in 10ms
		 */
		uint32_t _gettime(void) 
		{
			uint32_t t;
		#if !defined(__APPLE__)
			struct timespec ti;
			clock_gettime(CLOCK_MONOTONIC, &ti);
			t = (uint32_t)(ti.tv_sec & 0xffffff) * 100;
			t += ti.tv_nsec / 10000000;
		#else
			struct timeval tv;
			gettimeofday(&tv, NULL);
			t = (uint32_t)(tv.tv_sec & 0xffffff) * 100;
			t += tv.tv_usec / 10000;
		#endif
			return t;
		}

		/**
		 * add a timer_node
		 */
		void add_node(struct timer_node *node)
		{
			int time = node->expire;
			int current_time = m_time;
		
			/**
			 * near events
			 */	
			if ((time|TIME_NEAR_MASK)==(current_time|TIME_NEAR_MASK)) {
				list_add_tail(&node->node, &near[time&TIME_NEAR_MASK]);
			}
			else {
				int i;
				int mask=TIME_NEAR << TIME_LEVEL_SHIFT;
				for (i=0;i<3;i++) {
					if ((time|(mask-1))==(current_time|(mask-1))) {
						break;
					}
					mask <<= TIME_LEVEL_SHIFT;
				}
				list_add_tail(&node->node, &t[i][((time>>(TIME_NEAR_SHIFT + i*TIME_LEVEL_SHIFT)) & TIME_LEVEL_MASK)-1]);	
			}
		}

		/**
		 * execute callback
		 */
		void execute();
	};
}

#endif //
