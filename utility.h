#ifndef _UTILITY_H_
#define _UTILITY_H_

#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif


inline int is_path_abs(const char* path)
{
    return path[0] == '\\' || path[1] == ':';
}

//get the path of the running program (not current path/cwd)
//e.g. if SBEMU located and C:\SNDDRV\SBEMU.EXE, then returns C:\SNDDRV
int get_program_path(char* buf, int size);

//transform a given path to abs path
//simple concat without processing ".\" and ".\\"
char* get_abs_path(char* dest, int size, const char* path);

#endif