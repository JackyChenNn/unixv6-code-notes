/*
 * The user structure.
 * One allocated per process.
 * Contains all per process data
 * that doesn't need to be referenced
 * while the process is swapped.
 * The user block is USIZE*64 bytes
 * long; resides at virtual kernel
 * loc 140000; contains the system
 * stack per user; is cross referenced
 * with the proc structure for the
 * same process.
 */
struct user
{
	int	u_rsav[2];		/* save r5,r6 when exchanging stacks */
	int	u_fsav[25];		/* save fp registers */
					/* rsav and fsav must be first in structure */
					/* when the processor is PDP-11/40 will not be used. */
	char	u_segflg;		/* flag for IO; user or kernel space */
	char	u_error;		/* return error code */
	/*
	* the effective and real is determined
	* when user login the system
	* exp. user 101 excute a program,
	* effctive user id is 101; if user 101
	* use command "su" to change effective
	* user id to 0(superuser), but the
	* real user id still be 101.
	*/
	char	u_uid;			/* effective user id */
	char	u_gid;			/* effective group id */
	char	u_ruid;			/* real user id */
	char	u_rgid;			/* real group id */
	int		u_procp;		/* pointer to proc structure */
	char	*u_base;		/* base address for IO */
	char	*u_count;		/* bytes remaining for IO */
	char	*u_offset[2];	/* offset in file for IO */
	int		*u_cdir;		/* pointer to inode of current directory */
	char	u_dbuf[DIRSIZ];	/* current pathname component */
	char	*u_dirp;		/* current pointer to inode */
	struct	{				/* current directory entry */
		int		u_ino;		/* inode number */
		char	u_name[DIRSIZ];
							/* directory and file name */
	} u_dent;
	int	*u_pdir;			/* inode of parent directory of dirp */
	int	u_uisa[16];			/* prototype of segmentation addresses */
	int	u_uisd[16];			/* prototype of segmentation descriptors */
	int	u_ofile[NOFILE];	/* pointers to file structures of open files */
	int	u_arg[5];			/* arguments to current system call */
	int	u_tsize;			/* text size (*64) */
	int	u_dsize;			/* data size (*64) */
	int	u_ssize;			/* stack size (*64) */
	int	u_sep;				/* flag for I and D separation */
	int	u_qsav[2];			/* label variable for quits and interrupts */
	int	u_ssav[2];			/* label variable for swapping */
	int	u_signal[NSIG];		/* disposition of signals */
	int	u_utime;			/* this process user time */
	int	u_stime;			/* this process system time */
	int	u_cutime[2];		/* sum of childs' utimes */
	int	u_cstime[2];		/* sum of childs' stimes */
	int	*u_ar0;				/* address of users saved R0 */
	int	u_prof[4];			/* profile arguments */
	char	u_intflg;		/* catch intr from sys */
					/* kernel stack per user
					 * extends from u + USIZE*64
					 * backward not to reach here
					 */
} u;

/* u_error codes */
#define	EFAULT	106	/* exp. transmit data failed. */
#define	EPERM	1	/* user is not the superuser */
#define	ENOENT	2	/* specify file is not exist */
#define	ESRCH	3	/* signal's target process is not exist, or is terminated. */
#define	EINTR	4	/* signal is processed during the system call process */
#define	EIO	5		/* I/0 error */
#define	ENXIO	6	/* device is not exist */
#define	E2BIG	7	/* use system call exec to transmit the parameter bigger than 512 bytes */
#define	ENOEXEC	8	/* unrecognized format with executor(magic number) */
#define	EBADF	9	/* try to operate the unopenned file */
#define	ECHILD	10	/* cannot find the child process when excute system call wait */
#define	EAGAIN	11	/* cannot find the empty elements in proc[], when excute system call fork*/
#define	ENOMEM	12	/* try to allocate the more memory of the available capacity to process*/
#define	EACCES	13	/* have no permission to access file or directory */
#define	ENOTBLK	15	/* the file is not represented block file */
#define	EBUSY	16	/* the hanging point as an object is busy when excute system call(mount, umount) */
#define	EEXIST	17	/* the file is exist when excute system call link */
#define	EXDEV	18	/* try to link the file of other devices */
#define	ENODEV	19	/* the device is not exist */
#define	ENOTDIR	20	/* not a directory */
#define	EISDIR	21	/* try to write in directory */
#define	EINVAL	22	/* invallid parameter */
#define	ENFILE	23	/* file[] overflow */
#define	EMFILE	24	/* user.u_ofile[] overflow */
#define	ENOTTY	25	/* the file is not represented terminal */
#define	ETXTBSY	26	/* txt file is busy */
#define	EFBIG	27	/* file too big */
#define	ENOSPC	28	/* block device have not enough capacity */
#define	ESPIPE	29	/* excute system call seek to pipe file */
#define	EROFS	30	/* try to update the file or directory on read-only block device */
#define	EMLINK	31	/* too many links to the file */
#define	EPIPE	32	/* pipe file is damaged */
