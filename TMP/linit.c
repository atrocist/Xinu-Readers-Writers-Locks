#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <stdio.h>
#include <lock.h>

rwlock_t ltab[NLOCKS]; /* rwlock table */
int next_lock;

void linit(void)
{
    int lid = 0 , pid = 0;
    rwlock_t *lptr = NULL;
	struct pentry *pptr = NULL;
	//kprintf("inside linit\n");
	int next_lock = NLOCKS -1;
    for (lid = 0; lid < NLOCKS; lid++) {
        lptr = &ltab[lid];
		lptr->lstate = LS_UNINIT;
		lptr->nareaders = 0;         
		lptr->nawriters = 0;        
		lptr->nwreaders = 0;         
		lptr->nwwriters = 0;        
		
		lptr->nextq = EMPTY;          
		lptr->nextop_type = EMPTY;         
		lptr->nextpid = EMPTY;           
		
		lptr->lrqhead = newqueue();
        lptr->lrqtail = 1 + lptr->lrqhead;	
		lptr->lwqhead = newqueue();
        lptr->lwqtail = 1 + lptr->lwqhead;
		bzero(lptr->rel_map, (NPROC * sizeof(unsigned char)));
		bzero(lptr->del_map, (NPROC * sizeof(unsigned char)));
		bzero(lptr->proc_map, (NPROC * sizeof(unsigned char)));
    }
    return;
}

int lclear_maps(int pid)
{
    int lid = EMPTY;
	rwlock_t *lptr = NULL;
	struct pentry *pptr = NULL;
    STATWORD ps;
    disable(ps);
    if (isbadpid(pid)) {
		restore(ps);
		return SYSERR;
    }
	pptr= &proctab[pid];
    for (lid = 0; lid < NLOCKS; ++lid) {
        lptr= &ltab[lid];
		lptr->del_map[pid] = 0;
        lptr->rel_map[pid] = 0;
        lptr->proc_map[pid] = 0;
    }
    restore(ps);
    return OK;
}


