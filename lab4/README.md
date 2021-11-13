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
	if((ret = envid2env(envid, &env, 0) < 0)) return -E_BAD_ENV;
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