/*
 * schedule.h -- Job control.
 *
 * The scheduler maintains a list of jobs in a queue.  A job means a function to
 * be called along with an argument to be passed at the call.  After the call
 * the scheduler takes the return value of the funtion to decide wether the job
 * have to be placed in the queue again or can be removed.
 * It is the resposibility of an external timer function to make periodical
 * calls of schedule() to perform the jobs.
 *
 * AltF4 - Jan 31, 2002
 *
 */
#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__


void  sched_init (void (*ctrl_func)(long msec));
			/* Register a control function to be used by the scheduler to set a
			 * timer value for periodically calling of the schedule() function.
			 * If a job is registered and no other job was in the queue before
			 * 'ctrl_func' will be called with the number of milliseconds as the
			 * argument.  Else if the last job of the queue will be unregistered
			 * these function is called with a '-1' argument to signalize that no
			 * more periodical calls are needed at the moment.
			*/


typedef int (*SCHED_FUNC) (void *, long);
			/* Function type of jobs to be registered with the scheduler.  When the
			 * job is called by the scheduler its return value is interpreted as
			 * following:
			 * 0:  (JOB_DONE) remove from the queue.  the job is responsible of
			 *     freeing all no longer used resources.
			 * +1: (JOB_KEEP) hold in queue for later restart, the job's priority
			 *     stays unchanged.
			 * -1: (JOB_AGED) hold in queue, but decreases the priority a bit
			 *     because the job got older.
			 * -2: (JOB_NOOP) same as -1 but sort in in the job at the very end of
			 *     the queue.  intended for jobs which are waiting for something.
			 */

LONG  schedule (int max_jobs);
			/* Perform scheduling of registered jobs.  'max_jobs` determines
			 * the maximum number of jobs to be scheduled:
			 * = 0: no limit, that means all jobs which are actually in the queue
			 *      and also all which may be registered additional while this run,
			 * > 0: up to this number of jobs including new ones posibly registered
			 *      while this run,
			 * < 0: all jobs in the queue, but without newly registered.
			 * The return value is either '0' if no more jobs are registered
			 * or the number of milliseconds after that the next scheduling run
			 * should be performed.
			*/

BOOL  sched_insert (SCHED_FUNC, void * arg, long hash, int prio);
			/* Register the function 'job' to run at the next call of schedule(),
			 * with 'arg' passed as its first argument and a 0 as the second.
			*/
ULONG sched_remove (SCHED_FUNC, void * arg);
			/* Unregister all jobs for that 'job' and 'arg' matches.  If 'job' is
			 * a NULL argument it will be taken as a wildcard and only 'arg' is
			 * used for comparsion.
			 * The return value is the number of unregistered jobs.
			*/
ULONG sched_clear (long hash);
			/* Performs immediately all pending jobs for that 'hash' matches, with
			 * their long argument set to 'hash' and unregister them independent
			 * from their return value.  This is normally used to remove jobs for
			 * objects which will become invalidated.
			 * The return value is the number of unregistered jobs.
			*/


#endif /*__SCHEDULE_H__*/

