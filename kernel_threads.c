
#include "tinyos.h"
#include "kernel_cc.h"
#include "kernel_sched.h"
#include "kernel_proc.h"

/** 
  @brief Create a new thread in the current process.
  */
Tid_t CreateThread(Task task, int argl, void* args)
{
  fprintf(stderr, "CreateThread 1st\n");

  Tid_t ret = Exec_thread(task, argl, args);

  if (ret==NOTHREAD){
	return NOTHREAD;
  }else{
    return ret;
  }
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t ThreadSelf()
{
	return (Tid_t) CURTHREAD;
}

/**
  @brief Join the given thread.
  */
int ThreadJoin(Tid_t tid, int* exitval)
{

	Mutex_Lock(& kernel_mutex);

	fprintf(stderr, "Thread Join 1\n" );

	Tid_t caller_thread = ThreadSelf();

	PCB* caller_proc = CURPROC;

	MTCB* t_mtcb = serach_thread(caller_proc->mtcb_table,tid);/**Check if the given tid is valid and it belongs to the same process*/

	if (t_mtcb==NULL || t_mtcb->tid == caller_thread || t_mtcb->join_state == DETACHED || t_mtcb->join_state == JOINED)
		goto error;

	fprintf(stderr, "Thread Join 2\n" );
	t_mtcb->join_state = JOINED;
	t_mtcb->avail=0;
	t_mtcb->joined_by=tid;


	
		//while (t_mtcb->tcb->state != EXITED)
    		//Cond_Wait(& kernel_mutex, & t_mtcb->tcb->state);
 	
	fprintf(stderr, "Thread Join 3\n" );

	fprintf(stderr, "Thread Join correct\n" );
	exitval = &t_mtcb->exitval;
	Mutex_Unlock(& kernel_mutex);
	return 0;

	error:
	fprintf(stderr, "Thread Join error\n" );
	Mutex_Unlock(& kernel_mutex);
	return -1;
}

/**
  @brief Detach the given thread.
  */
int ThreadDetach(Tid_t tid)
{
	return -1;
}

/**
  @brief Terminate the current thread.
  */
void ThreadExit(int exitval)
{

}


/**
  @brief Awaken the thread, if it is sleeping.

  This call will set the interrupt flag of the
  thread.

  */
int ThreadInterrupt(Tid_t tid)
{
	return -1;
}


/**
  @brief Return the interrupt flag of the 
  current thread.
  */
int ThreadIsInterrupted()
{
	return 0;
}

/**
  @brief Clear the interrupt flag of the
  current thread.
  */
void ThreadClearInterrupt()
{

}



MTCB* serach_thread(MTCB* t_table, Tid_t tid_s)
{
  int i;

  MTCB* s_table;

  for(i=0;i<MAX_PTHREADS;i++){
  	s_table=&t_table[i];
  	fprintf(stderr, "Looking for(tid): %li  , found(tid): %li\n",tid_s,s_table->tid );
    if(t_table[i].tid == tid_s)
      goto send;
  }
  fprintf(stderr, "No thread found!!\n");
  return NULL;

  send:
  fprintf(stderr, "Thread found on pos: %d\n", i);
  return &t_table[i];
}

