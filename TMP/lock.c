#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <stdio.h>
#include <lock.h>

int lock(int lid, int ltype, int lprio)
{
    rwlock_t *lptr = NULL;
    struct pentry *pptr = NULL;
	int lstate = EMPTY;  
	int wprio = EMPTY;
    STATWORD ps;
    disable(ps);
	
    if (isbadlid(lid)) {
		restore(ps);
		return SYSERR;
    }

    if (!isvalidltype(ltype)) {
        restore(ps);
		return SYSERR;
    }

    lptr = &ltab[lid];
    pptr= &proctab[currpid];
	lstate = lptr->lstate;
    
	//lock was deleted or never created
	if(lstate == LS_UNINIT || lstate == LS_DELETED){
		restore(ps);
		return SYSERR;
	}else if(lstate == LS_FREE){
		//check if the lock is stale
		if (lptr->del_map[currpid] == 1) {
                restore(ps);
				return SYSERR;
        }
		//Lock is free.change the lstate and return OK.
        if (ltype == READ) {
            lptr->lstate = LS_READ;
            lptr->nareaders++;      
        } else if (ltype == WRITE) {
            lptr->lstate = LS_WRITE;
			lptr->nawriters++;
        } else {
        //invalid argument to lock routine
        }
		//increment the number of locks held by this proc
		//add the pid to release pid map
		lptr->rel_map[currpid] = 1;
		//add the pid to users pid map
		lptr->proc_map[currpid] = 1;
		//add the lock to the process's lock map
		pptr->plid_map[lid] = 1;
		restore(ps);
		return OK;
	}else if(lstate == LS_READ){
		//check if the lock is stale
		if (lptr->del_map[currpid] == 1) {
            restore(ps);
			return SYSERR;
        }
		if (ltype == READ) {
			wprio = q[q[lptr->lwqtail].qprev].qkey;
			//check if there are any writers
            if (lptr->nwwriters && (wprio > lprio)) {
				//writers prio > requesting prio so put requesting proc in wait queue
                pptr->plid = lid;
                pptr->plid_type = ltype;
                pptr->plocktime = ctr1000;
				lptr->nwreaders++;
                insert(currpid, lptr->lrqhead, lprio);
                pptr->pwaitret = OK;
                lschedule_next_proc(lid);                 
				pptr->pstate = PRWAIT;			
				resched();
				//proc got rescheduled
				lptr->proc_map[currpid] = 1;
				//the proc got the lock coz someone released it
				if (pptr->pwaitret == OK) {
					//pptr->nlocks++;
					pptr->plid = EMPTY;
					pptr->plid_type = EMPTY;
					lptr->rel_map[currpid] = 1;
					pptr->plid_map[lid] = 1;
				}
				//the proc was woken up coz the lock was deleted
				restore(ps);
				return pptr->pwaitret;
            } else {
				//no writers so the proc gets the lock
				lptr->nareaders++;
                //pptr->nlocks++;
				lptr->rel_map[currpid] = 1;
				lptr->proc_map[currpid] = 1;
				pptr->plid_map[lid] = 1;
				restore(ps);
				return OK;
            }
        } else if (ltype == WRITE) {
			//writer locks are exclusive so put the proc in wait queue
            pptr->plid = lid;
            pptr->plid_type = ltype;               
            pptr->plocktime = ctr1000;
			lptr->nwwriters++;
            insert(currpid, lptr->lwqhead, lprio);
            pptr->pwaitret = OK;
            lschedule_next_proc(lid);
			pptr->pstate = PRWAIT;			
			resched();
			lptr->proc_map[currpid] = 1;
			//the proc got the lock coz someone released it
			if (pptr->pwaitret == OK) {
			//	pptr->nlocks++;
				pptr->plid = EMPTY;
				pptr->plid_type = EMPTY;
				lptr->rel_map[currpid] = 1;
				pptr->plid_map[lid] = 1;
			}
			//the proc was woken up coz the lock was deleted
			restore(ps);
			return pptr->pwaitret;
        } else {
			//should never happen invalid lock type
			restore(ps);
			return SYSERR;
        }
	}else if(lstate == LS_WRITE){
		/* Chek if this is a request on a stale lid. */
        if (lptr->del_map[currpid] == 1) {
            restore(ps);
			return SYSERR;               
        }            
		//lock is being held by a writer put the requesting proc in wait queue
        pptr->plid = lid;
        pptr->plid_type = ltype;
        pptr->plocktime = ctr1000;
        if (ltype== READ) {
			lptr->nwreaders++;
			insert(currpid, lptr->lrqhead, lprio);
        } else if (ltype == WRITE) {              
            lptr->nwwriters++;
            insert(currpid, lptr->lwqhead, lprio);
        } else {
            /* Should never happen. */
            restore(ps);
			return SYSERR;
        }
        pptr->pwaitret = OK;
        lschedule_next_proc(lid);
		pptr->pstate = PRWAIT;			
		resched();
		lptr->proc_map[currpid] = 1;
		if (pptr->pwaitret == OK) {
		//	pptr->nlocks++;
			pptr->plid = EMPTY;
			pptr->plid_type = EMPTY;
			lptr->rel_map[currpid] = 1;
			pptr->plid_map[lid] = 1;
		}
		restore(ps);
		return pptr->pwaitret;
	}else{
		//invalid lock state
		restore(ps);
		return SYSERR;
	}
	return SYSERR;
}


void lschedule_next_proc(int lid)
{
    int nwreaders = 0;
    int nwrite = 0;
    int rprio = 0;
    int wprio = 0;
	int rpid = -1;
	int wpid = -1;
    unsigned int rtime = 0;
    unsigned int wtime = 0;
    rwlock_t *lptr = NULL;

    if (isbadlid(lid)) {
        return;
    }

    lptr = &ltab[lid];
    nwreaders = lptr->nwreaders;
    nwrite = lptr->nwwriters;
    rprio = q[q[lptr->lrqtail].qprev].qkey;
	rpid = q[lptr->lrqtail].qprev;
    rtime = proctab[rpid].plocktime;
	wpid = q[lptr->lwqtail].qprev;
    wtime = proctab[wpid].plocktime;
	wprio = q[q[lptr->lwqtail].qprev].qkey;
	
    if ((0 == nwreaders) && (0 != nwrite)) {
		//no waiting readers hence select a proc from write queue
        lptr->nextq = wpid;
		lptr->nextop_type = WRITE;
		lptr->nextpid = q[lptr->lwqtail].qprev;
		return;
    } else if ((0 != nwreaders) && (0 == nwrite)) {
        //no waiting writers so select a proc from read queue
        lptr->nextq = rpid;
		lptr->nextop_type = READ;
		lptr->nextpid = q[lptr->lrqtail].qprev;
		return;
    } else if ((0 != nwreaders) && (0 != nwrite)) {	
		if (rprio > wprio) {
			//reader prio is greater than writer prio
			lptr->nextq = rpid;
			lptr->nextop_type = READ;
			lptr->nextpid = q[lptr->lrqtail].qprev;
			return;			
		} else if (rprio < wprio) {
            //writer prio is greater than reader prio
			lptr->nextq = wpid;
			lptr->nextop_type = WRITE;
			lptr->nextpid = q[lptr->lwqtail].qprev;
			return;
        } else if (rprio == wprio) {
            if (wtime > rtime) {
                if ((wtime - rtime) < 1000) {
					//reader came before but writer came within 1 sec
                    lptr->nextq = wpid;
					lptr->nextop_type = WRITE;
					lptr->nextpid = q[lptr->lwqtail].qprev;
					return;
                } else {
                    //reader came before but writer came after 1 sec
                    lptr->nextq = rpid;
					lptr->nextop_type = READ;
					lptr->nextpid = q[lptr->lrqtail].qprev;
					return;
                }
            } else if (wtime <= rtime) {
                //writer came before
                lptr->nextq = wpid;
				lptr->nextop_type = WRITE;
				lptr->nextpid = q[lptr->lwqtail].qprev;
				return;
            }
        }
    } else if ((0 == nwreaders) && (0 == nwrite)) {
        //no more waiters
        lptr->nextq = EMPTY;
		lptr->nextop_type =EMPTY;
		lptr->nextpid = EMPTY;
		return;
    }
    return;
}

