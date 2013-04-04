#include "command.h"
#include "context.h"
#include "env.h"
#include "timer.h"
#include "group_mgr.h"
#include "errorlog.h"

namespace fibernet
{
	const char * Command::exec(Context * ctx, const char * cmd,const char * param) 
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
			Timer::instance()->timeout(ctx->m_handle, ti, session);
			sprintf(ctx->m_result, "%d", session);
			return ctx->m_result;
		}

		/**
		 * Syntax: LOCK
		 * lock the MQ, the message delivered to this MQ is not dispatched
		 * after the session sent a message.
		 */
		if (strcmp(cmd,"LOCK") == 0) {
			if (ctx->m_init == false) {
				return NULL;
			}
			ctx->queue->lock(ctx->m_sess_id+1);
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
				sprintf(ctx->m_result, ":%x", ctx->m_handle);
				return ctx->m_result;
			} else if (param[0] == '.') {
				if (Handle::instance()->reg_name(ctx->m_handle, param + 1)) {
					return param +1;
				} 
				return NULL;
			} else {
				assert(ctx->m_handle!=0);
				struct remote_name *rname = (struct remote_name*)malloc(sizeof(*rname));
				_copy_name(rname->name, param);
				rname->handle = ctx->m_handle;
				Harbor::instance()->reg(rname);
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
				if (Handle::instance()->reg_name(ctx->m_handle, param + 1)) {
					return param+1;
				} 
				return NULL;
			} else {
				struct remote_name *rname = (struct remote_name*)malloc(sizeof(*rname));
				_copy_name(rname->name, name);
				rname->handle = handle_id;
				Harbor::instance()->reg(rname);
			}
			return NULL;
		}

		/**
		 * Synax: NOW
		 * get current time, in 10ms
		 */
		if (strcmp(cmd,"NOW") == 0) {
			uint32_t ti = Timer::instance()->gettime();
			sprintf(ctx->m_result,"%u",ti);
			return ctx->m_result;
		}

		/**
		 * Syntax EXIT
		 * destroy context
		 */
		if (strcmp(cmd,"EXIT") == 0) {
			Handle::instance()->retire(ctx->m_handle);
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
				handle = Handle::instance()->get_handle(param+1);
			} else {
				errorlog(ctx, "Can't kill %s",param);
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
			if (new_ctx == NULL) {
				fprintf(stderr, "Launch %s %s failed\n",mod,args);
				return NULL;
			} else {
				_id_to_hex(ctx->m_result, new_ctx->m_handle);
				return ctx->m_result;
			}
		}

		/**
		 * Syntax: GETENV key
		 * Application level environment variable
		 */
		if (strcmp(cmd,"GETENV") == 0) {
			return Env::instance()->get(param);
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
			
			Env::instance()->set(key,param);
			return NULL;
		}

		/**
		 * Syntax: STARTTIME
		 * starttime in secs, since epoch
		 */
		if (strcmp(cmd,"STARTTIME") == 0) {
			uint32_t sec = Timer::instance()->gettime_fixsec();
			sprintf(ctx->m_result,"%u",sec);
			return ctx->m_result;
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
			return _group_command(ctx, cmd, group_handle,addr);
		}

		/**
		 * syntax: ENDLESS
		 * set endless
		 */
		if (strcmp(cmd,"ENDLESS") == 0) {
			if (ctx->m_endless) {
				strcpy(ctx->m_result, "1");
				ctx->m_endless = false;
				return ctx->m_result;
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

	const char * Command::_group_command(Context * ctx, const char * cmd, int group_handle, uint32_t addr) 
	{
		uint32_t self;
		if (addr != 0) {
			if (Harbor::instance()->isremote(addr)) {
				errorlog(ctx, "Can't add remote handle %x",addr);
				return NULL;
			}
			self = addr;
		} else {
			self = ctx->m_handle;
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
			_id_to_hex(ctx->m_result, addr);
			return ctx->m_result;
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
}
