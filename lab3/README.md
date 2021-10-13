# lab3

## Exercise 3.1
```c++
/* Step 3, Allocate proper size of physical memory for global array `envs`,
 * for process management. Then map the physical address to `UENVS`. */
envs = (struct Env *)alloc(NENV * sizeof(struct Env), BY2PG, 1);
n = ROUND(NENV * sizeof(struct Env), BY2PG);
boot_map_segment(pgdir, UENVS, n, PADDR(envs), PTE_R);
```

## Exercise 3.2
```c++
/*Step 1: Initial env_free_list. */
LIST_INIT(&env_free_list);

/*Step 2: Travel the elements in 'envs', init every element(mainly initial its status, mark it as free)
  * and inserts them into the env_free_list as reverse order. */
for(i=NENV-1; i>=0; --i) {
  envs[i].env_status = ENV_FREE;
  LIST_INSERT_HEAD(&env_free_list, &envs[i], env_link);
}
```

## Thinking 3.1
也不是必须，但是这样有很多好处。插入空闲链表时最好逆序插入，这样完了之后链表头部对应的是 `envs[0]` ，在空闲链表使用时最先使用的也是 `envs[0]` ，这样的好处是最近使用过的放在头部，类似LRU算法，有助于提升效率。

## Exercise 3.3
```c++
int envid2env(u_int envid, struct Env **penv, int checkperm)
{
        struct Env *e;
    /* Hint:
 *      *  If envid is zero, return the current environment.*/
    /*Step 1: Assign value to e using envid. */
    if(envid == 0) {
      *penv = curenv;
      return 0;
    }
    
    e = envs + ENVX(envid);

    if (e->env_status == ENV_FREE || e->env_id != envid) {
      *penv = 0;
      return -E_BAD_ENV;
    }
    /* Hint:
 *      *  Check that the calling environment has legitimate permissions
 *           *  to manipulate the specified environment.
 *                *  If checkperm is set, the specified environment
 *                     *  must be either the current environment.
 *                          *  or an immediate child of the current environment.If not, error! */
    /*Step 2: Make a check according to checkperm. */

    if (checkperm && e != curenv && e->env_parent_id != curenv->env_id) {
      *penv = 0;
      return -E_BAD_ENV;
    }

    *penv = e;
    return 0;
}
```

## Thinking 3.2
```c++
u_int mkenvid(struct Env *e)
{
	static u_long next_env_id = 0;

    /*Hint: lower bits of envid hold e's position in the envs array. */
	u_int idx = e - envs;

    /*Hint:  high bits of envid hold an increasing number. */
	return (++next_env_id << (1 + LOG2NENV)) | idx;
}
```
低位通过进程块下标得到，高位是由一个持续增长的数来决定的。这样的好处是可以使进程ID唯一（如果只有低位那么在重复分配时就会重复），而且通过进程号的低 `1 + LOG2NENV` 位就能得到PCB的下标。

如果不判断 `e->env_id != envid` ，就可能导致原本未分配而为 `0` 的进程ID混入到正常的进程中。

## Exercise 3.4
```c++
static int
env_setup_vm(struct Env *e)
{

	int i, r;
	struct Page *p = NULL;
	Pde *pgdir;

  /*Step 1: Allocate a page for the page directory using a function you completed in the lab2.
    * and add its reference.
    *pgdir is the page directory of Env e, assign value for it. */
  if ((r = page_alloc(&p)) < 0) {/* Todo here*/
    panic("env_setup_vm - page alloc error\n");
    return r;
  }

  /*Step 2: Zero pgdir's field before UTOP. */
  for(i = 0; i < PDX(UTOP); ++i) {
    pgdir[i] = 0;
  }

  /*Step 3: Copy kernel's boot_pgdir to pgdir. */
  for(i = PDX(UTOP); i <= PDX(~0); ++i) {
    if(i != PDX(VPT) && i != PDX(UVPT)) { // except VPT and UVPT
      pgdir[i] = boot_pgdir[i];
    }
  }

  /* Hint:
    *  The VA space of all envs is identical above UTOP
    *  (except at VPT and UVPT, which we've set below).
    *  See ./include/mmu.h for layout.
    *  Can you use boot_pgdir as a template?
    */

  /*Step 4: Set e->env_pgdir and e->env_cr3 accordingly. */
  e->env_pgdir = pgdir;
  e->env_cr3 = PADDR(pgdir);

  /*VPT and UVPT map the env's own page table, with
   *different permissions. */
	e->env_pgdir[PDX(VPT)]   = e->env_cr3;
  e->env_pgdir[PDX(UVPT)]  = e->env_cr3 | PTE_V | PTE_R;
	return 0;
}
```

## Thinking 3.3

```c++
/*o      ULIM     -----> +----------------------------+------------0x8000 0000-------    
  o                      |         User VPT           |     PDMAP                /|\ 
  o      UVPT     -----> +----------------------------+------------0x7fc0 0000    |
  o                      |         PAGES              |     PDMAP                 |
  o      UPAGES   -----> +----------------------------+------------0x7f80 0000    |
  o                      |         ENVS               |     PDMAP                 |
  o  UTOP,UENVS   -----> +----------------------------+------------0x7f40 0000    |
  o  UXSTACKTOP -/       |     user exception stack   |     BY2PG                 | */
```

根据 `UTOP` 的定义:
```c++
#define UENVS (UPAGES - PDMAP)
#define UTOP UENVS
```
