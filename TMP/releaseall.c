#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <stdio.h>
#include <q.h>
#include <lock.h>

int releaseall(num_locks, args)
    int num_locks;
    int args;
{
    unsigned int *pargs = NULL;
    int ret_val = OK;
    int lid = EMPTY;
    STATWORD ps;
	//kprintf("inside release all\n");
    disable(ps);
    if (num_locks == 0) {
    /* Nothing to release. Bad call. */
		kprintf("nothing to release\n");
		restore(ps);
		return SYSERR;
    }
    pargs = (unsigned int *) (&args) + (num_locks - 1);
    for (; num_locks > 0; num_locks--) {
        lid = *(pargs--);
        if(SYSERR == lunlock((int)lid, currpid)){
		//	kprintf("failed to release a lock\n");
			ret_val = SYSERR;
		}
    }
    resched();
    restore(ps);
	return ret_val;
}

int lunlock(int lid, int pid)
{
    int lstate = EMPTY;
	rwlock_t *lptr = NULL;
	struct pentry *pptr = NULL;
	
    if (isbadlid(lid)) {
        return SYSERR;
    }
    
    if (isbadpid(pid)) {
        return SYSERR;
    }
	pptr = &proctab[pid];
	lptr = &ltab[lid];
	lstate = lptr->lstate;
	
	//check if lock is stale
    if (lptr->del_map[pid] == 1) {
		//kprintf("lock is stale\n");
        return SYSERR;
    }

	//check if this proc can release the lock
    if (lptr->rel_map[pid] == 1) {
		//lock is not in a valid state
		if(lstate == LS_UNINIT || lstate == LS_FREE || lstate == LS_DELETED){
		//	kprintf("lock is in state %d\n",lstate);			
			return SYSERR;
		}else if(lstate == LS_READ || lstate == LS_WRITE){
			    lptr->rel_map[pid] = 0;
                pptr->plid_map[lid] = 0;
                if (lstate == LS_READ) {
					//more active readers so no need to wake a process from wait queue
                    lptr->nareaders--;
                    if (lptr->nareaders != 0) {
                        
						return OK;
                    }
                } else {
					lptr->nawriters--;              
                }
				//if no active readers then schedule a process from wait queue
                if (lptr->nextop_type == READ) {
                    if (TRUE == l_get_all_waiting_readers(lid)){
						return OK;
					}
                } else if (WRITE == lptr->nextop_type) {
                    if (TRUE == l_get_next_waiting_writer(lid)){
                        
						return OK;
					}
                } else {
                    lptr->lstate = LS_FREE;
                    
					return OK;					
                }			
		}else{
			//this should not happen
			kprintf("invalid state\n");
			return SYSERR;
		}
    }else{
		kprintf("lid does not belong to this process\n");
        return SYSERR;
    }
}

int l_get_all_waiting_readers(int lid)
{
    int nextpid = EMPTY;
    int nextop_type = EMPTY;
	rwlock_t *lptr = NULL;
	
	if (isbadlid(lid)) {

        return FALSE;
    }
	lptr = &ltab[lid];
	nextop_type = lptr->nextop_type;
    /* Set the lstate based on the next available waiter. */
    if (READ == nextop_type) {
        lptr->lstate = LS_READ;
    } else if (EMPTY == nextop_type) {
        lptr->lstate = LS_FREE;
    } else {
		//this should not happen
        return FALSE;
    }

    /* Fetch all possible readers. */
    while (READ == lptr->nextop_type) {
		nextpid = getlast(lptr->lrqtail);
        if (EMPTY == nextpid) {
            break;
        }
        lptr->nwreaders--;
        lptr->nareaders++;
        ready(nextpid, RESCHNO);
        lptr->rel_map[nextpid] = 1;
        lschedule_next_proc(lid);
    }
    return TRUE;
}

int l_get_next_waiting_writer(int lid)
{
    int nextpid = EMPTY;
    int nextop_type = EMPTY;
	rwlock_t *lptr = NULL;
	
	if (isbadlid(lid)) {

        return FALSE;
    }
	lptr = &ltab[lid];
	nextop_type = lptr->nextop_type;
    /* Set the lstate based on the next available waiter. */
	
    if (WRITE == nextop_type) {
        lptr->lstate = LS_WRITE;
    } else if (EMPTY == nextop_type) {
        lptr->lstate = LS_FREE;
    } else {
        //should never happen
        return FALSE;
    }
	nextpid = getlast(lptr->lwqtail);
    if (EMPTY == nextpid) {
        return FALSE;
    }

    lptr->nwwriters--;
    lptr->nawriters++;

    /* Enqueue the waiting writer in ready queue. */
    ready(nextpid, RESCHNO);
    lptr->rel_map[nextpid] = 1;
    lschedule_next_proc(lid);
    return TRUE;
}
