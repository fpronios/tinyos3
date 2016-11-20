
#include <assert.h>
#include "kernel_cc.h"
#include "kernel_proc.h"
#include "kernel_streams.h"

#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_RED     "\x1b[31m"

/* 
 The process table and related system calls:
 - Exec
 - Exit
 - WaitPid
 - GetPid
 - GetPPid

 */

/* The process table */
PCB PT[MAX_PROC];
unsigned int process_count;

PCB* get_pcb(Pid_t pid)
{
  return PT[pid].pstate==FREE ? NULL : &PT[pid];
}

Pid_t get_pid(PCB* pcb)
{
  return pcb==NULL ? NOPROC : pcb-PT;
}

/* Initialize a PCB */
static inline void initialize_PCB(PCB* pcb)
{
  pcb->pstate = FREE;
  pcb->argl = 0;
  pcb->args = NULL;

  for(int i=0;i<MAX_FILEID;i++)
    pcb->FIDT[i] = NULL;

  rlnode_init(& pcb->children_list, NULL);
  rlnode_init(& pcb->exited_list, NULL);
  rlnode_init(& pcb->children_node, pcb);
  rlnode_init(& pcb->exited_node, pcb);
  pcb->child_exit = COND_INIT;
}


static PCB* pcb_freelist;

void initialize_processes()
{
  /* initialize the PCBs */
  for(Pid_t p=0; p<MAX_PROC; p++) {
    initialize_PCB(&PT[p]);
  }

  /* use the parent field to build a free list */
  PCB* pcbiter;
  pcb_freelist = NULL;
  for(pcbiter = PT+MAX_PROC; pcbiter!=PT; ) {
    --pcbiter;
    pcbiter->parent = pcb_freelist;
    pcb_freelist = pcbiter;
  }

  process_count = 0;

  /* Execute a null "idle" process */
  if(Exec(NULL,0,NULL)!=0)
    FATAL("The scheduler process does not have pid==0");
}


/*
  Must be called with kernel_mutex held
*/
PCB* acquire_PCB()
{
  PCB* pcb = NULL;

  if(pcb_freelist != NULL) {
    pcb = pcb_freelist;
    pcb->pstate = ALIVE;
    pcb_freelist = pcb_freelist->parent;
    process_count++;
  }

  return pcb;
}

/*
  Must be called with kernel_mutex held
*/
void release_PCB(PCB* pcb)
{
  pcb->pstate = FREE;
  pcb->parent = pcb_freelist;
  pcb_freelist = pcb;
  process_count--;
}


/*
 *
 * Process creation
 *
 */

/*
	This function is provided as an argument to spawn,
	to execute the main thread of a process.
*/
void start_main_thread()
{
  int exitval;

  Task call =  CURPROC->main_task;
  int argl = CURPROC->argl;
  void* args = CURPROC->args;

  exitval = call(argl,args);
  Exit(exitval);
}


/*
	System call to create a new process.
 */
Pid_t Exec(Task call, int argl, void* args)
{
  PCB *curproc, *newproc;
  
  Mutex_Lock(&kernel_mutex);

  /* The new process PCB */
  newproc = acquire_PCB();

  if(newproc == NULL) goto finish;  /* We have run out of PIDs! */

  if(get_pid(newproc)<=1) {
    /* Processes with pid<=1 (the scheduler and the init process) 
       are parentless and are treated specially. */
    newproc->parent = NULL;
  }
  else
  {
    /* Inherit parent */
    curproc = CURPROC;

    /* Add new process to the parent's child list */
    newproc->parent = curproc;
    rlist_push_front(& curproc->children_list, & newproc->children_node);

    /* Inherit file streams from parent */
    for(int i=0; i<MAX_FILEID; i++) {
       newproc->FIDT[i] = curproc->FIDT[i];
       if(newproc->FIDT[i])
          FCB_incref(newproc->FIDT[i]);
    }
  }
  

  /* Set the main thread's function */
  newproc->main_task = call;
  //newproc->mtcb_list =
  rlnode_new(&newproc->mtcb_list);

  /* Copy the arguments to new storage, owned by the new process */
  newproc->argl = argl;
  if(args!=NULL) {
    newproc->args = malloc(argl);
    memcpy(newproc->args, args, argl);
  }
  else
    newproc->args=NULL;

  /* 
    Create and wake up the thread for the main function. This must be the last thing
    we do, because once we wakeup the new thread it may run! so we need to have finished
    the initialization of the PCB.
   */
  if(call != NULL) 
  {

    //rlist_init(mtcb_list,NULL);
    newproc->main_thread = spawn_thread(newproc, start_main_thread,NULL);
    wakeup(newproc->main_thread);
  }

  init_mtcb_table(get_pid(newproc));
  newproc->alive_threads= 0;

finish:
  Mutex_Unlock(&kernel_mutex);
  return get_pid(newproc);
}


/* System call */
Pid_t GetPid()
{
  return get_pid(CURPROC);
}


Pid_t GetPPid()
{
  return get_pid(CURPROC->parent);
}


static void cleanup_zombie(PCB* pcb, int* status)
{
  if(status != NULL)
    *status = pcb->exitval;

  rlist_remove(& pcb->children_node);
  rlist_remove(& pcb->exited_node);

  release_PCB(pcb);
}


static Pid_t wait_for_specific_child(Pid_t cpid, int* status)
{
  Mutex_Lock(& kernel_mutex);

  /* Legality checks */
  if((cpid<0) || (cpid>=MAX_PROC)) {
    cpid = NOPROC;
    goto finish;
  }

  PCB* parent = CURPROC;
  PCB* child = get_pcb(cpid);
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


static Pid_t wait_for_any_child(int* status)
{
  Pid_t cpid;
  Mutex_Lock(&kernel_mutex);

  PCB* parent = CURPROC;

  /* Make sure I have children! */
  if(is_rlist_empty(& parent->children_list)) {
    cpid = NOPROC;
    goto finish;
  }

  while(is_rlist_empty(& parent->exited_list)) {
    Cond_Wait(& kernel_mutex, & parent->child_exit);
  }

  PCB* child = parent->exited_list.next->pcb;
  assert(child->pstate == ZOMBIE);
  cpid = get_pid(child);
  cleanup_zombie(child, status);

finish:
  Mutex_Unlock(& kernel_mutex);
  return cpid;
}


Pid_t WaitChild(Pid_t cpid, int* status)
{
  /* Wait for specific child. */
  if(cpid != NOPROC) {
    return wait_for_specific_child(cpid, status);
  }
  /* Wait for any child */
  else {
    return wait_for_any_child(status);
  }

}


void Exit(int exitval)
{
  /* Right here, we must check that we are not the boot task. If we are, 
     we must wait until all processes exit. */
  if(GetPid()==1) {
    while(WaitChild(NOPROC,NULL)!=NOPROC);
  }

  /* Now, we exit */
  Mutex_Lock(& kernel_mutex);

  PCB *curproc = CURPROC;  /* cache for efficiency */

  /* Do all the other cleanup we want here, close files etc. */
  if(curproc->args) {
    free(curproc->args);
    curproc->args = NULL;
  }

  /* Clean up FIDT */
  for(int i=0;i<MAX_FILEID;i++) {
    if(curproc->FIDT[i] != NULL) {
      FCB_decref(curproc->FIDT[i]);
      curproc->FIDT[i] = NULL;
    }
  }

  /* Reparent any children of the exiting process to the 
     initial task */
  PCB* initpcb = get_pcb(1);
  while(!is_rlist_empty(& curproc->children_list)) {
    rlnode* child = rlist_pop_front(& curproc->children_list);
    child->pcb->parent = initpcb;
    rlist_push_front(& initpcb->children_list, child);
  }

  /* Add exited children to the initial task's exited list 
     and signal the initial task */
  if(!is_rlist_empty(& curproc->exited_list)) {
    rlist_append(& initpcb->exited_list, &curproc->exited_list);
    Cond_Broadcast(& initpcb->child_exit);
  }

  /* Put me into my parent's exited list */
  if(curproc->parent != NULL) {   /* Maybe this is init */
    rlist_push_front(& curproc->parent->exited_list, &curproc->exited_node);
    Cond_Broadcast(& curproc->parent->child_exit);
  }

  /* Disconnect my main_thread */
  curproc->main_thread = NULL;

  /* Now, mark the process as exited. */
  curproc->pstate = ZOMBIE;
  curproc->exitval = exitval;

  /* Bye-bye cruel world */
  sleep_releasing(EXITED, & kernel_mutex);
}



Fid_t OpenInfo()
{
	return NOFILE;
}


/*
  System call to create a new thread.
 */
Tid_t Exec_thread (Task call, int argl, void* args)
{
  fprintf(stderr, "Exec_thread 1\n");
  PCB *newproc;

  MTCB* mtcb_loc = (MTCB*) malloc (sizeof(MTCB*));



  Mutex_Lock(&kernel_mutex);

  newproc = CURPROC;
  newproc->main_task = call; 
  
//fprintf(stderr, "Exec_thread 2\n");

  if(call != NULL) 
  {
    newproc->main_thread = spawn_thread(newproc, start_main_thread,NULL);
    wakeup(newproc->main_thread);
  }
  else 
  {
    return NOTHREAD;
  }

  Mutex_Unlock(&kernel_mutex);


  mtcb_loc->tcb = newproc->main_thread;
  mtcb_loc->task = newproc->main_task;
  mtcb_loc->argl = newproc->argl;
  mtcb_loc->args = newproc->args;
  mtcb_loc->exitval = newproc->exitval;
  mtcb_loc->join_state = JOINABLE;
  mtcb_loc->interrupt_state = NOT_INTERRUPTED;
  mtcb_loc->thread_exit = COND_INIT;

  mtcb_loc->avail=0;

  //fprintf(stderr, "MTCB_LOC: %p  newproc: %d\n", mtcb_loc,get_pid(newproc));
  Tid_t ret = mtcb_insert(mtcb_loc, get_pid(newproc));

  fprintf(stderr, "Return value %li\n", ret);
  if(ret==NOTHREAD){
    fprintf(stderr, "CANNOT CREATE MORE THREADS!! \n");
  }
print_mtcb(get_pid(newproc));


  return ret;

  
}


/**
  A function to initialize the mtcb table
*/

void init_mtcb_table(Pid_t proc){
  fprintf(stderr, "init table Pid_t %d\n", proc);
  PCB* process = get_pcb(proc);
  int i=0;
  for(i=0;i<MAX_PTHREADS;i++){
    process->mtcb_table[i].avail=1;
  }
}

/**
  Called to insert in an available mtcb the new thread info
  Returns the tid if successfull 
  NULL if something went wrong
*/

Tid_t mtcb_insert(MTCB* mtcb, Pid_t proc){

  
  MTCB* mtcb_addr = return_avail_mtcb(proc);
  if(mtcb_addr==NULL)
    goto end;

  PCB* temp=get_pcb(proc);
  temp->main_thread->owner_mtcb = mtcb_addr;

  mtcb_addr->tid =(Tid_t) mtcb->tcb;
  mtcb_addr->tcb = mtcb->tcb;
  mtcb_addr->task = mtcb->task;
  mtcb_addr->argl = mtcb->argl;
  mtcb_addr->args = mtcb->args;
  mtcb_addr->exitval = mtcb->exitval;
  mtcb_addr->join_state = mtcb->join_state;
  mtcb_addr->interrupt_state = mtcb->interrupt_state;
  mtcb_addr->avail=mtcb->avail;

  return (Tid_t)mtcb->tcb;

  end:

  return NOTHREAD;
}

/**
  A function to print the Processes' active threads
*/

void print_mtcb(Pid_t proc){
  PCB* process = get_pcb(proc);
  for(int i=0;i<MAX_PTHREADS;i++)
  fprintf(stderr, "avail: %2d, join state: %2d   \n", process->mtcb_table[i].avail,process->mtcb_table[i].join_state);
}


/**
    Returns the first available mtcb slot
*/

MTCB* return_avail_mtcb (Pid_t proc){
fprintf(stderr, "return avail Pid: %d \n",proc);
PCB* process = get_pcb(proc);

int i=0;
  for(i=0;i<MAX_PTHREADS;i++){
    if(process->mtcb_table[i].avail==1){
      goto found;
    }
  }
  //puts("ERRRROORR");
  return NULL;
  found:
 // fprintf(stderr, "return avail Pid: %d Return value %p ++++ +1: %p\n",proc, &process->mtcb_table[i],&process->mtcb_table[i+1]);
  return &process->mtcb_table[i];
}


