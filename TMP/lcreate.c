#include <kernel.h>
#include <stdio.h>
#include <proc.h>
#include <lock.h>
/* 
*lcreate: create a new lock and return its lid
*/
int lcreate (void)
{
    int lid = 0;
	rwlock_t *lptr = NULL;
    STATWORD ps;
    disable(ps);
	//kprintf("inside lcreate\n");
	//get an uninitialized or deleted  lock
	lid = lget_free_lid();
    if (lid == SYSERR) {
		//all locks used up
		//kprintf("all locks used up");
        restore(ps);
        return SYSERR;
    }
	lptr = &ltab[lid];
	//set the lock users map for current pid
	lptr->proc_map[currpid] = 1;
    restore(ps);
    return lid;
}

/*
*lget_free_lid: returns an uninitialized or deleted lock
*/
int lget_free_lid (void)
{
    int i = 0,lid = 0;
	int lstate = EMPTY;
	rwlock_t *lptr = NULL;   
    for (i = 0; i < NLOCKS; i++) {
		lid = next_lock--;
		if(next_lock < 0){
			next_lock = NLOCKS-1;
		}
		lptr = &ltab[lid];
		lstate = lptr->lstate;
        if (lstate == LS_UNINIT || lstate ==LS_DELETED ) {
            lptr->lstate = LS_FREE;
            return lid;
        }
    }
    return SYSERR;
}

