#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <stdio.h>
#include <lock.h>

int ldelete(int lid)
{    
	rwlock_t *lptr = NULL;
	struct pentry *pptr = NULL;
    int resched_flag = 0;
    int lstate = EMPTY;
    int pid = 0;
    unsigned int i = 0;
    STATWORD ps;

    disable(ps);
    if (isbadlid(lid)) {
		restore(ps);
		return SYSERR;
    }
    
	lptr = &ltab[lid];
    pptr = &proctab[currpid];
	lstate = lptr->lstate;
	
	//lock is already deleted or it was never created
    if(lstate == LS_UNINIT || lstate == LS_DELETED){
		restore(ps);
		//kprintf("lock state uninit or deleted %d\n",lstate);
		return SYSERR;
	}
	else if(lstate == LS_FREE){
		//lock is free just set it to unitialized state
		lptr->lstate = LS_UNINIT;
        if(SYSERR == lreset_lock_entry_on_delete(lid)){
			restore(ps);
			//kprintf("failed to reset lock entry\n");
			return SYSERR;
		}
		//set deleted map for all procs which accessed this lock till now
		for (i = 0; i < NPROC; i++) {
			lptr->del_map[i] = lptr->proc_map[i];
		}
		restore(ps);
		return OK;
	}else if(lstate == LS_READ || lstate == LS_WRITE){
		lptr->lstate = LS_DELETED;
	}else{
		//this should never happen
		kprintf("ERROR invalid lock state calling process %d, %s\n",currpid,__func__);
		restore(ps);
		return SYSERR;
	}
	//wake up all waiting procs for this lock but set the waitret to DELETED
    if (lptr->nwreaders != 0) {
        resched_flag = 1;
		pid = getfirst(lptr->lrqhead);
        while (!isbadpid(pid)) {
            proctab[pid].pwaitret = DELETED;
            ready(pid, RESCHNO);
			pid = getfirst(lptr->lrqhead);
        }
    }
    if (lptr->nwwriters != 0) {
        resched_flag = 1;
		pid = getfirst(lptr->lwqhead);
        while (!isbadpid(pid)) {
            proctab[pid].pwaitret = DELETED;
            ready(pid, RESCHNO);
			pid = getfirst(lptr->lwqhead);
        }
    }	
    if(SYSERR == lreset_lock_entry_on_delete(lid)){
		restore(ps);
		return SYSERR;
	}
	for (i = 0; i < NPROC; ++i) {
        lptr->del_map[i] = lptr->proc_map[i];
    }
	if (resched_flag) {
        resched();
    }	
    restore(ps);
    return OK;
}

int lreset_lock_entry_on_delete(int lid)
{
    rwlock_t *lptr = NULL;
	//do not change the wait queue or delete maps
    if (isbadlid(lid)) {   
        return SYSERR;
    }
    lptr = &ltab[lid];
    lptr->nareaders = 0;
	lptr->nawriters = 0;
	lptr->nwreaders =0;
	lptr->nwwriters = 0;
    lptr->nextq = EMPTY;
	lptr->nextop_type = EMPTY;
	lptr->nextpid = EMPTY;
    bzero(lptr->rel_map, (NPROC * sizeof(unsigned char)));
	return OK;
}