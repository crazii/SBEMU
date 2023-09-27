#include <dpmi/dpmi.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// http://www.techhelpmanual.com/346-dos_environment.html
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
    DPMI_CopyLinear(DPMI_PTR2L(buf), env<<4, size);
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
    DPMI_CopyLinear(env<<4, DPMI_PTR2L(buf), size);
    free(buf);
    
    return 0;
}
