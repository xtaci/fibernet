#ifndef _RWLOCK_H_
#define _RWLOCK_H_

namespace fibernet 
{
	class rwlock 
	{
	private:
		int write;
		int read;
	public:	
		rwlock():write(0), read(0) { }

		/**
		 * read lock
		 */
		inline void rlock() 
		{
			for (;;) {
				while(write) {
					__sync_synchronize();
				}

				__sync_add_and_fetch(&read,1);

				if (write) {
					__sync_sub_and_fetch(&read,1);
				} else {
					break;
				}
			}
		}

		/**
		 * write lock
		 */
		inline void wlock() 
		{
			while (__sync_lock_test_and_set(&lock->write,1)) {}
			while(lock->read) {
				__sync_synchronize();
			}
		}

		/**
		 * write unlock 
		 */
		inline void wunlock(struct rwlock *lock) 
		{
			__sync_lock_release(&lock->write);
		}

		/**
		 * read unlock
		 */
		inline void runlock(struct rwlock *lock) 
		{
			__sync_sub_and_fetch(&lock->read,1);
		}
	};
}

#endif
