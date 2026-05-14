#ifndef _UTILITY_H_
#define _UTILITY_H_

inline int is_path_abs(const char* path)
{
    return path[0] == '\\' || path[1] == ':';
}

//get the path of the running program (not current path/cwd)
//e.g. if SBEMU located and C:\SNDDRV\SBEMU.EXE, then returns C:\SNDDRV
int get_program_path(char* buf, int size);

#endif