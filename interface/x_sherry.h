#ifndef ___X_SRY_H_
#define ___X_SRY_H_

extern void x_sry_wrdt_apply(uint8 *data, int len);
extern void CloseSryWindow(void);
extern int OpenSryWindow(char *opts);
extern void x_sry_redraw_ctl(int);
extern void x_sry_close(void);
extern void x_sry_update(void);
extern void x_sry_event(void);

#endif /* ___X_SRY_H_ */
