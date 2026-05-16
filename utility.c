#include <dpmi/dpmi.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "utility.h"

// http://www.techhelpmanual.com/346-dos_environment.html
// NOTE: this replaces the libc's default implementation of 'setenv'
// the default setenv only sets env for current program (and its children)
// this implementation set a global env (env block of COMMAND.COM) 
int setenv(const char *name, const char *value, int rewrite)
{
    int namelen;
    int vallen;
    if(name == NULL || value == NULL || (namelen=strlen(name)) == 0 || (vallen=strlen(value)) == 0)
        return -1;

    //find PSP of COMMAND.COM
    uint32_t psp = 0;
    uint32_t parent = 0;
    {
        DPMI_REG r = {0};
        r.h.ah = 0x62;
        DPMI_CallRealModeINT(0x21, &r);
        parent = psp = r.w.bx;
    }
    do
    {
        psp = parent;
        //printf("PSP:%x\n",psp);
        parent = DPMI_LoadW((psp<<4)+0x16);
    }while(parent != psp);
    uint16_t env = DPMI_LoadW((parent<<4)+0x2C);
    uint16_t mcb = env - 1;
    uint16_t size = DPMI_LoadW((mcb<<4) + 0x03) << 4;
    //printf("size:%d\n",size);

    char* buf = (char*)malloc(size+namelen+1+vallen+1);
    memset(buf, 0, size+namelen+1+vallen+1);
    DPMI_LMemcpy(DPMI_PTR2L(buf), env<<4, size);
    char* s;
    s = buf;
    do
    {
#if DEBUG && 0
        s += printf("%s\n", s);
#else
        s += strlen(s) + 1;
#endif        
    }while(*s);
    
    if((s-buf+1) + (namelen+1)+1+(vallen+1) > size) //not enough space. TODO: allocate new
    {
        free(buf);
        return -1;
    }
    
    s = buf;
    do
    {
        int len = strlen(s);
        if(memicmp(s, name, namelen) == 0 && s[namelen] == '=')
        {
            if(rewrite)
            {
                memmove(s, s+len+1, size-(s-buf+len+1));
                if(!*s) break; //bugfix (exposed in FreeDOS)
                len = strlen(s);
            }
            else
            {
                free(buf);
                return -1;
            }
        }
        s += len+1;
    }while(*s);
    
    *(s + sprintf(s, "%s=%s", name, value)+1)='\0';
    //size = namelen + vallen + 3;

    #if DEBUG && 0
    s = buf;
    do
    {
        s += printf("%s\n", s);
    }while(*s);
    #endif

    //DPMI_StoreW((mcb<<4), (size+15)>>4);
    DPMI_LMemcpy(env<<4, DPMI_PTR2L(buf), size);
    free(buf);
    
    return 0;
}

#ifdef DJGPP
extern char **__crt0_argv;
#define __argv __crt0_argv
#endif

int get_program_path(char* buf, int size)
{
    int len = strlen(__argv[0]);
    len = min(len, size-1);

    memcpy(buf, __argv[0], len);
    buf[len] = '\0';
    //djgpp path uses /
    for(int i = 0; i < len; ++i)
    {
        if(buf[i] == '/') buf[i] = '\\';
    }

    int i = len;
    while(buf[i] != '\\' && i > 0) --i;

    if(buf[i] != '\\') //not full path?
        i = 0;

    buf[i] = '\0';
    return i;
}

char* get_abs_path(char* dest, int size, const char* path)
{
    if(!is_path_abs(path))
    {
        char p_path[_MAX_PATH];
        int p_path_len;
        p_path_len = get_program_path(p_path, sizeof(p_path));

        int len = strlen(path);
        if(p_path_len + 1 < sizeof(p_path)
            && p_path_len + len + 1 < size)
        {
            p_path[p_path_len++] = '\\';
            p_path[p_path_len] = '\0';

            memcpy(dest, p_path, p_path_len);
            memcpy(dest+p_path_len, path, len + 1);
            return dest;
        }
    }

    int len = min(strlen(path),size-1);
    memcpy(dest, path, len+1);
    dest[len] = '\0';
    return dest;
}

void* load_file(const char* file, uint32_t buff_offset, uint32_t* size)
{
    *size = 0;
    FILE* fp = fopen(file, "rb");
    if(!fp)
        return NULL;
    
    BOOL ok = FALSE;
    char* buf = NULL;
    do
    {
        if(fseek(fp, 0, SEEK_END))
            break;
        long p = ftell(fp);
        if(p == -1)
            break;
        *size = p;
        if(fseek(fp, 0, SEEK_SET))
            break;
        buf = (char*)malloc(p+buff_offset);
        if(!buf)
            break;

        if(fread(buf+buff_offset, p, 1, fp) != 1)
            break;
        ok = TRUE;
    } while(0);

    if(!ok)
    {
        *size = 0;
        free(buf);
        buf = NULL;
    }
    fclose(fp);
    return buf;
}
