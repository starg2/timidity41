#ifndef ___EXPLODE_H_
#define ___EXPLODE_H_

typedef struct _ExplodeHandler *ExplodeHandler;

enum explode_method_t
{
    EXPLODE_LIT8,
    EXPLODE_LIT4,
    EXPLODE_NOLIT8,
    EXPLODE_NOLIT4
};

extern ExplodeHandler open_explode_handler(
	long (* read_func)(char *buf, long size, void *user_val),
	int method,
	long compsize, long origsize,
	void *user_val);

extern long explode(ExplodeHandler decoder,
		    char *decode_buff,
		    long decode_buff_size);

extern void close_explode_handler(ExplodeHandler decoder);


#endif /* ___EXPLODE_H_ */
