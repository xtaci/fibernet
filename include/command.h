#ifndef __FIBERNET_COMMAND_H__
#define __FIBERNET_COMMAND_H__

namespace fibernet 
{
	/**
	 * command sub-system
	 */
	class Command 
	{
	public:
		/**
		 * execute a command under context.
		 */
		const char * exec(Context * ctx, const char * cmd , const char * param) 
		{
			/**
			 * Syntax: TIMEOUT N*10ms
			 * a new session id is returned
			 * when time's up, a message is delivered with 
			 * the same session id
			 */
			if (strcmp(cmd,"TIMEOUT") == 0) {
				char * session_ptr = NULL;
				int ti = strtol(param, &session_ptr, 10);
				int session = ctx->newsession();
				if (session < 0) 
					return NULL;
				skynet_timeout(ctx->handle(), ti, session);
				sprintf(ctx->result, "%d", session);
				return ctx->result;
			}

			/**
			 * Syntax: LOCK
			 * lock the MQ, the message delivered to this MQ is not dispatched
			 * after the session sent a message.
			 */
			if (strcmp(cmd,"LOCK") == 0) {
				if (ctx->init == false) {
					return NULL;
				}
				ctx->queue->lock(ctx->session_id+1);
				return NULL;
			}

			/**
			 * Syntax: REG / REG .name / REG harbor
			 * 1. get context handle
			 * 2. register a name 
			 * 3. register to harbor 
			 */
			if (strcmp(cmd,"REG") == 0) {
				if (param == NULL || param[0] == '\0') {
					sprintf(ctx->result, ":%x", ctx->handle);
					return ctx->result;
				} else if (param[0] == '.') {
					return Handle::instance()->reg_name(ctx->handle, param + 1);
				} else {
					assert(ctx->handle!=0);
					struct remote_name *rname = malloc(sizeof(*rname));
					_copy_name(rname->name, param);
					rname->handle = ctx->handle;
					skynet_harbor_register(rname);
					return NULL;
				}
			}

			/**
			 * Syntax: NAME name :handle
			 * name a :handle, like REG
			 */
			if (strcmp(cmd,"NAME") == 0) {
				int size = strlen(param);
				char name[size+1];
				char handle[size+1];
				sscanf(param,"%s %s",name,handle);
				if (handle[0] != ':') {
					return NULL;
				}
				uint32_t handle_id = strtoul(handle+1, NULL, 16);
				if (handle_id == 0) {
					return NULL;
				}
				if (name[0] == '.') {
					return skynet_handle_namehandle(handle_id, name + 1);
				} else {
					struct remote_name *rname = malloc(sizeof(*rname));
					_copy_name(rname->name, name);
					rname->handle = handle_id;
					skynet_harbor_register(rname);
				}
				return NULL;
			}

			/**
			 * Synax: NOW
			 * get current time, in 10ms
			 */
			if (strcmp(cmd,"NOW") == 0) {
				uint32_t ti = skynet_gettime();
				sprintf(ctx->result,"%u",ti);
				return ctx->result;
			}

			/**
			 * Syntax EXIT
			 * destroy context
			 */
			if (strcmp(cmd,"EXIT") == 0) {
				Handle::instance()->retire(ctx->handle);
				return NULL;
			}

			/**
			 * Syntax:  KILL :handle / KILL .name 
			 */
			if (strcmp(cmd,"KILL") == 0) {
				uint32_t handle = 0;
				if (param[0] == ':') {
					handle = strtoul(param+1, NULL, 16);
				} else if (param[0] == '.') {
					handle = skynet_handle_findname(param+1);
				} else {
					skynet_error(context, "Can't kill %s",param);
					// todo : kill global service
				}
				if (handle) {
					Handle::instance()->retire(handle);
				}
				return NULL;
			}

			/**
			 * Syntax: LAUNCH name param
			 * Launch a module with parm, return handle in hex;
			 */
			if (strcmp(cmd,"LAUNCH") == 0) {
				size_t sz = strlen(param);
				char tmp[sz+1];
				strcpy(tmp,param);
				char * args = tmp;
				char * mod = strsep(&args, " \t\r\n");
				args = strsep(&args, "\r\n");
				Context * new_ctx = ContextFactory::create(mod,args);
				if (inst == NULL) {
					fprintf(stderr, "Launch %s %s failed\n",mod,args);
					return NULL;
				} else {
					_id_to_hex(ctx->result, new_ctx->handle);
					return ctx->result;
				}
			}

			/**
			 * Syntax: GETENV key
			 * Application level environment variable
			 */
			if (strcmp(cmd,"GETENV") == 0) {
				return skynet_getenv(param);
			}

			/**
			 * Syntax: SETENV key value
			 */
			if (strcmp(cmd,"SETENV") == 0) {
				size_t sz = strlen(param);
				char key[sz+1];
				int i;
				for (i=0;param[i] != ' ' && param[i];i++) {
					key[i] = param[i];
				}
				if (param[i] == '\0')
					return NULL;

				key[i] = '\0';
				param += i+1;
				
				skynet_setenv(key,param);
				return NULL;
			}

			/**
			 * Syntax: STARTTIME
			 * starttime in secs, since epoch
			 */
			if (strcmp(cmd,"STARTTIME") == 0) {
				uint32_t sec = skynet_gettime_fixsec();
				sprintf(ctx->result,"%u",sec);
				return ctx->result;
			}

			/**
			 * GROUP commands, see _group_command
			 */
			if (strcmp(cmd,"GROUP") == 0) {
				int sz = strlen(param);
				char tmp[sz+1];
				strcpy(tmp,param);
				tmp[sz] = '\0';
				char cmd[sz+1];
				int group_handle=0;
				uint32_t addr=0;
				sscanf(tmp, "%s %d :%x",cmd,&group_handle,&addr);
				return _group_command(context, cmd, group_handle,addr);
			}

			/**
			 * syntax: ENDLESS
			 * set endless
			 */
			if (strcmp(cmd,"ENDLESS") == 0) {
				if (ctx->endless) {
					strcpy(ctx->result, "1");
					ctx->endless = false;
					return ctx->result;
				}
				return NULL;
			}

			/**
			 * syntax: ABORT
			 * destroy all context
			 */
			if (strcmp(cmd,"ABORT") == 0) {
				Handle::instance()->retireall();
				return NULL;
			}

			return NULL;
		}
	private:
		/**
		 * group command, start with GROUP command params
		 */
		static const char * _group_command(Context * ctx, const char * cmd, int group_handle, uint32_t addr) 
		{
			uint32_t self;
			if (addr != 0) {
				if (skynet_harbor_message_isremote(v)) {
					skynet_error(ctx, "Can't add remote handle %x",v);
					return NULL;
				}
				self = addr;
			} else {
				self = ctx->handle;
			}

			/**
			 * Syntax: ENTER group_handle handle / ENTER group_handle
			 */
			if (strcmp(cmd, "ENTER") == 0) {
				GroupManager::instance()->enter(group_handle, self);
				return NULL;
			}

			/**
			 * Syntax: LEAVE group_handle handle / LEAVE group_handle
			 */
			if (strcmp(cmd, "LEAVE") == 0) {
				GroupManager::instance()->leave(group_handle, self);
				return NULL;
			}

			/**
			 * Syntax: QUERY group_handle
			 */
			if (strcmp(cmd, "QUERY") == 0) {
				uint32_t addr = GroupManager::instance()->query(group_handle);
				if (addr == 0) {
					return NULL;
				}
				_id_to_hex(ctx->result, addr);
				return ctx->result;
			}

			/**
			 * Syntax: CLEAR group_handle
			 */
			if (strcmp(cmd, "CLEAR") == 0) {
				GroupManager::instance()->clear(group_handle);
				return NULL;
			}

			return NULL;
		}

		static void _id_to_hex(char * str, uint32_t id) 
		{
			int i;
			static char hex[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };
			str[0] = ':';
			for (i=0;i<8;i++) {
				str[i+1] = hex[(id >> ((7-i) * 4))&0xf];
			}
			str[9] = '\0';
		}

		static void _copy_name(char name[GLOBALNAME_LENGTH], const char * addr) 
		{
			int i;
			for (i=0;i<GLOBALNAME_LENGTH && addr[i];i++) {
				name[i] = addr[i];
			}
			for (;i<GLOBALNAME_LENGTH;i++) {
				name[i] = '\0';
			}
		}

	}
}

#endif //
