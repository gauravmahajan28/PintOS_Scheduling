#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "../lib/kernel/customPriorityQueue.h"


#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/** ADDED */
/**
*	Values are added from stanford websites
*/
static const int  maximumNiceness = 20;
static const int minimumNiceness = -20;
static const int defaultNiceness = 0;
static const int numberOfIntegerBits = 17;
static const int numberOfFractionBits = 14;
/** ADDED END */


/** ADDED */
static int loadAverage;
/** ADDED END*/

/** ADDED */
static struct list sleepingThreads;    // old approach, not being used

static CustomPriorityQueue customPriorityQueue;  // implemented custom priority queue to maintain threads sorted based on time
/** ADDED END */


/** ADDED */
struct semaphore lockOnSleepingList;
/** ADDED END */


/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);

  /* ADDED */
  list_init(&sleepingThreads);    // NOT BEING USED
  //size defined heuristically 
  
  customPriorityQueueInit(&customPriorityQueue, 1024);   // implemented custom priority queue

  /** ADDED */
  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();

  /** ADDED */
  /**
  *	added as per stanford website
  */
  initial_thread->nicenessValue = 0;	
  initial_thread->previousCPUBurst = 0;	

}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  
  /* ADDED */
  //sema_init (&lockOnSleepingList, 1);   // NOT BEING USED, used disable interrupts approach
  
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();
  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUedToSleepCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack' 
     member cannot be observed. */
  old_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;


//  printf("name of current thread = %s with priority = %d \n and new threads has name = %s and has prioroty = %d",thread_current()->name, thread_current()->priority, name, priority);	

  intr_set_level (old_level);

  /* Add to run queue. */
  thread_unblock (t);

  // ADDED
  /**
  *	checking if mlfqs flag is set or not
  */
  if( thread_mlfqs == false && ((t->priority) > (thread_current()->priority)) && (thread_current() != idle_thread))
  {
//	printf("thread %s should have be running with priority = %d  now but %s is running with priority %d \n",t->name, t->priority, thread_current()->name, thread_current()->priority);
  	thread_yield();
//	printf("after yield , %s thread is running with priority = %d\n",thread_current()->name, thread_current()->priority);
  }
  else if(thread_mlfqs == true && thread_current() != idle_thread)
  {
       // override thread priority
	t->priority = thread_get_priority();
	if(((t->priority) > (thread_current()->priority)) && (thread_current() != idle_thread))
	{
		thread_yield();
	}
  }
  /** ADDED END */
  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
//  printf("unblocking %s thread\n",t->name);
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  /** ADDED */
  list_insert_ordered(&ready_list,&t->elem, compareThreadPriorities, NULL);


 // schedule();		
//  printf("adding thread %s to ready queue\n",t->name);	
 //   list_push_back (&ready_list, &t->elem);

  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();

  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);
  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
// printf("in yield\n");
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) 
  {
	 /** ADDED */
	  list_insert_ordered(&ready_list,&cur->elem, compareThreadPriorities, NULL);	
	  //list_push_back (&ready_list, &cur->elem);
  }  
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
	void
thread_foreach (thread_action_func *func, void *aux)
{
	struct list_elem *e;

	ASSERT (intr_get_level () == INTR_OFF);

	for (e = list_begin (&all_list); e != list_end (&all_list);
			e = list_next (e))
	{
		struct thread *t = list_entry (e, struct thread, allelem);
		func (t, aux);
	}
}

/* Sets the current thread's priority to NEW_PRIORITY. */
	void
thread_set_priority (int new_priority) 
{
	/* ADDED */
	enum intr_level old_level;
	old_level = intr_disable ();
	struct thread *currentThread = thread_current();
	currentThread->priority = new_priority;
	if (list_empty (&ready_list))
	{
		// do nothing
//		printf("empty ready queue\n");
	}	
	else
	{
		struct thread *maxPriorityThreadInReady = list_entry (list_front(&ready_list), struct thread, elem);

	//	printf("currently max prioity thread in ready queue = %s \n",maxPriorityThreadInReady->name);
		if(maxPriorityThreadInReady->priority > currentThread->priority)
		{
			intr_set_level (old_level);
			thread_yield();
		}	
	}
	intr_set_level (old_level);
}

/* Returns the current thread's priority. */
	int
thread_get_priority (void) 
{
	if(thread_mlfqs == false)
		return thread_current ()->priority;
	else if(thread_mlfqs == true)
		return getThreadPriorityForAdvancedScheduler();
}

/* Sets the current thread's nice value to NICE. */
	void
thread_set_nice (int nice UNUSED) 
{
	/** ADDED */
//	enum intr_level old_level;
//	old_level = intr_disable ();


	enum intr_level old_level = intr_disable ();
//	thread_current()->nice = nice;
//	mlfqs_priority(thread_current());
//	test_max_priority();


	thread_current()->nicenessValue= nice;

	intr_set_level (old_level);
	/** ADDED */
	int newPriority = thread_get_priority();
	//	thread_current()->priority = newPriority;

	thread_set_priority(newPriority);
	/* Not yet implemented. */

	//	intr_set_level (old_level);
}

/* Returns the current thread's nice value. */
	int
thread_get_nice (void) 
{
	/* Not yet implemented. */
	/** ADDED */
	return thread_current()->nicenessValue;

	//	return 0;
}

/* Returns 100 times the system load average. */
	int
thread_get_load_avg (void) 
{
	/* Not yet implemented. */
	return convertToIntegerRoundingToNear(multiplyFixedPointToInteger(loadAverage, 100));
	//	return 0;
}

/* Returns 100 times the current thread's recent_cpu value. */
	int
thread_get_recent_cpu (void) 
{
	/* Not yet implemented. */
	//	return 0;

	/** ADDED */

	return (convertToIntegerRoundingToNear(multiplyFixedPointToInteger(thread_current()->previousCPUBurst, 100)));

	// as per formula
	/*	int currentLoadAverage = 2 * loadAverage / 100;
		recentCPUUsage =  100 * convertToIntegerRoundingToNear((calculateFixedPointDivision(currentLoadAverage, currentLoadAverage + 1) + convertToFixedPoint(recentCPUUsage) / 100) * thread_get_nice()); 

		return recentCPUUsage;*/
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
	static void
idle (void *idle_started_ UNUSED) 
{
	struct semaphore *idle_started = idle_started_;
	idle_thread = thread_current ();
	sema_up (idle_started);

	for (;;) 
	{
		/* Let someone else run. */
		intr_disable ();
		thread_block ();

		/* Re-enable interrupts and wait for the next one.

		   The `sti' instruction disables interrupts until the
		   completion of the next instruction, so these two
		   instructions are executed atomically.  This atomicity is
		   important; otherwise, an interrupt could be handled
		   between re-enabling interrupts and waiting for the next
		   one to occur, wasting as much as one clock tick worth of
		   time.

		   See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
		   7.11.1 "HLT Instruction". */
		asm volatile ("sti; hlt" : : : "memory");
	}
}

/* Function used as the basis for a kernel thread. */
	static void
kernel_thread (thread_func *function, void *aux) 
{
	ASSERT (function != NULL);

	intr_enable ();       /* The scheduler runs with interrupts off. */
	function (aux);       /* Execute the thread function. */
	thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
	struct thread *
running_thread (void) 
{
	uint32_t *esp;

	/* Copy the CPU's stack pointer into `esp', and then round that
	   down to the start of a page.  Because `struct thread' is
	   always at the beginning of a page and the stack pointer is
	   somewhere in the middle, this locates the curent thread. */
	asm ("mov %%esp, %0" : "=g" (esp));
	return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
	static bool
is_thread (struct thread *t)
{
	return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
	static void
init_thread (struct thread *t, const char *name, int priority)
{
	ASSERT (t != NULL);
	ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT (name != NULL);

	memset (t, 0, sizeof *t);
	t->status = THREAD_BLOCKED;
	strlcpy (t->name, name, sizeof t->name);
	t->stack = (uint8_t *) t + PGSIZE;
	t->priority = priority;
	/** ADDED */
	//	thread_set_priority(priority);

	/** ADDED */
	if(strcmp(name, "main") != 0)
	{	
		t->nicenessValue = thread_current()->nicenessValue;
		t->previousCPUBurst = thread_current()->previousCPUBurst;
	}
	else
	{
		//	printf("I am main \n");
	}

	t->magic = THREAD_MAGIC;
	list_push_back (&all_list, &t->allelem);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
	static void *
alloc_frame (struct thread *t, size_t size) 
{
	/* Stack data is always allocated in word-size units. */
	ASSERT (is_thread (t));
	ASSERT (size % sizeof (uint32_t) == 0);

	t->stack -= size;
	return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
	static struct thread *
next_thread_to_run (void) 
{
	if (list_empty (&ready_list))
		return idle_thread;
	else
		return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
	void
thread_schedule_tail (struct thread *prev)
{
	struct thread *cur = running_thread ();

	ASSERT (intr_get_level () == INTR_OFF);

	/* Mark us as running. */
	cur->status = THREAD_RUNNING;

	/* Start new time slice. */
	thread_ticks = 0;

#ifdef USERPROG
	/* Activate the new address space. */
	process_activate ();
#endif

	/* If the thread we switched from is dying, destroy its struct
	   thread.  This must happen late so that thread_exit() doesn't
	   pull out the rug under itself.  (We don't free
	   initial_thread because its memory was not obtained via
	   palloc().) */
	if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
	{
		ASSERT (prev != cur);
		palloc_free_page (prev);
	}
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
	static void
schedule (void) 
{

	wakeUpSleepingThreads();	

	struct thread *cur = running_thread ();
	struct thread *next = next_thread_to_run ();
	struct thread *prev = NULL;

	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (cur->status != THREAD_RUNNING);
	ASSERT (is_thread (next));

	if (cur != next)
		prev = switch_threads (cur, next);
	thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
	static tid_t
allocate_tid (void) 
{
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire (&tid_lock);
	tid = next_tid++;
	lock_release (&tid_lock);

	return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);


/** ADDED */
static bool compareThreadsOnWakeUpTime(struct list_elem *thread1, struct list_elem *thread2, void *aux UNUSED)
{
	struct thread *currThread = list_entry(thread1, struct thread, elem);
	struct thread *nextThread = list_entry(thread2, struct thread, elem);
	//	printf("comparing in comparator between %s thread and %s thread",currThread->name, nextThread->name);
	if(currThread->time < nextThread->time)
		return true; // true
	return false; // false
}

/** ADDED */
static bool compareThreadPriorities(struct list_elem *thread1, struct list_elem *thread2, void *aux UNUSED)
{
	struct thread *currThread = list_entry(thread1, struct thread, elem);
	struct thread *nextThread = list_entry(thread2, struct thread, elem);
	//	printf("comparing in comparator between %s thread and %s thread",currThread->name, nextThread->name);
	if(currThread->priority > nextThread->priority)
		return true; // true
	return false; // false
}


/** ADDED */
void putThreadToSleep( int time)
{

//	printf("putting thread to sleep\n");
	enum intr_level old_level;

	old_level = intr_disable ();
	struct thread *currentThread = thread_current();
	if(currentThread != idle_thread)
	{	
		currentThread->time = time;
		currentThread->status = THREAD_SLEPT;
		insertIntoCustomPriorityQueue(&customPriorityQueue, currentThread);
		schedule ();
	}
	intr_set_level (old_level);
}

/** ADDED */

void wakeUpSleepingThreads()
{

	 //printf("deleting into priority queue \n");

	int currentTime = timer_ticks();
	while(1)
	{

		if(isCustomPriorityQueueEmpty(&customPriorityQueue))
			return;
		else
		{
			struct thread *minimumThread = getMinimumFromCustomPriorityQueue(&customPriorityQueue);
			if(minimumThread->time > currentTime)
				return;

			else
			{
				struct thread *t = getMinimumFromCustomPriorityQueue(&customPriorityQueue);
				deleteFromCustomPriorityQueue(&customPriorityQueue);
				t->status = THREAD_READY;
				list_insert_ordered(&ready_list,&t->elem, compareThreadPriorities, NULL);

			}
		
		}


		/*		
		if(list_empty(&sleepingThreads))
			return;

		else
		{
			struct list_elem *sleepingElement = list_front(&sleepingThreads);
			struct thread *sleepingThread = list_entry(sleepingElement, struct thread, elem);

			//			printf("got from sleeping list = %s",sleepingThread->name);
			if(sleepingThread->time > currentTime)
			{
				return;
			}
			else
			{
				list_pop_front(&sleepingThreads);
				sleepingThread->status = THREAD_READY;
				//	thread_unblock(sleepingThread);
				*//* ADDED */
				//list_insert_ordered(&ready_list,&sleepingThread->elem, compareThreadPriorities, NULL);	
				//	list_push_back(&ready_list, &sleepingThread->elem);
	//		}
	//	}
	}
}


/** ADDED */
int getThreadPriorityForAdvancedScheduler()
{
/*	struct thread *t = thread_current();
	if (t == idle_thread)
	{
		return;
	}
	int term1 = int_to_fp(PRI_MAX);
	int term2 = div_mixed( t->previousCPUBurst, 4);
	int term3 = 2*t->nicenessValue;
	term1 = sub_fp(term1, term2);
	term1 = sub_mixed(term1, term3);
	t->priority = fp_to_int(term1);
	if (t->priority < PRI_MIN)
	{
		t->priority = PRI_MIN;
	}
	if (t->priority > PRI_MAX)
	{
		t->priority = PRI_MAX;
	}
	return t->priority;
*/
	if(thread_current() != idle_thread)
	{
		int maxThreadPriority;
		// from stanford formula
		maxThreadPriority = convertToFixedPoint(PRI_MAX);
		int recentCPU = thread_current()->previousCPUBurst;

		recentCPU = (divideFixedPointByInteger(recentCPU, 4));

//		recentCPU = convertToIntegerRoundingToNear(recentCPU);

		int threadNicenessValue = thread_current()->nicenessValue;

		threadNicenessValue *= 2;

		int priority = convertToIntegerRoundingToNear( subtractIntegerFromFixedPoint(calculateFixedPointSubtraction(maxThreadPriority, recentCPU), threadNicenessValue));
		if(priority < 0)
			priority = 0;
		if(priority > 63)
			priority = 63;

		thread_set_priority_for_thread(thread_current(), priority);
	}
	/*
	   int maxThreadPriority;
	// from stanford formula
	maxThreadPriority = PRI_MAX;
	int recentCPU = thread_get_recent_cpu();
	recentCPU = convertToFixedPoint(divideFixedPointByInteger(recentCPU, 4));

	recentCPU = convertToIntegerRoundingToNear(recentCPU);

	int threadNicenessValue = thread_get_nice();
	threadNicenessValue *= 2;

	int priority = convertToIntegerRoundingToNear(subtractIntegerFromFixedPoint(subtractFixedPointFromInteger(recentCPU, maxThreadPriority),threadNicenessValue));

	if(priority < 0)
	priority = 0;
	if(priority > 63)
	priority = 63;

	return priority;
	 */

}

/** ADDED */
void calculateCPULoadAverage()
{
	int numberOfReadyThreads;
	numberOfReadyThreads = list_size(&ready_list);

	if(thread_current() == idle_thread)
	{
		// do nothing
		//		numberOfReadyThreads = 0;
	}
	else
	{
		numberOfReadyThreads++;
	}

		loadAverage = calculateFixedPointAddition(calculateFixedPointMultiplication(divideFixedPointByInteger(convertToFixedPoint(59), 60), loadAverage), multiplyFixedPointToInteger(divideFixedPointByInteger(convertToFixedPoint(1),60), numberOfReadyThreads));
	/*int term2 = list_size(&ready_list);
	if (thread_current() != idle_thread)
	{
		term2++;
	}
	int term1 = div_mixed(int_to_fp(59), 60);
	term1 = mult_fp(term1, loadAverage);
	term2 = div_mixed(int_to_fp(term2), 60);
	loadAverage = add_fp(term1, term2);
*/

}

/** ADDED */
int convertToIntegerRoundingToNear(int x)
{
	int fraction = 1 << numberOfFractionBits; // calculatiog 2 ** 14 as per stanford 
	if(x >= 0)
	{
		return (x + fraction / 2) / fraction;
	}
	else
	{
		return (x - fraction / 2) / fraction;
	}
}



/** ADDED */
int convertToFixedPoint(int n)
{
	int fraction = 1 << numberOfFractionBits; // calculatiog 2 ** 14 as per stanford 
	return n * fraction;
}


/** ADDED */
int calculateFixedPointMultiplication(int x, int y)
{
	int fraction = 1 << numberOfFractionBits; // calculatiog 2 ** 14 as per stanford 
	return ((int64_t)(x)) * y / fraction;	
}

/** ADDED */
int calculateFixedPointDivision(int x, int y)
{
	int fraction = 1 << numberOfFractionBits; // calculatiog 2 ** 14 as per stanford 
	return ((int64_t)(x)) * fraction / y;	
}

/** ADDED */
int calculateFixedPointAddition(int x, int y)
{
	return (x+y);
}


/** ADDED */
int calculateFixedPointSubtraction(int x, int y)
{
	return (x-y);	
}

/** ADDED */
int addFixedPointToInteger(int x, int n)
{
	int fraction = 1 << numberOfFractionBits; // calculatiog 2 ** 14 as per stanford 
	return x + n * fraction;	
}

/** ADDED */
int subtractIntegerFromFixedPoint(int x, int n)
{
	int fraction = 1 << numberOfFractionBits; // calculatiog 2 ** 14 as per stanford 
	return x - n * fraction;	
}

/** ADDED */
int subtractFixedPointFromInteger(int x, int n)
{
	int fraction = 1 << numberOfFractionBits; // calculatiog 2 ** 14 as per stanford 
	return  n * fraction - x;	
}

/** ADDED */
int multiplyFixedPointToInteger(int x, int n)
{
	return x * n;	
}

/** ADDED */
int divideFixedPointByInteger(int x, int n)
{
	return x / n;	
}

/** ADDED */
void calculateAllThreadsPriorities()
{
	struct list_elem *iterator;
	struct thread *eachThread;

	iterator = list_begin(&all_list);
	while(iterator != list_end(&all_list))
	{
		eachThread = list_entry(iterator, struct thread, allelem);
		getAndSetThreadPriority(eachThread);
		iterator = list_next(iterator);		
	}
	list_sort(&ready_list, compareThreadPriorities, NULL);
}

/** ADDED */
void getAndSetThreadPriority(struct thread *t)
{	
	if(t != idle_thread)
	{
		int maxThreadPriority;
		// from stanford formula
		maxThreadPriority = convertToFixedPoint(PRI_MAX);
		int recentCPU = t->previousCPUBurst;

		recentCPU = (divideFixedPointByInteger(recentCPU, 4));

//		recentCPU = convertToIntegerRoundingToNear(recentCPU);

		int threadNicenessValue = t->nicenessValue;

		threadNicenessValue *= 2;

		int priority = convertToIntegerRoundingToNear( subtractIntegerFromFixedPoint(calculateFixedPointSubtraction(maxThreadPriority, recentCPU), threadNicenessValue));
		if(priority < 0)
			priority = 0;
		if(priority > 63)
			priority = 63;

		thread_set_priority_for_thread(t, priority);
	}

}
	void
thread_set_priority_for_thread (struct thread *t, int new_priority) 
{
	/* ADDED */
	enum intr_level old_level;
	old_level = intr_disable ();
	t->priority = new_priority;
	intr_set_level (old_level);

}

/** ADDED */
void calculateAllThreadsCPUBursts()
{
	struct list_elem *iterator;
	struct thread *eachThread;

	iterator = list_begin(&all_list);
	while(iterator != list_end(&all_list))
	{
		eachThread = list_entry(iterator, struct thread, allelem);
		//		printf("adding cpu burst of %s thread\n",eachThread->name);
		getAndSetThreadCPUBurst(eachThread);
		getAndSetThreadPriority(eachThread);
		iterator = list_next(iterator);		
	}
	list_sort(&ready_list, compareThreadPriorities, NULL);
}

/** ADDED */
void getAndSetThreadCPUBurst(struct thread *t)
{	

	if(t != idle_thread)
	{
		// from stanford formula
		int cpuBurst = addFixedPointToInteger(calculateFixedPointMultiplication(calculateFixedPointDivision(multiplyFixedPointToInteger(loadAverage, 2), addFixedPointToInteger(multiplyFixedPointToInteger(loadAverage, 2), 1)), t->previousCPUBurst), t->nicenessValue);


		/*	int term1 = mult_mixed(loadAverage, 2);
			term1 = div_fp(term1, add_mixed(term1, 1) );
			term1 = mult_fp(term1, t->previousCPUBurst);
			t->previousCPUBurst = add_mixed(term1, t->nicenessValue);*/

		thread_set_cpu_burst_for_thread(t, cpuBurst);
	}
}
void thread_set_cpu_burst_for_thread (struct thread *t, int cpuBurst) 
{
	/* ADDED */
	enum intr_level old_level;
	old_level = intr_disable ();
	t->previousCPUBurst = cpuBurst;
	intr_set_level (old_level);
}



/** ADDED */
void updateCPUBurst()
{
	if(thread_current() != idle_thread)
	{
		thread_current()->previousCPUBurst = addFixedPointToInteger(thread_current()->previousCPUBurst,1);
	}

}
