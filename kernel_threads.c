
#include "tinyos.h"
#include "kernel_cc.h"
#include "kernel_sched.h"
#include "kernel_proc.h"


void start_thread() {
  fprintf(stderr, "start_thread\n" );
    CURTHREAD->owner_mtcb = CURPROC->temp;
    assert(CURTHREAD->owner_mtcb != NULL);
    fprintf(stderr, "start_thread 1\n");
    Task call = CURTHREAD->owner_pcb->thread_task; ///main task was here
    int argl = CURTHREAD->owner_mtcb->argl;
    void *args = CURTHREAD->owner_mtcb->args;
    fprintf(stderr, "start_thread 2\n");
    int ex_val = call(argl, args);
    ThreadExit(ex_val);
}


MTCB *initMTCB() {
  fprintf(stderr, "initMTCB\n" );
    MTCB* MTCB_loc = (MTCB *) malloc(sizeof(MTCB));
    
    MTCB_loc->join_state = JOINABLE;
    MTCB_loc->interrupt_state= NOT_INTERRUPTED;
    MTCB_loc->exit_state=NOT_EXITED;
    rlnode_new(&MTCB_loc->mtcb_node);
    MTCB_loc->thread_exit = COND_INIT;

    rlnode_new(&MTCB_loc->mtcb_node);
    MTCB_loc->alive_threads = 0;
    fprintf(stderr, "initMTCB Returned: %p\n",MTCB_loc );
    return MTCB_loc;
}


/** 
  @brief Create a new thread in the current process.
  */
Tid_t CreateThread(Task task, int argl, void* args)
{
  fprintf(stderr, "CURRENT PID %d\n", get_pid(CURPROC));
  fprintf(stderr, "CreateThread\n");
    Mutex_Lock(&kernel_mutex);

    rlnode mtcbNode;

    MTCB* MTCB_loc = initMTCB();
    rlnode_new(&mtcbNode);

    fprintf(stderr, "Create thread after new rlnode\n" );
    MTCB_loc->task = task;
    CURPROC->temp = MTCB_loc;

    CURTHREAD->state_spinlock = MUTEX_INIT;
    CURTHREAD->phase = CTX_CLEAN;

    MTCB_loc->argl = argl;
    if (args != NULL) {
        MTCB_loc->args = args;
    } else {
        MTCB_loc->args = NULL;
    }

    mtcbNode.mtcb = MTCB_loc;
    fprintf(stderr, "Create thread before new rlnode\n" );
    

    rlist_push_front(&CURPROC->mtcb_list, &mtcbNode);
    fprintf(stderr, "Create thread after rlist_push_front\n" );

    if (task != NULL) {
        CURPROC->alive_threads++;
        fprintf(stderr, "active\n" );
        spawn_thread(CURPROC, start_thread, MTCB_loc);
        fprintf(stderr, "spawn_thread\n" );
        fprintf(stderr, "After spawn Returns: %lu on MTCB: %p\n", MTCB_loc->tid,MTCB_loc);
        wakeup(MTCB_loc->tcb);
        fprintf(stderr, "wakeup?\n" );
    }
    fprintf(stderr, "before return\n" );
    Mutex_Unlock(&kernel_mutex);
    fprintf(stderr, "CreateThread Returns: %lu \n", MTCB_loc->tid);
    return MTCB_loc->tid;

}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t ThreadSelf()
{
	return (Tid_t) CURTHREAD;
}


static inline rlnode* rlist_find_tid(rlnode* List, Tid_t tid)
{
  fprintf(stderr, "rlist_find_tid\n" );
  rlnode* i= List->next;
  while(i!=List) {
    if(i->mtcb->tid == tid){
      return i;
    }
    else
    {
      i = i->next;
      if(i==NULL)
        goto null_ret;
    }

  }
  null_ret:
  return NULL;
}



void onDestroy(MTCB *mtcb) {//Free mem used by PTCB
    rlist_remove(&mtcb->mtcb_node);
    free(mtcb);
}




/**
  @brief Join the given thread.
  */
int ThreadJoin(Tid_t tid, int* exitval)
{
  fprintf(stderr, "ThreadJoin\n");
   Mutex_Lock(&kernel_mutex);

    int ret = 0;
    
    TCB *t_join = (TCB *) tid;
    
    if (t_join->owner_mtcb) {
        Mutex_Lock(&kernel_mutex);
        return -1;
    }
    MTCB *new_mtcb = t_join->owner_mtcb;

    if (tid == (Tid_t) CURTHREAD || new_mtcb == NULL || new_mtcb->join_state==DETACHED) 
    {//Can NOT join
        Mutex_Unlock(&kernel_mutex);
        return -1;
    }

    Thread_CondVar wait_thread;
    wait_thread.cv = COND_INIT;
    wait_thread.tid = tid;

    rlnode mtcbNode;

    rlnode_init(&mtcbNode, &wait_thread);
    rlist_push_back(&new_mtcb->mtcb_node, &mtcbNode);

    new_mtcb->alive_threads++;

    while (new_mtcb->join_state!=DETACHED && new_mtcb->exit_state!=HAS_EXITED) //Standard procedure.
    {
        Cond_Wait(&kernel_mutex, &wait_thread.cv);
      
    }


    if (new_mtcb->join_state==DETACHED) {//There is this possibility as well.
        ret = -1;
        Mutex_Unlock(&kernel_mutex);
        return ret;
    }
    if (exitval) {//When we dont have an exit value yet so add one. Avoid errors that way!
        *exitval = new_mtcb->exitval;//Return the exitval of the given thread
        ret = 0;
    }

    if (new_mtcb->alive_threads == 0) {//The last one
        onDestroy(new_mtcb);
        ret = 0;
    }
    new_mtcb->alive_threads--;
    Mutex_Unlock(&kernel_mutex);
    return ret;
}


/**
  @brief Detach the given thread.
  */
int ThreadDetach(Tid_t tid)
{
	Mutex_Lock(&kernel_mutex);
    int ret = -1;

    rlnode *node = rlist_find_tid(&CURPROC->mtcb_list, tid);

    if (node != NULL && node->mtcb->exit_state!=HAS_EXITED) {
        node->mtcb->join_state = DETACHED;
        ret = 0;
    }

    Mutex_Unlock(&kernel_mutex);
    return ret;

}


void signal_cv(rlnode *List) {
    rlnode *i = List->next;
    while (i != List) {
        if (i == NULL) {        //You can never be too sure..
            break;
        }
        Cond_Signal(&i->wait_thread->cv);
        i = i->next;
        if (i == NULL) {
            break;
        }
    }
}


/**
  @brief Terminate the current thread.
  */
void ThreadExit(int exitval)
{

    Mutex_Lock(&kernel_mutex);

    TCB *tcb = (TCB *) ThreadSelf();
    MTCB* mtcb = tcb->owner_mtcb;
    mtcb->exitval = exitval;

    signal_cv(&mtcb->mtcb_node);

    CURPROC->alive_threads--;

    sleep_releasing(EXITED, &kernel_mutex);



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
  Mutex_Lock(&kernel_mutex);
  if(CURTHREAD->owner_mtcb->interrupt_state == NOT_INTERRUPTED)
    return 0;
  else
    return 1;

  Mutex_Unlock(&kernel_mutex);
}

/**
  @brief Clear the interrupt flag of the
  current thread.
  */
void ThreadClearInterrupt()
{
    Mutex_Lock(&kernel_mutex);
    CURTHREAD->owner_mtcb->interrupt_state == NOT_INTERRUPTED;
    Mutex_Unlock(&kernel_mutex);

}

/**


A method to find the mtcb containing yje given tid

*/


