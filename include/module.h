#ifndef __FIBERNET_MODULE_H__
#define	__FIBERNET_MODULE_H__

#include <assert.h>
#include <string.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "context.h"

#define MAX_MODULE_TYPE 32

namespace fibernet {
	
	typedef void * (*fibernet_dl_create)(void);
	typedef int (*fibernet_dl_init)(void * inst, Context *, const char * parm);
	typedef void (*fibernet_dl_release)(void * inst);

	/**
	 * Module definition
	 */
	struct Module {
		const char * name;			// the name of the module
		void * module;				// dlopen result, handle
		fibernet_dl_create create;	// point to create function 
		fibernet_dl_init init;		// point to init function
		fibernet_dl_release release;	// pointer to release function

		/**
		 * module create/init/release
		 */
		void * call_create() 
		{
			if (create) {
				return create();
			} else {
				return (void *)(intptr_t)(~0);
			}
		}

		int call_init(void * inst, Context *ctx, const char * parm) 
		{
			return init(inst, ctx, parm);
		}

		void call_release(void *inst) 
		{
			if (release) {
				release(inst);
			}
		}
	};

	/**
	 * Global Modules definition
	 */
	class GlobalModules {
	private:
		int m_count;				// module count
		int m_lock;					// lock
		const char * m_path;		// module path
		Module m[MAX_MODULE_TYPE];	// all modules

		static GlobalModules * _instance;
	public:

		static void create_instance(const char * path)
		{
			if (!_instance) _instance = new GlobalModules(path);
		}

		static GlobalModules * instance() { return _instance;}

		/**
		 * query a module 
		 */
		Module * query(const char * name) 
		{
			Module * result = find(name);
			if (result)
				return result;

			while(__sync_lock_test_and_set(&m_lock,1)) {}

			result = find(name); // double check

			if (result == NULL && m_count < MAX_MODULE_TYPE) {
				int index = m_count;
				void * dl = try_open(name);
				if (dl) {
					m[index].name = name;
					m[index].module = dl;

					if (open_sym(&m[index]) == 0) {
						m[index].name = strdup(name);
						m_count ++;
						result = &m[index];
					}
				}
			}

			__sync_lock_release(&m_lock);

			return result;
		}

		void insert(Module *mod) 
		{
			while(__sync_lock_test_and_set(&m_lock,1)) {}

			Module * m = find(mod->name);
			assert(m == NULL && m_count < MAX_MODULE_TYPE);
			int index = m_count;
			m[index] = *mod;
			++m_count;
			__sync_lock_release(&m_lock);
		}

	private:
		GlobalModules(const char * path):m_count(0),m_lock(0) { m_path = strdup(path); }

		/**
		 * try opening a .so 
		 */
		void * try_open(const char * name) 
		{
			const char * path = m_path;
			// substitute ? mark, like
			// service/?.so 
			size_t path_size = strlen(path);
			size_t name_size = strlen(name);

			int sz = path_size + name_size;
			char tmp[sz];
			int i;
			for (i=0;path[i]!='?' && path[i]!='\0';i++) {
				tmp[i] = path[i];
			}
			memcpy(tmp+i,name,name_size);
			if (path[i] == '?') {
				strcpy(tmp+i+name_size,path+i+1);
			} else {
				fprintf(stderr,"Invalid C service path\n");
				exit(1);
			}

			void * dl = dlopen(tmp, RTLD_NOW | RTLD_GLOBAL);

			if (dl == NULL) {
				fprintf(stderr, "try open %s failed : %s\n",tmp,dlerror());
			}

			return dl;
		}

		/**
		 * find a module by name
		 */
		Module * find(const char * name) 
		{
			int i;
			for (i=0;i<m_count;i++) {
				if (strcmp(m[i].name,name)==0) {
					return &m[i];
				}
			}
			return NULL;
		}

		/**
		 * load module symbol
		 */
		int open_sym(Module *mod) 
		{
			size_t name_size = strlen(mod->name);
			char tmp[name_size + 9]; // create/init/release , longest name is release (7)
			memcpy(tmp, mod->name, name_size);

			strcpy(tmp+name_size, "_create");
			*(void **) (&mod->create) = dlsym(mod->module, tmp);
			strcpy(tmp+name_size, "_init");
			*(void **) (&mod->init) = dlsym(mod->module, tmp);
			strcpy(tmp+name_size, "_release");
			*(void **) (&mod->release) = dlsym(mod->module, tmp);

			return mod->init == NULL;
		}


	};
}

#endif //
