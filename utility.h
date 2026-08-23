#ifndef _UTILITY_H_
#define _UTILITY_H_

#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif

#ifdef __cplusplus
extern "C"
{
#endif

extern unsigned char fpu_buffer[512] __attribute__((aligned(16)));
extern int fpu_state_saved;

//fpu state save
inline void FPUSS()
{
    if(!fpu_state_saved)
    {
        fpu_save(fpu_buffer);
        fpu_state_saved = 1;
    }
}

//fpu state restore
inline void FPUSR()
{
    if(fpu_state_saved)
    {
        fpu_restore(fpu_buffer);
        fpu_state_saved = 0;
    }
}

inline int is_path_abs(const char* path)
{
    return path[0] == '\\' || path[1] == ':';
}

//get the path of the running program (not current path/cwd)
//e.g. if SBEMU located and C:\SNDDRV\SBEMU.EXE, then returns C:\SNDDRV
int get_program_path(char* buf, int size);

//transform a given path to abs path using get_program_path
//simple concat without processing ".\" and ".\\"
char* get_abs_path(char* dest, int size, const char* path);

//load file to memory using malloc.
void* load_file(const char* file, uint32_t buff_offset, uint32_t* size);

#ifdef __cplusplus
}
#endif

#endif