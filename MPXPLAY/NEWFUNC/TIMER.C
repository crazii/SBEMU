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
//function: timer/scheduler/signalling (timed function calls)

//#define MPXPLAY_USE_DEBUGF 1
#define MPXPLAY_DEBUG_OUTPUT stdout

#include "newfunc.h"
#include <mpxplay.h>
#include <mpxinbuf.h>
#ifdef __DOS__
#include <conio.h>
#endif

#if defined(__DOS__)
#define MPXPLAY_TIMEDS_INITSIZE  32
#elif defined(MPXPLAY_GUI_CONSOLE)
#define MPXPLAY_TIMEDS_INITSIZE  48
#else // MPXPLAY_GUI_QT uses more scheduled functions (but probably not more than 32)
#define MPXPLAY_TIMEDS_INITSIZE  64
#endif
#define MPXPLAY_TIMERS_STACKSIZE_SMALL  32768
#ifdef MPXPLAY_LINK_DLLLOAD // from newfunc.h -> dll_load.h
 #define MPXPLAY_TIMERS_STACKSIZE_LARGE 256144
#else
 #define MPXPLAY_TIMERS_STACKSIZE_LARGE MPXPLAY_TIMERS_STACKSIZE_SMALL
#endif
#define MTF_FLAG_LOCK 1
#define MPXPLAY_TIMER_MUTEXLOCK_TIMEOUT 1000

typedef struct mpxplay_timed_s{
 void *func;
 void *data;
 unsigned long timer_flags;
 unsigned long refresh_delay;  // or signal event (SIGNALTYPE)
 unsigned long refresh_counter;

#ifdef __DOS__
 char *ownstackmem;     // for int08 functions
 void __far *oldstack;
 char __far *newstack;
#else
 mpxp_thread_id_type thread_id;  // at MPXPLAY_TIMERTYPE_THREAD
#endif
}mpxplay_timed_s;

typedef void (*call_timedfunc_nodata)(void);
typedef void (*call_timedfunc_withdata)(void *);

static void newfunc_timer_delete_entry(mpxplay_timed_s *mtf);
static void mpxplay_timer_delete_int08_funcs(void);

extern volatile unsigned int intsoundcontrol;
extern unsigned long intdec_timer_counter;
extern struct mainvars mvps;

static volatile mpxplay_timed_s *mpxplay_timed_functions;
static volatile unsigned long mtf_storage,mtf_flags;
static void *timer_mutex_handler;
#ifdef __DOS__
static volatile unsigned int oldint08_running;
volatile unsigned long int08counter;
#endif
volatile unsigned long mpxplay_signal_events;

#ifndef __DOS__
#if defined(PDS_THREADS_POSIX_THREAD)
static void *newfunc_timer_threadfunc(void *tharg)
#else
static unsigned int newfunc_timer_threadfunc(void *tharg)
#endif
{
 mpxplay_timed_s *mtf = (mpxplay_timed_s *)tharg;
 mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT,"newfunc_timer_threadfunc START data:%8.8X", (unsigned int)mtf->data);
 do{
  if(mtf->func){
   if(mtf->data)
    ((call_timedfunc_withdata)(mtf->func))(mtf->data);
   else
    ((call_timedfunc_nodata)(mtf->func))();
  }
  if(!funcbit_test(mtf->timer_flags, MPXPLAY_TIMERTYPE_REPEAT))
   break;
  pds_threads_sleep(mtf->refresh_delay * 10); // FIXME
 }while(!funcbit_test(mtf->timer_flags, MPXPLAY_TIMERFLAG_THEXITEVENT));
 mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT,"newfunc_timer_threadfunc END data:%8.8X", (unsigned int)mtf->data);
 funcbit_enable(mtf->timer_flags, MPXPLAY_TIMERFLAG_THEXITDONE);
 pds_threads_sleep(MPXPLAY_TIMER_MUTEXLOCK_TIMEOUT);  // required for pds_threads_thread_close -> TerminateThread
#if defined(PDS_THREADS_POSIX_THREAD)
 return NULL;
#else
 return 0;
#endif
}
#endif // ifndef __DOS__

static unsigned int newfunc_timer_alloc(void)
{
 unsigned int newsize;
 mpxplay_timed_s *mtf;

 if(mtf_storage)
  newsize=mtf_storage*2;
 else
  newsize=MPXPLAY_TIMEDS_INITSIZE;

 mtf=pds_calloc(newsize,sizeof(mpxplay_timed_s));
 if(!mtf)
  return 0;
 funcbit_smp_enable(mtf_flags,MTF_FLAG_LOCK);
 if(mpxplay_timed_functions){
  pds_smp_memcpy((char *)mtf,(char *)mpxplay_timed_functions,mtf_storage*sizeof(mpxplay_timed_s));
  pds_free((void *)mpxplay_timed_functions);
 }
 funcbit_smp_pointer_put(mpxplay_timed_functions,mtf);
 funcbit_smp_int32_put(mtf_storage,newsize);
 funcbit_smp_disable(mtf_flags,MTF_FLAG_LOCK);
 if(!timer_mutex_handler)
  pds_threads_mutex_new(&timer_mutex_handler);
 return newsize;
}

static void mpxplay_timer_close(void)
{
 mpxplay_timed_s *mtf_begin_ptr = (mpxplay_timed_s *)mpxplay_timed_functions;
 if(mtf_begin_ptr){
  mpxplay_timed_s *mtf = mtf_begin_ptr;
  unsigned int i;

  funcbit_smp_enable(mtf_flags,MTF_FLAG_LOCK);
  mpxplay_timer_delete_int08_funcs();
  funcbit_smp_pointer_put(mpxplay_timed_functions,NULL);

  for(i = 0; i< mtf_storage; i++){
#ifdef __DOS__
   if(mtf->ownstackmem)
    pds_free(mtf->ownstackmem);
#endif
   newfunc_timer_delete_entry(mtf);
   mtf++;
  }
  pds_free(mtf_begin_ptr);
  funcbit_smp_disable(mtf_flags,MTF_FLAG_LOCK);
 }
 funcbit_smp_int32_put(mtf_storage,0);
 pds_threads_mutex_del(&timer_mutex_handler);
}

static mpxplay_timed_s *newfunc_timer_search_entry(mpxplay_timed_s *mtf,void *func,void *data)
{
 mpxplay_timed_s *mte=((mpxplay_timed_s *)mpxplay_timed_functions)+mtf_storage;

 if(!mtf)
  mtf=(mpxplay_timed_s *)mpxplay_timed_functions;
 if(!mtf)
  return NULL;
 if(mtf>=mte)
  return NULL;

 do{
#ifndef __DOS__
  if(funcbit_test(mtf->timer_flags, MPXPLAY_TIMERTYPE_THREAD) && funcbit_test(mtf->timer_flags,MPXPLAY_TIMERFLAG_THEXITDONE))
   newfunc_timer_delete_entry(mtf);
#endif // ifndef __DOS__
  if(mtf->func==func)
   if(mtf->data==data)
    return mtf;
  mtf++;
 }while(mtf<mte);

 return NULL;
}

static mpxplay_timed_s *newfunc_timer_getfree_entry(void)
{
 mpxplay_timed_s *mtf;

 mtf=newfunc_timer_search_entry(NULL,NULL,NULL);

 if(!mtf)
  if(newfunc_timer_alloc())
   mtf=newfunc_timer_search_entry(NULL,NULL,NULL);

 return mtf;
}

static int newfunc_timer_add_entry(mpxplay_timed_s *mtf,void *func,void *data,unsigned int timer_flags,unsigned int refresh_delay)
{
 funcbit_smp_pointer_put(mtf->func,func);
 funcbit_smp_pointer_put(mtf->data,data);
 funcbit_smp_int32_put(mtf->timer_flags,timer_flags);
 funcbit_smp_int32_put(mtf->refresh_delay,refresh_delay);
 funcbit_smp_int32_put(mtf->refresh_counter, pds_threads_timer_tick_get());
#ifdef __DOS__
 if(funcbit_test(timer_flags,(MPXPLAY_TIMERFLAG_OWNSTACK|MPXPLAY_TIMERFLAG_OWNSTCK2))){
  unsigned int stack_size = (timer_flags&MPXPLAY_TIMERFLAG_OWNSTCK2)? MPXPLAY_TIMERS_STACKSIZE_LARGE:MPXPLAY_TIMERS_STACKSIZE_SMALL;
  if((timer_flags&(MPXPLAY_TIMERFLAG_OWNSTACK|MPXPLAY_TIMERFLAG_OWNSTCK2)) != (mtf->timer_flags&(MPXPLAY_TIMERFLAG_OWNSTACK|MPXPLAY_TIMERFLAG_OWNSTCK2))){ // stack size has changed
   if(mtf->ownstackmem){
    pds_free(mtf->ownstackmem);
    mtf->ownstackmem = NULL;
   }
  }
  if(!mtf->ownstackmem){
   mtf->ownstackmem=(char *)pds_malloc(stack_size + 32);
   if(!mtf->ownstackmem){
    newfunc_timer_delete_entry(mtf);
    return -1;
   }
   mtf->newstack=(char far *)(mtf->ownstackmem + stack_size);
  }
 }
#else
 if(timer_flags & MPXPLAY_TIMERTYPE_THREAD)
 {
  mtf->thread_id = pds_threads_thread_create((mpxp_thread_func)newfunc_timer_threadfunc, (void *)mtf,
                  ((timer_flags & MPXPLAY_TIMERFLAG_LOWPRIOR)? MPXPLAY_THREAD_PRIORITY_LOW : MPXPLAY_THREAD_PRIORITY_NORMAL),
                  ((timer_flags & MPXPLAY_TIMERFLAG_MULTCORE)? 0 : MPXPLAY_AUDIODECODER_THREAD_AFFINITY_MASK));
  mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT,"newfunc_timer_add_entry pds_threads_thread_create data:%8.8X thid:%d mc:%d", (unsigned int)data, mtf->thread_id, ((timer_flags & MPXPLAY_TIMERFLAG_MULTCORE)? 1 :0));
  if(!mtf->thread_id){
   newfunc_timer_delete_entry(mtf);
   return -1;
  }
 }
#endif
 return (int)(mtf-mpxplay_timed_functions); // index in mpxplay_timed_functions
}

static void newfunc_timer_delete_entry(mpxplay_timed_s *mtf)
{
 if(mtf){
#ifndef __DOS__
  if((mtf->timer_flags & MPXPLAY_TIMERTYPE_THREAD) && (mtf->thread_id > 0)){
   int timeout_counter = MPXPLAY_TIMER_MUTEXLOCK_TIMEOUT / MPXPLAY_THREADS_SHORTTASKSLEEP; // ~ 1.5 sec
   funcbit_enable(mtf->timer_flags,MPXPLAY_TIMERFLAG_THEXITEVENT);
   while(!funcbit_test(mtf->timer_flags,MPXPLAY_TIMERFLAG_THEXITDONE) && (--timeout_counter)){ pds_threads_sleep(MPXPLAY_THREADS_SHORTTASKSLEEP); }
   pds_threads_thread_close(mtf->thread_id);
   mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT,"newfunc_timer_delete_entry pds_threads_thread_close data:%8.8X", (unsigned int)mtf->data);
  }
  mtf->thread_id = 0;
#endif
  funcbit_smp_pointer_put(mtf->func,NULL);
  funcbit_smp_pointer_put(mtf->data,NULL);
  funcbit_smp_int32_put(mtf->timer_flags,0);
 }
}

//------------------------------------------------------------------------
unsigned long mpxplay_timer_secs_to_counternum(unsigned long secs)
{
 mpxp_int64_t cn;     // 1000.0ms/55.0ms = 18.181818 ticks per sec
 pds_fto64i((float)secs*(1000.0/55.0)*(float)INT08_DIVISOR_DEFAULT/(float)INT08_DIVISOR_NEW,&cn);
 return cn;
}

unsigned long mpxplay_timer_msecs_to_counternum(unsigned long msecs)
{
 mpxp_int64_t cn;     // 1000.0ms/55.0ms = 18.181818 ticks per sec
 pds_fto64i((float)(msecs + 54) / 55.0 * (float)INT08_DIVISOR_DEFAULT / (float)INT08_DIVISOR_NEW, &cn); // +54 : round up
 return cn;
}

int mpxplay_timer_addfunc(void *func,void *data,unsigned int timer_flags,unsigned int refresh_delay)
{
 mpxplay_timed_s *mtf=NULL;
 int res = -1;

 if(!func)
  return res;

 PDS_THREADS_MUTEX_LOCK(&timer_mutex_handler, MPXPLAY_TIMER_MUTEXLOCK_TIMEOUT);

 if(!(funcbit_test(timer_flags,MPXPLAY_TIMERFLAG_MULTIPLY)))
  mtf=newfunc_timer_search_entry(NULL,func,data); // update previous instance if exists

 if(!mtf)
  mtf=newfunc_timer_getfree_entry();

 if(!mtf)
  goto err_out_addfunc;

 if(!data && funcbit_test(timer_flags,MPXPLAY_TIMERFLAG_MVPDATA))
  data=&mvps;

 res = newfunc_timer_add_entry(mtf,func,data,timer_flags,refresh_delay);

err_out_addfunc:
 PDS_THREADS_MUTEX_UNLOCK(&timer_mutex_handler);
 return res;
}

int mpxplay_timer_modifyfunc(void *func,void *data,int timer_flags,int refresh_delay)
{
 mpxplay_timed_s *mtf;
 int res = -1;

 if(!func)
  return res;

 PDS_THREADS_MUTEX_LOCK(&timer_mutex_handler, MPXPLAY_TIMER_MUTEXLOCK_TIMEOUT);

 mtf=newfunc_timer_search_entry(NULL,func,data);
 if(mtf){
  if(timer_flags>=0)
   funcbit_smp_int32_put(mtf->timer_flags,timer_flags);
  if(refresh_delay>=0)
   funcbit_smp_int32_put(mtf->refresh_delay,refresh_delay);
  res = (int)(mtf-mpxplay_timed_functions);
 }

 PDS_THREADS_MUTEX_UNLOCK(&timer_mutex_handler);
 return res;
}

int mpxplay_timer_modifyhandler(void *func,int handlernum_index,int timer_flags,int refresh_delay)
{
 int res = -1;
 PDS_THREADS_MUTEX_LOCK(&timer_mutex_handler, MPXPLAY_TIMER_MUTEXLOCK_TIMEOUT);
 if((handlernum_index>=0) && (handlernum_index<=mtf_storage) && mpxplay_timed_functions){
  mpxplay_timed_s *mtf=(mpxplay_timed_s *)&mpxplay_timed_functions[handlernum_index];
  if(mtf->func==func){
   if(timer_flags>=0)
    funcbit_smp_int32_put(mtf->timer_flags,timer_flags);
   if(refresh_delay>=0)
    funcbit_smp_int32_put(mtf->refresh_delay,refresh_delay);
   res = handlernum_index;
  }
 }
 PDS_THREADS_MUTEX_UNLOCK(&timer_mutex_handler);
 return res;
}

void mpxplay_timer_deletefunc(void *func,void *data)
{
 mpxplay_timed_s *mtf;

 if(func){
  PDS_THREADS_MUTEX_LOCK(&timer_mutex_handler, MPXPLAY_TIMER_MUTEXLOCK_TIMEOUT);
  mtf=newfunc_timer_search_entry(NULL,func,data);
  if(mtf){
   if(funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERFLAG_BUSY))
    funcbit_smp_disable(mtf->timer_flags,MPXPLAY_TIMERTYPE_REPEAT);
   else
    newfunc_timer_delete_entry(mtf);
  }
  PDS_THREADS_MUTEX_UNLOCK(&timer_mutex_handler);
 }
}

void mpxplay_timer_deletehandler(void *func,int handlernum_index)
{
 PDS_THREADS_MUTEX_LOCK(&timer_mutex_handler, MPXPLAY_TIMER_MUTEXLOCK_TIMEOUT);
 if((handlernum_index>=0) && (handlernum_index<=mtf_storage) && mpxplay_timed_functions){
  mpxplay_timed_s *mtf=(mpxplay_timed_s *)&mpxplay_timed_functions[handlernum_index];
  if(mtf->func==func){
   if(funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERFLAG_BUSY))
    funcbit_smp_disable(mtf->timer_flags,MPXPLAY_TIMERTYPE_REPEAT);
   else
    newfunc_timer_delete_entry(mtf);
  }
 }
 PDS_THREADS_MUTEX_UNLOCK(&timer_mutex_handler);
}

void mpxplay_timer_executefunc(void *func)
{
 mpxplay_timed_s *mtf;

 if(func){
  PDS_THREADS_MUTEX_LOCK(&timer_mutex_handler, MPXPLAY_TIMER_MUTEXLOCK_TIMEOUT);
  mtf=newfunc_timer_search_entry(NULL,func,NULL);
  PDS_THREADS_MUTEX_UNLOCK(&timer_mutex_handler);
  if(mtf && mtf->func){
   if(mtf->data)
    ((call_timedfunc_withdata)(mtf->func))(mtf->data);
   else
    ((call_timedfunc_nodata)(mtf->func))();
  }
 }
}

#ifndef SBEMU
// currently returns 1 if there is delay, returns 0 if no
unsigned int mpxplay_timer_lowpriorstart_wait(void)
{
 if(!mpxplay_check_buffers_full(&mvps))
  return 1;
 return 0;
}
#endif

//------------------------------------------------------------------------

void mpxplay_timer_reset_counters(void)
{
 mpxplay_timed_s *mtf=(mpxplay_timed_s *)funcbit_smp_pointer_get(mpxplay_timed_functions);
 unsigned int i,clearint08=!funcbit_smp_test(mvps.aui->card_handler->infobits,SNDCARD_INT08_ALLOWED); // ???
 if(!mtf)
  return;
 if(funcbit_smp_test(mtf_flags,MTF_FLAG_LOCK))
  return;
 PDS_THREADS_MUTEX_LOCK(&timer_mutex_handler, MPXPLAY_TIMER_MUTEXLOCK_TIMEOUT);
 for(i=0;i<funcbit_smp_int32_get(mtf_storage);i++){
  if(funcbit_smp_pointer_get(mtf->func)){

   if(funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERTYPE_REPEAT) && !funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERTYPE_SIGNAL)){
    if(pds_threads_timer_tick_get() > funcbit_smp_int32_get(mtf->refresh_delay))
     funcbit_smp_int32_put(mtf->refresh_counter, pds_threads_timer_tick_get() - mtf->refresh_delay);
    else
     funcbit_smp_int32_put(mtf->refresh_counter, 0);
   }

   if(clearint08)
    funcbit_smp_disable(mtf->timer_flags,MPXPLAY_TIMERTYPE_INT08);
  }
  mtf++;
 }
 funcbit_smp_disable(mpxplay_signal_events, MPXPLAY_SIGNALMASK_TIMER); // !!!
 PDS_THREADS_MUTEX_UNLOCK(&timer_mutex_handler);
}

#ifdef __DOS__
#define MPXPLAY_TIMER_MAINCYCLE_EXCLUSION (MPXPLAY_TIMERTYPE_INT08|MPXPLAY_TIMERFLAG_BUSY)
#else
#define MPXPLAY_TIMER_MAINCYCLE_EXCLUSION (MPXPLAY_TIMERTYPE_INT08|MPXPLAY_TIMERTYPE_THREAD|MPXPLAY_TIMERFLAG_BUSY)
#endif

void mpxplay_timer_execute_maincycle_funcs(void) // not reentrant!
{
 unsigned int i;
 volatile unsigned long signal_events;
 if(!funcbit_smp_pointer_get(mpxplay_timed_functions))
  return;
 if(funcbit_smp_test(mtf_flags,MTF_FLAG_LOCK))
  return;
 signal_events=funcbit_smp_int32_get(mpxplay_signal_events);
 funcbit_smp_disable(mpxplay_signal_events,MPXPLAY_SIGNALMASK_TIMER);
 for(i=0;i<funcbit_smp_int32_get(mtf_storage);i++){
  mpxplay_timed_s *mtf=(mpxplay_timed_s *)&mpxplay_timed_functions[i];
  void *mtf_func=funcbit_smp_pointer_get(mtf->func);
  if(mtf_func && !funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMER_MAINCYCLE_EXCLUSION)){

   if(funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERTYPE_SIGNAL)){
    if(funcbit_smp_test(signal_events,mtf->refresh_delay)){

#if defined(__DOS__) && !defined(SBEMU)
     if(funcbit_test(mtf->timer_flags,MPXPLAY_TIMERFLAG_INDOS)){ // ???
      if(pds_filehand_check_infilehand())    //
       continue;                             //
      if(pds_indos_flag())                   //
       continue;                             //
     }                                       //
#endif

     funcbit_smp_enable(mtf->timer_flags,MPXPLAY_TIMERFLAG_BUSY);

     if(funcbit_smp_pointer_get(mtf->data))
      ((call_timedfunc_withdata)(mtf_func))(mtf->data);
     else
      ((call_timedfunc_nodata)(mtf_func))();

     mtf=(mpxplay_timed_s *)&mpxplay_timed_functions[i]; // function may modify mpxplay_timed_functions (alloc -> new begin pointer)

     if(!funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERTYPE_REPEAT))
      newfunc_timer_delete_entry(mtf);

     funcbit_smp_disable(mtf->timer_flags,MPXPLAY_TIMERFLAG_BUSY);
     funcbit_smp_enable(mpxplay_signal_events,MPXPLAY_SIGNALTYPE_REALTIME); // ???
    }
   }else{
    if(pds_threads_timer_tick_get() >= (funcbit_smp_int32_get(mtf->refresh_counter) + funcbit_smp_int32_get(mtf->refresh_delay))){
     #ifndef SBEMU
     if(funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERFLAG_LOWPRIOR)){
      if(!mpxplay_check_buffers_full(&mvps))
       continue;
     }
     #endif

#if defined(__DOS__) && !defined(SBEMU)
     if(funcbit_test(mtf->timer_flags,MPXPLAY_TIMERFLAG_INDOS)){ // ???
      if(pds_filehand_check_infilehand())    //
       continue;                             //
      if(pds_indos_flag())                   //
       continue;                             //
     }                                       //
#endif

     funcbit_smp_enable(mtf->timer_flags,MPXPLAY_TIMERFLAG_BUSY);

     funcbit_smp_int32_put(mtf->refresh_counter, pds_threads_timer_tick_get());

     if(funcbit_smp_pointer_get(mtf->data))
      ((call_timedfunc_withdata)(mtf_func))(mtf->data);
     else
      ((call_timedfunc_nodata)(mtf_func))();

     mtf=(mpxplay_timed_s *)&mpxplay_timed_functions[i];

     if(!funcbit_smp_int32_get(mtf->refresh_delay))
     {
      mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT,"mpxplay_timer_execute_maincycle_funcs RT by func:%8.8X data:%8.8X flags:%8.8X delay:%d", (unsigned int)mtf->func, (unsigned int)mtf->data, mtf->timer_flags, mtf->refresh_delay);
      funcbit_smp_enable(mpxplay_signal_events,MPXPLAY_SIGNALTYPE_REALTIME);
     }

     if(!funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERTYPE_REPEAT))
      newfunc_timer_delete_entry(mtf);

     funcbit_smp_disable(mtf->timer_flags,MPXPLAY_TIMERFLAG_BUSY);
    }
   }

  }
 }
 if(funcbit_smp_test(mpxplay_signal_events,MPXPLAY_SIGNALMASK_TIMER))
 {
  mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT,"mpxplay_timer_execute_maincycle_funcs RT by signals %8.8X", (mpxplay_signal_events&MPXPLAY_SIGNALMASK_TIMER));
  funcbit_smp_enable(mpxplay_signal_events,MPXPLAY_SIGNALTYPE_REALTIME);
 }
}

#if defined(__DOS__) && defined(__WATCOMC__)

void call_func_ownstack_withdata(void *data,void *func,void far **oldstack,char far **newstack);
#pragma aux call_func_ownstack_withdata parm [eax][edx][ebx][ecx] = \
  "mov word ptr [ebx+4],ss" \
  "mov dword ptr [ebx],esp" \
  "lss esp,[ecx]" \
  "call edx" \
  "lss esp,[ebx]"

void call_func_ownstack_withdata_sti(void *data,void *func,void far **oldstack,char far **newstack);
#pragma aux call_func_ownstack_withdata_sti parm [eax][edx][ebx][ecx] = \
  "mov word ptr [ebx+4],ss" \
  "mov dword ptr [ebx],esp" \
  "lss esp,[ecx]" \
  "sti"\
  "call edx" \
  "cli"\
  "lss esp,[ebx]"

void call_func_ownstack_nodata(void *func,void far **oldstack,char far **newstack);
#pragma aux call_func_ownstack_nodata parm [eax][edx][ebx] = \
  "mov word ptr [edx+4],ss" \
  "mov dword ptr [edx],esp" \
  "lss esp,[ebx]" \
  "call eax" \
  "lss esp,[edx]"

void call_func_ownstack_nodata_sti(void *func,void far **oldstack,char far **newstack);
#pragma aux call_func_ownstack_nodata_sti parm [eax][edx][ebx] = \
  "mov word ptr [edx+4],ss" \
  "mov dword ptr [edx],esp" \
  "sti"\
  "lss esp,[ebx]" \
  "call eax" \
  "lss esp,[edx]"\
  "cli"

#endif // __DOS__ && __WATCOMC__

#define MPXPLAY_TIMER_MAX_PARALELL_INT08_THREADS 8

#define MPXPLAY_TIMER_INT08_EXCLUSION (MPXPLAY_TIMERTYPE_SIGNAL|MPXPLAY_TIMERTYPE_THREAD|MPXPLAY_TIMERFLAG_BUSY)

void mpxplay_timer_execute_int08_funcs(void)
{
 static volatile unsigned int recall_counter;
 unsigned int i;
 if(!funcbit_smp_pointer_get(mpxplay_timed_functions))
  return;
 if(funcbit_smp_test(mtf_flags,MTF_FLAG_LOCK))
  return;
 if(funcbit_smp_int32_get(recall_counter)>=MPXPLAY_TIMER_MAX_PARALELL_INT08_THREADS)
  return;
 funcbit_smp_int32_increment(recall_counter);
 for(i=0;(i<funcbit_smp_int32_get(mtf_storage)) && funcbit_smp_pointer_get(mpxplay_timed_functions);i++){
  mpxplay_timed_s *mtf=(mpxplay_timed_s *)&mpxplay_timed_functions[i];
  void *mtf_func=funcbit_smp_pointer_get(mtf->func);
  if(mtf_func && funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERTYPE_INT08) && !funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMER_INT08_EXCLUSION)){
   if(pds_threads_timer_tick_get() >= (funcbit_smp_int32_get(mtf->refresh_counter) + funcbit_smp_int32_get(mtf->refresh_delay))){

#if defined(__DOS__) && !defined(SBEMU)
    if(funcbit_test(mtf->timer_flags,MPXPLAY_TIMERFLAG_INDOS)){
     if(oldint08_running)
      continue;
     if(pds_filehand_check_infilehand())
      continue;
     if(pds_indos_flag())
      continue;
    }
#endif

    funcbit_smp_enable(mtf->timer_flags,MPXPLAY_TIMERFLAG_BUSY);

    funcbit_smp_int32_put(mtf->refresh_counter, pds_threads_timer_tick_get());

#if defined(__DOS__) && defined(__WATCOMC__)
    if(funcbit_test(mtf->timer_flags,(MPXPLAY_TIMERFLAG_OWNSTACK|MPXPLAY_TIMERFLAG_OWNSTCK2))){
     unsigned int stack_size = (mtf->timer_flags&MPXPLAY_TIMERFLAG_OWNSTCK2)? MPXPLAY_TIMERS_STACKSIZE_LARGE:MPXPLAY_TIMERS_STACKSIZE_SMALL;
     mtf->newstack=(char far *)(mtf->ownstackmem + stack_size); // ???
     if(mtf->data){
      if(funcbit_test(mtf->timer_flags,MPXPLAY_TIMERFLAG_STI))
       call_func_ownstack_withdata_sti(mtf->data,mtf_func,&mtf->oldstack,&mtf->newstack);
      else
       call_func_ownstack_withdata(mtf->data,mtf_func,&mtf->oldstack,&mtf->newstack);
     }else{
      if(funcbit_test(mtf->timer_flags,MPXPLAY_TIMERFLAG_STI))
       call_func_ownstack_nodata_sti(mtf_func,&mtf->oldstack,&mtf->newstack);
      else
       call_func_ownstack_nodata(mtf_func,&mtf->oldstack,&mtf->newstack);
     }
    }else
#endif
    { // no stack, no sti
     if(funcbit_smp_pointer_get(mtf->data))
      ((call_timedfunc_withdata)(mtf_func))(mtf->data);
     else
      ((call_timedfunc_nodata)(mtf_func))();
    }

    if(!funcbit_smp_pointer_get(mpxplay_timed_functions))
     break;

    mtf=(mpxplay_timed_s *)&mpxplay_timed_functions[i];

    if(!funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERTYPE_REPEAT))
     newfunc_timer_delete_entry(mtf);

    funcbit_smp_disable(mtf->timer_flags,MPXPLAY_TIMERFLAG_BUSY);
   }
  }
 }

 if(funcbit_smp_int32_get(recall_counter)>0)
  funcbit_smp_int32_decrement(recall_counter);
}

static void mpxplay_timer_delete_int08_funcs(void)
{
 unsigned int i, retry, countend = pds_threads_timer_tick_get() + 64;
 if(!funcbit_smp_pointer_get(mpxplay_timed_functions))
  return;
 do{
  mpxplay_timed_s *mtf=(mpxplay_timed_s *)mpxplay_timed_functions;
  retry=0;
  for(i=0;i<mtf_storage;i++,mtf++){
   if(mtf->func && funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERTYPE_INT08)){
    if(!funcbit_smp_test(mtf->timer_flags,MPXPLAY_TIMERFLAG_BUSY) || (pds_threads_timer_tick_get() > countend)){
     funcbit_smp_enable(mtf->timer_flags,MPXPLAY_TIMERFLAG_BUSY);
     newfunc_timer_delete_entry(mtf);
    }else{
     funcbit_smp_disable(mtf->timer_flags,MPXPLAY_TIMERTYPE_REPEAT);
     retry=1;
    }
   }
  }
 }while(retry);
}

//----------------------------------------------------------------------
// INT08 and win32 thread handling (interrupt decoder/dma_monitor/etc.)

#ifdef MPXPLAY_WIN32

static mpxp_thread_id_type int08_thread_handle;
static mpxp_thread_id_type handle_maincycle1,handle_maincycle2;
static void *timer_maincycle_init, *main_cycle1, *main_cycle2;
static mpxp_thread_id_type int08_thread_id;
//static unsigned int int08_timer_handle;
static int int08_timer_period;

#if defined(PDS_THREADS_POSIX_THREAD)
static void *newhandler_08_thread(void)
#else
static unsigned int newhandler_08_thread(void *unused)
#endif
{
 int08_thread_id = pds_threads_threadid_current();

 do{
  funcbit_smp_enable(intsoundcontrol,INTSOUND_INT08RUN);
  intdec_timer_counter = pds_threads_timer_tick_get();
  pds_threads_thread_suspend(handle_maincycle1);
  pds_threads_thread_suspend(handle_maincycle2);
  do{
   mpxplay_timer_execute_int08_funcs();
  }while((mvps.aui->card_infobits&AUINFOS_CARDINFOBIT_PLAYING) && !funcbit_test(mvps.aui->card_infobits,AUINFOS_CARDINFOBIT_DMAFULL) && (mvps.idone == MPXPLAY_ERROR_OK));// || (mvps.idone == MPXPLAY_ERROR_INFILE_SYNC_IN)));
  funcbit_smp_disable(intsoundcontrol,INTSOUND_INT08RUN);
  pds_threads_thread_resume(handle_maincycle1, MPXPLAY_THREAD_PRIORITY_NORMAL);
  pds_threads_thread_resume(handle_maincycle2, MPXPLAY_THREAD_PRIORITY_NORMAL);
  pds_threads_sleep(5);  // FIXME: on windows only and assuming more than 1/200 timer period
   //pds_threads_timer_waitable_lock(int08_timer_handle, int08_timer_period);
 }while(mpxplay_timed_functions);
 return 0;
}

void newfunc_newhandler08_init(void)
{
 int08_timer_period = pds_threads_timer_period_set((int)(1000.0 / (float)INT08_CYCLES_NEW));
 //int08_timer_handle = pds_threads_timer_waitable_create(int08_timer_period);
 pds_threads_set_singlecore();
#if defined(MPXPLAY_THREADS_HYPERTREADING_DISABLE)
 pds_threads_hyperthreading_disable();
#endif
 int08_thread_handle = pds_threads_thread_create((mpxp_thread_func)newhandler_08_thread, NULL, MPXPLAY_THREAD_PRIORITY_HIGHER, MPXPLAY_AUDIODECODER_THREAD_AFFINITY_MASK);
}

unsigned int newfunc_newhandler08_is_current_thread(void)
{
 if(pds_threads_threadid_current()==int08_thread_id)
  return 1;
 return 0;
}

void newfunc_newhandler08_waitfor_threadend(void)
{
 if(!newfunc_newhandler08_is_current_thread()){
  unsigned int retry=10;
  while(funcbit_smp_test(intsoundcontrol,INTSOUND_INT08RUN) && (--retry))
   pds_threads_sleep(0);
 }
}

void newfunc_timer_threads_suspend(void)
{
 funcbit_smp_disable(intsoundcontrol,INTSOUND_DECODER);
 if(int08_thread_handle){
  newfunc_newhandler08_waitfor_threadend();
  pds_threads_thread_suspend(int08_thread_handle);
 }
 pds_threads_thread_suspend(handle_maincycle1);
 pds_threads_thread_suspend(handle_maincycle2);
}

void newfunc_newhandler08_close(void)
{
 newfunc_timer_threads_suspend();
 mpxplay_timer_close();
 pds_threads_thread_close(int08_thread_handle);
 pds_threads_thread_close(handle_maincycle1);
 pds_threads_thread_close(handle_maincycle2);
 //pds_threads_timer_waitable_close(int08_timer_handle);
 pds_threads_timer_period_reset(int08_timer_period);
}

#if defined(PDS_THREADS_POSIX_THREAD)
static void *thread_maincycle_1(void *mvp_p)
#else
static unsigned int thread_maincycle_1(void *mvp_p)
#endif
{
 if(timer_maincycle_init)
  ((call_timedfunc_nodata)(timer_maincycle_init))();
 do{
  ((call_timedfunc_nodata)(main_cycle1))();
  if(main_cycle2 || funcbit_test(intsoundcontrol, INTSOUND_INT08RUN))
   pds_threads_sleep(0);
 }while(mvps.partselect);
 return 0;
}

#if defined(PDS_THREADS_POSIX_THREAD)
static void *thread_maincycle_2(void *mvp_p)
#else
static unsigned int thread_maincycle_2(void *mvp_p)
#endif
{
 do{
  ((call_timedfunc_nodata)(main_cycle2))();
  if(!funcbit_smp_test(mpxplay_signal_events, MPXPLAY_SIGNALMASK_OTHER))
   pds_threads_sleep(1000 / INT08_CYCLES_NEW);
  else
   pds_threads_sleep(0);
 }while(mvps.partselect);
 return 0;
}

unsigned int newfunc_newhandler08_maincycles_init(struct mainvars *mvp,void *cycleinit, void *cycle1,void *cycle2)
{
 if(cycle1){
  timer_maincycle_init = cycleinit;
  main_cycle1 = cycle1;
  handle_maincycle1 = pds_threads_thread_create((mpxp_thread_func)thread_maincycle_1, (void *)mvp, MPXPLAY_THREAD_PRIORITY_NORMAL, MPXPLAY_AUDIODECODER_THREAD_AFFINITY_MASK);
 }
 if(cycle2){
  main_cycle2 = cycle2;
  handle_maincycle2 = pds_threads_thread_create((mpxp_thread_func)thread_maincycle_2, (void *)mvp, MPXPLAY_THREAD_PRIORITY_NORMAL, MPXPLAY_AUDIODECODER_THREAD_AFFINITY_MASK);
 }
 return 1;
}

#elif defined(__DOS__)

static unsigned int oldint08_timercount;
int_handler_t oldint08_handler;

void loades(void);
#pragma aux loades = "push ds" "pop es"

void savefpu(void);
#pragma aux savefpu = "sub esp,200" "fsave [esp]"

void clearfpu(void);
#pragma aux clearfpu = "finit"

void restorefpu(void);
#pragma aux restorefpu = "frstor [esp]" "add esp,200"

void cld(void);
#pragma aux cld="cld"

#if defined(DJGPP)
#define loades() { asm("push %ds\n\t pop %es");}
#define savefpu() {asm("sub $200, %esp\n\t fsave (%esp)");}
#define clearfpu() {asm("finit");}
#define restorefpu() {asm("frstor (%esp)\n\t add $200, %esp");}
#define cld() {asm("cld");}
#endif

static void __interrupt __loadds newhandler_08(void)
{
 savefpu();
 clearfpu();
#ifdef __WATCOMC__
 loades();
#endif

 int08counter++; // for the general timing

 intdec_timer_counter+=INT08_DIVISOR_NEW; // for CPU usage (at interrupt-decoder)

 oldint08_timercount+=INT08_DIVISOR_NEW; // for the old-int08 handler

 if((oldint08_timercount&0xFFFF0000) && !oldint08_running){
  oldint08_running=1;
  oldint08_timercount-=0x00010000;
  pds_call_int_handler(oldint08_handler);
  cld();
  oldint08_running=0;
 }else{
  outp(0x20,0x20);
 }

 mpxplay_timer_execute_int08_funcs();

 restorefpu();
}

void newfunc_newhandler08_init(void)
{
 if(!pds_valid_int_handler(oldint08_handler)){
  oldint08_handler=(int_handler_t)pds_dos_getvect(MPXPLAY_TIMER_INT);
  //printf("INT08: %04x:%08x\n", oldint08_handler.sel, oldint08_handler.off);
  pds_dos_setvect(MPXPLAY_TIMER_INT, pds_int_handler(newhandler_08));
  outp(0x43, 0x34);
  outp(0x40, (INT08_DIVISOR_NEW&0xff));
  outp(0x40, (INT08_DIVISOR_NEW>>8));
 }
}

void newfunc_timer_threads_suspend(void)
{
 funcbit_smp_disable(intsoundcontrol,INTSOUND_DECODER);
}

void newfunc_newhandler08_close(void)
{
 newfunc_timer_threads_suspend();
 mpxplay_timer_close();
 if(pds_valid_int_handler(oldint08_handler)){
  pds_dos_setvect(MPXPLAY_TIMER_INT,oldint08_handler);
  outp(0x43, 0x34);
  outp(0x40, 0x00);
  outp(0x40, 0x00);
 }
}

#else // other OSes :)

#endif
