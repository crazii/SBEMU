//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2012 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: routines in newfunc.lib

#ifndef mpxplay_newfunc_h
#define mpxplay_newfunc_h

#include <stdio.h>
#include <stddef.h> // for offsetof

#ifdef __WATCOMC__
 #define NEWFUNC_ASM 1
#endif

#ifdef _WIN64
#define MPXPLAY_ARCH_X64 1
#elif defined(NEWFUNC_ASM)
#define MPXPLAY_NEWFUNC_X86_ASM 1
#endif
#if defined(WIN32) || defined(__WINDOWS_386__) || defined(__NT__) || defined(_WIN32) || defined(_WIN64)
#define MPXPLAY_WIN32 1
#endif
#ifdef MPXPLAY_WIN32
 #define MPXPLAY_FSIZE64 1
 //#define MPXPLAY_FILEDATES_ALL 1 // use created and accessed dates too
 #define MPXPLAY_UTF8 1
 #if defined(MPXPLAY_GUI_QT)
  #define MPXPLAY_USE_SMP  1  // use only CPU 0 (disable other CPU cores / threads) (it also can be controlled by mpxplay_programcontrol)
  // #define MPXPLAY_THREADS_HYPERTREADING_DISABLE 1 // disable HT on Core0 by default (it also can be controlled by mpxplay_programcontrol)
  // #define MPXPLAY_USE_IXCH_SMP 0  // enable InterlockedExhange functions -> it causes more problem than it solves, do not enable
 #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MPXPLAY_UTF8
#define MAX_PATHNAMELEN 300*3
#define MAX_PATHNAMEU08 MAX_PATHNAMELEN // utf8
#define MAX_PATHNAMEU16 300             // utf16
#define MAX_STRLEN      2048            // in bytes
#else
#define MAX_PATHNAMELEN 300
#define MAX_PATHNAMEU08 MAX_PATHNAMELEN*3
#define MAX_STRLEN      1024
#endif

typedef unsigned int   mpxp_bool_t;
typedef long long      mpxp_int64_t;
typedef unsigned long long mpxp_uint64_t;
typedef long           mpxp_int32_t;
typedef unsigned long  mpxp_uint32_t;
typedef short          mpxp_int16_t;
typedef unsigned short mpxp_uint16_t;
typedef signed char    mpxp_int8_t;
typedef unsigned char  mpxp_uint8_t;
typedef float          mpxp_float_t;
typedef double         mpxp_double_t;
#ifdef MPXPLAY_UTF8
typedef mpxp_uint8_t   mpxp_char_t;
typedef mpxp_uint16_t  mpxp_wchar_t;
#else
typedef char           mpxp_char_t;
typedef char           mpxp_wchar_t;
#endif
#ifdef MPXPLAY_ARCH_X64
typedef mpxp_int64_t  mpxp_ptrsize_t;
#else
typedef int           mpxp_ptrsize_t;
#endif
#ifdef MPXPLAY_FSIZE64
 typedef mpxp_int64_t  mpxp_filesize_t;
 #define __WATCOM_INT64__ 1
#else
 typedef long          mpxp_filesize_t;
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef offsetof
#define offsetof(T, F) ((unsigned int)((char *)&((T *)0)->F))
#endif

#ifndef max
#define max(a,b) (((a)>(b))? (a):(b))
#endif

#ifndef min
#define min(a,b) (((a)<(b))? (a):(b))
#endif

#if defined(SBEMU) && defined(DJGPP)
#include <pc.h>
#define inpd inportl
#define outpd outportl
#endif

#if defined(NEWFUNC_ASM) && defined(__WATCOMC__)
 typedef char mpxp_float80_t[10];
 void pds_float80_put(mpxp_float80_t *ptr,double val);
 #pragma aux pds_float80_put parm[esi][8087] = "fstp tbyte ptr [esi]"
 float pds_float80_get(mpxp_float80_t *ptr);
 #pragma aux pds_float80_get parm[esi] value[8087] = "fld tbyte ptr [esi]"
 mpxp_uint32_t pds_bswap16(mpxp_uint32_t);
 #pragma aux pds_bswap16 parm [eax] value [eax] = "xchg al,ah"
 mpxp_uint32_t pds_bswap32(mpxp_uint32_t);
 #if (_CPU_ >= 486) || (_M_IX86>=400)
  #pragma aux pds_bswap32 parm [eax] value [eax] = "bswap eax"
 #else
  #pragma aux pds_bswap32 parm [eax] value [eax] = "xchg al,ah" "rol eax,16" "xchg al,ah"
 #endif
 void pds_cpu_hlt(void);
 #pragma aux pds_cpu_hlt ="hlt"
#else
 typedef long double mpxp_float80_t; // !!! 64bit in WATCOMC, 96bit in GNU
 #define pds_float80_put(p,v) *(p)=(v)
 #define pds_float80_get(p)   (*((mpxp_float80_t *)(p)))
 #define pds_bswap16(a) ((((a)&0xff)<<8)|(((a)&0xff00)>>8))
 #define pds_bswap32(a) ((((a)&0xff)<<24)|(((a)&0xff00)<<8)|(((a)&0xff0000)>>8)|(((a)>>24)&0xff))
 #define pds_cpu_hlt
#endif

#if defined(__GNUC__) || defined(DJGPP)
 #define pds_swapv32(a) __asm__ ( "bswapl %0\n" : "=r" (a) : "0" (a) )
#elif defined (_WIN32) && !defined(_WIN32_WCE)
 #define pds_swapv32(a) __asm mov eax,a __asm bswap eax __asm mov a, eax
#endif
/*void pds_ftoi(mpxp_double_t,mpxp_int32_t *); // rather use -zro option at wcc386
 #pragma aux pds_ftoi parm [8087][esi] = "fistp dword ptr [esi]"
 void pds_fto64i(mpxp_double_t,mpxp_int64_t *);
 #pragma aux pds_fto64i parm [8087][esi] = "fistp qword ptr [esi]"*/
#define pds_ftoi(ff,ii)   (*(ii)=(mpxp_int32_t)(ff))
#define pds_fto64i(ff,ii) (*(ii)=(mpxp_int64_t)(ff))

#define funcbit_test(var,bit)       ((var)&(bit))
#define funcbit_enable(var,bit)     ((var)|=(bit))
#define funcbit_disable(var,bit)    ((var)&=~(bit))
#define funcbit_inverse(var,bit)    ((var)^=(bit))
//#define funcbit_copy(var1,var2,bit) ((var1)|=(var2)&(bit))
#define funcbit_copy(var1,var2,bit) ((var1)=((var1)&(~(bit)))|((var2)&(bit)))

// note LE: lowest byte first, highest byte last
#define PDS_GETB_8S(p)   *((mpxp_int8_t *)(p))               // signed 8 bit (1 byte)
#define PDS_GETB_8U(p)   *((mpxp_uint8_t *)(p))              // unsigned 8 bit (1 byte)
#define PDS_GETB_LE16(p) *((mpxp_int16_t *)(p))              // 2bytes LE to short
#define PDS_GETB_LEU16(p)*((mpxp_uint16_t *)(p))             // 2bytes LE to unsigned short
#define PDS_GETB_BE16(p) pds_bswap16(*((mpxp_uint16_t *)(p)))// 2bytes BE to unsigned short
#define PDS_GETB_LE32(p) *((mpxp_int32_t *)(p))              // 4bytes LE to long
#define PDS_GETB_LEU32(p) *((mpxp_uint32_t *)(p))            // 4bytes LE to unsigned long
#define PDS_GETB_BE32(p) pds_bswap32(*((mpxp_uint32_t *)(p)))// 4bytes BE to unsigned long
#define PDS_GETB_LE24(p) ((PDS_GETB_LEU32(p))&0x00ffffff)
#define PDS_GETB_BE24(p) ((PDS_GETB_BE32(p))>>8)
#define PDS_GETB_LE64(p) *((mpxp_int64_t *)(p))              // 8bytes LE to int64
#define PDS_GETB_LEU64(p) *((mpxp_uint64_t *)(p))            // 8bytes LE to uint64
#define PDS_GETB_BEU64(p) ((((mpxp_uint64_t)PDS_GETB_BE32(p))<<32)|((mpxp_uint64_t)PDS_GETB_BE32(((mpxp_uint8_t *)(p)+4))))
#define PDS_GETBD_BEU64(d,p) *(((mpxp_uint32_t *)(d))+1)=PDS_GETB_BE32(p); *((mpxp_uint32_t *)(d))=PDS_GETB_BE32(((mpxp_uint32_t *)(p))+1)
#define PDS_GET4C_LE32(a,b,c,d) ((mpxp_uint32_t)(a) | ((mpxp_uint32_t)(b) << 8) | ((mpxp_uint32_t)(c) << 16) | ((mpxp_uint32_t)(d) << 24))
#define PDS_GETS_LE32(p) ((char *)&(p))                    // unsigned long to 4 bytes string

#define PDS_PUTB_8S(p,v)   *((mpxp_int8_t *)(p))=(v)               //
#define PDS_PUTB_8U(p,v)   *((mpxp_uint8_t *)(p))=(v)              //
#define PDS_PUTB_LE16(p,v) *((mpxp_int16_t *)(p))=(v)              //
#define PDS_PUTB_LEU16(p,v) *((mpxp_uint16_t *)(p))=(v)            //
#define PDS_PUTB_BEU16(p,v) *((mpxp_uint16_t *)(p))=pds_bswap16((v))//
#define PDS_PUTB_LE24(p,v) *((mpxp_uint8_t *)(p))=((v)&0xff); PDS_PUTB_LE16(((mpxp_uint8_t*)p+1),((v)>>8))
#define PDS_PUTB_LE32(p,v) *((mpxp_int32_t *)(p))=(v)              // long to 4bytes LE
#define PDS_PUTB_BEU32(p,v) *((mpxp_uint32_t *)(p))=pds_bswap32((v)) // long to 4bytes BE
#define PDS_PUTB_LE64(p,v) *((mpxp_int64_t *)(p))=(v)              // int64 to 8bytes LE
#define PDS_PUTB_BEU64(p,v) *((mpxp_uint32_t *)(p)+1)=pds_bswap32((v)&0xffffffff); *((mpxp_uint32_t *)(p))=pds_bswap32((mpxp_uint64_t)(v)>>32)

#define pds_newfunc_regp_clear(regp) pds_memset(regp,0,sizeof(union REGPACK))
#define pds_newfunc_regs_clear(regs) pds_memset(regs,0,sizeof(union REGS))
#define pds_newfunc_sregs_clear(sregs) pds_memset(sregs,0,sizeof(struct SREGS))

#define TEXTCURSORSHAPE_NORMAL  0x0607
#define TEXTCURSORSHAPE_FULLBOX 0x0007
#define TEXTCURSORSHAPE_HIDDEN  0x2000

#define PDS_DIRECTORY_SEPARATOR_CHAR_DOSWIN '\\'
#define PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP '/'
#define PDS_DIRECTORY_SEPARATOR_STR_DOSWIN  "\\"
#define PDS_DIRECTORY_SEPARATOR_STR_UNXFTP  "/"

#if defined(__DOS__) || defined(WIN32) || defined(_WIN32) || defined(MPXPLAY_WIN32)
 #define PDS_DIRECTORY_SEPARATOR_CHAR PDS_DIRECTORY_SEPARATOR_CHAR_DOSWIN
 #define PDS_DIRECTORY_SEPARATOR_CHRO PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP
 #define PDS_DIRECTORY_SEPARATOR_STR  PDS_DIRECTORY_SEPARATOR_STR_DOSWIN
 #define PDS_DIRECTORY_ROOTDIR_STR "c:\\"
#else // Linux
 #define PDS_DIRECTORY_SEPARATOR_CHAR PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP
 #define PDS_DIRECTORY_SEPARATOR_CHRO PDS_DIRECTORY_SEPARATOR_CHAR_DOSWIN
 #define PDS_DIRECTORY_SEPARATOR_STR  PDS_DIRECTORY_SEPARATOR_STR_UNXFTP
 #define PDS_DIRECTORY_ROOTDIR_STR "c:/"
#endif

#define PDS_DIRECTORY_DRIVESTRLENMAX 10 // have to be enough ("ftpes:" = 6)
#define PDS_DIRECTORY_DRIVE_STR   "c:"
#define PDS_DIRECTORY_ALLDIR_STR  "*.*"
#define PDS_DIRECTORY_ALLFILE_STR "*.*" // supported (media) files
#define PDS_DIRECTORY_ALLFILES_STR "*.?*" // all files (not only media)

//uselfn
#define USELFN_ENABLED         1 // lfn is enabled
#define USELFN_AUTO_SFN_TO_LFN 2 // auto sfn to lfn conversion at playlists

typedef struct dosmem_t{
 unsigned short selector;
 unsigned short segment;
 char *linearptr;
}dosmem_t;

#if defined(DJGPP)
typedef struct xmsmem_t{
 unsigned short remap;
 unsigned short xms;
 unsigned short handle;
 char *physicalptr;
 char *linearptr;
}xmsmem_t;
typedef xmsmem_t cardmem_t;
#define pds_cardmem_physicalptr(cardmem, ptr) ((cardmem)->physicalptr + ((char*)(ptr) - (cardmem)->linearptr))
#else
typedef dosmem_t cardmem_t;
#define pds_cardmem_physicalptr(cardmem, ptr) (ptr)
#endif

//for DOS interrupt callings
typedef struct rminfo{
 long EDI;
 long ESI;
 long EBP;
 long reserved_by_system;
 long EBX;
 long EDX;
 long ECX;
 long EAX;
 short flags;
 short ES,DS,FS,GS,IP,CS,SP,SS;
}rminfo;

#define RMINFO_FLAG_CARRY 1

typedef struct pds_fdate_t{
 unsigned short twosecs : 5;
 unsigned short minutes : 6;
 unsigned short hours   : 5;
 unsigned short day     : 5;
 unsigned short month   : 4;
 unsigned short year    : 7;
}pds_fdate_t;

typedef struct pds_find_t{
 void *ff_data;            // lfn_find_t or pds_dos_find_t
 mpxp_ptrsize_t ff_handler;// _findfirst
 void *drive_data;         // mpxplay_diskdrive_data_s
 unsigned int attrib;      // attribute byte for file
 struct pds_fdate_t cdate; // creation date/time
 struct pds_fdate_t fdate; // date of last write to file (modification time)
 struct pds_fdate_t adate; // access date/time
 mpxp_filesize_t size;     // filesize
 char name[MAX_PATHNAMELEN]; // found file name
}pds_find_t;

typedef struct pds_subdirscan_t{
 struct pds_find_t *ff;    // for finding files in current dir
 struct pds_find_t *subdir_ff_datas;  // for finding next (sub)dir
 char **subdir_names;                 // saved subdir names (to rebuild currdir at level change)
 char **subdir_masks;                 // non constant directory names given in startdir
 unsigned int nb_subdirmasks;
 int subdir_level,subdir_reach,subfile_reach;
 unsigned int flags;       // DIRSCAN_FLAG_
 unsigned int scan_attrib; // argument
 char subdirmasks[MAX_PATHNAMELEN];    // subdir_masks are pointed here
 char startdir[MAX_PATHNAMELEN];       // argument
 char scan_names[MAX_PATHNAMELEN];     // argument (ie: *.*)
 char currdir[MAX_PATHNAMELEN];   // in search
 char prevdir[MAX_PATHNAMELEN];
 char fullname[MAX_PATHNAMELEN];  // currdir + ff->name
}pds_subdirscan_t;

//newfunc.c
extern void newfunc_init(char *);
extern void newfunc_close(void);

//bitstrm.c
typedef struct mpxplay_bitstreambuf_s{
 unsigned long bitpos;     // we assume that we never store more than 4Gbits
 unsigned long storedbits; // detto
 unsigned char *buffer;    // begin of buffer
 unsigned long bufsize;
}mpxplay_bitstreambuf_s;

extern struct mpxplay_bitstreambuf_s *mpxplay_bitstream_alloc(unsigned int required_bufsize);
extern void mpxplay_bitstream_free(struct mpxplay_bitstreambuf_s *bs);
extern void mpxplay_bitstream_init(struct mpxplay_bitstreambuf_s *bs,unsigned char *data,unsigned int bytes);
extern void mpxplay_bitstream_consolidate(struct mpxplay_bitstreambuf_s *bs,unsigned int do_bytealign);
//extern int  mpxplay_bitstream_fill(struct mpxplay_bitstreambuf_s *bs,struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,unsigned int needbytes); // fill with a file-read
extern unsigned int mpxplay_bitstream_putbytes(struct mpxplay_bitstreambuf_s *bs,unsigned char *srcbuf,unsigned int newbytes); // fill with (new) bytes from a buffer
extern void mpxplay_bitstream_reset(struct mpxplay_bitstreambuf_s *bs);

extern unsigned char *mpxplay_bitstream_getbufpos(struct mpxplay_bitstreambuf_s *bs);
extern int  mpxplay_bitstream_lookbytes(struct mpxplay_bitstreambuf_s *bs,unsigned char *destbuf,unsigned int needbytes);
extern int  mpxplay_bitstream_readbytes(struct mpxplay_bitstreambuf_s *bs,unsigned char *destbuf,unsigned int needbytes);
extern int  mpxplay_bitstream_skipbytes(struct mpxplay_bitstreambuf_s *bs,int skipbytes);
extern long mpxplay_bitstream_leftbytes(struct mpxplay_bitstreambuf_s *bs);

extern mpxp_uint32_t mpxplay_bitstream_get_byte(struct mpxplay_bitstreambuf_s *bs);
extern mpxp_uint32_t mpxplay_bitstream_get_le16(struct mpxplay_bitstreambuf_s *bs);
extern mpxp_uint32_t mpxplay_bitstream_get_le32(struct mpxplay_bitstreambuf_s *bs);
extern mpxp_uint64_t mpxplay_bitstream_get_le64(struct mpxplay_bitstreambuf_s *bs);
extern mpxp_uint32_t mpxplay_bitstream_get_be16(struct mpxplay_bitstreambuf_s *bs);
extern mpxp_uint32_t mpxplay_bitstream_get_be32(struct mpxplay_bitstreambuf_s *bs);
extern mpxp_uint64_t mpxplay_bitstream_get_be64(struct mpxplay_bitstreambuf_s *bs);

extern int  mpxplay_bitstream_getbit1_be(struct mpxplay_bitstreambuf_s *bs);
extern long mpxplay_bitstream_getbits_be24(struct mpxplay_bitstreambuf_s *bs,unsigned int bits);
extern mpxp_uint32_t mpxplay_bitstream_getbits_ube32(struct mpxplay_bitstreambuf_s *bs,unsigned int bits);
extern mpxp_int64_t mpxplay_bitstream_getbits_be64(struct mpxplay_bitstreambuf_s *bs,unsigned int bits);
extern int mpxplay_bitstream_getbit1_le(struct mpxplay_bitstreambuf_s *bs);
extern long mpxplay_bitstream_getbits_le24(struct mpxplay_bitstreambuf_s *bs,unsigned int bits);
extern mpxp_int64_t mpxplay_bitstream_getbits_le64(struct mpxplay_bitstreambuf_s *bs,unsigned int bits);
extern int  mpxplay_bitstream_skipbits(struct mpxplay_bitstreambuf_s *bs,int bits); // bits can be negative, return MPXPLAY_ERROR_MPXINBUF_nnn
extern long mpxplay_bitstream_leftbits(struct mpxplay_bitstreambuf_s *bs);

//cpu.c
#define CPU_X86_STDCAP_MSR     (1<< 5) // Model-Specific Registers, RDMSR, WRMSR
#define CPU_X86_STDCAP_MTRR    (1<<12) // Memory Type Range Registers
#define CPU_X86_STDCAP_PGE     (1<<13) // Page Global Enable
#define CPU_X86_STDCAP_PAT     (1<<16) // Page Attribute Table
#define CPU_X86_STDCAP_MMX     (1<<23)
#define CPU_X86_STDCAP_SSE1    (1<<25)
#define CPU_X86_STDCAP_SSE2    (1<<26)
#define CPU_X86_EXTCAP_MMXEXT  (1<<23) // ??? 22 or 23
#define CPU_X86_EXTCAP_3DNOW2  (1<<30)
#define CPU_X86_EXTCAP_3DNOW1  (1<<31)
#define CPU_X86_MMCAP_MMX      0x0001 // standard MMX
#define CPU_X86_MMCAP_MMXEXT   0x0002 // SSE integer functions or AMD MMX ext
#define CPU_X86_MMCAP_3DNOW    0x0004 // AMD 3DNOW
#define CPU_X86_MMCAP_SSE      0x0008 // SSE functions
#define CPU_X86_MMCAP_SSE2     0x0010 // PIV SSE2 functions
#define CPU_X86_MMCAP_3DNOWEXT 0x0020 // AMD 3DNowExt

extern int mpxplay_cpu_capables_std,mpxplay_cpu_capables_ext,mpxplay_cpu_capables_mm;
extern void newfunc_cpu_init(void);
extern unsigned int pds_cpu_cpuid_test(void);
extern int pds_cpu_cpuid_get(unsigned int eax_p,int *ebx_p,int *ecx_p,int *edx_p);
extern unsigned int pds_cpu_mtrr_enable_wc(unsigned long phys_ptr,unsigned long size_kb);
extern void pds_cpu_mtrr_disable_wc(unsigned long phys_ptr);
extern void pds_fpu_setround_near(void);
extern void pds_fpu_setround_chop(void);

//dll_load.c
#ifndef SBEMU
#include "dll_load.h"
extern mpxplay_module_entry_s *newfunc_dllload_getmodule(unsigned long moduletype_major,unsigned long moduletype_minor,char *modulename_minor,mpxplay_module_entry_s *prev_module);
extern unsigned int newfunc_dllload_reloadmodule(mpxplay_module_entry_s *module);
extern unsigned int newfunc_dllload_disablemodule(unsigned long moduletype_major,unsigned long moduletype_minor,char *modulename_minor,mpxplay_module_entry_s *module);
//extern void newfunc_dllload_unloadmodule(unsigned long moduletype_major,unsigned long moduletype_minor,char *modulename_minor,mpxplay_module_entry_s *module);
//extern void newfunc_dllload_keepmodule(unsigned long moduletype_major,unsigned long moduletype_minor,char *modulename_minor,mpxplay_module_entry_s *module);
extern void newfunc_dllload_list_dlls(void);
extern void newfunc_dllload_closeall(void);
#ifdef MPXPLAY_WIN32
#define PDS_WIN32DLLCALLFUNC_RETTYPE_VOID   0 // void
#define PDS_WIN32DLLCALLFUNC_RETTYPE_INT32  1 // mpxp_int32_t
#define PDS_WIN32DLLCALLFUNC_RETTYPE_PTR    2 // mpxp_ptrsize_t
#define PDS_WIN32DLLCALLFUNC_RETTYPE_MASK   3
#define PDS_WIN32DLLCALLFUNC_ARG2TYPE_NUM   4 // 2. argument is number (1st is ctx at SSL_CTX_set_options)
#define PDS_WIN32DLLCALLFUNC_OPTIONAL       256 // function is optional, don't reject dll
typedef struct pds_win32dllcallfunc_t{
 char *funcname;
 void *funcptr;
 unsigned int argnum;
 unsigned int infobits;
}pds_win32dllcallfunc_t;
extern mpxp_ptrsize_t newfunc_dllload_winlib_load(char *libname);
extern void newfunc_dllload_winlib_close(mpxp_ptrsize_t dllhandle);
extern unsigned int newfunc_dllload_winlib_getfuncs(mpxp_ptrsize_t dllhandle,struct pds_win32dllcallfunc_t *funcs);
extern unsigned int newfunc_dllload_kernel32_init(void);
#define MPXPLAY_KERNEL32FUNC_GETCONSOLEWINDOW 0
extern mpxp_ptrsize_t newfunc_dllload_kernel32_call(unsigned int funcnum,mpxp_ptrsize_t *args,unsigned int argnum);
extern mpxp_ptrsize_t newfunc_dllload_call_proc_stackbased_argn(struct pds_win32dllcallfunc_t *func,mpxp_ptrsize_t *args,unsigned int argnum);
extern mpxp_ptrsize_t newfunc_dllload_winlib_callfunc(struct pds_win32dllcallfunc_t *func,void *data1,void *data2,void *data3);
#if defined(MPXPLAY_GUI_QT) || defined(MPXPLAY_LINK_ORIGINAL_FFMPEG)
#if defined(__GNUC__)
#include <sys/time.h>
#endif
extern time_t pds_mkgmtime(struct tm *t_in);
#endif // defined(MPXPLAY_GUI_QT) || defined(MPXPLAY_LINK_ORIGINAL_FFMPEG)
#endif // MPXPLAY_WIN32
#endif

//dpmi.c
#ifndef MPXPLAY_WIN32

#if defined(DJGPP)
#include <sys/segments.h>
#define far
#define __far
#define __interrupt
#define __loadds
typedef struct _farptr {unsigned long off;unsigned short sel;} farptr;//DJGPP has no 16:32 flat model far ptr.
typedef farptr int_handler_t;
static farptr pds_fardata(void* d) { farptr ptr = {(long)d, _my_ds()}; return ptr;}
static void pds_call_int_handler(int_handler_t h) { asm("pushf\n\t lcall *%0" ::"m"(h));} //simulate a iret frame for handler
static int_handler_t pds_int_handler(void (*f)(void)) { int_handler_t h = {(long)f, _my_cs()}; return h;}
static int pds_valid_int_handler(int_handler_t h) {return h.off && h.sel;}
#else
typedef void* far farptr;
typedef void (__far __interrupt *int_handler_t)();
#define pds_fardata(d) (d)
static void pds_call_int_handler(int_handler_t h) {h();}
#define pds_int_handler(f) (f)
#define pds_valid_int_handler(h) (h)
#endif//DJGPP

#define PDS_INT2X_DOSMEM_SIZE  512
extern long pds_dpmi_segment_to_selector(unsigned int segment);
extern void far *pds_dpmi_getrmvect(unsigned int intno);
extern void pds_dpmi_setrmvect(unsigned int intno, unsigned int segment,unsigned int offset);
#if defined(DJGPP)
extern farptr pds_dpmi_getexcvect(unsigned int intno);
extern void pds_dpmi_setexcvect(unsigned int intno, farptr vect);
extern farptr pds_dos_getvect(unsigned int intno);
extern void pds_dos_setvect(unsigned int intno, farptr vect);

extern int  pds_dpmi_xms_allocmem(xmsmem_t *,unsigned int size);
extern void pds_dpmi_xms_freemem(xmsmem_t *);
#else
extern void far *pds_dpmi_getexcvect(unsigned int intno);
extern void pds_dpmi_setexcvect(unsigned int intno, void far *vect);
extern void far *pds_dos_getvect(unsigned int intno);
extern void pds_dos_setvect(unsigned int intno, void far *vect);
#endif
extern int  pds_dpmi_dos_allocmem(dosmem_t *,unsigned int size);
extern void pds_dpmi_dos_freemem(dosmem_t *);
extern void pds_dpmi_realmodeint_call(unsigned int intnum,struct rminfo *rmi);
extern unsigned long pds_dpmi_map_physical_memory(unsigned long phys_addr,unsigned long memsize);
extern void pds_dpmi_unmap_physycal_memory(unsigned long linear_address);
#define pds_dpmi_rmi_clear(rmi) pds_memset(rmi,0,sizeof(struct rminfo))
#endif // MPXPLAY_WIN32

//drivehnd.c
#define DRIVE_TYPE_NONE    0 // compatible values with win32 GetDriveType()
#define DRIVE_TYPE_NOROOT  1
#define DRIVE_TYPE_FLOPPY  2 // removable
#define DRIVE_TYPE_HD      3
#define DRIVE_TYPE_NETWORK 4 // remote
#define DRIVE_TYPE_CD      5
#define DRIVE_TYPE_RAMDISK 6
#define DRIVE_TYPE_VIRTUAL 32 // far remote
#define SUBDIRSCAN_FLAG_SUBDIR  1 // we was not here yet
#define SUBDIRSCAN_FLAG_UPDIR   2 //
extern unsigned int pds_int2x_dosmems_allocate(void);
extern void pds_int2x_dosmems_free(void);
extern void pds_check_lfnapi(char *prgname);
//extern void pds_dir_save(unsigned int *savedrive,char *savedir);
//extern void pds_dir_restore(unsigned int savedrive,char *savedir);
extern void pds_fullpath(char *fullname,char *name);
extern void pds_getcwd(char *pathbuf); // pathbuf must be min. MAX_PATHNAMELEN chars long
extern void pds_getdcwd(int drive,char *pathbuf); // pathbuf must be min. MAX_PATHNAMELEN chars long
extern int  pds_chdir(char *setdir);
extern int  pds_mkdir(char *newdirname);
extern int  pds_rmdir(char *dirname);
extern int  pds_rename(char *oldfilename,char *newfilename);
extern int  pds_unlink(char *filename);
extern int  pds_fileattrib_reset(char *filename,unsigned int clearflags);
extern int  pds_fileattrib_get(char *filename);
extern unsigned int pds_findfirst(char *path,int attrib,struct pds_find_t *ffblk);
extern unsigned int pds_findnext(struct pds_find_t *ffblk);
extern void pds_findclose(struct pds_find_t *ffblk);
extern unsigned int pds_subdirscan_open(char *path_and_filename,unsigned int attrib,struct pds_subdirscan_t *dsi);
extern int pds_subdirscan_findnextfile(struct pds_subdirscan_t *dsi);
extern void pds_subdirscan_close(struct pds_subdirscan_t *dsi);
extern mpxp_filesize_t pds_getfilesize(char *filename);
extern void pds_truename_dos(char *shortname,char *name);
extern unsigned int pds_getdrive(void);
extern void pds_setdrive(int drivenum);
extern void pds_drivesinfo(unsigned int *currdrive,unsigned int *lastdrive);
extern void pds_drive_getvolumelabel(unsigned int drive,char *labelbuf,unsigned int labbuflen);
extern unsigned int pds_drive_getspace(char *drvpath, mpxp_int64_t *free_space, mpxp_int64_t *total_space);
extern unsigned int pds_chkdrive(unsigned int drive);
extern unsigned int pds_dir_exists(char *path);
extern unsigned int pds_network_check(void);
extern int pds_network_query_assign(unsigned int index,char *drvname,unsigned int maxdrvlen,char *volname,unsigned int maxvollen);
extern void pds_drives_flush(void);
extern void pds_sfn_limit(char *fn);
extern int pds_access(char *path,int amode);

//errorhnd.c
extern void newfunc_errorhnd_int24_init(void);
extern void newfunc_error_handlers_init(void);
extern void newfunc_error_handlers_close(void);
extern void newfunc_exception_handlers_close(void);
//extern void pds_mswin_previousinstance_close(void);

//filehand.c
#define MPXPLAY_ERROR_FILEHAND_OK             0
#define MPXPLAY_ERROR_FILEHAND_USERABORT   -200
#define MPXPLAY_ERROR_FILEHAND_MEMORY      -201
#define MPXPLAY_ERROR_FILEHAND_CANTOPEN    -202
#define MPXPLAY_ERROR_FILEHAND_CANTCREATE  -203
#define MPXPLAY_ERROR_FILEHAND_CANTREAD    -204
#define MPXPLAY_ERROR_FILEHAND_CANTWRITE   -205
#define MPXPLAY_ERROR_FILEHAND_CANTSEEK    -206
#define MPXPLAY_ERROR_FILEHAND_READONLY    -209
#define MPXPLAY_ERROR_FILEHAND_DELETE      -210
#define MPXPLAY_ERROR_FILEHAND_RENAME      -211
#define MPXPLAY_ERROR_FILEHAND_REMOVEDIR   -212
#define MPXPLAY_ERROR_FILEHAND_CHANGEATTR  -213
#define MPXPLAY_ERROR_FILEHAND_CANTPERFORM -214
#define MPXPLAY_ERROR_FILEHAND_CANTCOPY    -215
#define MPXPLAY_ERROR_FILEHAND_COPYDIR     -216
#define MPXPLAY_ERROR_FILEHAND_SAMEFILE    -217
#define MPXPLAY_ERROR_FILEHAND_SAMEDIR     -218
#define MPXPLAY_ERROR_FILEHAND_MULTITO1    -219
#define MPXPLAY_ERROR_FILEHAND_SKIPFILE    -220 // not error, just sign

extern void pds_indosflag_init(void);
extern unsigned int pds_indos_flag(void);
extern unsigned int pds_filehand_check_infilehand(void);
extern unsigned int pds_filehand_check_entrance(void);
extern void pds_filehand_lock_entrance(void);
extern void pds_filehand_unlock_entrance(void);
extern int  pds_open_read(char *filename,unsigned int mode);  // mode: O_RDONLY| O_BINARY|O_TEXT
extern int  pds_open_write(char *filename,unsigned int mode); //       O_RDWR| O_BINARY|O_TEXT
extern int  pds_open_create(char *filename,unsigned int mode); //      O_RDWR| O_BINARY|O_TEXT
extern int  pds_dos_read(int,char *,unsigned int);
extern int  pds_dos_write(int,char *,unsigned int);
extern void pds_close(int);
extern mpxp_filesize_t pds_lseek(int,mpxp_filesize_t,int);
extern mpxp_filesize_t pds_tell(int);
extern int  pds_eof(int);
extern mpxp_filesize_t pds_filelength(int);
extern int  pds_chsize(int filehand,mpxp_filesize_t size);
extern FILE *pds_fopen(char *,char *);
extern int pds_fclose(FILE *fp);

//keyboard.c
#ifndef __DOS__
#ifndef SHIFT_PRESSED
 #define SHIFT_PRESSED 0x00000010L
#endif
#ifndef RIGHT_CTRL_PRESSED
 #define RIGHT_CTRL_PRESSED 0x00000004L
#endif
#ifndef RIGHT_ALT_PRESSED
 #define RIGHT_ALT_PRESSED 0x00000001L
#endif
#endif
#ifdef MPXPLAY_UTF8
#define PDS_KEYCODE_GET_EXTKEY(k) (k&0xffff)
#define PDS_KEYCODE_GET_UNICODE(k) (k>>16)
#define PDS_KEYCODE_PUT_MERGE(u,s) (((u)<<16)|(s))
#else
#define PDS_KEYCODE_GET_EXTKEY(k) (k)
#define PDS_KEYCODE_GET_UNICODE(k) (0)
#endif
extern void newfunc_keyboard_init(void);
extern unsigned int pds_kbhit(void);
extern unsigned int pds_keyboard_get_keycode(void);
extern unsigned int pds_keyboard_look_keycode(void);
extern void pds_keyboard_push_keycode(unsigned int newkeycode);
extern unsigned int pds_extgetch(void);
extern unsigned int pds_look_extgetch(void);
extern unsigned int pds_wipeout_by_extkey(unsigned int extkey);
extern void pds_pushkey(unsigned int newextkey);
extern unsigned int newfunc_keyboard_char_to_extkey(char c);
extern char newfunc_keyboard_extkey_to_char(unsigned short extkey);
extern unsigned int newfunc_keyboard_winkey_to_extkey(unsigned int control,unsigned int virtkeycode,char asciicode);

//memory.c
extern void newfunc_memory_init(void);
#ifdef NEWFUNC_ASM
 extern void pds_memset(void *,int,unsigned int);
 extern void pds_qmemreset(void *,unsigned int);
 extern void pds_qmemcpy(void *,void *,unsigned int);
 extern void pds_qmemcpyr(void *,void *,unsigned int);
 #define pds_memcmp(t,s,l)   memcmp(t,s,l)
#else
 #include <string.h>
 #define pds_memset(t,v,l)   memset(t,v,l)
 #define pds_qmemreset(t,l)  memset(t,0,(l)*4))  // TODO: replace pds_qmem functions with normal mem based one everywhere
 #define pds_qmemcpy(t,s,l)  memcpy(t,s,(l)*4)
 #define pds_qmemcpyr(t,s,l) memmove(t,s,(l)*4)
 extern int pds_memcmp(const void *dest_ptr, const void *src_ptr, unsigned int len_bytes);
#endif
extern void pds_memcpy(void *,const void *,unsigned int);
extern void pds_memxch(char *,char *,unsigned int);
extern void pds_mem_reverse(char *addr,unsigned int len);
#ifdef MPXPLAY_USE_IXCH_SMP
 extern void pds_smp_memcpy(char *addr_dest,char *addr_src,unsigned int len);
 extern void pds_smp_memxch(char *addr1,char *addr2,unsigned int len);
 extern void pds_smp_memset(char *addr,int value,unsigned int len);
 #define pds_smp_memrefresh(addr,len) pds_smp_memcpy(addr,addr,len)
#else
 #define pds_smp_memrefresh(addr,len)
 #define pds_smp_memcpy(t,s,l) pds_memcpy(t,s,l)
 #define pds_smp_memxch(t,s,l) pds_memxch(t,s,l)
 #define pds_smp_memset(t,v,l) pds_memset(t,v,l)
#endif
extern void *pds_malloc(unsigned int bufsize);
extern void *pds_calloc(unsigned int nitems,unsigned int itemsize);
extern void *pds_realloc(void *bufptr,unsigned int bufsize);
extern void pds_free(void *bufptr);

//mixed.c
#define MPXPLAY_MSWIN_VERSIONID_XP    0x0501  // 5.1
#define MPXPLAY_MSWIN_VERSIONID_VISTA 0x0600  // 6.0
#define MPXPLAY_MSWIN_VERSIONID_WIN7  0x0601  // 6.1

extern unsigned long pds_rand(int);
extern unsigned int pds_mswin_getver(void);
extern void pds_mswin_setapplicationtitle_utf8(char *);
extern void pds_shutdown_atx(void);
extern void mpxplay_system_opendefaultapplication(char *filename);

//string.c
extern unsigned int pds_strcpy(char *,char *); // returns the length of string!
extern unsigned int pds_strmove(char *dest,char *src); // returns the length of string
extern unsigned int pds_strncpy(char *dest,char *src,unsigned int maxlen); // returns the length of string!
extern unsigned int pds_strcat(char *,char *); // returns the lenght of string!
extern int  pds_strcmp(char *strp1,char *strp2);
extern int  pds_stricmp(char *,char *);
extern unsigned int pds_stri_compare(char *strp1,char *strp2); // returns 1 if equal, else 0 (no pointer check!)
extern int  pds_strricmp(char *,char *);
extern int  pds_strlicmp(char *,char *);
extern int  pds_strncmp(char *,char *,unsigned int);
extern int  pds_strnicmp(char *strp1,char *strp2,unsigned int counter);
extern unsigned int pds_strlen(char *strp);
extern unsigned int pds_strlenc(char *strp,char seek);
//extern unsigned int pds_strlencn(char *strp,char seek,unsigned int maxlen);
extern char *pds_strchr(char *,char);
extern char *pds_strrchr(char *,char);
extern char *pds_strnchr(char *strp,char seek,unsigned int len);
extern char *pds_strstr(char *s1,char *s2);
extern char *pds_strstri(char *s1,char *s2);
extern unsigned int pds_strcutspc(char *src); // returns (new)len
extern void pds_str_url_decode(char *src);
extern unsigned int pds_str_clean(char *str); // returns (new)len
extern void pds_str_conv_forbidden_chars(char *str,char *fromchars,char *tochars);
extern unsigned int pds_str_extendc(char *str,unsigned int newlen,char c); // returns len
extern unsigned int pds_str_fixlenc(char *str,unsigned int newlen,char c); // returns (new)len
extern int pds_str_limitc(char *src, char *dest, unsigned int newlen, char limit_c); // returns (new)len
extern char *pds_str_getwordn(char *str, unsigned int wordcount); // returns pointer
extern unsigned int pds_utf8_str_centerize(char *str,unsigned int maxlen,unsigned int ispath); // returns utf8 (old)len
extern void  pds_listline_slice(char **listparts,char *cutchars,char *listline);
extern void mpxplay_newfunc_string_listline_slice(char **listparts, unsigned int maxparts, char cutchar, char *listline);
extern unsigned int pds_chkstr_letters(char *str);
extern unsigned int pds_chkstr_uppercase(char *str);
extern void pds_str_uppercase(char *str);
extern void pds_str_lowercase(char *str);
extern unsigned int pds_log10(long value);
extern void  pds_ltoa(int,char *);
//extern void  pds_ltoa16(int,char *);
extern long  pds_atol(char *);
extern mpxp_int64_t pds_atoi64(char *);
extern long  pds_atol16(char *);
extern void pds_str_to_hexs(char *src,char *dest,unsigned int destlen);
extern void pds_hexs_to_str(char *src,char *dest,unsigned int destlen);
extern char *pds_getfilename_from_fullname(char *fullname);
extern char *pds_getfilename_any_from_fullname(char *fullname);
extern char *pds_getfilename_noext_from_fullname(char *strout,char *fullname);
extern char *pds_getfilename_any_noext_from_fullname(char *strout,char *fullname);
extern char *pds_getpath_from_fullname(char *path,char *fullname);
extern char *pds_getpath_nowildcard_from_filename(char *path,char *fullname);
extern char *pds_filename_get_extension_from_shortname(char *filename);
extern char *pds_filename_get_extension(char *fullname);
extern unsigned int pds_filename_conv_slashes_to_local(char *filename);
extern unsigned int pds_filename_conv_slashes_to_unxftp(char *filename);
extern void  pds_filename_conv_forbidden_chars(char *filename);
extern int   pds_getdrivenum_from_path(char *path);
extern unsigned int pds_path_is_dir(char *path);
extern unsigned int pds_filename_check_absolutepath(char *path);
extern unsigned int pds_filename_remove_relatives(char *filename);
extern unsigned int pds_filename_build_fullpath(char *destbuf,char *currdir,char *filename);
extern unsigned int pds_filename_assemble_fullname(char *destbuf,char *path,char *name);
extern unsigned int pds_filename_wildcard_chk(char *filename);
extern unsigned int pds_filename_wildcard_cmp(char *fullname,char *mask);

extern unsigned int pds_UTF16_strlen(mpxp_uint16_t *strp);
#ifdef MPXPLAY_UTF8
extern unsigned int pds_str_CP437_to_UTF16LE(mpxp_wchar_t *utf16,char *src,unsigned int dest_buflen);
extern mpxp_wchar_t pds_cvchar_CP437_to_UTF16LE(mpxp_wchar_t c);
extern unsigned int pds_strn_UTF8_to_UTF16LE_u8bl(mpxp_uint16_t *utf16,mpxp_uint8_t *utf8,unsigned int dest_buflen,int dest_strlen,unsigned int *ut8_blen);
extern unsigned int pds_str_UTF8_to_UTF16LE(mpxp_uint16_t *utf16,mpxp_uint8_t *utf8,unsigned int dest_buflen);
extern unsigned int pds_strn_UTF16LE_to_UTF8(mpxp_uint8_t *utf8,mpxp_uint16_t *utf16,unsigned int dest_buflen,int dest_strlen);
extern unsigned int pds_str_UTF16LE_to_UTF8(mpxp_uint8_t *utf8,mpxp_uint16_t *utf16,unsigned int dest_buflen);
extern unsigned int pds_utf8_strlen(mpxp_uint8_t *utf8);
extern unsigned int pds_utf8_strpos(mpxp_uint8_t *utf8,unsigned int pos);
extern int pds_utf8_stricmp(char *str1,char *str2);
extern unsigned int pds_utf8_is_strstri(char *str1,char *str2);
extern unsigned int pds_utf8_filename_wildcard_cmp(mpxp_uint8_t *fullname,mpxp_uint8_t *mask);
extern unsigned int pds_utf8_wildcard_is_strstri(mpxp_uint8_t *str,mpxp_uint8_t *mask);
extern unsigned int pds_utf8_filename_wildcard_rename(mpxp_uint8_t *destname,mpxp_uint8_t *srcname,mpxp_uint8_t *mask);
extern unsigned int pds_wchar_strcpy(mpxp_uint16_t *dest,mpxp_uint16_t *src);
extern unsigned int pds_wchar_strmove(mpxp_wchar_t *dest,mpxp_wchar_t *src);
extern mpxp_wchar_t *pds_wchar_strrchr(mpxp_wchar_t *strp,mpxp_wchar_t seek);
#define pds_wchar_strlen(strp) pds_UTF16_strlen(strp)
#define pds_strlen_mpxnative(strp) pds_utf8_strlen((mpxp_uint8_t *)strp)
#define pds_strpos_mpxnative(strp,pos) pds_utf8_strpos((mpxp_uint8_t *)strp,pos)
#else
#define pds_cvchar_CP437_to_UTF16LE(c) (c)
#define pds_utf8_strlen(utf8) pds_strlen(utf8)
#define pds_utf8_strpos(utf8,pos) (pos)
//#define pds_utf8_strcut(utf8,len) utf8[len]=0
#define pds_utf8_stricmp(str1,str2) pds_stricmp(str1,str2)
#define pds_utf8_is_strstri(str1,str2) pds_strstri(str1,str2)
#define pds_utf8_filename_wildcard_cmp(fullname,mask) pds_filename_wildcard_cmp(fullname,mask)
#define pds_utf8_wildcard_is_strstri(str,mask) pds_wildcard_is_strstri(str,mask)
#define pds_wchar_strlen(str) pds_strlen(str)
#define pds_wchar_strcpy(dest,src) pds_strcpy(dest,src)
#define pds_wchar_strmove(dest,src) pds_strmove(dest,src)
extern unsigned int pds_wildcard_is_strstri(char *str,char *mask);
extern unsigned int pds_filename_wildcard_rename(char *destname,char *srcname,char *mask);
#define pds_utf8_filename_wildcard_cmp(fullname,mask) pds_filename_wildcard_cmp(fullname,mask)
#define pds_utf8_wildcard_is_strstri(str,mask) pds_wildcard_is_strstri(str,mask)
#define pds_utf8_filename_wildcard_rename(destname,srcname,mask) pds_filename_wildcard_rename(destname,srcname,mask)
#define pds_strlen_mpxnative(strp) pds_strlen(strp)
#define pds_strpos_mpxnative(strp,pos) ((pds_strlen(strp) >= pos)? (pds_strlen(strp) - 1) : pos)  // FIXME: a faster way?
#endif

//textdisp.c
#ifdef MPXPLAY_GUI_CONSOLE
#define TEXTSCREEN_DEFAULT_MODE   3 // 80x25
#define TEXTSCREEN_BLOCKS_PER_CHAR 2 // char + color (must be same width)
#ifdef MPXPLAY_UTF8
#define TEXTSCREEN_BYTES_PER_CHAR 4 // wchar + color
#define TEXTSCREEN_BYTES_ATTRIB_POS 2
typedef mpxp_uint32_t mpxp_textbuf_t;
#else
#define TEXTSCREEN_BYTES_PER_CHAR 2 // char + color
#define TEXTSCREEN_BYTES_ATTRIB_POS 1
typedef mpxp_uint16_t mpxp_textbuf_t;
#endif
extern void newfunc_textdisplay_init(void);
extern void newfunc_textdisplay_close(void);
extern void pds_textdisplay_charxy(unsigned int,unsigned int,unsigned int,mpxp_wchar_t);
#define pds_textdisplay_charxybk(color,bkcolor,outx,outy,c) (pds_textdisplay_charxy((bkcolor<<4)|color,outx,outy,c))
extern unsigned int pds_textdisplay_textxy(unsigned int,unsigned int,unsigned int,char *);
#define pds_textdisplay_textxybk(color,bkcolor,outx,outy,string) (pds_textdisplay_textxy((bkcolor<<4)|color,outx,outy,string))
extern unsigned int pds_textdisplay_utf8_textxync(unsigned int color,unsigned int outx,unsigned int outy,char *string_s,unsigned int len,char c);
#define pds_textdisplay_utf8_textxyncbk(color,bkcolor,outx,outy,string,len,c) (pds_textdisplay_utf8_textxync((bkcolor<<4)|color,outx,outy,string,len,c))
extern void pds_textdisplay_wchar_textxyan(unsigned int x,unsigned int y,mpxp_wchar_t *text,unsigned short *attribs,unsigned int len);
extern void pds_textdisplay_clrscr(void);
extern void pds_textdisplay_scrollup(unsigned int);
extern void pds_textdisplay_printf(char *);
extern unsigned int pds_textdisplay_getbkcolorxy(unsigned int,unsigned int);
extern unsigned int pds_textdisplay_lowlevel_getbkcolorxy(unsigned int,unsigned int);
extern void pds_textdisplay_setcolorxy(unsigned int,unsigned int,unsigned int);
extern void pds_textdisplay_setbkcolorxy(unsigned int,unsigned int,unsigned int);
extern void pds_textdisplay_lowlevel_setbkcolorxy(unsigned int,unsigned int,unsigned int);
extern void pds_textdisplay_spacecxyn(unsigned int,unsigned int,unsigned int,unsigned int);
#ifdef MPXPLAY_UTF8
extern void pds_textdisplay_char2buf(unsigned int color,mpxp_textbuf_t *bufptr,char c);
#else
#define pds_textdisplay_char2buf(color,bufptr,c) *(bufptr)=((unsigned short)c)|((unsigned short)color<<8)
#endif
#define pds_textdisplay_char2bufbk(color,bkcolor,bufptr,c) pds_textdisplay_char2buf((color|(bkcolor<<4)),bufptr,c)
extern unsigned int pds_textdisplay_utf8_text2bufn(unsigned int color,mpxp_textbuf_t *buf,char *string_s,unsigned int maxlen);
#define pds_textdisplay_utf8_text2bufnbk(color,bkcolor,buf,string,len) (pds_textdisplay_utf8_text2bufn(((bkcolor<<4)|color),buf,string,len))
extern unsigned int pds_textdisplay_text2buf(unsigned int color,mpxp_textbuf_t *buf,char *string_s,unsigned int len);
#define pds_textdisplay_text2bufbk(color,bkcolor,buf,string,len) (pds_textdisplay_text2buf(((bkcolor<<4)|color),buf,string,len))
extern unsigned int pds_textdisplay_text2field(unsigned int color,mpxp_textbuf_t *buf,char *string_s);
#define pds_textdisplay_text2fieldbk(color,bkcolor,buf,string) (pds_textdisplay_text2field(((bkcolor<<4)|color),buf,string))
extern unsigned int pds_textdisplay_utf8_text2fieldnn(unsigned int color,mpxp_textbuf_t *destbuf,char *string_s,unsigned int minlen,int maxlen);
#define pds_textdisplay_utf8_text2fieldnnbk(color,bkcolor,buf,string,minlen,maxlen) (pds_textdisplay_utf8_text2fieldnn(((bkcolor<<4)|color),buf,string,minlen,maxlen))

extern void pds_textdisplay_textbufxyn(unsigned int outx,unsigned int outy,mpxp_textbuf_t *buf,unsigned int maxlen);
extern void pds_textdisplay_consolevidmem_merge(unsigned short *attribs,mpxp_wchar_t *text,char *destbuf,unsigned int bufsize);
extern void pds_textdisplay_consolevidmem_separate(unsigned short *attribs,mpxp_wchar_t *text,char *srcbuf,unsigned int bufsize);
extern void pds_textdisplay_consolevidmem_read(unsigned short *attribs,mpxp_wchar_t *text,unsigned int maxsize);
extern void pds_textdisplay_consolevidmem_write(unsigned short *attribs,mpxp_wchar_t *text,unsigned int maxsize);
extern void pds_textdisplay_consolevidmem_xywrite(unsigned int x,unsigned int y,unsigned short *attribs,mpxp_wchar_t *text,unsigned int len);
extern void pds_textdisplay_vidmem_save(void);
extern void pds_textdisplay_vidmem_restore(void);
extern unsigned int pds_textdisplay_setmode(unsigned int mode);
extern unsigned int pds_textdisplay_getmode(void);
extern unsigned int pds_textdisplay_setlastmode(void);
extern void pds_textdisplay_setresolution(unsigned int lines);
extern void pds_textdisplay_getresolution(void);
extern unsigned int pds_textdisplay_getcursor_y(void);
extern void pds_textdisplay_setcursor_position(unsigned int x,unsigned int y);
extern void pds_textdisplay_setcursorshape(long);
extern void pds_textdisplay_resetcolorpalette(void);
#else // MPXPLAY_GUI_CONSOLE
  #define pds_textdisplay_printf(text) { fprintf(stdout, "%s", (text)); fprintf(stdout, "\n"); fflush(stdout); }
#endif

//threads.c (main.cpp)
#define MPXPLAY_ERROR_RANGE_MUTEX    MPXPLAY_ERROR_RANGE_GENERAL
#define MPXPLAY_ERROR_MUTEX_ARGS     (MPXPLAY_ERROR_RANGE_MUTEX - 1)
#define MPXPLAY_ERROR_MUTEX_CREATE   (MPXPLAY_ERROR_RANGE_MUTEX - 2)
#define MPXPLAY_ERROR_MUTEX_UNINIT   MPXPLAY_ERROR_OK              // currently it's not error
#define MPXPLAY_ERROR_MUTEX_LOCKED   (MPXPLAY_ERROR_RANGE_MUTEX - 4)

#define MPXPLAY_MUTEXTIME_SHORT      10
#define MPXPLAY_MUTEXTIME_NOLOCK     0x7eeeeeee

//#define PDS_THREADS_MUTEX_DEBUG 1
#if defined(__GNUC__)
 //#define PDS_THREADS_POSIX_THREAD 1
 //#define PDS_THREADS_POSIX_TIMER  1
#endif
#ifdef PDS_THREADS_MUTEX_DEBUG
 extern int pds_threads_mutex_lock_debug(void **mup, int timeoutms, const char *filename, const char *funcname, unsigned int linenum);
 #define PDS_THREADS_MUTEX_LOCK(mup, timeoutms) pds_threads_mutex_lock_debug(mup, timeoutms, __FILE__, __FUNCTION__, __LINE__)
#else
 extern int pds_threads_mutex_lock(void **mup, int timeoutms);
 #define PDS_THREADS_MUTEX_LOCK(mup, timeoutms) pds_threads_mutex_lock(mup, timeoutms)
#endif
extern int pds_threads_mutex_new(void **mup);
extern void pds_threads_mutex_del(void **mup);
extern void pds_threads_mutex_unlock(void **mup);
#define PDS_THREADS_MUTEX_UNLOCK(mup) pds_threads_mutex_unlock(mup)
extern int pds_threads_cond_new(void **cop);
extern void pds_threads_cond_del(void **cop);
extern int pds_threads_cond_wait(void **cop, void **mup);
extern int pds_threads_cond_timedwait(void **cop, void **mup, int msecs);
extern int pds_threads_cond_signal(void **cop);

#if !defined(__DOS__)
typedef mpxp_ptrsize_t mpxp_thread_id_type;
#if defined(PDS_THREADS_POSIX_THREAD)
typedef void * (*mpxp_thread_func)(void *);
#else
typedef unsigned (__stdcall *mpxp_thread_func)(void *);
#endif
typedef enum {MPXPLAY_THREAD_PRIORITY_IDLE = -15, MPXPLAY_THREAD_PRIORITY_LOWEST = -2, MPXPLAY_THREAD_PRIORITY_LOW = -1, MPXPLAY_THREAD_PRIORITY_NORMAL = 0, MPXPLAY_THREAD_PRIORITY_HIGHER = 1, MPXPLAY_THREAD_PRIORITY_HIGHEST = 2, MPXPLAY_THREAD_PRIORITY_RT = 15} mpxp_threadpriority_type;
#define MPXPLAY_AUDIODECODER_THREAD_AFFINITY_MASK 0x00000001  // because there's no inter-core communication between old Mpxplay's threads
extern int  pds_threads_get_number_of_processors(void);
extern void pds_threads_set_singlecore(void);
#if defined(MPXPLAY_THREADS_HYPERTREADING_DISABLE)
extern void pds_threads_hyperthreading_disable(void);
#endif
extern void pds_threads_thread_set_affinity(mpxp_thread_id_type threadId, mpxp_ptrsize_t affinity_mask);
extern mpxp_thread_id_type pds_threads_thread_create(mpxp_thread_func function, void *arg, mpxp_threadpriority_type priority, mpxp_ptrsize_t affinity);
extern void pds_threads_thread_close(mpxp_thread_id_type thread);
extern mpxp_thread_id_type pds_threads_threadid_current(void);
extern void pds_threads_thread_set_priority(mpxp_thread_id_type thread, mpxp_threadpriority_type priority);
extern void pds_threads_thread_suspend(mpxp_thread_id_type thread);
extern void pds_threads_thread_resume(mpxp_thread_id_type thread, mpxp_threadpriority_type priority);
extern int pds_threads_timer_period_set(unsigned int timer_period_ms);
extern void pds_threads_timer_period_reset(unsigned int timer_period_ms);
#if 0
extern unsigned int pds_threads_timer_waitable_create(unsigned int period_ms);
extern void pds_threads_timer_waitable_lock(unsigned int handler, unsigned int period_ms);
extern void pds_threads_timer_waitable_close(unsigned int handler);
#endif // 0
#endif // !defined(__DOS__)
extern int pds_threads_timer_tick_get(void);
extern void pds_threads_sleep(int timeoutms);
#define MPXPLAY_THREADS_SHORTTASKSLEEP 10

//time.c
extern unsigned long pds_gettimeh(void); // clock time in hsec
extern mpxp_int64_t pds_gettimem(void);  // clock time in msec
extern mpxp_int64_t pds_gettimeu(void);  // clock time in usec
extern unsigned long pds_gettime(void);  // current time in 0x00HHMMSS
extern unsigned long pds_getdate(void);  // current date in 0xYYYYMMDD
extern mpxp_uint64_t pds_getdatetime(void); // get local date and time in 0xYYYYMMDD00HHMMSS format
extern mpxp_uint64_t pds_utctime_to_localtime(mpxp_uint64_t utc_datetime_val); // convert UTC 0xYYYYMMDD00HHMMSS to local 0xYYYYMMDD00HHMMSS
extern mpxp_int32_t pds_datetimeval_difftime(mpxp_uint64_t datetime_val1, mpxp_uint64_t datetime_val0); // calculate time_ms diff between two 0xYYYYMMDD00HHMMSS values
extern mpxp_int32_t pds_datetimeval_elapsedtime(mpxp_uint64_t datetime_val); // returns elapsed time_ms from datetime_val 0xYYYYMMDD00HHMMSS
extern mpxp_int32_t pds_timeval_to_seconds(mpxp_uint32_t datetime_val); // convert 0x0000000000HHMMSS to seconds
extern mpxp_int64_t pds_datetimeval_to_seconds(mpxp_uint64_t datetime_val); // convert 0xYYYYMMDD00HHMMSS to seconds
extern unsigned long pds_strtime_to_hextime(char *timestr,unsigned int houralign);  // "hh:mm:ss" to 0x00hhmmss
extern int pds_hextime_to_strtime(unsigned long hextime, char *timestr, unsigned int buflen);  // 0x00hhmmss to "hh:mm:ss"
extern int pds_gettimestampstr(char *timestampstr, unsigned int buflen); // current date and time in YYYYMMDDHHMMSS string
extern unsigned long pds_strtime_to_hexhtime(char *timestr);  // "hh:mm:ss.nn" to 0xhhmmssnn
#define PDS_HEXTIME_TO_SECONDS(t) ((t>>16)*3600+((t>>8)&0xff)*60+(t&0xff))
#define PDS_SECONDS_TO_HEXTIME(s) (((s / 3600) << 16) | (((s % 3600) / 60) << 8) | (s % 60))
#define PDS_HEXHTIME_TO_HSECONDS(t) ((t>>24)*360000+((t>>16)&0xff)*6000+((t>>8)&0xff)*100+(t&0xff))
extern void pds_delay_10us(unsigned int ticks);
extern void pds_mdelay(unsigned long msec);

//timer.c
#define MPXPLAY_TIMERTYPE_WAKEUP        0  // wake up once (delete after running)
#define MPXPLAY_TIMERTYPE_REPEAT   (1<< 0) // repeat forever (don't delete after running)
#define MPXPLAY_TIMERTYPE_INT08    (1<< 2) // run from int08 (else run from main_cycle)
#define MPXPLAY_TIMERTYPE_SIGNAL   (1<< 3) // signalling (call func at mpxplay_signal_events) (cannot use it with INT08)
#if defined(__DOS__)
#define MPXPLAY_TIMERTYPE_THREAD   MPXPLAY_TIMERTYPE_WAKEUP // under dos there's no threads
#else
#define MPXPLAY_TIMERTYPE_THREAD   (1<< 4) // execute in a separate thread
#endif

#define MPXPLAY_TIMERFLAG_MULTIPLY (1<< 6) // multiply instance (eg: same function with timer and signal, or repeated functions) (note: same functions with different datas are automatically MULTIPLY) (same functions with same datas and flags shall not have MULTIPLY flags, eg: close functions)
#define MPXPLAY_TIMERFLAG_LOWPRIOR (1<< 7) // runs in lower priority (check buffers fullness)
#define MPXPLAY_TIMERFLAG_BUSY     (1<< 8) // busy flag (no re-entrance)
#define MPXPLAY_TIMERFLAG_OWNSTACK (1<< 9) // use a separated small (32k) stack for the function (for int08 functions)
#define MPXPLAY_TIMERFLAG_STI      (1<<10) // disable maskable interrupts while function running (for int08 functions)
#define MPXPLAY_TIMERFLAG_INDOS    (1<<11) // function calls DOS routines (check indos flag before)
#define MPXPLAY_TIMERFLAG_OWNSTCK2 (1<<12) // use a separated large (256k) stack for the function (for int08 functions)
#define MPXPLAY_TIMERFLAG_MVPDATA  (1<<13) // passdata=mvp
#define MPXPLAY_TIMERFLAG_MULTCORE (1<<14) // use multicore at thread (don't set affinity to core 0)
//internal flags
#ifndef __DOS__
#define MPXPLAY_TIMERFLAG_THEXITEVENT  (1<<28) // exit from thread init
#define MPXPLAY_TIMERFLAG_THEXITDONE   (1<<29) // exit from thread done
#endif

//mtf->signal_flags
#define MPXPLAY_SIGNALEVENT_EXECUTE_ONCE 0 // execute at event
#define MPXPLAY_SIGNALEVENT_EXECUTE_MORE 1 //
#define MPXPLAY_SIGNALEVENT_DELAY_START  8 // execute delay (timer) after event
#define MPXPLAY_SIGNALEVENT_DELAY_STOP  16 //
#define MPXPLAY_SIGNALEVENT_DELAY_RESET 32 //

//events at TIMERTYPE_SIGNAL (mpxplay_signal_events)
#define MPXPLAY_SIGNALTYPE_KEYBOARD      (1 << 0)
#define MPXPLAY_SIGNALTYPE_MOUSE         (1 << 1)
#define MPXPLAY_SIGNALTYPE_DISPMESSAGE   (1 << 4) // display_message called
#define MPXPLAY_SIGNALTYPE_CLEARMESSAGE  (1 << 5) // clear message called
#define MPXPLAY_SIGNALTYPE_DESKTOP   (MPXPLAY_SIGNALTYPE_DISPMESSAGE|MPXPLAY_SIGNALTYPE_CLEARMESSAGE)
#define MPXPLAY_SIGNALTYPE_NEWFILE       (1 << 8)
#define MPXPLAY_SIGNALTYPE_USER      (MPXPLAY_SIGNALTYPE_KEYBOARD|MPXPLAY_SIGNALTYPE_MOUSE)
#define MPXPLAY_SIGNALMASK_TIMER     (MPXPLAY_SIGNALTYPE_KEYBOARD|MPXPLAY_SIGNALTYPE_MOUSE|MPXPLAY_SIGNALTYPE_DESKTOP|MPXPLAY_SIGNALTYPE_NEWFILE)
//other events (not used at TIMERTYPE_SIGNAL)
#define MPXPLAY_SIGNALTYPE_REALTIME      (1 <<12) // realtime (delay=0) function is in the timer (don't use cpu-halt)
#define MPXPLAY_SIGNALTYPE_DISKACCESS    (1 <<13) // file read/write
#define MPXPLAY_SIGNALTYPE_DISKDRIVRUN   (1 <<14) // diskdrive (open) is running
#define MPXPLAY_SIGNALTYPE_DISKDRIVTERM  (1 <<15) // terminate diskdrive (open)
#define MPXPLAY_SIGNALMASK_OTHER     (MPXPLAY_SIGNALTYPE_REALTIME|MPXPLAY_SIGNALTYPE_DISKACCESS|MPXPLAY_SIGNALTYPE_DISKDRIVTERM)
#define MPXPLAY_SIGNALTYPE_GUIREADY      (1 <<16) // gui is ready to display (windows) (it's not cleared)
#define MPXPLAY_SIGNALTYPE_GUIREBUILD    (1 <<17) // rebuild gui
#define MPXPLAY_SIGNALTYPE_NORMALEXIT    (1 <<18) // normal exit (not a crash)

extern unsigned long mpxplay_timer_secs_to_counternum(unsigned long secs);
extern unsigned long mpxplay_timer_msecs_to_counternum(unsigned long msecs);
extern int mpxplay_timer_addfunc(void *callback_func,void *callback_data,unsigned int timer_flags,unsigned int refresh_delay); // returns -1 on error, else returns a handlernum_index value
extern int mpxplay_timer_modifyfunc(void *callback_func,void *callback_data,int flags,int refresh_delay); // if flags==-1 or refresh_delay==-1, then do not modify that value
extern int mpxplay_timer_modifyhandler(void *callback_func,int handlernum_index,int timer_flags,int refresh_delay);
extern void mpxplay_timer_deletefunc(void *callback_func,void *callback_data);
extern void mpxplay_timer_deletehandler(void *callback_func,int handlernum_index); // used at TIMERFLAG_MULTIPLY
extern void mpxplay_timer_executefunc(void *func);
extern unsigned int mpxplay_timer_lowpriorstart_wait(void);
extern void mpxplay_timer_reset_counters(void);
extern void mpxplay_timer_execute_maincycle_funcs(void);
extern void mpxplay_timer_execute_int08_funcs(void);
extern void newfunc_newhandler08_init(void);
extern unsigned int newfunc_newhandler08_is_current_thread(void);
#ifdef MPXPLAY_WIN32
extern void newfunc_newhandler08_waitfor_threadend(void);
#else
#define newfunc_newhandler08_waitfor_threadend()
#endif
extern void newfunc_timer_threads_suspend(void);
extern void newfunc_newhandler08_close(void);

// safe funcbit handling for multi processors
#ifdef MPXPLAY_USE_IXCH_SMP
#include <windows.h>
#define funcbit_smp_test(var,bit)       (InterlockedCompareExchange((LONG *)&(var),(LONG)(var),(LONG)(var))&(bit))
#define funcbit_smp_enable(var,bit)     InterlockedExchange((LONG *)&(var),InterlockedCompareExchange((LONG *)&(var),(LONG)(var),(LONG)(var))|(bit))
#define funcbit_smp_disable(var,bit)    InterlockedExchange((LONG *)&(var),InterlockedCompareExchange((LONG *)&(var),(LONG)(var),(LONG)(var))&(~(bit)))
#define funcbit_smp_inverse(var,bit)    InterlockedExchange((LONG *)&(var),InterlockedCompareExchange((LONG *)&(var),(LONG)(var),(LONG)(var))^(bit))
#define funcbit_smp_copy(var1,var2,bit) InterlockedExchange((LONG *)&(var1), \
         (InterlockedCompareExchange((LONG *)&(var1),(LONG)(var1),(LONG)(var1)) & ~(bit)) | (funcbit_smp_int32_get(var2) & (bit)))
//#define funcbit_smp_int32_get(var)       InterlockedExchange((LONG *)&(var),(LONG)(var))
//#define funcbit_smp_pointer_get(var)     (void *)InterlockedExchange((LONG *)&(var),(LONG)(var))
#define funcbit_smp_int32_get(var)       (var)
#define funcbit_smp_int64_get(var)       (var)
#define funcbit_smp_pointer_get(var)     (var)
#define funcbit_smp_int32_put(var,val)   InterlockedExchange((LONG *)&(var),(LONG)(val))
#define funcbit_smp_int64_put(var,val)   if(sizeof(var)==sizeof(mpxp_int64_t)){ InterlockedExchange((LONG *)&(var),(LONG)((val)&0xffffffff));InterlockedExchange((LONG *)(&(var)+4),(LONG)(((mpxp_int64_t)val)>>32));}else funcbit_smp_int32_put(var,val);
#ifdef MPXPLAY_FSIZE64
#define funcbit_smp_filesize_put(var,val) funcbit_smp_int64_put(var,val)
#else
#define funcbit_smp_filesize_put(var,val) funcbit_smp_int32_put(var,val)
#endif
#define funcbit_smp_pointer_put(var,val) InterlockedExchange((LONG *)&(var),(LONG)(val))
#define funcbit_smp_int32_increment(var) InterlockedIncrement((LONG *)&(var))
#define funcbit_smp_int32_decrement(var) InterlockedDecrement((LONG *)&(var))

#else
#define funcbit_smp_test(var,bit)       funcbit_test(var,bit)
#define funcbit_smp_enable(var,bit)     funcbit_enable(var,bit)
#define funcbit_smp_disable(var,bit)    funcbit_disable(var,bit)
#define funcbit_smp_inverse(var,bit)    funcbit_inverse(var,bit)
#define funcbit_smp_copy(var1,var2,bit) funcbit_copy(var1,var2,bit)
#define funcbit_smp_int32_get(var)       (var)
#define funcbit_smp_int64_get(var)       (var)
#define funcbit_smp_pointer_get(var)     (var)
#define funcbit_smp_int32_put(var,val)   (var)=(val)
#define funcbit_smp_int64_put(var,val)   (var)=(val)
#ifdef MPXPLAY_FSIZE64
#define funcbit_smp_filesize_put(var,val) funcbit_smp_int64_put(var,val)
#else
#define funcbit_smp_filesize_put(var,val) funcbit_smp_int32_put(var,val)
#endif
#define funcbit_smp_pointer_put(var,val) (var)=(val)
#define funcbit_smp_int32_increment(var) var++
#define funcbit_smp_int32_decrement(var) var--
#endif

#ifdef MPXPLAY_USE_DEBUGF

#include <stdarg.h>
#include <inttypes.h>
#include <time.h>

#if !defined(MPXPLAY_USE_DEBUGMSG) && !defined(MPXPLAY_USE_WARNINGMSG) && defined(__GNUC__) && ((__STDC_VERSION__ >= 199901L) || defined(__cplusplus))
#define MPXPLAY_EXTENDED_DEBUG
#endif

#ifdef MPXPLAY_USE_DEBUGMSG
extern void display_static_message(unsigned int linepos,unsigned int blink,char *msg);
#endif
#ifdef MPXPLAY_USE_WARNINGMSG
extern void display_warning_message(char *message);
#endif

#ifdef MPXPLAY_EXTENDED_DEBUG
static inline void mpxplay_debug_create_timestamp(char *outbuf, unsigned int bufsize)
{
 mpxp_int64_t timemsec;
 time_t timesec;
 struct tm *t;

 timemsec = pds_gettimem();
 timesec = timemsec / 1000;
 t = localtime(&timesec);

 if(t)
 {
  snprintf(outbuf, bufsize, "%2.2d:%2.2d:%2.2d.%3.3d", t->tm_hour, t->tm_min, t->tm_sec, (int)(timemsec % 1000));
 }
 else
 {
  outbuf[0] = 0;
 }
}
#endif

#ifdef MPXPLAY_EXTENDED_DEBUG
static void mpxplay_debug_f(FILE *fp, const char *srcfile, int linenum, const char *format, ...)
#else
static void mpxplay_debugf(FILE *fp, const char *format, ...)
#endif
{
 va_list ap;
 char sout[4096];
#ifdef MPXPLAY_EXTENDED_DEBUG
 char timestamp[64];
#endif

 va_start(ap,format);
 vsnprintf(sout, 4096, format, ap);
 va_end(ap);

 if(fp){
#ifdef MPXPLAY_EXTENDED_DEBUG
  mpxplay_debug_create_timestamp(timestamp, sizeof(timestamp));
  fprintf(fp,"%s %-12s %4d %s\n",timestamp, pds_getfilename_from_fullname((char *)srcfile), linenum, sout);
#else
  fprintf(fp,"%s\n",sout);
#endif
  fflush(fp);
 }else{
#ifdef MPXPLAY_USE_DEBUGMSG
  display_static_message(1,0,sout);
#elif defined(MPXPLAY_USE_WARNINGMSG)
  display_warning_message(sout);
#elif MPXPLAY_LINKMODE_DLL
  fprintf(stdout, sout);
  fprintf(stdout, "\n");
  fflush(stdout);
#elif MPXPLAY_GUI_CONSOLE
  pds_textdisplay_printf(sout);
#endif
 }
}

#ifdef MPXPLAY_EXTENDED_DEBUG
#define mpxplay_debugf(fp, format, ...) mpxplay_debug_f(fp, __FILE__, __LINE__, format, ## __VA_ARGS__)
#endif

#else
 #define mpxplay_debugf(...)
#endif// MPXPLAY_USE_DEBUGF

#ifdef __cplusplus
}
#endif

#endif // mpxplay_newfunc_h
