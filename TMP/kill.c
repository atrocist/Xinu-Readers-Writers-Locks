#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <sem.h>
#include <mem.h>
#include <io.h>
#include <q.h>
#include <stdio.h>
#include <lock.h>

/*------------------------------------------------------------------------
 * kill  --  kill a process and remove it from the system
 *------------------------------------------------------------------------
 */
SYSCALL kill(int pid)
{
	STATWORD ps;    
	struct	pentry	*pptr;		/* points to proc. table for pid*/
	int	dev;

	disable(ps);
	if (isbadpid(pid) || (pptr= &proctab[pid])->pstate==PRFREE) {
		restore(ps);
		return(SYSERR);
	}
	if (--numproc == 0)
		xdone();

	dev = pptr->pdevs[0];
	if (! isbaddev(dev) )
		close(dev);
	dev = pptr->pdevs[1];
	if (! isbaddev(dev) )
		close(dev);
	dev = pptr->ppagedev;
	if (! isbaddev(dev) )
		close(dev);
	
	send(pptr->pnxtkin, pid);

	freestk(pptr->pbase, pptr->pstklen);
        lproc_killed(pid);
        lclear_maps(pid);
	switch (pptr->pstate) {

	case PRCURR:	pptr->pstate = PRFREE;	/* suicide */
			resched();

	case PRWAIT:	semaph[pptr->psem].semcnt++;

	case PRREADY:	dequeue(pid);
			pptr->pstate = PRFREE;
			break;

	case PRSLEEP:
	case PRTRECV:	unsleep(pid);
						/* fall through	*/
	default:	pptr->pstate = PRFREE;
	}
	restore(ps);
	return(OK);
}

int lproc_killed(int pid)
{
    int plid = EMPTY;
    int lid = EMPTY;
    struct pentry *pptr = NULL;
    STATWORD ps;

    disable(ps);
    if (isbadpid(pid)) {
        restore(ps);
		return SYSERR;
    }
    pptr = &proctab[pid];

    for (lid = 0; lid < NLOCKS; lid++) {
        if (pptr->plid_map[lid] == 1) {
            if (SYSERR == lunlock(lid, pid)) {
                return SYSERR;
            } 
        }
    }
    if (OK == lremove_pid_from_lock(pid)) {
		restore(ps);
		return OK;
    }else{	
		restore(ps);
		return SYSERR;
	}
}

int lremove_pid_from_lock(int pid)
{
    int plid = EMPTY;
    int plid_type = EMPTY;
    int iter_pid = EMPTY;
    int tmp_head = EMPTY;
    int tmp_tail = EMPTY;
    struct pentry *pptr = NULL;
    rwlock_t *lptr = NULL;
    STATWORD ps;
    int new_prio = EMPTY;
    int new_pid = EMPTY;
    int tmp_pid = EMPTY;
    int pprio = EMPTY;
    
    disable(ps);
	
    if (isbadpid(pid)) {    
        restore(ps);
		return SYSERR;    
    }
	
	
    pptr = &proctab[pid];
    pprio = pptr->pprio;
    plid = pptr->plid;
    plid_type = pptr->plid_type;
    if(isbadlid(plid)){
		
		//not waiting on a lock
		if(plid == EMPTY){
			restore(ps);
			return OK;
		}
		else{
			restore(ps);
			return SYSERR;
		}
	}
	lptr=&ltab[plid];

    /* If the pid is not in any of the waitlists, return OK. */
    if (PRWAIT != pptr->pstate) {
        restore(ps);
		return OK;
    }
    /* Check both read & wait lists. If found in one, it will not be in the
     * other list.
     */
    if (READ == plid_type) {
        tmp_head = lptr->lrqhead;
        tmp_tail = lptr->lrqtail;
    } else if (WRITE == plid_type) {
        tmp_head = lptr->lwqhead;
        tmp_tail = lptr->lwqtail;
    } else {
		//should never happen
        restore(ps);
		return SYSERR;
    }

   /* Remove the pid from a lid's waitlist, if present. */
    iter_pid = q[tmp_tail].qprev;
    while (iter_pid != tmp_head) {
        if (pid == iter_pid) {
            /* Got our victim. Remove her. */
            q[q[iter_pid].qprev].qnext = q[iter_pid].qnext;
            q[q[iter_pid].qnext].qprev = q[iter_pid].qprev;
            q[iter_pid].qnext = q[iter_pid].qprev = EMPTY;           
            break;
        }
        iter_pid = q[iter_pid].qprev;
    }
    
    if(READ==plid_type){
			lptr->nwreaders--;
	}else{
		lptr->nwwriters--;
	}
    pptr->plid = EMPTY;
	pptr->plid_type = EMPTY;

    restore(ps);
    return OK;
}

