#ifndef __FIBERNET_MONITOR_H__
#define __FIBERNET_MONITOR_H__

#include <stdint.h>

namespace fibernet
{
	class Monitor 
	{
	private:
		int version;
		int check_version;
		uint32_t m_src;
		uint32_t m_dest;

	public:
		Monitor(): version(0), check_version(0), m_src(0), m_dest(0) {}

		void trigger(uint32_t src, uint32_t dest) 
		{
			m_src = src;
			m_dest= dest;
			__sync_fetch_and_add(&version , 1);
		}

		void check();
	};
}

#endif //
