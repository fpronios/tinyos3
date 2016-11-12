
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"

void start_thread()
{
  //fprintf(stderr, "start_main_thread\n");

  int exitval;

  Task call =  CURPROC->main_task;
  int argl = CURPROC->argl;
  void* args = CURPROC->args;

  exitval = call(argl,args);
  Exit(exitval);
}

/** 
  @brief Create a new thread in the current process.
  */
Tid_t CreateThread(Task task, int argl, void* args)
{
  MTCB* mtcb_loc = (MTCB*)malloc(sizeof(MTCB));

  //mtcb_loc->
  
  //PCB *curproc, *newproc;

    mtcb_loc->owner_pcb = CURPROC;
    

    
    mtcb_loc->pid=get_pid(CURPROC);
    mtcb_loc->t_interrupt= NOT_INTERRUPTED;
    mtcb_loc->join_state = JOINABLE;
    
    mtcb_loc->handled_tcb = CURPROC->main_thread;
    mtcb_loc->args = args; 
    mtcb_loc->argl = argl;

    //rlnode_init(& CURPROC->mtcb_list, mtcb_loc);  /* Intrusive list node */

    rlist_push_front(& CURPROC->mtcb_list, mtcb_loc);

    printf("################ %d\n", mtcb_loc->pid);

	
  mtcb_loc->handled_tcb = spawn_thread(NULL, mtcb_loc->owner_pcb, start_thread);
  
return mtcb_loc->handled_tcb;

  if (mtcb_loc->owner_pcb != NULL){
    //wakeup(newproc->main_thread);
    return mtcb_loc->handled_tcb;
  }else{
    return NOTHREAD;
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





