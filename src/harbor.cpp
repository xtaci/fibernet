#include "harbor.h"
#include "context.h"

namespace fibernet
{
	/**
	 * check result before other ops.
	 */
	bool Harbor::start(const char * master, const char *local) 
	{
		size_t sz = strlen(master) + strlen(local) + 32;
		char args[sz];
		sprintf(args, "%s %s %d",master,local,HARBOR >> HANDLE_REMOTE_SHIFT);
		Context * ctx = ContextFactory::create("harbor",args);
		if (ctx == NULL) {
			return false;
		}
		REMOTE = ctx;

		return true;
	}
	
	void Harbor::send(struct remote_message *rmsg, uint32_t source, int session)
	{
		int type = rmsg->sz >> HANDLE_REMOTE_SHIFT;
		rmsg->sz &= HANDLE_MASK;
		assert(type != PTYPE_SYSTEM && type != PTYPE_HARBOR);
		REMOTE->send(rmsg, sizeof(*rmsg) , source, type , session);
	}

	void Harbor::reg(struct remote_name *rname) 
	{
		int i;
		int number = 1;
		for (i=0;i<GLOBALNAME_LENGTH;i++) {
			char c = rname->name[i];
			if (!(c >= '0' && c <='9')) {
				number = 0;
				break;
			}
		}
		assert(number == 0);
		REMOTE->send(rname, sizeof(*rname), 0, PTYPE_SYSTEM , 0);
	}
}
