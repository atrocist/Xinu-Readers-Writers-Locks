#ifndef _LOCK_H_
#define _LOCK_H_

#define NLOCKS          50

#define READ         1
#define WRITE        2

#define LS_UNINIT       0
#define LS_DELETED     	1
#define LS_FREE         2
#define LS_READ         3
#define LS_WRITE        4

#define isvalidltype(ltype) ((READ == ltype) || (WRITE == ltype))
#define isbadlid(LID) ((LID < 0) || (LID >= NLOCKS))

typedef struct rwlock_t {
    int lstate;         /* lock state*/
    int lrqhead;           /* head of lock's read queue */
    int lrqtail;           /* tail of lock's read queue */
    int lwqhead;           /* head of lock's write queue */
    int lwqtail;           /* tail of lock's write queue */
    int nextq;          /* which queue will be granted lock next*/
    int nextop_type;         /* next proc is a reader or writer */
    int nextpid;        /* pid of proc which will be granted the lock */
    int nareaders;         /* number of active readers */
    int nawriters;        /* number of active writers - 0 or 1 */
    int nwreaders;         /* number of readers waiting on this lock */
    int nwwriters;        /* number of writers waiting on this lock */
	unsigned char del_map[NPROC];  /* delete pid bitmap for this lock */
    unsigned char rel_map[NPROC];  /* release pid bitmap for this lock */
    unsigned char proc_map[NPROC];  /* records pid bitmapr for this lock */
}rwlock_t;

extern rwlock_t ltab[];
extern unsigned long ctr1000;
extern next_lock;

extern void linit(void);
extern int lclear_maps(int pid);
extern int lcreate(void);
extern int lget_free_lid(void);
extern int ldelete(int lid);
extern int lreset_lock_entry_on_delete(int lid);
extern int lock(int lid, int ltype, int lprio);
extern void lschedule_next_proc(int lid);
extern int releaseall(int numlocks, int args,...);
extern int lunlock(int lid, int pid);
extern int lproc_killed(int pid);
extern int lremove_pid_from_lock(int pid);
extern int l_get_all_waiting_readers(int lid);
extern int l_get_next_waiting_writer(int lid);
#endif