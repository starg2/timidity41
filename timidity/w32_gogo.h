#ifdef __W32_GOGO_H__
#define __W32_GOGO_H__

extern MERET MPGE_initializeWork(void);
extern MERET MPGE_terminateWork(void);
extern MERET MPGE_setConfigure(MPARAM mode, UPARAM dwPara1, UPARAM dwPara2 );
extern MERET MPGE_getConfigure(MPARAM mode, void *para1 );
extern MERET MPGE_detectConfigure(void);
extern MERET MPGE_processFrame(void);
extern MERET MPGE_closeCoder(void);
extern MERET MPGE_endCoder(void);
extern MERET MPGE_getVersion( unsigned long *vercode,  char *verstring );
extern MERET MPGE_getUnitStates( unsigned long *unit);
extern int MPGE_available;

extern int gogo_dll_check(void);

#endif /* __W32_GOGO_H__ */
