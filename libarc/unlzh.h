#ifndef ___LZH_H_
#define ___LZH_H_

typedef struct _UNLZHHandler *UNLZHHandler;

extern UNLZHHandler open_unlzh_handler(long (* read_func)(char*,long,void*),
				       const char *method,
				       long compsize, long origsize,
				       void *user_val);
extern long unlzh(UNLZHHandler decoder, char *buff, long buff_size);
extern void close_unlzh_handler(UNLZHHandler decoder);

extern char *lzh_methods[];

#endif /* ___LZH_H_ */
