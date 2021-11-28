# lab4

## Exercise 4.1
```S
LEAF(msyscall)
    // TODO: execute a `syscall` instruction and return from msyscall
    syscall
    jr ra
    nop
END(msyscall)
```

## Thinking 4.1
内核在保护现场的时候，通过 `SAVE_ALL // Macro used to save trapframe` 把寄存器的内容存到了 `struct Trapframe` 的 `regs` ，即通用寄存器的内容，它们保存到了 `struct Env` 里的 `env_tf` ，也就是把寄存器的内容转移到了内存中，这样就实现了保护现场。

对于MIPS参数传递，MIPS ABI制定了一个规范，这就如同于32位汇编中的 `__fastcall` 前两个参数使用 `ecx` 和 `edx` 传递一样:
* 如果函数参数个数小于等于4，则将参数依次存入 `a0` ~ `a3` 寄存器中，并在栈帧底部保留 `16` 字节的空间（即 `sp-16` ），但并不一定使用这些空间。
* 如果函数参数个数大于4，则前4个参数依次存入 `a0` ~ `a3` 寄存器中，从第5个参数开始，依次在前4个参数预留空间之外的空间内存储，即没有寄存器去保存这些值。
* 举例，如果一个C函数有6个参数，在汇编语言中需要调用的时候，应当将前4个参数存在 `a0` ~ `a3` 寄存器中，第5个参数存在 `16(sp)` 的位置，第6个参数存在 `20(sp)` 的位置。区间0~15的空间保留但不使用。
* 返回值在 `v0` 寄存器中（某些特殊的情况也会用到 `v1` ，但不常见）。

理论上，就算保护现场，也不会清理现场的痕迹，虽然可以从 `env_tf.regs` 里去读取 `a0` ~ `a3` 寄存器的值，但那样做毕竟慢，如果没有破坏现场，那么调用到 `sys_*` 的时候肯定可以直接读取。但是问题在于，我观察到 `handle_sys` 中似乎对现场进行了破坏，包括 `TODO: Copy the syscall number into $a0.` 这样的犯罪预备以及 `addiu a0, a0, -__SYSCALL_BASE // a0 <- relative syscall number` 这样的犯罪既遂，无疑破灭了我们直接去访问 `a0` ~ `a3` 的可能性。所以系统陷入内核后只能从 `env_tf.regs` 里去读取。

只要把前四个参数恢复到对应的寄存器中，后两个参数放在栈的原位，这样对 `sys_*` 就是无感的。

系统调用对 `struct Trapframe` 做出的更改是，首先对epc+4，以保证用户态能够执行下一条指令。另外要把 `sys_*` 的返回值存到 `v0` ，以便于返回用户态的时候用户态拿到结果。

## Exercise 4.2
```S
NESTED(handle_sys,TF_SIZE, sp)
    SAVE_ALL                            // Macro used to save trapframe
    CLI                                 // Clean Interrupt Mask
    nop
    .set at                             // Resume use of $at

    // TODO: Fetch EPC from Trapframe, calculate a proper value and store it back to trapframe.
    lw      t0, TF_EPC(sp)              // t0 <- EPC
    addi    t0, t0, 4                   // t0 <- t0 + 4
    sw      t0, TF_EPC(sp)              // EPC <- t0

    // TODO: Copy the syscall number into $a0.
    lw      a0, 16(sp)                  // a0 <- Trapframe($a0)
    
    addiu   a0, a0, -__SYSCALL_BASE     // a0 <- relative syscall number
    sll     t0, a0, 2                   // t0 <- relative syscall number times 4
    la      t1, sys_call_table          // t1 <- syscall table base
    addu    t1, t1, t0                  // t1 <- table entry of specific syscall
    lw      t2, 0(t1)                   // t2 <- function entry of specific syscall

    lw      t0, TF_REG29(sp)            // t0 <- user's stack pointer
    lw      t3, 16(t0)                  // t3 <- the 5th argument of msyscall
    lw      t4, 20(t0)                  // t4 <- the 6th argument of msyscall

    // TODO: Allocate a space of six arguments on current kernel stack and copy the six arguments to proper location
    addiu   sp, sp, -24
    sw      t3, 16(sp)
    sw      t4, 20(sp)
    
    jalr    t2                          // Invoke sys_* function
    nop
    
    // TODO: Resume current kernel stack
    
    sw      v0, TF_REG2(sp)             // Store return value of function sys_* (in $v0) into trapframe

    j       ret_from_exception          // Return from exeception
    nop
END(handle_sys)
```

## Exercise 4.3
```c++
int sys_mem_alloc(int sysno, u_int envid, u_int va, u_int perm)
{
	// Your code here.
	struct Env *env;
	struct Page *ppage;
	int ret;
	ret = 0;
	if(va >= UTOP || va < 0) return -E_UNSPECIFIED;
  if(!(perm & PTE_V) || (perm & PTE_COW)) return -E_INVAL;
	if((ret = envid2env(envid, &env, 1)) < 0) return -E_BAD_ENV;
	if((ret = page_alloc(&ppage)) < 0) return -E_NO_MEM;
	if((ret = page_insert(env->env_pgdir, &ppage, va, perm)) < 0) return -E_NO_MEM;
	return 0;
}
```

## Exercise 4.4
```c++
int sys_mem_map(int sysno, u_int srcid, u_int srcva, u_int dstid, u_int dstva,
				u_int perm)
{
	int ret;
	u_int round_srcva, round_dstva;
	struct Env *srcenv;
	struct Env *dstenv;
	struct Page *ppage;
	Pte *ppte;

	ppage = NULL;
	ret = 0;
	round_srcva = ROUNDDOWN(srcva, BY2PG);
	round_dstva = ROUNDDOWN(dstva, BY2PG);

  //your code here
	if(round_srcva >= UTOP || round_srcva < 0 || round_dstva >= UTOP || round_dstva < 0) return -E_UNSPECIFIED;
	if(!(perm & PTE_V)) return -E_INVAL;
	if((ret = envid2env(srcid, &srcenv, 1)) < 0) return -E_BAD_ENV;
	if((ret = envid2env(dstid, &dstenv, 1)) < 0) return -E_BAD_ENV;
	if((ppage = page_lookup(srcenv->env_pgdir, round_srcva, &ppte)) == NULL) return -E_UNSPECIFIED;
	if((perm & PTE_R) && !((*ppte) & PTE_R)) return -E_INVAL; // restriction that can't go from non-writable to writable
	if((ret = page_insert(dstenv->env_pgdir, &ppage, round_dstva, perm)) < 0) return -E_NO_MEM;
	return ret;
}
```

## Exercise 4.5
```c++
int sys_mem_unmap(int sysno, u_int envid, u_int va)
{
	// Your code here.
	int ret = 0;
	struct Env *env;
	if(va >= UTOP || va < 0) return -E_INVAL;
	if((ret = envid2env(envid, &env, 0)) < 0) return -E_BAD_ENV;
	page_remove(env->env_pgdir, va);
	return ret;
	//	panic("sys_mem_unmap not implemented");
}
```

## Exercise 4.6
```c++
void sys_yield(void)
{
	struct Trapframe *src = (struct Trapframe *)(KERNEL_SP - sizeof(struct Trapframe));
	struct Trapframe *dst = (struct Trapframe *)(TIMESTACK - sizeof(struct Trapframe));
	bcopy((void*)src, (void*)dst, sizeof(struct Trapframe));
	sched_yield();
}
```

## Exercise 4.7
```c++
void sys_ipc_recv(int sysno, u_int dstva)
{
	if(dstva >= UTOP || dstva < 0) return;
	curenv->env_ipc_recving = 1;
	curenv->env_ipc_dstva = dstva;
	curenv->env_status = ENV_NOT_RUNNABLE;
	sys_yield();
}

int sys_ipc_can_send(int sysno, u_int envid, u_int value, u_int srcva,
					           u_int perm)
{
	int r;
	struct Env *e;
	struct Page *p;
	if(srcva >= UTOP || srcva < 0) return -E_INVAL;
	if((r = envid2env(envid, &e, 0)) < 0) return -E_BAD_ENV;
	if(e->env_ipc_recving != 1) return -E_IPC_NOT_RECV;
	if(srcva != 0) {
		e->env_ipc_perm = perm | PTE_V | PTE_R;
		if((p = page_lookup(curenv->env_pgdir, srcva, 0)) <= 0) return -E_INVAL;
		else if(page_insert(e->env_pgdir, p, e->env_ipc_dstva, perm) < 0) return -E_INVAL;
	}
	e->env_ipc_from = curenv->env_id;
	e->env_ipc_value = value;
	e->env_ipc_perm = perm;
	e->env_ipc_recving = 0;
	e->env_status = ENV_RUNNABLE;
	return 0;
}
```

## Thinking 4.2
说明子进程很可能父进程共享了内存空间，这样效率最高。

说明子进程恢复到的上下文位置是fork所在的位置。


## Thinking 4.3
选C。

fork在调用前的代码（包括fork）都只被调用了一遍，只有fork以后的代码才被父子进程各执行一遍，产生的返回值用来区分父子进程，所以返回值不同。

## Exercise 4.8
```c++
int sys_env_alloc(void)
{
	int r;
	struct Env *e;

	// Your code here.
	if((r = env_alloc(&e, curenv->env_id)) < 0) { // Allocate struct Env to e
		return r;
	}
	bcopy((void *)(KERNEL_SP - sizeof(struct Trapframe)), &(e->env_tf), sizeof(struct Trapframe));
	e->env_tf = curenv->env_tf; 			// Trapframe copy
	e->env_tf.pc = e->env_tf.cp0_epc; // Set program counter
	e->env_status = ENV_NOT_RUNNABLE; // Set process status
	e->env_pri = curenv->env_pri;			// Set priority
	e->env_tf.regs[2] = 0;						// Set return value of child process

	return e->env_id;
	//	panic("sys_env_alloc not implemented");
}
```

## Exercise 4.9
实现 `fork` 函数最值得注意的是，需要先设置缺页中断处理 `pgfault` ，然后才能调用 `syscall_env_alloc` ，否则会在异常中无法自拔。
```c++
int fork(void)
{
	u_int newenvid;
	extern struct Env *envs;
	extern struct Env *env;
	u_int i;

	// Your code here.

	// The parent installs pgfault using set_pgfault_handler
	set_pgfault_handler(pgfault);

	//alloc a new alloc
	newenvid = syscall_env_alloc();
	if(newenvid == 0) { // child process
		env = envs + ENVX(syscall_getenvid());
	} else { // father process
		syscall_mem_alloc(newenvid, UXSTACKTOP - BY2PG, PTE_V | PTE_R | PTE_LIBRARY);
		syscall_set_pgfault_handler(newenvid, __asm_pgfault_handler, UXSTACKTOP);
		syscall_set_env_status(newenvid, ENV_RUNNABLE);
	}
	return newenvid;
}
```

## Thinking 4.4
从 `0` 到 `USERSTACKTOP` 的地址空间里，对于不是共享的和只读的页面可以保护起来。`[UTEXT, USTACKTOP]` 可以被保护，而 `[USTACKTOP, UTOP]` 的位置不能被保护，即不能被 `duppage` ，原因是如果我们将 `[UXSTACKTOP-BY2PG, UXSTACKTOP]` 这块区域 `duppage` 到子进程，那么父进程与子进程的错误栈就一样了。但是由于两个进程的调度时机不同，子进程应该为其分配一个新的错误栈。而 `[USTACKTOP, USTACKTOP+BY2PG]` 的区域是无效内存，因此也不应该被保护。此外，在这个区域中我们对于只读的页不能加上 `PTE_COW` 权限，而应该只给可写的页或者原本是 `PTE_COW` 权限的页加上 `PTE_COW` 权限。因为我们不能将原本不可写的页的权限改成可写。

## Thinking 4.5
`vpt` 是指向用户页表的指针， `vpd` 是指向用户页目录的指针。它们在 `user/entry.S` 中定义如下:
```S
vpt:
	.word UVPT

	.globl vpd
vpd:
	.word (UVPT+(UVPT>>12)*4)

	.globl __pgfault_handler
	__pgfault_handler:
	.word 0


	.extern libmain

	.text
	.globl _start
```

它们在 `mmu.h` 中使用 `extern` 声明如下:
```c++
extern volatile Pte *vpt[];
extern volatile Pde *vpd[];
```

通过使用这两者，可以实现在用户地址空间内对内核地址的访问，例如 `va` 的页表地址是 `(*vpt)[VPN(va)]` ，页目录地址是 `(*vpd)[VPN(va)>>10]` 。

之所以能使用，没别的原因，就是因为它们指向了正确的地址。

从 `*vpd = (UVPT+(UVPT>>12)*4)` 体现了页表自映射机制，页目录就在页表的其中一块。

进程并不能通过这种方式修改自己的页表项，如果页表数据让用户进程控制，那区分内核空间和用户空间也没有什么意思了。这样的话的用户进程就能看见很多关键数据，安全性怎么来保证呢？用户进程的页表是覆盖整个地址空间的，也就是说，用户进程页表中的内核部分对所有进程来说都是一样的，这怎么能让用户进程去控制呢？

## Exercise 4.10
```c++
static void duppage(u_int envid, u_int pn)
{
  u_int addr;
  u_int perm;
  
  addr = pn * BY2PG;
  perm = (*vpt)[pn] & 0xfff;
  if(!(perm & PTE_R) || !(perm & PTE_V)) {        // readonly page || invalid page : no change
    syscall_mem_map(0, addr, envid, addr, perm);
  } else if(perm & PTE_LIBRARY) {                 // shared page : no change
    syscall_mem_map(0, addr, envid, addr, perm);
  } else if(perm & PTE_COW) {                     // COW page : no change
    syscall_mem_map(0, addr, envid, addr, perm);
  } else {                                        // add COW         
    perm |= PTE_COW;
    syscall_mem_map(0, addr, envid, addr, perm);  // child process
    syscall_mem_map(0, addr, 0, addr, perm);      // father process
  }

  // user_panic("duppage not implemented");
}
```

## Exercise 4.11
```c++
void page_fault_handler(struct Trapframe *tf)
{
    struct Trapframe PgTrapFrame;
    extern struct Env *curenv;

    bcopy(tf, &PgTrapFrame, sizeof(struct Trapframe));

    if (tf->regs[29] >= (curenv->env_xstacktop - BY2PG) &&
        tf->regs[29] <= (curenv->env_xstacktop - 1)) {
            tf->regs[29] = tf->regs[29] - sizeof(struct  Trapframe);
            bcopy(&PgTrapFrame, (void *)tf->regs[29], sizeof(struct Trapframe));
        } else {
            tf->regs[29] = curenv->env_xstacktop - sizeof(struct  Trapframe);
            bcopy(&PgTrapFrame, (void *)curenv->env_xstacktop - sizeof(struct  Trapframe), sizeof(struct Trapframe));
        }
    // TODO: Set EPC to a proper value in the trapframe
    tf->cp0_epc = curenv->env_pgfault_handler;
    return;
}
```

## Thinking 4.6
当有写时复制的页面被修改的时候，在发生时钟中断的时候，出现“中断重入”。

因为需要在用户态处理异常，保护现场，防止寄存器被破坏。

## Exercise 4.12
```c++
int sys_set_pgfault_handler(int sysno, u_int envid, u_int func, u_int xstacktop)
{
	// Your code here.
	struct Env *env;
	int ret;
	if((ret = envid2env(envid, &env, 1)) < 0) return ret;
	env->env_pgfault_handler = func;
	env->env_xstacktop = xstacktop;
	return 0;
	//	panic("sys_set_pgfault_handler not implemented");
}
```

## Thinking 4.7
效率高，不用trap到内核就能处理了。而且像这种思路，就算内核出了问题也不会影响用户的处理。

通过将通用寄存器的值压入栈中，然后在使用时取出，可以避免破坏通用寄存器中所存储的值。在取出的时候先取出除sp之外的其他寄存器，然后在跳转返回的同时，利用延时槽的特性恢复sp。

## Exercise 4.13
```c++
static void pgfault(u_int va)
{
  u_int *tmp;
  // writef("fork.c:pgfault():\t va:%x\n",va);
  
  // map the new page at a temporary place
  tmp = UXSTACKTOP - 2 * BY2PG;
  va = ROUNDDOWN(va, BY2PG);
  if(!((*vpt)[VPN(va)] & PTE_COW)) {
    user_panic("no COW\n");
  }
  if(syscall_mem_alloc(0, tmp, PTE_V | PTE_R)) {
    user_panic("syscall_mem_alloc error!\n");
  }
  // copy the content
  user_bcopy((void *)va, tmp, BY2PG);
  // map the page on the appropriate place
  if(syscall_mem_map(0, tmp, 0, va, PTE_V | PTE_R) != 0) {
    user_panic("syscall_mem_map error!\n");
  }
  // unmap the temporary place
  if(syscall_mem_unmap(0, tmp) != 0) {
    user_panic("syscall_mem_unmap error!\n");
  }
}
```

## Thinking 4.8
需要先设置缺页中断处理 `pgfault` ，然后才能调用 `syscall_env_alloc` ，否则会在异常中无法自拔。

如果放在COW之后，会导致写时复制机制无法正常运行，缺页错误无法处理。

不需要，因为在父进程中的值与子进程的相同，所以不需要重新设置。

## Exercise 4.14
```c++
int sys_set_env_status(int sysno, u_int envid, u_int status)
{
	// Your code here.
	struct Env *env;
	int ret;
	if(status > 2 || status < 0) return -E_INVAL;
	if((ret = envid2env(envid, &env, 1)) < 0) return ret;
	env->env_status = status;
	if(status == ENV_RUNNABLE) {
		LIST_INSERT_HEAD(env_sched_list, env, env_sched_link);
	}
	return 0;
	//	panic("sys_env_set_status not implemented");
}
```

## Exercise 4.15
```c++
int fork(void)
{
  u_int newenvid;
  extern struct Env *envs;
  extern struct Env *env;
  u_int i;
  u_int parent_id = syscall_getenvid(); // save the father process's id
  u_int envid;

  // The parent installs pgfault using set_pgfault_handler
  set_pgfault_handler(pgfault);

  //alloc a new alloc
  newenvid = syscall_env_alloc();

  if(newenvid == 0) { // child process
    env = envs + ENVX(syscall_getenvid());
    env->env_parent_id = parent_id;
    return 0;
  }
  
  for(i=0; i<VPN(USTACKTOP); ++i) {
    if(((*vpd)[i >> 10]) && ((*vpt)[i])) {
      duppage(newenvid, i);
    }
  }

  if(syscall_mem_alloc(newenvid, UXSTACKTOP - BY2PG, PTE_V | PTE_R) < 0) {
    writef("Error at fork.c/fork. syscall_mem_alloc for Son_env failed.\n");
    return -1;
  }

  if(syscall_set_pgfault_handler(newenvid, __asm_pgfault_handler, UXSTACKTOP) < 0) {
    writef("Error at fork.c/fork. syscall_set_pgfault_handler for Son_env failed.\n");
    return -1;
  }
  
  syscall_set_env_status(newenvid, ENV_RUNNABLE);
  
  return newenvid;
}
```
