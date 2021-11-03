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
在进行拷贝时范围是 `PDX(UTOP)` 到 `PDX(~0)` ，很明显这基本就是内核态空间的位置，之所以需要拷贝 `boot_pgdir` 是为了让每个进程都有机会陷入到内核态从而访问对应的内核区域，要不然如果是空那进程陷入到内核态不就傻眼了吗。我猜测 `VPT` 和 `UVPT` 是不同进程不一样的，所以在这里留个坑，之后再设置。

参考 `mmu.h` 中的定义:
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

`ULIM` 是用户态空间和内核态空间的分界， `ULIM` 往下是用户态空间，往上是内核态空间，在我们实验中使用的是 `2G/2G` 模式，所以这里 `ULIM` 是 `0x80000000` 。而 `UTOP` 是用户可以使用的空间顶部，再往上的东西是 `UVPT` 和 `UPAGES` ，这些分别是 `boot_pgdir` 和 `pages` 的映射，指向了同一物理内存空间，但内核对 `UVPT` 与 `UPAGES` 设置了权限，用户只能读取，不可修改。

`CR3` 寄存器存储了页目录的物理地址，通过将 `env_cr3` 赋值给 `pgdir[PDX(UVPT)]` （页表中的某个地址，同时还作为页目录）可以完成页表的自映射，这样以后就能找到页目录了。

所有的物理地址都需要通过页表查询，每个进程的虚拟地址是相互隔离的，当进程切换时，虽然实际用的物理空间可能相同，但是通过页表的映射，让虚拟空间具有独立性，进程之间不会相互影响。

## Exercise 3.5
```c++
int
env_alloc(struct Env **new, u_int parent_id)
{
	int r;
	struct Env *e;
    
    /*Step 1: Get a new Env from env_free_list*/
    if(LIST_EMPTY(&env_free_list)) {
      *new = NULL;
      return -E_NO_FREE_ENV;
    }
    e = LIST_FIRST(&env_free_list);
    
    /*Step 2: Call certain function(has been implemented) to init kernel memory layout for this new Env.
     *The function mainly maps the kernel address to this new Env address. */
    r = env_setup_vm(e);
    if(r < 0) return r;

    /*Step 3: Initialize every field of new Env with appropriate values*/
    e->env_id = mkenvid(e);
    e->env_parent_id = parent_id;
    e->env_status = ENV_RUNNABLE;

    /*Step 4: focus on initializing env_tf structure, located at this new Env. 
     * especially the sp register,CPU status. */
    e->env_tf.cp0_status = 0x10001004;
    e->env_tf.regs[29] = USTACKTOP;

    /*Step 5: Remove the new Env from Env free list*/
    LIST_REMOVE(e, env_link);

    *new = e;
    return 0;
}
```

## Thinking 3.4
`user_data` 是将来创建进程后的进程控制块(PCB)指针，显然没有这个参数就不行。

我认为这种设计就非常类似于 `int scanf(const char *__format, ...)` 的 `va_list` 内的参数，在调用前挖个坑分配好空间，在调用时对里面的内容进行操作，本来是要返回给用户的，但却无需显式返回。

## Exercise 3.6

