#ifndef __FIBERNET_MONITOR_H__
#define __FIBERNET_MONITOR_H__

namespace fibernet
{
	class Monitor 
	{
	private:
		int version;
		int check_version;
		uint32_t m_src;
		uint32_t m_des;

	public:
		Monitor(): version(0), check_version(0), m_src(0), m_dest(0);

		void trigger(uint32_t src, uint32_t dest) 
		{
			m_src = src;
			m_dest= dest;
			__sync_fetch_and_add(&version , 1);
		}

		void check() 
		{
			if (version == check_version) {
				if (m_dest) {
					Context::endless(m_dest);
					skynet_error(NULL, "A message from [ :%08x ] to [ :%08x ] maybe in an endless loop", m_src, m_dest);
				}
			} else {
				check_version = version;
			}
		}
	};
}

#endif //
