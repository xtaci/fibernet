#ifndef	__FIBERNET_GROUP_H__
#define __FIBERNET_GROUP_H__

#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "fibernet.h"
#include "lib/hash_table.h"

#define DEFAULT_HASHTABLE_SIZE 1024

namespace fibernet
{
	class Context;
	/**
	 * manages all groups
	 */
	class GroupManager
	{
	private:
		alg::HashTable<Context *>  m_hash;
		int m_lock;

		static GroupManager * m_instance;

	public:
		
		static GroupManager * instance()
		{
			if (!m_instance) {
				m_instance = new GroupManager();
			}
			return m_instance;
		}

		/**
		 * query a group by a handle
		 */
		uint32_t query(int group_handle);

		/**
		 * a context handle enters a group
		 */
		void enter(int group_handle, uint32_t handle)
		{
			lock();

			Context * ctx = NULL;
			if (m_hash.contains(group_handle)) {
				ctx = m_hash[group_handle];	
			} else {
				ctx = create_group(group_handle);
			}

			unlock();

			send_command(ctx, "E", handle);
		}

		/**
		 * a context handle leaves a group
		 */
		void leave(int group_handle, uint32_t handle)
		{
			lock();

			if (m_hash.contains(group_handle)) {
				Context * ctx = m_hash[group_handle];	
				send_command(ctx, "L", handle);
			}

			unlock();
		}

		/**
		 * delete a group
		 */
		void clear(int group_handle); 

	private:
		GroupManager(): m_hash(DEFAULT_HASHTABLE_SIZE) {}

		GroupManager(const GroupManager&);
		GroupManager& operator= (const GroupManager&);

		inline void lock() { while (__sync_lock_test_and_set(&m_lock,1)) {} }
		inline void unlock() { __sync_lock_release(&m_lock); }

		Context * create_group(int handle);
		void send_command(Context *ctx, const char * cmd, uint32_t node);
	};
}

#endif //
