#ifndef FIBERNET_HANDLE_H
#define FIBERNET_HANDLE_H

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <string>

#include "rbtree.h"
#include "rwlock.h"
#include "fibernet.h"

#define HANDLE_REMOTE_SHIFT 24

namespace fibernet
{
	class Context;

	class Handle {
	private:
		rwlock lock;		// read-write lock for rbtree
		uint32_t m_harbor;		// harbor id
		uint32_t handle_index;	// for generate handle id
								// if we generate one id per second
								// it will overflow in 136 years

		alg::RBTree<std::string, uint32_t> n2h;	// name to handle
		alg::RBTree<uint32_t, std::string> h2n;	// handle to name

		alg::RBTree<uint32_t, Context *> h2c;	// handle to context

	public:
		Handle(int harbor):handle_index(1)
		{
			// reserve 0 for system
			m_harbor = (uint32_t) (harbor & 0xff) << HANDLE_REMOTE_SHIFT;
		}

		/**
		 * register the context, return the handle.
		 */
		uint32_t reg(Context *ctx) 
		{
			uint32_t handle;	

			lock.wlock();
			handle = handle_index++;
			handle |= m_harbor;
			h2c.insert(handle, ctx);
			lock.wunlock();
	
			ctx->init(handle);	

			return handle;
		}

		/**
		 * name a handle
		 */
		bool reg_name(uint32_t handle, const char *name) 
		{
			std::string tmp = name;
			bool ret = false;

			lock.wlock();
			// make sure handle exists
			
			if (h2c.contains(handle) && !n2h.contains(tmp)) {
				insert_name(name, handle);
				ret = true;	
			}

			lock.wunlock();

			return ret;
		}

		/**
 		 * retire a handle
		 */
		void retire(uint32_t handle) 
		{
			Context * ctx = NULL;

			lock.wlock();

			// delete from h2c
			if (h2c.contains(handle)) { 
				ctx = h2c[handle];
				h2c.delete_key(handle);
			}

			// delete from h2n, n2h
			std::string name;
			if (h2n.contains(handle)) {
				name = h2n[handle];
				h2n.delete_key(handle);
				n2h.delete_key(name);
			}

			lock.wunlock();

			if(ctx) ctx->release();
		}


		void retireall() 
		{
			uint32_t i;
			Context * ctx;

			for (i=1;i<handle_index;i++) {
				lock.rlock();

				if (h2c.contains(i)) {
					ctx = h2c[i];
				}

				lock.runlock();

				if (ctx != NULL) { retire(i);}
			}
		}

		/**
	     * get the context of the handle
		 */	
		Context * grab(uint32_t handle)
		{
			Context * ctx = NULL;
			lock.rlock();

			if(h2c.contains(handle))
				ctx = h2c[handle];

			lock.runlock();

			return ctx;
		}

		/**
		 * get the handle from a name
		 */
		uint32_t operator [] (const char * name)
		{
			std::string tmp = name;
			uint32_t handle = 0;

			lock.rlock();
			if (n2h.contains(tmp))
				handle = n2h[tmp];

			lock.runlock();

			return handle;
		}

	private:
	
		/**
		 * get the name from the handle
		 */	
		std::string operator[] (uint32_t handle)
		{
			Context * ctx;
			std::string name;

			lock.rlock();
			if (h2n.contains(handle))
				name = h2n[handle];

			lock.runlock();

			return name;
		}
	
		const char * insert_name(const char * name, uint32_t handle) 
		{
			std::string tmp = name;
			n2h.insert(tmp, handle);
			h2n.insert(handle, tmp);
			return name;
		}
	};
}

#endif
