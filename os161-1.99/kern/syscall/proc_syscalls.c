#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <synch.h>
#include <machine/trapframe.h>
#include <opt-A2.h>
#include <limits.h>
#include <kern/fcntl.h>
#include <vfs.h>
#include <test.h>

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */
void forkHelper(void* newtrapframe, unsigned long x); // helper declaration

void sys__exit(int exitcode) {
  struct addrspace *as;
  struct proc *p = curproc;

  #if OPT_A2
  lock_acquire(arrayLock);
  struct processInfo* pInfo= findProcess(curproc->pid);
  pInfo->active = false;
  pInfo->exitStatus = _MKWAIT_EXIT(exitcode);

  for (unsigned int i=0; i<array_num(processes); i++) {
    pInfo = array_get(processes,i);

    if (pInfo->parent == curproc->pid && pInfo->active == false) {
      removeProcess(pInfo->process);
    }
    if (pInfo->parent == curproc->pid) {
      pInfo->parent = -1;
    }
  }
  pInfo= findProcess(curproc->pid);
  if (pInfo->parent == -1) { // no parent
    removeProcess(curproc->pid);
  }
  else {
    cv_broadcast(waitQ,arrayLock);
  }

  lock_release(arrayLock);

  #else
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;
  #endif


  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);
  //kprintf("detaching thread from pid %d\n", curproc->pid);
  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = curproc->pid;
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */
  lock_acquire(arrayLock);
  struct processInfo* p = findProcess(pid);

  if (options != 0) { // The <em>options</em> argument should be 0
    lock_release(arrayLock);
    return EINVAL;
  }
  if (p == NULL) {
    lock_release(arrayLock);
    return ESRCH; // argument named a nonexistent process
  }
  if (p->parent != curproc->pid) {
    lock_release(arrayLock);
    return ECHILD; // named a process	current proc was not interested	in
  }
  if (status == NULL) {
    lock_release(arrayLock);
    return EFAULT;
  }
  // how to check if status ptr is valid? EFAULT


  while (p->active == true) {
    cv_wait(waitQ, arrayLock);
  }
  // if nobody is watching a process that has exited, remove process
  // set exit status after process exits
  exitstatus = p->exitStatus;
  removeProcess(pid);
  result = copyout((void *)&exitstatus,status,sizeof(int));

  lock_release(arrayLock);
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

void forkHelper(void* newtrapframe, unsigned long x) {
  (void)x; // ignore useless value

  enter_forked_process((struct trapframe *) newtrapframe);
}

int sys_fork(struct trapframe* tf, pid_t* retval) {
  struct proc* child = proc_create_runprogram(curproc->p_name);
  if (child == NULL) { 
    return ENPROC;
  }
  if (child->pid == -1) {
    proc_destroy(child);
    return ENPROC;
  }

  // set parent
  struct processInfo* childInfo = findProcess(child->pid);
  childInfo->parent = curproc->pid;

  // trap frame 
  struct trapframe* newtf = kmalloc(sizeof(struct trapframe));
  *newtf = *tf;

  as_copy(curproc->p_addrspace, &child->p_addrspace);
  if (child->p_addrspace==NULL){
    kfree(newtf);
    proc_destroy(child);
    return ENOMEM;
  }

  //memcpy(newtf,tf, sizeof(struct trapframe));
  int result = thread_fork(curthread->t_name, child, forkHelper, newtf, 0);

  if(result != 0) {
    kfree(newtf);
    as_destroy(child->p_addrspace);
    proc_destroy(child);
    return result;
  }

  // set return value to the child's process id
  *retval = child->pid;

  return 0;
}

int sys_execv(char* progname, char** args) {
 // CHECK FOR ERRORS
  if (progname == NULL || args == NULL) {
    return EFAULT;
  }
  if (strlen(progname) > PATH_MAX){
    return E2BIG;
  }

  // see runprogram.c
  int result;
  struct addrspace *as;
  struct addrspace *oldas;
  struct vnode *v;
  vaddr_t entrypoint, stackptr;


  int argc = 0;
  if (args != NULL) {
    while (args[argc] != NULL) {
      argc++;
    }
  }

  char** argsMem = kmalloc((argc + 1) * sizeof(char));
  // set null terminator
  argsMem[argc] = NULL;

  for (int i = 0; i < argc; i++) {
    argsMem[i] = kmalloc((strlen(args[i]) + 1) * sizeof(char));
    copyinstr((userptr_t)args[i], argsMem[i], strlen(args[i]) + 1, NULL);
  }

  char* temp = kstrdup(progname);
  if (temp == NULL) { return ENOMEM;}

  result = vfs_open(temp, O_RDONLY, 0, &v);
  if (result) { 
    kfree(temp);
    return result;
  }
  kfree(temp);

  // make new address space
  as = as_create();
  if (as == NULL) { return ENOMEM;}

  oldas=curproc_setas(as);
  as_activate();

  result = load_elf(v, &entrypoint);
  if (result) {
    vfs_close(v);
    return result;
  }
  vfs_close(v);

  result = as_define_stack(as, &stackptr);
  if (result) { return result;}
  // must increment stack pointer to be 8-byte aligned, see hint page for a2b
  stackptr -= stackptr % 8;

  vaddr_t len;
  vaddr_t argStack[argc + 1];
  argStack[argc]=0;

 // actual argument strings
  for (int i = argc-1; i >= 0; i--) {
    len = strlen(argsMem[i]) + 1;
    stackptr -= len;
    result = copyoutstr(argsMem[i], (userptr_t)stackptr, ARG_MAX,  &len);
    if (result) { return result;}

    argStack[i] = stackptr;
  }

  // pointers to copied arguments, note they are 4-byte items
  // so stackptr must be incremented to be properly aligned
  stackptr -= stackptr % 4;
  for (int i = argc; i >= 0; i--) {
    stackptr -= sizeof(vaddr_t);

    result = copyout(&argStack[i],(userptr_t)stackptr, sizeof(vaddr_t));
    if (result) { return result;}
  }

  userptr_t userAddress = (userptr_t)stackptr;
  enter_new_process(argc, userAddress, stackptr, entrypoint);
  
  panic("function enter_new_process in execv failed @proc_syscalls.c");
  return EINVAL;
}

