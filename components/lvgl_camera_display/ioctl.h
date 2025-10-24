#define _IOC_WRITE          1U
#endif

#ifndef _IOC_READ
#define _IOC_READ           2U
#endif

#define _IOC_NRMASK         ((1 << _IOC_NRBITS) - 1)
#define _IOC_TYPEMASK       ((1 << _IOC_TYPEBITS) - 1)
#define _IOC_SIZEMASK       ((1 << _IOC_SIZEBITS) - 1)
#define _IOC_DIRMASK        ((1 << _IOC_DIRBITS) - 1)

#define _IOC_NRSHIFT        0
#define _IOC_TYPESHIFT      (_IOC_NRSHIFT+_IOC_NRBITS)
#define _IOC_SIZESHIFT      (_IOC_TYPESHIFT+_IOC_TYPEBITS)
#define _IOC_DIRSHIFT       (_IOC_SIZESHIFT+_IOC_SIZEBITS)

#define _IOC(dir,type,nr,size) \
    (((dir)  << _IOC_DIRSHIFT) | \
     ((type) << _IOC_TYPESHIFT) | \
     ((nr)   << _IOC_NRSHIFT) | \
     ((size) << _IOC_SIZESHIFT))

#define _IOC_TYPECHECK(t) (sizeof(t))

#ifndef _IO
#define _IO(type,nr)        _IOC(_IOC_NONE,(type),(nr),0)
#endif

#ifndef _IOR
#define _IOR(type,nr,size)  _IOC(_IOC_READ,(type),(nr),(_IOC_TYPECHECK(size)))
#endif

#ifndef _IOW
#define _IOW(type,nr,size)  _IOC(_IOC_WRITE,(type),(nr),(_IOC_TYPECHECK(size)))
#endif

#ifndef _IOWR
#define _IOWR(type,nr,size) _IOC(_IOC_READ|_IOC_WRITE,(type),(nr),(_IOC_TYPECHECK(size)))
#endif

#ifdef __cplusplus
}
#endif
