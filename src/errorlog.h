#include "errorlog.h"

namespace fibernet
{
	static void errorlog(Context * context, const char *msg, ...) 
	{
		static int logger = -1;
		if (logger < 0) {
			logger = Handle::instance()->get_handle("logger");
		}
		if (logger < 0) {
			return;
		}

		char tmp[LOG_MESSAGE_SIZE];

		va_list ap;
		va_start(ap,msg);
		int len = vsnprintf(tmp, LOG_MESSAGE_SIZE, msg, ap);
		va_end(ap);

		if (len >= LOG_MESSAGE_SIZE) {
			len = LOG_MESSAGE_SIZE - 1;
			tmp[len] = '\0';
		}

		Messsage smsg;
		if (context == NULL) {
			smsg.source = 0;
		} else {
			smsg.source = context->handle();
		}
		smsg.session = 0;
		smsg.data = strdup(tmp);
		smsg.sz = len | (PTYPE_TEXT << HANDLE_REMOTE_SHIFT);
		logger->push(&smsg);
	}

}
