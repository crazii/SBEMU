//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2015 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: playlist definitions

#ifndef mpxplay_playlist_h
#define mpxplay_playlist_h

#ifdef __cplusplus
extern "C" {
#endif

#define PLAYLIST_MAX_SIDES           2
#ifdef MPXPLAY_GUI_QT
#define PLAYLIST_MAX_TABS_PER_SIDE   16
#else
#define PLAYLIST_MAX_TABS_PER_SIDE   8
#endif
#define PLAYLIST_MAX_COLUMNS_PER_TAB 4
#define PLAYLIST_MAX_ORDERKEYS       5  // 5. for internal use only

#define MAX_SUBLIST_LEVELS    8
#define MAX_ID3LEN         1024   //artist+title+etc.
#define MAX_ID3_DISKFILE_ARTIST_LEN  40 // FIXME:

#define LOADDIR_MAX_LOCAL_DRIVES   ('Z'-'A'+1)
#define LOADDIR_MAX_VIRTUAL_DRIVES ('7'-'0'+1)
#define LOADDIR_MAX_DRIVES (LOADDIR_MAX_LOCAL_DRIVES+LOADDIR_MAX_VIRTUAL_DRIVES)

#define LOADDIR_FIRSTDRIVE        0           // A:
#define LOADDIR_FIRSTDRV_BROWSER  2           // C:
#define LOADDIR_FIRSTDRV_VIRTUAL  LOADDIR_MAX_LOCAL_DRIVES // 0:

#define LOADDIR_DRIVE_DRIVENAME_LEN   3
#define LOADDIR_DRIVE_VOLUMENAME_LEN 31

//definitions for static playlist (if MaxFilenames>0 in mpxplay.ini)
#ifdef MPXPLAY_LINK_FULL
 #ifdef MPXPLAY_UTF8
 #define FILENAMELENGTH  80
 #define ID3LENGTH       80
 #else
 #define FILENAMELENGTH  64  //average ((9999+1999)*64=767872 bytes playlist size)
 #define ID3LENGTH       64  //average (dinamically used)
 #endif
#else
#define FILENAMELENGTH  32
#define ID3LENGTH       32
#endif

#ifdef __DOS__
#define PLAYLIST_ENTRIES_EXPAND_SIZE 1000 // at dynamic playlist
#else
#define PLAYLIST_ENTRIES_EXPAND_SIZE 3000 // new = curr*2 + expand_size
#endif
#define PLAYLIST_MAXFILENUMDIV_DIR 5 // max_filenum_dir=max_filenum_list/5
#define MAX_ID3LISTPARTS 4

// directory file type (HFT_DFT) masks
#define DFTM_DFT      0x10000000
#define DFTM_DRIVE    0x0004
#define DFTM_DIR      0x0008
#define DFTM_UPDIR    0x0001
#define DFTM_SUBDIR   0x0002
#define DFTM_PLAYLIST 0x0010
#define DFTM_UPLIST   0x0001
#define DFTM_SUBLIST  0x0002
#define DFTM_ROOT     0x0040  // change to root dir/list

// directory file types (0x1) in pei->entrytype
#define DFT_DRIVE    (DFTM_DFT|DFTM_DRIVE)
#define DFT_UPDIR    (DFTM_DFT|DFTM_DIR|DFTM_UPDIR)
#define DFT_SUBDIR   (DFTM_DFT|DFTM_DIR|DFTM_SUBDIR)
#define DFT_ROOTDIR  (DFTM_DFT|DFTM_DIR|DFTM_ROOT)
#define DFT_PLAYLIST (DFTM_DFT|DFTM_PLAYLIST)
#define DFT_UPLIST   (DFTM_DFT|DFTM_PLAYLIST|DFTM_UPLIST)
#define DFT_SUBLIST  (DFTM_DFT|DFTM_PLAYLIST|DFTM_SUBLIST)
#define DFT_ROOTLIST (DFTM_DFT|DFTM_PLAYLIST|DFTM_ROOT)

#define DFT_NOTCHECKED 0x00000000 // not checked, but possible valid audio file
#define DFT_UNKNOWN    0x00000001 // unknown/invalid entry
#define DFT_AUDIOFILE  0x20000000 // file is not a DFT

#ifdef MPXPLAY_GUI_CONSOLE
#define DFTSTR_DRIVE    "<DRIVE>"
#define DFTSTR_UPDIR    "<UP--DIR>"
#define DFTSTR_SUBDIR   "<SUB-DIR>"
#define DFTSTR_PLAYLIST "[playlist]"
#define DFTSTR_SUBLIST  "[sub-list]"
#define DFTSTR_UPLIST   "[up-list]"
#else
#define DFTSTR_DRIVE    "<DRV>"
#define DFTSTR_UPDIR    "<UP>"
#define DFTSTR_SUBDIR   "<DIR>"
#define DFTSTR_PLAYLIST "[LIST]"
#define DFTSTR_SUBLIST  "[LIST]"
#define DFTSTR_UPLIST   "[ UP ]"
#endif

//mpxplay_playlistcontrol
#define MPXPLAY_PLAYLISTC_NOLISTWARNINGS (1<<0)
#define MPXPLAY_PLAYLISTC_DIRBROWSER1    (1<<1) // -db
#define MPXPLAY_PLAYLISTC_DIRBROWSER2    (1<<2) // -db2
#define MPXPLAY_PLAYLISTC_ID3LIST_LOCAL  (1<<3) // -il
#define MPXPLAY_PLAYLISTC_NONFILT_SEARCH (1<<4) // non-filtered search (old alt-letter search method)
#define MPXPLAY_PLAYLISTC_MASK_CONFIG    ((1<<24)-1)
#define MPXPLAY_PLAYLISTC_DB2_LOADED     (1<<30) // internal only
#define MPXPLAY_PLAYLISTC_ID3LIST_GLOBAL (1<<31)
#define MPXPLAY_PLAYLISTC_MASK_ID3LIST (MPXPLAY_PLAYLISTC_ID3LIST_LOCAL|MPXPLAY_PLAYLISTC_ID3LIST_GLOBAL)

// editsidetype bits (left,right side)
#define PLT_ENABLED    (1<< 0)  // playlist used/enabled (not empty)
#define PLT_EXTENDED   (1<< 1)  // extended playlist (mxu,pls,extm3u)
#define PLT_EXTENDINC  (1<< 2)  // incomplete extended playlist (extm3u with missing infos)
#define PLT_DIRECTORY  (1<< 4)  // directory browser
#define PLT_LOADDIR_PROCESS      (1<<15) // directory loading running
#define PLT_SORTC_DESCENDING     (1<<16) // sort control (have to match with 'sortcontrol')
#define PLT_SORTC_MANUALCOPY     (1<<17) // ie: disable filename sort in directory browser at F5
#define PLT_SORTC_MAGNETHIGHLINE (1<<18) // magnetize (move) editorhighline at sort

// playlistload types
#define PLL_LOADLIST   (1<< 0) // load playlist (-@)
#define PLL_JUKEBOX    (1<< 2) // jukebox queue mode
#define PLL_DIRSCAN    (1<< 3) // scan directori(es) for files
#define PLL_DRIVESCAN  (1<< 4) // scan drive(s) for file(s)
#define PLL_FASTLIST   (1<< 5) // [fastlists] are used (not a command line list/file)
#define PLL_STDIN      (1<< 6) // load playlist from stdin
#define PLL_SUBLISTS   (1<< 7) // sublist flag (for startup)
#define PLL_FILTERED   (1<< 8) // the list is filtered
#define PLL_LOCKTAB    (1<< 9) // the tab is locked
#define PLL_RESTORED   (1<<10) // list(s) was restored from mpxptabs.ini (saved in a MPXPnnnn.CUE file) (not from command line)
#define PLL_FILTCHKSKP (1<<11) // list has filtered entries with skipped chkentry
#define PLL_CHG_MANUAL (1<<16) // manual playlist change (for "playlist changed" sign)
#define PLL_CHG_ENTRY  (1<<17) // entry(s) has changed (or tab shifted) (usually manual change)
#define PLL_CHG_LEN    (1<<18) // length has changed
#define PLL_CHG_ID3    (1<<19) // id3 has changed
#define PLL_CHG_FILTER (1<<20) // list/entry filter has changed
#define PLL_TYPE_CMDL  (PLL_LOADLIST|PLL_DIRSCAN|PLL_DRIVESCAN|PLL_STDIN) // command-line loading-types
#define PLL_TYPE_LOAD  (PLL_TYPE_CMDL|PLL_FASTLIST)  // all loading-types
#define PLL_TYPE_LISTS (PLL_TYPE_LOAD|PLL_SUBLISTS)  // all list types
#define PLL_TYPE_SAVED (PLL_TYPE_LISTS|PLL_JUKEBOX|PLL_FILTERED|PLL_LOCKTAB|PLL_RESTORED|PLL_FILTCHKSKP|PLL_CHG_MANUAL) // saved flags from psi->editloadtype to Oldlisttype
#define PLL_CHG_AUTO   (PLL_CHG_ENTRY|PLL_CHG_LEN|PLL_CHG_ID3|PLL_CHG_FILTER)
#define PLL_CHG_ALL    (PLL_CHG_MANUAL|PLL_CHG_AUTO)

// preloadinfo types
#define PLI_NOTLOAD    0  // don't load id3infos (load at skip only) (-inl)
#define PLI_PRELOAD    1  // load all infos at the start of program
#define PLI_PLAYLOAD   2  // load infos under playing (-ipl)
#define PLI_DISPLOAD   3  // load infos of displayed files (controlled from playlist editor) (-idl)
#define PLI_EHLINELOAD 4  // load infos of highlighted file (-"-) (-ihl)

// sortcontrol flag(s)
#define PLAYLIST_SORTCONTROL_DESCENDING  (1<<0) // descending sort (else ascending)
#define PLAYLIST_SORTCONTROL_CMD_DIR     (1<<1) // commander mode sort in directories (only DRIVEs & DIRs are in one group)
#define PLAYLIST_SORTCONTROL_NOAUTODESC  (1<<2) // disable auto descending for specified coloumns
#define PLAYLIST_SORTCONTROL_NOLISTTOP   (1<<3) // don't keep lists on top of directory
#define PLAYLIST_SORTCONTROL_CONFIG      (PLAYLIST_SORTCONTROL_CMD_DIR|PLAYLIST_SORTCONTROL_NOAUTODESC|PLAYLIST_SORTCONTROL_NOLISTTOP)

// loadid3tag
// external config
#define ID3LOADMODE_NONE              0  // don't use id3 infos at all (-in)
#define ID3LOADMODE_LIST        (1 << 0) // mxu,extm3u,id3list
#define ID3LOADMODE_FILE        (1 << 1) // id3tag from file
#define ID3LOADMODE_CLFN        (1 << 2) // try to create artist:title from a (long)filename
#define ID3LOADMODE_PREFER_LIST (1 << 3) // prefer infos from list (else prefer infos from file)(if list-id3infos (artist,title) exists, program will use those)
#define ID3LOADMODE_SLOWACCESS  (1 << 4) // load infos from slow-access devices (ftp,http) too
#define ID3LOADMODE_LOWLEVEL    (1 << 5) // load infos from low level devices (http) too
#define ID3LOADMODE_DEFAULT     (ID3LOADMODE_LIST|ID3LOADMODE_FILE|ID3LOADMODE_CLFN|ID3LOADMODE_LOWLEVEL)
#define ID3LOADMODE_ALL         (ID3LOADMODE_LIST|ID3LOADMODE_FILE|ID3LOADMODE_CLFN|ID3LOADMODE_LOWLEVEL)
#define ID3LOADMODE_NOFILE      (ID3LOADMODE_LIST|ID3LOADMODE_CLFN) // -inf
#define ID3LOADMODE_MASK_CONFIG (ID3LOADMODE_ALL|ID3LOADMODE_PREFER_LIST|ID3LOADMODE_SLOWACCESS) // saved in mpxplay.ini
// internal flags
#define ID3LOADMODE_OTHERENTRY  (1 <<  8) // get infos from other entry (prev index or search on other side) (it's not in the ALL !)
#define ID3LOADMODE_FASTSEARCH  (1 << 10) // we don't need deep infos (at collecting infos for editor)

// id3order types
#define ID3ORDER_ARTIST    I3I_ARTIST
#define ID3ORDER_TITLE     I3I_TITLE
#define ID3ORDER_ALBUM     I3I_ALBUM
#define ID3ORDER_YEAR      I3I_YEAR
#define ID3ORDER_COMMENT   I3I_COMMENT
#define ID3ORDER_GENRE     I3I_GENRE
#define ID3ORDER_TRACKNUM  I3I_TRACKNUM
#define ID3ORDER_TIME      7   // song duration
#define ID3ORDER_PATH      8   // path only (without filename)
#define ID3ORDER_FILENAME  9   // filename only (without path)
#define ID3ORDER_PATHFILE 10   // path + filename
#define ID3ORDER_FILEEXT  11   // file extension
#define ID3ORDER_FILESIZE 12   // file size
#define ID3ORDER_FILEDATE 13   // pei->filedate
#define ID3ORDER_DFT      14   // pei->entrytype
#define ID3ORDER_INDEX    15   // pei->pstime
#define ID3ORDER_DISABLED 16   // no order
#define ID3ORDER_HEXA_DISABLED 0x7FFFFFFF

// playlistsave types
#define PLST_AUTO     1   // save to mpxplay's dir
#define PLST_MANUAL   2   // save to starting/selected dir
#define PLST_MXU      4   // save in mxu format
#define PLST_EXTM3U   8   // winamp's style m3u (with time,artist,title infos)
#define PLST_CUE     16   //
#define PLST_M3U     256  // used internally only
#define PLST_LISTS   (PLST_MXU|PLST_EXTM3U|PLST_CUE)
#define PLST_DEFAULT PLST_EXTM3U //

// id3savefield types
#define IST_DIRECTORY 1   // separate directories
#define IST_FILENAME  2   // write filename in the lines
#define IST_AT_FIXED  4   // put artist:title at fixed position
#define IST_TIME      8   // write time in the lines
#define IST_BITRATE   16  // write bitrate
#define IST_FILESIZE  32  // write filesize
#define IST_FULLPATH  64  //
#define IST_DEFAULT  (IST_DIRECTORY|IST_AT_FIXED|IST_TIME|IST_BITRATE|IST_FILESIZE) //=61

// mxu flags (old mptime[]/pei->timesec)
#define MXUFLAG_TIMEMASK    0x000fffff // time (in seconds) is stored on 20 bits (it's enough for a more than 12 days long audio)
#define MXUFLAG_ENABLED     0x80000000 // audio file

// pei->infobits
#define PEIF_ENABLED        (1 <<  0)  // enabled entry (checked) - an audio file
#define PEIF_ID3LOADED      (1 <<  1)  // id3 loaded from the file (1 time at least)
#define PEIF_ID3EXIST       (1 <<  2)  // file has id3 info
#define PEIF_RNDPLAYED      (1 <<  3)  // entry is already played in random mode
#define PEIF_RNDSIGNED      (1 <<  4)  //
#define PEIF_ID3FILENAME    (1 <<  5)  // ID3 is created from filename only (artist - title.mp3)
#define PEIF_SELECTED       (1 <<  6)  // entry is selected in playlist editor
#define PEIF_INDEXED        (1 <<  7)  // entry is indexed (by CUE)
#define PEIF_ALLOCATED      (1 <<  8)  // pointers (filename,id3info) use dynamic data (allocated with malloc)
#define PEIF_SORTED         (1 <<  9)  // entry is already sorted (don't re-order it again)
#define PEIF_FILTEROUT      (1 << 10)  // hided by alt-letter search
#define PEIF_FULLTIMEADDED  (1 << 11)  // entry has been added to fulltime
#define PEIF_ID3MASK     (PEIF_ID3LOADED|PEIF_ID3EXIST)
#define PEIF_COPYMASK    (PEIF_ENABLED)
#define PEIF_CPYMASK_TAB (PEIF_COPYMASK|PEIF_ID3MASK|PEIF_INDEXED|PEIF_FILTEROUT)
#define PEIF_CUESAVEMASK (PEIF_ENABLED|PEIF_RNDPLAYED|PEIF_INDEXED|PEIF_FILTEROUT)

// add/delete/found
#define EDITLIST_MODE_FILENAME  (1<< 0)
#define EDITLIST_MODE_HEAD      (1<< 1)
#define EDITLIST_MODE_INDEX     (1<< 2)
#define EDITLIST_MODE_ID3       (1<< 3)
#define EDITLIST_MODE_ENTRY     (1<< 4)
#define EDITLIST_MODE_TABADD    (1<< 5)
#define EDITLIST_MODE_ALL      (EDITLIST_MODE_FILENAME|EDITLIST_MODE_HEAD|EDITLIST_MODE_INDEX|EDITLIST_MODE_ID3|EDITLIST_MODE_ENTRY)

//streams
#define PEI_TIMEMSEC_STREAM  (49*3600*1000) // 49 hours false stream length

#define PLAYLIST_IS_DIRECTORY(psiptr) (((psiptr)->editsidetype&PLT_DIRECTORY) && !(psiptr)->sublistlevel)

//------------------------------------------------------------------------
//playlist structures

#pragma pack(push,1)

typedef struct playlist_entry_info{
 unsigned long entrytype;  // HFT_ << 28
 unsigned long infobits;   // PEIF_
 unsigned long timemsec;   // time length in milliseconds (max 49 days on 32 bits -> enough)
 mpxp_filesize_t filesize;
 char *filename;
 char *id3info[I3I_MAX+1]; // !!! extend it to store filename without path
 struct mpxplay_diskdrive_data_s *mdds;
 struct mpxplay_infile_func_s *infile_funcs;
 unsigned long pstime; // play-start-time of the entry in msecs (CUE)
 unsigned long petime; // play-count-time of the entry in msecs (CUE)
 struct playlist_entry_info *myself; // sort helper
 pds_fdate_t  filedate;
}playlist_entry_info; // 72 bytes (!!! update editlist.c asm if this changes)

#pragma pack(pop)

typedef int check_order_func_t(struct playlist_entry_info *pei0,struct playlist_entry_info *pei1);

typedef struct playlist_side_info{
 unsigned long editsidetype;   // side infobits
 unsigned long editloadtype;   // PLL_ bits for side
 unsigned int sidenum;
 unsigned int tabnum;

 struct playlist_entry_info *editorhighline;

 struct playlist_entry_info *firstentry;   // 1. playlist entry
 struct playlist_entry_info *firstfile;    // 1. non dir entry
 struct playlist_entry_info *firstsong;    // 1. audio song (usually)
 struct playlist_entry_info *lastentry;    // last used entry
 struct playlist_entry_info *endentry;     // last possible entry

 unsigned int filenum_loaded;
 unsigned int filenum_allocated;
 int allfilenum;              // all possible entries (on this side)

 char *filenamesbeginp;       // area begin
 char *filenameslastp;        // first unused (last used + 1) memory point
 char *filenamesendp;         // area end

 char *id3infobeginp;         // area begin
 char *id3infolastp;          // first unused (last used + 1) memory point
 char *id3infoendp;           // area end

 struct playlist_entry_info *chkfilenum_begin;
 struct playlist_entry_info *chkfilenum_curr;
 struct playlist_entry_info *chkfilenum_end;

 struct playlist_entry_info *editor_from;
 struct playlist_entry_info *editor_from_prev; // for -idl

 unsigned int fulltimesec;
 unsigned int selected_files; // PEIF_SELECTED
 unsigned int filtered_files; // !PEIF_FILTEROUT

 unsigned int id3ordernum_primary; // ID3ORDER_
 check_order_func_t *id3ordertype[PLAYLIST_MAX_ORDERKEYS];

 struct playlist_side_info *psio; // other side's info
 struct mainvars *mvp;            // &mvps

 struct mpxplay_diskdrive_data_s *mdds;

 unsigned int currdrive;          // -db2
 char currdir[MAX_PATHNAMELEN];   // -db2

 unsigned int savelist_type;              // PLST_xxx
 unsigned long savelist_textcodetype;     // utf16,utf8,ascii
 char savelist_filename[MAX_PATHNAMELEN]; //

 unsigned int sublistlevel;       // -db2
 char sublistnames[MAX_SUBLIST_LEVELS+1][MAX_PATHNAMELEN];

 char restored_filename[MAX_PATHNAMELEN]; // MPXPnnnn.CUE (because sublistnames[sublistlevel] contains the savelist_filename after sublist load)
 mpxp_wchar_t mckic_searchstring[32];     // mpxplay_control_keyboard_id3search string (for all tabs) !!! in UTF16 at MPXPLAY_UTF8

}playlist_side_info;

#ifdef MPXPLAY_WIN32
typedef struct mpxp_credentials_s{
 unsigned long credentials_control;
 mpxp_ptrsize_t *credentials_hToken;
 int *retcodep;
 char *filename;
 void *passdata;
 void (*passfunc_retry)(void *passdata);
 void (*passfunc_skip)(void *passdata);
 void (*passfunc_statwinclose)(void *passdata);
 void (*passfunc_dealloc)(void *passdata);
 char credentials_username[96];
 char credentials_password[96];
 //char credentials_domain[128];
}mpxp_credentials_s;
#endif

typedef struct mpxplay_virtualdrivemount_s{
 struct playlist_side_info *psi;
 void *tw;
 unsigned int ti;
 unsigned int fullinitside;
 unsigned int retry;
 unsigned int allocated;
 mpxp_uint64_t count_end;
 unsigned int drivenum;
 char path[MAX_PATHNAMELEN];
 char errortext[MAX_PATHNAMELEN];
}mpxplay_virtualdrivemount_s;

//-------------------------------------------------------------------------
//playlist functions

//playlist.c
extern void mpxplay_playlist_init(struct mainvars *);
extern void mpxplay_playlist_close(struct mainvars *);
#ifdef MPXPLAY_GUI_CONSOLE
extern void playlist_init_pointers(struct mainvars *);
#else
#define playlist_init_pointers(mvp)
#endif
extern void playlist_peimyself_reset(struct playlist_side_info *psi,struct playlist_entry_info *firstentry,struct playlist_entry_info *lastentry);
extern struct playlist_entry_info *playlist_peimyself_search(struct playlist_side_info *psi,struct playlist_entry_info *pei_src);
extern void playlist_enable_side(struct playlist_side_info *);
extern void playlist_disable_side_partial(struct playlist_side_info *);
extern void playlist_disable_side_list(struct playlist_side_info *);
extern void playlist_disable_side_full(struct playlist_side_info *);
extern void mpxplay_playlist_sideclear_entries(struct playlist_side_info *);
extern void mpxplay_playlist_sideclear_sidetype(struct playlist_side_info *);
extern void mpxplay_playlist_sideclear_search(struct playlist_side_info *psi);
extern void mpxplay_playlist_sideclear_startup(struct playlist_side_info *psi);
extern void playlist_copy_side_infos(struct playlist_side_info *psi1,struct playlist_side_info *psi2);
extern void playlist_clear_freeopts(void);

extern char *mpxplay_playlist_startdir(void);
extern void mpxplay_save_startdir(struct mainvars *);
extern void mpxplay_restore_startdir(void);
extern void mpxplay_playlist_startfile_fullpath(char *fullpath,char *filename);

extern unsigned int playlist_buildlist_one(struct playlist_side_info *psi,char *listfile,unsigned int loadtype,char *dslp,char *filtermask);
extern unsigned int playlist_buildlist_all(struct playlist_side_info *psi,unsigned int multiply_inputs);
extern void playlist_get_allfilenames(struct mainvars *);
extern void playlist_init_playside(struct mainvars *);
extern void playlist_init_playsong(struct mainvars *);
extern void playlist_start_sideplay(struct mainvars *mvp,struct playlist_side_info *psi);
extern int  playlist_open_infile(struct playlist_side_info *psi,struct playlist_entry_info *pei,struct mpxpframe_s *frp);
extern void playlist_close_infile(struct mainvars *mvp,struct mpxpframe_s *frp,unsigned int stopclearflags);
extern void playlist_pei0_set(struct mainvars *mvp,struct playlist_entry_info *pei,unsigned long update_mode);

extern void playlist_editorhighline_check(struct playlist_side_info *psi);
extern void playlist_editorhighline_seek(struct playlist_side_info *psi,long offset,int whence);
extern void playlist_editorhighline_set(struct playlist_side_info *,struct playlist_entry_info *);
extern void playlist_editorhighline_set_nocenter(struct playlist_side_info *psi,struct playlist_entry_info *pei);
extern void playlist_change_editorside(struct mainvars *);

#define PLAYLIST_SEARCHFLAG_BACKWARD (1 << 0)  // backward search (from the end to begin)
#define PLAYLIST_SEARCHFLAG_FASTSRCH (1 << 1)  // fast search mode (simple string compare)
extern playlist_entry_info *playlist_search_filename(struct playlist_side_info *psi,char *filename,long timempos,struct playlist_entry_info *pei_begin,unsigned long flags);
extern void playlist_search_lastdir(struct playlist_side_info *psi,char *lastdir);
extern void playlist_search_firstsong(struct playlist_side_info *psi);
extern void playlist_change_sublist_or_directory(struct playlist_side_info *,unsigned long head);
extern void playlist_reload_side(struct mainvars *,struct playlist_side_info *);
extern void playlist_reload_dirs(struct mainvars *);
extern void playlist_commandermode_switch(struct mainvars *mvp);
extern mpxp_int32_t mpxplay_playlist_modify_entry_alltab_by_mdds_and_filename(void *mdds, unsigned int diskdrv_cb_cmd, void *data1, void *data2);
#define MPXPLAY_PLAYLIST_MODIFY_ENTRY_ALLTAB_CTRLFLAG_EXCLUSIVE  (1 << 0) // update only one side (PLT_LOADDIR_PROCESS or playside, if mdds is NULL) (else update all tabs)
extern mpxp_int32_t mpxplay_playlist_modify_entry_alltab_by_filename(void *p_mdds, unsigned int diskdrv_cb_cmd, mpxp_ptrsize_t data1, mpxp_ptrsize_t data2, unsigned int ctrl_flags);
extern mpxp_int32_t mpxplay_playlist_modify_entry_alltab_sheduler_init(void *p_mdds, unsigned int diskdrv_cb_cmd, mpxp_ptrsize_t data1, mpxp_uint32_t datalen1, mpxp_ptrsize_t data2, mpxp_uint32_t datalen2);

extern void playlist_write_id3tags(struct mainvars *);
extern void playlist_savelist_save_list(struct mainvars *);

//chkentry.c
extern void playlist_chkentry_get_allfileinfos(struct mainvars *);
extern int  playlist_chkentry_get_onefileinfos_play(struct playlist_side_info *,struct playlist_entry_info *,struct mpxpframe_s *frp);
extern int  playlist_chkentry_get_onefileinfos_entry(struct playlist_side_info *psi,struct playlist_entry_info *pei);
extern void playlist_chkentry_get_onefileinfos_is(struct playlist_side_info *,struct playlist_entry_info *);
extern void playlist_chkentry_get_onefileinfos_allagain(struct playlist_side_info *psi,struct playlist_entry_info *pei,struct mpxpframe_s *frp,unsigned int id3loadflags,unsigned int openmode);
extern int  playlist_chkentry_get_onefileinfos_from_file(struct playlist_side_info *psi,struct playlist_entry_info *pei,struct mpxpframe_s *frp,unsigned int id3loadflags,unsigned int found,unsigned int openmode);
extern void playlist_chkentry_load_id3tag_from_file(struct playlist_side_info *psi,struct playlist_entry_info *pei,struct mpxpframe_s *frp,unsigned int id3loadflags,unsigned int found);
extern void playlist_chkentry_create_id3infos_from_filename(struct playlist_side_info *psi,struct playlist_entry_info *pei,unsigned int id3loadflags);

extern void playlist_chkfile_start_norm(struct playlist_side_info *psi,struct playlist_entry_info *startsong);
extern void playlist_chkfile_start_disp(struct playlist_side_info *psi,struct playlist_entry_info *startsong,struct playlist_entry_info *endsong);
extern mpxp_bool_t playlist_chkfile_start_ehline(struct playlist_side_info *psi,struct playlist_entry_info *pei);
extern void playlist_chkfile_stop(struct playlist_side_info *);

extern unsigned long playlist_entry_get_timemsec(struct playlist_entry_info *pei);
extern void playlist_fulltime_add(struct playlist_side_info *psi,struct playlist_entry_info *pei);
extern void playlist_fulltime_del(struct playlist_side_info *psi,struct playlist_entry_info *pei);
extern void playlist_fulltime_clearside(struct playlist_side_info *psi);
extern void playlist_fulltime_getside(struct playlist_side_info *psi);
extern unsigned int playlist_fulltime_getelapsed(struct mainvars *,unsigned int cleartime);

extern void playlist_chkentry_enable_entries(struct playlist_side_info *psi);

//editlist.c
extern struct playlist_entry_info *playlist_editlist_addfileone_postproc(struct playlist_side_info *psi_dest,struct playlist_entry_info *pei_dest);
extern void playlist_editlist_addfile_any(struct playlist_side_info *psi_src,struct playlist_entry_info *pei_src,char *source_filtermask);
extern void playlist_editlist_copy_entry(struct playlist_side_info *psi_src,struct playlist_entry_info *pei_src);
extern struct playlist_side_info *playlist_editlist_tabs_init(struct mainvars *mvp);
extern void playlist_editlist_tabs_close(struct mainvars *mvp);
extern struct playlist_side_info *playlist_editlist_tabp_get(struct mainvars *mvp,int side);
extern struct playlist_side_info *playlist_editlist_tabpsi_get(struct mainvars *mvp,int side,int tabnum);
extern int  playlist_editlist_editside_chg(struct mainvars *mvp,int side);
extern void playlist_editlist_sidetab_select(struct mainvars *mvp,unsigned int side,unsigned int tabnum);
extern void playlist_editlist_tab_select(struct mainvars *mvp,int side,int tabnum);
extern struct playlist_side_info *playlist_editlist_tab_skip(struct mainvars *mvp);
#define EDITLIST_ADDTABMODE_BLANK 1
#define EDITLIST_ADDTABMODE_TOEND 2
struct playlist_side_info *playlist_editlist_tab_add(struct mainvars *mvp, unsigned int mode, int source_sidenum, int source_tabnum, int dest_sidenum);
extern int playlist_editlist_tab_del(struct mainvars *mvp,int side,int tabnum);
extern void playlist_editlist_tab_clear(struct playlist_side_info *psi);
extern void playlist_editlist_tab_close(struct playlist_side_info *psi);
extern struct playlist_entry_info *playlist_editlist_add_entry(struct playlist_side_info *psi);
extern void playlist_editlist_del_entry(struct playlist_side_info *psi,struct playlist_entry_info *pei);
extern int  playlist_editlist_add_filename(struct playlist_side_info *psi,struct playlist_entry_info *pei,char *filename); // returns len or errorcode
extern int  playlist_editlist_add_id3_one(struct playlist_side_info *psi,struct playlist_entry_info *pei,unsigned int i,char *str,int len); // returns len or errorcode
extern unsigned int playlist_editlist_addfile_one(struct playlist_side_info *psi_src,struct playlist_side_info *psi_dest,struct playlist_entry_info *pei_src,struct playlist_entry_info *pei_dest,unsigned int modify);
extern void playlist_editlist_delfile_one(struct playlist_side_info *psi,struct playlist_entry_info *pei,unsigned int modify);
extern void playlist_editlist_del_filename(struct playlist_side_info *psi,struct playlist_entry_info *pei);
extern void playlist_editlist_del_id3_one(struct playlist_side_info *psi,struct playlist_entry_info *pei,unsigned int i);
extern void playlist_editlist_del_id3_all(struct playlist_side_info *psi,struct playlist_entry_info *pei);
extern unsigned int playlist_editlist_entry_skip(struct playlist_side_info *psi, struct playlist_entry_info **pei_pos, int direction);
extern struct playlist_entry_info *playlist_editlist_entry_seek(struct playlist_side_info *psi, struct playlist_entry_info *pei, long offset, int whence);
extern int  playlist_editlist_entry_diff(struct playlist_side_info *psi, struct playlist_entry_info *pei_base, struct playlist_entry_info *pei_pos);
extern unsigned int playlist_editlist_allocated_copy_entry(struct playlist_entry_info *pei_dest,struct playlist_entry_info *pei_src);
extern void playlist_editlist_allocated_clear_entry(struct playlist_entry_info *pei);
extern void playlist_editlist_delete_entry_manual(struct playlist_side_info *psi,struct playlist_entry_info *pei);
extern void playlist_editlist_addfile_ins_ehl(struct playlist_side_info *psi_src,struct playlist_entry_info *pei_src);
extern void playlist_editlist_shiftfile(struct playlist_side_info *,int);
extern void playlist_editlist_mouse_shiftfile(struct mainvars *mvp,struct playlist_entry_info *);
extern unsigned int playlist_editlist_side_clear(struct mainvars *mvp);
extern void playlist_editlist_copyside(struct playlist_side_info *psi_src);
extern void playlist_editlist_addfile_selected_group(struct playlist_side_info *psi_src);
extern void playlist_editlist_copy_selected_group(struct playlist_side_info *psi_src);
extern void playlist_editlist_move_selected_group(struct playlist_side_info *psi_src);
extern void playlist_editlist_delfile_selected_group(struct playlist_side_info *psi);
extern void playlist_editlist_group_select_one(struct playlist_side_info *psi, struct playlist_entry_info *pei);
extern void playlist_editlist_group_select_range(struct playlist_side_info *psi, struct playlist_entry_info *pei_begin, struct playlist_entry_info *pei_end);
extern void playlist_editlist_group_select_all(struct playlist_side_info *psi);
extern void playlist_editlist_group_unselect_all(struct playlist_side_info *psi);
extern void playlist_editlist_group_invert_selection(struct playlist_side_info *psi);
extern void playlist_editlist_groupselect_open(struct playlist_side_info *psi,unsigned int unselect);
extern void playlist_editlist_id3filter(struct mainvars *);
extern void playlist_editlist_compare_directories(struct mainvars *mvp);
extern void playlist_editlist_insert_index(struct mainvars *mvp);
extern void playlist_editlist_delete_index(struct mainvars *mvp);
extern struct playlist_entry_info *playlist_editlist_updatesides_add_dft(struct mainvars *mvp,char *filename,unsigned int dft_type);

//diskfile.c
#define DISKFILE_FILECOPY_BARLEN 54
#ifdef MPXPLAY_UTF8
#define DISKFILE_FILECOPY_BARBUFLEN ((DISKFILE_FILECOPY_BARLEN+1)*3)
#else
#define DISKFILE_FILECOPY_BARBUFLEN (DISKFILE_FILECOPY_BARLEN+1)
#endif
extern void playlist_diskfile_filecopy_makebar(char *barstr,unsigned int percent);
#ifdef MPXPLAY_WIN32
extern unsigned int playlist_diskfile_credentials_logoff(struct mpxp_credentials_s *cri);
extern unsigned int playlist_diskfile_credentials_errorhand(struct mpxp_credentials_s *cri,unsigned int do_cr);
#endif
extern void playlist_diskfile_copy_or_move(struct mainvars *mvp,unsigned int move);
extern void playlist_diskfile_delete_init(struct mainvars *mvp);
extern void playlist_diskfile_show_multifileinfos(struct mainvars *mvp);
extern void playlist_diskfile_rename_by_id3(struct mainvars *mvp);

//fileinfo.c
extern void playlist_fileinfo_edit_infos(struct mainvars *mvp);

//id3list.c
extern void playlist_id3list_close(void);
extern void playlist_id3list_load(struct mainvars *,struct playlist_side_info *);
extern unsigned int get_onefileinfos_from_id3list(struct playlist_side_info *psi,struct playlist_entry_info *pei_dest,unsigned int fastsearch);
extern void playlist_id3list_save(struct mainvars *);

//jukebox.c
extern void playlist_jukebox_add_entry(struct mainvars *mvp,struct playlist_side_info *psi_src);
extern unsigned int playlist_jukebox_skip(struct mainvars *mvp);
extern void playlist_jukebox_switch(struct mainvars *mvp);
extern void playlist_jukebox_set(struct mainvars *mvp,unsigned int loadtype_priority);

//loaddir.c
extern void playlist_loaddir_diskdrives_unmount(void);
extern struct mpxplay_diskdrive_data_s *playlist_loaddir_drivenum_to_drivemap(int drivenum);
extern struct mpxplay_diskdrive_data_s *playlist_loaddir_path_to_drivehandler(struct playlist_side_info *psi, char *path);
extern struct mpxplay_diskdrive_data_s *playlist_loaddir_filename_to_drivehandler(char *filename);
extern int playlist_loaddir_getcwd(struct mpxplay_diskdrive_data_s *mdds,char *buf,unsigned int buflen);
extern unsigned int playlist_get_extension_number(char *);
extern unsigned int playlist_get_extension_filetype(char *);
extern unsigned int playlist_loaddir_virtualdrive_mount(struct mpxplay_virtualdrivemount_s *vdm);
extern void playlist_loaddir_drive_unmount(struct playlist_side_info *psi,struct mpxplay_diskdrive_data_s *mdds);
extern void playlist_loaddir_scandrives(struct playlist_side_info *psi,char *searchpath,char *dslp);
extern void playlist_loaddir_initdirside(struct playlist_side_info *psi,char *dirname,char *oridirname);
extern void playlist_loaddir_initbrowser_all(struct mainvars *mvp,char *dir);
extern void playlist_loaddir_initbrowser_tab(struct playlist_side_info *psi,char *dir);
extern void playlist_loaddir_buildbrowser(struct playlist_side_info *);
extern int playlist_loaddir_browser_get_drives(struct playlist_side_info *psi, unsigned int first_drivenum, int last_drivenum);
extern struct playlist_side_info *playlist_loaddir_changedir(struct playlist_side_info *psi,unsigned long head);
extern unsigned int playlist_loaddir_browser_gotodir(struct playlist_side_info *psi,char *newdir);
extern void playlist_loaddir_search_paralell_dir(struct playlist_side_info *psi,int step);
extern void playlist_loaddir_switch_to_playlist(struct playlist_side_info *psi);
extern int  playlist_loaddir_selectdrive_execute(struct playlist_side_info *psi, unsigned char drive);
extern void playlist_loaddir_select_drive(struct mainvars *mvp,unsigned int side);
extern void playlist_loaddir_select_drive_retry(struct playlist_side_info *psi);
extern void playlist_loaddir_makedir_open(struct playlist_side_info *psi);
extern int playlist_loaddir_disk_unload_load(struct playlist_side_info *psi);
#ifdef MPXPLAY_GUI_QT
extern void mpxplay_playlist_loaddir_dtvdrive_refresh(void);
extern void mpxplay_playlist_loaddir_dvbprogram_start(char *dvb_filename, char *local_filename, mpxp_bool_t is_local_file);
#endif

//loadlist.c
extern unsigned int playlist_loadlist_check_extension(char *filename);
extern unsigned int playlist_loadlist_mainload(struct playlist_side_info *psi,char *listname,unsigned int loadtype,char *filtermask);
extern unsigned int playlist_loadlist_get_header_by_ext(struct playlist_side_info *psi,struct playlist_entry_info *pei,char *filename);
extern void playlist_loadlist_load_autosaved_list(struct playlist_side_info *psi);

//loadsub.c
extern unsigned int playlist_loadsub_check_extension(struct playlist_side_info *psi,char *filename);
extern void playlist_loadsub_setnewinputfile(struct playlist_side_info *psi,char *newinputfile,unsigned long loadtype);
extern char *playlist_loadsub_getinputfile(struct playlist_side_info *psi);
extern void playlist_loadsub_addsubdots(struct playlist_side_info *psi);
extern struct playlist_side_info *playlist_loadsub_sublist_change(struct playlist_side_info *psi,unsigned long head);
extern void playlist_loadsub_subdirs_to_sublist(struct playlist_side_info *psi);
extern void playlist_loadsub_search_paralell_list(struct playlist_side_info *psi,int step);
extern unsigned int playlist_loadsub_sublist_setlevels(struct playlist_side_info *psi,char *sublists);
extern unsigned int playlist_loadsub_sublist_getlevels(struct playlist_side_info *psi,char *destbuf,unsigned int buflen);
extern void playlist_loadsub_sublist_clear(struct playlist_side_info *psi);
extern char *playlist_loadsub_get_dftstr(unsigned int entrytype);

//randlist.c
extern void playlist_randlist_clearall(struct playlist_side_info *);
extern void playlist_randlist_delete(struct playlist_entry_info *);
extern void playlist_randlist_correctq(struct playlist_side_info *psi,struct playlist_entry_info *firstentry,struct playlist_entry_info *lastentry);
extern void playlist_randlist_xchq(struct playlist_entry_info *pei1,struct playlist_entry_info *pei2);
extern unsigned int playlist_getsongcounter(struct mainvars *mvp);
extern void playlist_randlist_pushq(struct playlist_side_info *psi,struct playlist_entry_info *);
extern playlist_entry_info *playlist_randlist_popq(void);
extern void playlist_randlist_setsignflag(struct playlist_entry_info *);
extern void playlist_randlist_resetsignflag(struct playlist_entry_info *pei);
extern struct playlist_entry_info *playlist_randlist_getnext(struct playlist_side_info *);
extern struct playlist_entry_info *playlist_randlist_getprev(struct playlist_entry_info *loc_newfilenum);
extern void playlist_randlist_randomize_side(struct playlist_side_info *psi);

//savelist.c
extern char *playlist_savelist_get_savename(unsigned int list_type);
extern unsigned int playlist_savelist_gettype_from_filename(char *filename);
extern void mpxplay_playlist_savelist_set_fileextension_by_listtype(unsigned int list_type, char *filename_buffer); // modifies filename extension in place!
extern int playlist_savelist_save_playlist(struct mainvars *mvp,struct playlist_side_info *psi,char *file_name,unsigned int list_type);
extern int playlist_savelist_save_editedside(struct playlist_side_info *psi);
extern void playlist_savelist_manual_save(struct playlist_side_info *psi);
extern void playlist_savelist_clear(struct playlist_side_info *psi);

//skiplist.c
extern unsigned int playlist_skip(struct mainvars *mvp);
extern struct playlist_entry_info *playlist_get_newfilenum(struct mainvars *mvp);
extern struct playlist_entry_info *playlist_get_nextalbum(struct playlist_entry_info *,struct playlist_side_info *,int step,unsigned int steplevel,unsigned int ring);
extern void playlist_newsong_enter(struct mainvars *,struct playlist_side_info *psie);
extern void playlist_nextsong_select(struct mainvars *,struct playlist_side_info *psie);
extern void playlist_skiplist_reset_loadnext(struct mainvars *mvp);

//sortlist.c
extern void playlist_sortlist_init(struct mainvars *mvp);
extern void playlist_sortlist_clear(struct playlist_side_info *psi);
extern mpxp_uint32_t playlist_sortlist_get_orderkeys_in_hexa(struct playlist_side_info *psi);
extern void playlist_sortlist_set_orderkeys_from_hexa(struct playlist_side_info *psi,mpxp_uint32_t value);
extern struct playlist_entry_info *playlist_order_entry_block(struct playlist_side_info *psi,struct playlist_entry_info *pei_src,struct playlist_entry_info *firstentry,struct playlist_entry_info *lastentry);
extern struct playlist_entry_info *playlist_order_entry(struct playlist_side_info *,struct playlist_entry_info *pei_src);
extern void playlist_order_block(struct playlist_side_info *psi,struct playlist_entry_info *firstentry,struct playlist_entry_info *lastentry);
extern void playlist_order_side(struct playlist_side_info *psi);
extern void playlist_order_dft(struct playlist_side_info *psi);
extern void playlist_order_filenames_block(struct playlist_side_info *psi,struct playlist_entry_info *firstentry,struct playlist_entry_info *lastentry);
extern void playlist_swap_entries(struct playlist_side_info *psi,struct playlist_entry_info *e1,struct playlist_entry_info *e2);
extern unsigned int playlist_sortlist_is_preordered_type(struct playlist_side_info *psi);
extern void playlist_sortlist_do_sort_col_n(struct playlist_side_info *psi, unsigned int col, int control);
extern unsigned int playlist_sortlist_get_editorcolumn_from_priordernum(struct playlist_side_info *psi);
extern void playlist_sortlist_set_psi_sortcontrol(struct playlist_side_info *psi, unsigned int column, unsigned int sort_control);

//textconv.c
extern void mpxplay_playlist_textconv_init(void);
extern void mpxplay_playlist_textconv_close(void);
extern int mpxplay_playlist_textconv_by_cpsrcname(char *cp_src_name,char *src_string,int src_len,char *dest_string,unsigned int dest_buflen);
extern int mpxplay_playlist_textconv_by_texttypes(mpxp_uint32_t convtype,char *src_string,int src_len,char *dest_string,unsigned int dest_buflen);

//diskdriv/openftp.c
extern void mpxplay_diskdrive_openftp_connect(struct playlist_side_info *psi);
extern void mpxplay_diskdrive_openftp_disconnect(struct playlist_side_info *psi);
extern void mpxplay_diskdrive_openftp_close(void);

#ifdef __cplusplus
}
#endif

#endif // mpxplay_playlist_h
