#
/*
 */

#include "../param.h"
#include "../systm.h"
#include "../user.h"
#include "../proc.h"
#include "../buf.h"
#include "../reg.h"
#include "../inode.h"

/*
 * exec system call.
 * Because of the fact that an I/O buffer is used
 * to store the caller's arguments during exec,
 * and more buffers are needed to read in the text file,
 * deadly embraces waiting for free buffers are possible.
 * Therefore the number of processes simultaneously
 * running in exec has to be limited to NEXEC.
 */
#define EXPRI	-1

exec()
{
	int ap, na, nc, *bp;
	int ts, ds, sep;
	register c, *ip;
	register char *cp;
	extern uchar;

	/*
	 * pick up file names
	 * and check various modes
	 * for execute permission
	 */

	/*
	 * get the element of the file corresponding to the inode[]
	 * through the path of the program execution file specified
	 * by the user as a parameter.
	 * If the user specifies a file path that does not exist, or
	 * does not have permission to access the file, namei() will
	 * return NULL.
	 */
	ip = namei(&uchar, 0);
	if(ip == NULL)
		return;
	// Confirm that the number of processes executing exec() is less than NEXEC
	while(execnt >= NEXEC)
		// if greater or equal than, sleep()
		sleep(&execnt, EXPRI);
		// until smaller than NEXEC
	// the number of processes executing exec() increase 1
	execnt++;
	// get block device buffers that have not been allocated to other block devices
	bp = getblk(NODEV);
	// execute access() to check the execution permission of the program file
	// determine the file is not a special file.
	if(access(ip, IEXEC) || (ip->i_mode&IFMT)!=0)
		goto bad;

	/*
	 * pack up arguments into
	 * allocated disk buffer
	 */
	// Set cp to the address of the buffer data area
	// clear na: number of parameters
	// nc: total number of bytes of parameters
	cp = bp->b_addr;
	na = 0;
	nc = 0;
	/*
	 * The parameters passed to the program by the user are processed in turn.
	 * fuuword() is the data of 1 word length copied from the virtual address
	 * space shown in the previous mode to the virtual address space shown in 
	 * the current mode.
	 */
	while(ap = fuword(u.u_arg[1])) {
		// when the while loop ended, the amount of parameters = na
		na++;
		// fuword failed, return -1
		if(ap == -1)
			goto bad;
		// read the next parameter
		u.u_arg[1] =+ 2;
		// Store parameters into the buffer sequentially in bytes
		for(;;) {
			c = fubyte(ap++);
			if(c == -1)
				goto bad;
			*cp++ = c;
			// after the loop, the total bytes of parameters = nc
			nc++;
			// the length of buffer area is only 512 bytes.
			if(nc > 510) {
				u.u_error = E2BIG;
				goto bad;
			}
			// parameters ending with 0(NULL)
			if(c == 0)
				break;
		}
	}
	// when nc is odd, add 1 byte to buffer area
	// let the length of data become even
	if((nc&1) != 0) {
		*cp++ = 0;
		nc++;
	}

	/*
	 * read in first 8 bytes
	 * of file for segment
	 * sizes:
	 * w0 = 407/410/411 (410 implies RO text) (411 implies sep ID)
	 * w1 = text size
	 * w2 = data size
	 * w3 = bss size
	 */

	u.u_base = &u.u_arg[0];
	u.u_count = 8;
	u.u_offset[1] = 0;
	u.u_offset[0] = 0;
	u.u_segflg = 1;
	readi(ip);
	u.u_segflg = 0;
	if(u.u_error)
		goto bad;
	sep = 0;
	if(u.u_arg[0] == 0407) {
		u.u_arg[2] =+ u.u_arg[1];
		u.u_arg[1] = 0;
	} else
	if(u.u_arg[0] == 0411)
		sep++; else
	if(u.u_arg[0] != 0410) {
		u.u_error = ENOEXEC;
		goto bad;
	}
	if(u.u_arg[1]!=0 && (ip->i_flag&ITEXT)==0 && ip->i_count!=1) {
		u.u_error = ETXTBSY;
		goto bad;
	}

	/*
	 * find text and data sizes
	 * try them out for possible
	 * exceed of max sizes
	 */

	ts = ((u.u_arg[1]+63)>>6) & 01777;
	ds = ((u.u_arg[2]+u.u_arg[3]+63)>>6) & 01777;
	// Update user APR and userspace
	// SSIZE is initial length of stack area
	if(estabur(ts, ds, SSIZE, sep))
		goto bad;

	/*
	 * allocate and clear core
	 * at this point, committed
	 * to the new image
	 */

	u.u_prof[3] = 0;
	// release the currently used code segment
	xfree();
	// shrink the data segment to the same length as the user[]
	expand(USIZE);
	// allocate the memory used by the code segment
	xalloc(ip);
	// ensure the data area
	c = USIZE+ds+SSIZE;
	expand(c);
	while(--c >= USIZE)
		clearseg(u.u_procp->p_addr+c);

	/*
	 * read in data segment
	 */
	// Change the starting position of the data area of the process
	// to the position where the virtual address is 0.
	estabur(0, ds, 0, 0);
	u.u_base = 0;
	// 020: the length of the program file header.
	u.u_offset[1] = 020+u.u_arg[1];
	u.u_count = u.u_arg[2];
	// read the program into the data area of the process
	readi(ip);

	/*
	 * initialize stack segment
	 */

	u.u_tsize = ts;
	u.u_dsize = ds;
	u.u_ssize = SSIZE;
	u.u_sep = sep;
	// xxecute estabur() to finalize the user space.
	estabur(u.u_tsize, u.u_dsize, u.u_ssize, u.u_sep);
	// transfer the parameters saved in the buffer to the stack area
	// set cp as the address of the data area of the buffer.
	cp = bp->b_addr;
	// calculate the address of the stack pointer
	ap = -nc - na*2 - 4;
	// set it as the sp of the user process.
	u.u_ar0[R6] = ap;
	// na: the number of parameters
	// -> to the user space address pointer.
	suword(ap, na);
	// set c to the user space address where the parameter is saved
	c = -nc;
	// store parameters and addresses in the stack area
	while(na--) {
		suword(ap=+2, c);
		do
			subyte(c++, *cp);
		while(*cp++);
	}
	// set the next address of the parameter address to -1
	suword(ap+2, -1);

	/*
	 * set SUID/SGID protections, if no tracing
	 */

	if ((u.u_procp->p_flag&STRC)==0) {
		if(ip->i_mode&ISUID)
			if(u.u_uid != 0) {
				u.u_uid = ip->i_uid;
				u.u_procp->p_uid = ip->i_uid;
			}
		if(ip->i_mode&ISGID)
			u.u_gid = ip->i_gid;
	}

	/*
	 * clear sigs, regs and return
	 */
	// If the value of u.u_signal[n] is an even number,
	// it will be cleared to 0, if it is an odd number,
	// the original data will be retained.
	c = ip;
	for(ip = &u.u_signal[0]; ip < &u.u_signal[NSIG]; ip++)
		if((*ip & 1) == 0)
			*ip = 0;
	// clear the values of r0~r5 and r7 of the user process to 0
	for(cp = &regloc[0]; cp < &regloc[6];)
		u.u_ar0[*cp++] = 0;
	u.u_ar0[R7] = 0;
	// clear the registers for floating-point arithmetic to 0.
	for(ip = &u.u_fsav[0]; ip < &u.u_fsav[25];)
		*ip++ = 0;
	ip = c;

bad:
	// decrement the reference counter of the element in the inode[]
	// representing the program execution file by 1
	iput(ip);
	// free the buffer used for parameter processing.
	brelse(bp);
	if(execnt >= NEXEC)
		// wake up other processes waiting to enter exec() processing.
		wakeup(&execnt);
	execnt--;
}

/*
 * exit system call:
 * pass back caller's r0
 */
rexit()
{

	u.u_arg[0] = u.u_ar0[R0] << 8;
	exit();
}

/*
 * Release resources.
 * Save u. area for parent to look at.
 * Enter zombie state.
 * Wake up parent and init processes,
 * and dispose of children.
 */
exit()
{
	register int *q, a;
	register struct proc *p;

	u.u_procp->p_flag =& ~STRC;
	for(q = &u.u_signal[0]; q < &u.u_signal[NSIG];)
		*q++ = 1;
	for(q = &u.u_ofile[0]; q < &u.u_ofile[NOFILE]; q++)
		if(a = *q) {
			*q = NULL;
			closef(a);
		}
	iput(u.u_cdir);
	xfree();
	a = malloc(swapmap, 1);
	if(a == NULL)
		panic("out of swap");
	p = getblk(swapdev, a);
	bcopy(&u, p->b_addr, 256);
	bwrite(p);
	q = u.u_procp;
	mfree(coremap, q->p_size, q->p_addr);
	q->p_addr = a;
	q->p_stat = SZOMB;

loop:
	for(p = &proc[0]; p < &proc[NPROC]; p++)
	if(q->p_ppid == p->p_pid) {
		wakeup(&proc[1]);
		wakeup(p);
		for(p = &proc[0]; p < &proc[NPROC]; p++)
		if(q->p_pid == p->p_ppid) {
			p->p_ppid  = 1;
			if (p->p_stat == SSTOP)
				setrun(p);
		}
		swtch();
		/* no return */
	}
	q->p_ppid = 1;
	goto loop;
}

/*
 * Wait system call.
 * Search for a terminated (zombie) child,
 * finally lay it to rest, and collect its status.
 * Look also for stopped (traced) children,
 * and pass back status from them.
 */
wait()
{
	register f, *bp;
	register struct proc *p;

	f = 0;

loop:
	for(p = &proc[0]; p < &proc[NPROC]; p++)
	if(p->p_ppid == u.u_procp->p_pid) {
		f++;
		if(p->p_stat == SZOMB) {
			u.u_ar0[R0] = p->p_pid;
			bp = bread(swapdev, f=p->p_addr);
			mfree(swapmap, 1, f);
			p->p_stat = NULL;
			p->p_pid = 0;
			p->p_ppid = 0;
			p->p_sig = 0;
			p->p_ttyp = 0;
			p->p_flag = 0;
			p = bp->b_addr;
			u.u_cstime[0] =+ p->u_cstime[0];
			dpadd(u.u_cstime, p->u_cstime[1]);
			dpadd(u.u_cstime, p->u_stime);
			u.u_cutime[0] =+ p->u_cutime[0];
			dpadd(u.u_cutime, p->u_cutime[1]);
			dpadd(u.u_cutime, p->u_utime);
			u.u_ar0[R1] = p->u_arg[0];
			brelse(bp);
			return;
		}
		if(p->p_stat == SSTOP) {
			if((p->p_flag&SWTED) == 0) {
				p->p_flag =| SWTED;
				u.u_ar0[R0] = p->p_pid;
				u.u_ar0[R1] = (p->p_sig<<8) | 0177;
				return;
			}
			p->p_flag =& ~(STRC|SWTED);
			setrun(p);
		}
	}
	if(f) {
		sleep(u.u_procp, PWAIT);
		goto loop;
	}
	u.u_error = ECHILD;
}

/*
 * fork system call.
 */
fork()
{
	register struct proc *p1, *p2;

	//将p1指向父进程的proc结构体
	p1 = u.u_procp;
	//通过p2从起始位置遍历proc[]寻找未使用的元素，找到后
	//将p2指向该元素，然后跳转到found。
	for(p2 = &proc[0]; p2 < &proc[NPROC]; p2++)
		if(p2->p_stat == NULL)
			goto found;
	//如果没找到可用元素，设置u_error code为11，跳转到out
	u.u_error = EAGAIN;
	goto out;

found:
	// 找到未使用的元素后，调用newproc()生成新的进程。
	if(newproc()) {
		/*
		*	将用户进程的r0设定为父进程的proc.p_pid
		*	以此作为系统调用fork对子进程的返回值
		*/
		u.u_ar0[R0] = p1->p_pid;
		u.u_cstime[0] = 0;
		u.u_cstime[1] = 0;
		u.u_stime = 0;
		u.u_cutime[0] = 0;
		u.u_cutime[1] = 0;
		u.u_utime = 0;
		return;
	}
	/*
	*	将父进程的r0设为子进程的proc.p_pid
	*	以此作为系统调用fork对父进程的返回值
	*/
	u.u_ar0[R0] = p2->p_pid;

out:
	//跳到下一语句 "bec 2f"(fork.s)
	u.u_ar0[R7] =+ 2;
}

/*
 * break system call.
 *  -- bad planning: "break" is a dirty word in C.
 */
sbreak()
{
	register a, n, d;
	int i;

	/*
	 * set n to new data size
	 * set d to new-old
	 * set n to new total size
	 */

	n = (((u.u_arg[0]+63)>>6) & 01777);
	if(!u.u_sep)
		n =- nseg(u.u_tsize) * 128;
	if(n < 0)
		n = 0;
	d = n - u.u_dsize;
	n =+ USIZE+u.u_ssize;
	if(estabur(u.u_tsize, u.u_dsize+d, u.u_ssize, u.u_sep))
		return;
	u.u_dsize =+ d;
	if(d > 0)
		goto bigger;
	a = u.u_procp->p_addr + n - u.u_ssize;
	i = n;
	n = u.u_ssize;
	while(n--) {
		copyseg(a-d, a);
		a++;
	}
	expand(i);
	return;

bigger:
	expand(n);
	a = u.u_procp->p_addr + n;
	n = u.u_ssize;
	while(n--) {
		a--;
		copyseg(a-d, a);
	}
	while(d--)
		clearseg(--a);
}
