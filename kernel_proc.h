#ifndef __KERNEL_PROC_H
#define __KERNEL_PROC_H

/**
  @file kernel_proc.h
  @brief The process table and process management.

  @defgroup proc Processes
  @ingroup kernel
  @brief The process table and process management.

  This file defines the PCB structure and basic helpers for
  process access.

  @{
*/ 

#include "tinyos.h"
#include "kernel_sched.h"


#define MAX_PTHREADS 10
/**
  @brief PID state

  A PID can be either free (no process is using it), ALIVE (some running process is
  using it), or ZOMBIE (a zombie process is using it).
  */
typedef enum pid_state_e {
  FREE,   /**< The PID is free and available */
  ALIVE,  /**< The PID is given to a process */
  ZOMBIE  /**< The PID is held by a zombie */
} pid_state;

/**
  @brief Process Control Block.

  This structure holds all information pertaining to a process.
 */
typedef struct process_control_block {
  pid_state  pstate;      /**< The pid state for this PCB */

  PCB* parent;            /**< Parent's pcb. */
  int exitval;            /**< The exit value */

  TCB* main_thread;       /**< The main thread */
  Task main_task;         /**< The main thread's function */
  Task thread_task;
  int argl;               /**< The main thread's argument length */
  void* args;             /**< The main thread's argument string */

  rlnode mtcb_list;
  //rlnode mtcb_node;
  MTCB mtcb_table[MAX_PTHREADS];

  rlnode children_list;   /**< List of children */
  rlnode exited_list;     /**< List of exited children */

  rlnode children_node;   /**< Intrusive node for @c children_list */
  rlnode exited_node;     /**< Intrusive node for @c exited_list */
  CondVar child_exit;     /**< Condition variable for @c WaitChild */

  FCB* FIDT[MAX_FILEID];  /**< The fileid table of the process */

  MTCB* temp;
  unsigned int alive_threads;

} PCB;


/**
  @brief Initialize the process table.

  This function is called during kernel initialization, to initialize
  any data structures related to process creation.
*/
void initialize_processes();

/**
  @brief Get the PCB for a PID.

  This function will return a pointer to the PCB of 
  the process with a given PID. If the PID does not
  correspond to a process, the function returns @c NULL.

  @param pid the pid of the process 
  @returns A pointer to the PCB of the process, or NULL.
*/
PCB* get_pcb(Pid_t pid);

/**
  @brief Get the PID of a PCB.

  This function will return the PID of the process 
  whose PCB is pointed at by @c pcb. If the pcb does not
  correspond to a process, the function returns @c NOPROC.

  @param pcb the pcb of the process 
  @returns the PID of the process, or NOPROC.
*/
Pid_t get_pid(PCB* pcb);

/** @} */


void init_mtcb_table(Pid_t proc);

Tid_t Exec_thread (Task call, int argl, void* args);

Tid_t mtcb_insert(MTCB* mtcb, Pid_t proc);

void print_mtcb(Pid_t proc);

MTCB* return_avail_mtcb (Pid_t proc);

MTCB* serach_thread(MTCB* t_table, Tid_t tid_s);

//static inline rlnode* rlist_find_tid(rlnode* List, Tid_t tid);







#endif
