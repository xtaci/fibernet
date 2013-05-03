#ifndef __FIBERNET_ENV_H__
#define __FIBERNET_ENV_H__

#include <lua.h>
#include <lauxlib.h>

#include <stdlib.h>
#include <assert.h>

namespace fibernet
{
	class Env 
	{
	private:
		int m_lock;
		lua_State *L;
		static Env * m_instance;

	public:
		static Env * instance() 
		{
			if (!m_instance) m_instance = new Env;
			return m_instance;
		}

		const char * get(const char *key) 
		{
			lock();

			lua_State *L = E->L;
			
			lua_getglobal(L, key);
			const char * result = lua_tostring(L, -1);
			lua_pop(L, 1);

			unlock();

			return result;
		}

		void set(const char *key, const char *value) 
		{
			lock();
			
			lua_State *L = E->L;
			lua_getglobal(L, key);
			assert(lua_isnil(L, -1));
			lua_pop(L,1);
			lua_pushstring(L,value);
			lua_setglobal(L,key);

			unlock();
		}

	private:
		Env(const Env&);
		Env& operator= (const Env&);

		Env():m_lock(0)
		{
			L = luaL_newstate();
		}

		inline void lock() { while (__sync_lock_test_and_set(&m_lock,1)) {} }
		inline void unlock() { __sync_lock_release(&m_lock); }
	};
}


#endif //
