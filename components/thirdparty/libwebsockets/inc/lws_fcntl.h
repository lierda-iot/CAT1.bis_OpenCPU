
#ifndef	_LWS__DEFAULT_FCNTL_H_
#ifdef __cplusplus
extern "C" {
#endif
#define	_LWS__DEFAULT_FCNTL_H_

/*
 * Flag values for open(2) and fcntl(2)
 * The kernel adds 1 to the open modes to turn it into some
 * combination of FREAD and FWRITE.
 */
#ifndef O_RDONLY
#define	O_RDONLY	0		/* +1 == FREAD */
#endif

#ifndef O_WRONLY
#define	O_WRONLY	1		/* +1 == FWRITE */
#endif

#ifndef O_RDWR
#define	O_RDWR		2		/* +1 == FREAD|FWRITE */
#endif

#ifndef O_APPEND
#define	O_APPEND	0x0008
#endif

#ifndef O_CREAT
#define	O_CREAT		0x0200
#endif

#ifndef O_TRUNC
#define	O_TRUNC		0x0200
#endif

#ifndef O_EXCL
#define	O_EXCL		0x0800
#endif

#ifndef O_SYNC
#define O_SYNC		0x2000
#endif

/*	O_NDELAY	_FNDELAY 	set in include/fcntl.h */
/*	O_NDELAY	_FNBIO 		set in include/fcntl.h */
#ifndef O_NONBLOCK
#define	O_NONBLOCK	0x4000
#endif

#ifndef O_NONBLOCK
#define	O_NOCTTY	0x8000
#endif

#ifndef O_ACCMODE
#define	O_ACCMODE	(O_RDONLY|O_WRONLY|O_RDWR)
#endif

#ifdef __cplusplus
}
#endif
#endif	/* !_LWS__DEFAULT_FCNTL_H_ */
