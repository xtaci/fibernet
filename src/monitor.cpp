#include "monitor.h"
#include "context.h"
#include "errorlog.h"

namespace fibernet
{
	void Monitor::check() 
	{
		if (version == check_version) {
			if (m_dest) {
				Context::endless(m_dest);
				errorlog(NULL, "A message from [ :%08x ] to [ :%08x ] maybe in an endless loop", m_src, m_dest);
			}
		} else {
			check_version = version;
		}
	}
}
