
#include "tinyos.h"
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
	Tid_t caller_thread = ThreadSelf();

	PCB* caller_proc = CURPROC;

	MTCB* t_mtcb = serach_thread(caller_proc->mtcb_table,tid);/**Check if the given tid is valid and it belongs to the same process*/

	if (t_mtcb==NULL || t_mtcb->tid == caller_thread || t_mtcb->join_state == DETACHED || t_mtcb->join_state == JOINED)
		goto error;

	t_mtcb->join_state = JOINED;
	t_mtcb->avail=0;

	while (t_mtcb->tcb->state != EXITED)
	{
		fprintf(stderr, "Wait for thread to exit: \n");
	}


	while(is_rlist_empty(& parent->exited_list)) {
    Cond_Wait(& kernel_mutex, & parent->child_exit);
  }


	exitval = &t_mtcb->exitval;

	error:
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


static Tid_t wait_for_specific_thread (Tid_t cpid, int* status)
{
  Mutex_Lock(& kernel_mutex);

  /* Legality checks */
  //if((cpid<0) || (cpid>=MAX_PROC)) {
  //  cpid = NOPROC;
  //  goto finish;
  //}

  TCB* parent = CURPROC;
  TCB* child = (TCB*) cpid;
  if( child == NULL || child->parent != parent)
  {
    cpid = NOPROC;
    goto finish;
  }

  /* Ok, child is a legal child of mine. Wait for it to exit. */
  while(child->pstate == ALIVE)
    Cond_Wait(& kernel_mutex, & parent->child_exit);
  
  cleanup_zombie(child, status);
  
finish:
  Mutex_Unlock(& kernel_mutex);
  return cpid;
}