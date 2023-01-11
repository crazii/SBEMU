#include "DPMI.H"
#include "XMS.H"
#include <conio.h>
#include <stdlib.h>

DPMI_ADDRESSING DPMI_Addressing;

void DPMI_SetAddressing(DPMI_ADDRESSING* inputp newaddr, DPMI_ADDRESSING* outputp oldaddr)
{
    #if defined(__BC__) || defined(__WC__)
    newaddr->physical = TRUE;
    #endif

    *oldaddr = DPMI_Addressing;
    DPMI_Addressing = *newaddr;
}

#if defined(__WC__)

#define UNMAP_ADDR(addr) (addr)
#define LOAD_DS() 
#define RESTORE_DS() 

#elif defined(__DJ2__)

#define UNMAP_ADDR(addr) ((DPMI_Addressing.physical) ? DPMI_L2P(addr) : addr) //must be called before ds change
#define LOAD_DS() _ASM_BEGIN _ASM(push ds) _ASM(push dword ptr _DPMI_Addressing) _ASM(pop ds) _ASM_END
#define RESTORE_DS() _ASM_BEGIN _ASM(pop ds) _ASM_END

#elif defined(__BC__)

#define UNMAP_ADDR(addr) (addr)
#define LOAD_DS() _ASM_BEGIN _ASM(push ds) _ASM(push word ptr DPMI_Addressing) _ASM(pop ds) _ASM_END
#define RESTORE_DS() _ASM_BEGIN _ASM(pop ds) _ASM_END

#endif

#if defined(__DJ2__)
uint8_t DPMI_LoadB(uint32_t addr)
{
    uint8_t ret;
    addr = UNMAP_ADDR(addr);
    LOAD_DS();
    ret = *(uint8_t*)addr;
    RESTORE_DS();
    return ret;
}

void DPMI_StoreB(uint32_t addr, uint8_t val)
{
    addr = UNMAP_ADDR(addr);
    LOAD_DS();
    *(uint8_t*)addr = val;
    RESTORE_DS();
}

void DPMI_MaskB(uint32_t addr, uint8_t mand, uint8_t mor)
{
    uint8_t val;
    addr = UNMAP_ADDR(addr);
    LOAD_DS();
    val = *(uint8_t*)addr;
    val &= mand;
    val |= mor;
    *(uint8_t*)addr = val;
    RESTORE_DS();
}

uint16_t DPMI_LoadW(uint32_t addr)
{
    uint16_t ret;
    addr = UNMAP_ADDR(addr);
    LOAD_DS();
    ret = *(uint16_t*)addr;
    RESTORE_DS();
    return ret;
}

void DPMI_StoreW(uint32_t addr, uint16_t val)
{
    addr = UNMAP_ADDR(addr);
    LOAD_DS();
    *(uint16_t*)addr = val;
    RESTORE_DS();
}

void DPMI_MaskW(uint32_t addr, uint16_t mand, uint16_t mor)
{
    uint16_t val;
    addr = UNMAP_ADDR(addr);
    LOAD_DS();
    val = *(uint16_t*)addr;
    val &= mand;
    val |= mor;
    *(uint16_t*)addr = val;
    RESTORE_DS();
}

uint32_t DPMI_LoadD(uint32_t addr)
{
    uint32_t ret;
    addr = UNMAP_ADDR(addr);
    LOAD_DS();
    ret = *(uint32_t*)addr;
    RESTORE_DS();
    return ret;
}

void DPMI_StoreD(uint32_t addr, uint32_t val)
{
    addr = UNMAP_ADDR(addr);
    LOAD_DS();
    *(uint32_t*)addr = val;
    RESTORE_DS();
}

void DPMI_MaskD(uint32_t addr, uint32_t mand, uint32_t mor)
{
    uint32_t val;
    addr = UNMAP_ADDR(addr);
    LOAD_DS();
    val = *(uint32_t*)addr;
    val &= mand;
    val |= mor;
    *(uint32_t*)addr = val;
    RESTORE_DS();
}
#endif

#if defined(__DJ2__) || defined(__WC__)
void DPMI_CopyLinear(uint32_t dest, uint32_t src, uint32_t size) //TODO: use memcpy directly
{
    uint32_t size4 = size >> 2;
    uint32_t size1 = size & 0x3;
    uint32_t i;

    dest = UNMAP_ADDR(dest);
    src = UNMAP_ADDR(src);

    LOAD_DS()
    for(i = 0; i < size4; ++i)
    {
        *(((uint32_t*)dest)+i) = *(((uint32_t*)src)+i);
    }

    src += size4 * 4;
    dest += size4 * 4;
    for(i = 0; i < size1; ++i)
    {
        *(((uint8_t*)dest)+i) = *(((uint8_t*)src)+i);
    }
    RESTORE_DS()
}

void DPMI_SetLinear(uint32_t dest, uint8_t val, uint32_t size)
{
    uint32_t size4 = size >> 2;
    uint32_t size1 = size & 0x3;
    uint32_t val32 = ((uint32_t)val) | ((uint32_t)val<<8) | ((uint32_t)val<<16) | ((uint32_t)val<<24);
    uint32_t i;

    dest = UNMAP_ADDR(dest);

    LOAD_DS()
    for(i = 0; i < size4; ++i)
    {
        *(((uint32_t*)dest)+i) = val32;
    }

    dest += size4 * 4;
    for(i = 0; i < size1; ++i)
    {
        *(((uint8_t*)dest)+i) = val;
    }
    RESTORE_DS()
}

int32_t DPMI_CompareLinear(uint32_t addr1, uint32_t addr2, uint32_t size)
{
    uint32_t size4 = size >> 2;
    uint32_t size1 = size & 0x3;
    uint32_t i;
    int32_t result = 0;

    addr1 = UNMAP_ADDR(addr1);
    addr2 = UNMAP_ADDR(addr2);

    LOAD_DS()
    for(i = 0; i < size4; ++i)
    {
        result = (int32_t)(*(((uint32_t*)addr1)+i) - *(((uint32_t*)addr2)+i));
        if(result != 0)
        {
            result = (int32_t)i;
            break;
        }
    }

    if(result == 0)
    {
        addr2 += size4 * 4;
        addr1 += size4 * 4;
    }
    else //re-compare in bytes
    {
        addr2 += (uint32_t)result * 4;
        addr1 += (uint32_t)result * 4;
        size1 = 4;
    }
    for(i = 0; i < size1; ++i)
    {
        result = (int32_t)((uint32_t)*(((uint8_t*)addr1)+i) - (uint32_t)*(((uint8_t*)addr2)+i));
        if(result != 0)
            break;
    }
    RESTORE_DS()
    return result;
}

#elif defined(__BC__)//BC doesn't support linear addr, but TASM does, use asm

//note: cdecl ABI: eax, edx, ecx caller preserved.

uint8_t DPMI_LoadB(uint32_t addr)
{
    LOAD_DS();
    __asm {
        mov ecx, dword ptr addr
        mov al, byte ptr ds:[ecx]
    }
    RESTORE_DS();
    return _AL;
}

void DPMI_StoreB(uint32_t addr, uint8_t val)
{
    LOAD_DS();
    __asm {
        mov ecx, dword ptr addr
        mov al, val
        mov byte ptr ds:[ecx], al
    }
    RESTORE_DS();
}

void DPMI_MaskB(uint32_t addr, uint8_t mand, uint8_t mor)
{
    LOAD_DS();
    __asm {
        mov ecx, dword ptr addr
        mov al, byte ptr ds:[ecx]
        and al, byte ptr mand
        or al, byte ptr mor
        mov byte ptr ds:[ecx], al
    }
    RESTORE_DS();
}

uint16_t DPMI_LoadW(uint32_t addr)
{
    LOAD_DS();
    __asm {
        mov ecx, dword ptr addr
        mov ax, word ptr ds:[ecx]
    }
    RESTORE_DS();
    return _AX;
}

void DPMI_StoreW(uint32_t addr, uint16_t val)
{
    LOAD_DS();
    __asm {
        mov ecx, dword ptr addr
        mov ax, word ptr val
        mov word ptr ds:[ecx], ax
    }
    RESTORE_DS();
}

void DPMI_MaskW(uint32_t addr, uint16_t mand, uint16_t mor)
{
    LOAD_DS();
    __asm {
        mov ecx, dword ptr addr
        mov ax, word ptr ds:[ecx]
        and ax, word ptr mand
        or ax, word ptr mor
        mov word ptr ds:[ecx], ax
    }
    RESTORE_DS();
}

//16bit int32_t: dx:ax
uint32_t DPMI_LoadD(uint32_t addr)
{
    uint32_t val;
    LOAD_DS();
    __asm {
        mov ecx, dword ptr addr
        mov eax, dword ptr ds:[ecx]
        mov dword ptr val, eax
        //ret cannot return with stack frame
    }
    RESTORE_DS();
    return val;    //return stack value instead of register, to make compiler happy
}

void DPMI_StoreD(uint32_t addr, uint32_t val)
{
    LOAD_DS();
    __asm {
        mov ecx, dword ptr addr
        mov eax, dword ptr val
        mov dword ptr ds:[ecx], eax
    }
    RESTORE_DS();
}

void DPMI_MaskD(uint32_t addr, uint32_t mand, uint32_t mor)
{
    LOAD_DS();
    __asm {
        mov ecx, dword ptr addr
        mov eax, dword ptr ds:[ecx]
        and eax, dword ptr mand
        or eax, dword ptr mor
        mov dword ptr ds:[ecx], eax
    }
    RESTORE_DS();
}

void DPMI_CopyLinear(uint32_t dest, uint32_t src, uint32_t size)
{
    LOAD_DS();
    __asm {
        push esi
        push edi
        push ebx
        mov esi, dword ptr src
        mov edi, dword ptr dest
        mov ecx, dword ptr size
        mov edx, ecx
        and ecx, 0xFFFFFFFC
        and edx, 0x3
        xor ebx, ebx
    }
loop4b:
    __asm {
        mov eax, dword ptr ds:[esi+ebx]
        mov dword ptr ds:[edi+ebx], eax
        add ebx, 4
        cmp ebx, ecx
        jne loop4b

        test edx, edx
        jz end
        add esi, ebx
        add edi, ebx
        xor ebx, ebx
    }
loop1b:
    __asm {
        mov al, byte ptr ds:[esi+ebx]
        mov byte ptr ds:[edi+ebx], al
        inc ebx
        cmp ebx, edx
        jne loop1b
    }
end:
    __asm {
        pop ebx
        pop edi
        pop esi
    }
    RESTORE_DS();
}

void DPMI_SetLinear(uint32_t dest, uint8_t val, uint32_t size)
{
    LOAD_DS();
    __asm {
        push edi
        push ebx
        mov al, byte ptr val
        mov ah, al
        mov bx, ax
        shl eax, 16
        mov ax, bx

        mov edi, dword ptr dest
        mov ecx, dword ptr size
        mov edx, ecx
        and ecx, 0xFFFFFFFC
        and edx, 0x3
        xor ebx, ebx
    }
loop4b:
    __asm {
        mov dword ptr ds:[edi+ebx], eax
        add ebx, 4
        cmp ebx, ecx
        jne loop4b

        test edx, edx
        jz end
        add edi, ebx
        xor ebx, ebx
    }
loop1b:
    __asm {
        mov byte ptr ds:[edi+ebx], al
        inc ebx
        cmp ebx, edx
        jne loop1b
    }
end:
    __asm {
        pop ebx
        pop edi
    }
    RESTORE_DS();
}

int32_t DPMI_CompareLinear(uint32_t addr1, uint32_t addr2, uint32_t size)
{
    int32_t result;
    LOAD_DS();
    __asm {
        push esi
        push edi
        push ebx
        mov esi, dword ptr addr2
        mov edi, dword ptr addr1
        mov ecx, dword ptr size
        mov edx, ecx
        and ecx, 0xFFFFFFFC
        and edx, 0x3
        xor ebx, ebx
    }
loop4b:
    __asm {
        mov eax, dword ptr ds:[edi+ebx]
        sub eax, dword ptr ds:[esi+ebx]
        jne not_equal4 //perform byte wide test on 4 bytes
        add ebx, 4
        cmp ebx, ecx
        jne loop4b

        test edx, edx
        jz equal
        add esi, ebx
        add edi, ebx
        xor ebx, ebx
        jmp loop1b
    }
not_equal4:
    __asm mov edx, 4
loop1b:
    __asm {
        mov al, byte ptr ds:[edi+ebx]
        sub al, byte ptr ds:[esi+ebx]
        jne not_qual
        inc ebx
        cmp ebx, edx
        jne loop1b
    }
equal:
    __asm {
        xor eax, eax //equal
        jmp _return
    }
not_qual:    
    __asm {
        mov eax, 1
        ja _return //use unsigned char compare, compatible to memcmp()
        not eax
    }
_return:
    __asm {
        pop ebx
        pop edi
        pop esi
        mov result, eax
    }

    RESTORE_DS();
    return result;
}

#endif

static uint32_t DPMI_DOSUMB(uint32_t input, BOOL alloc, BOOL UMB)
{
    uint32_t result = 0;
    uint16_t UMBlinkstate;
    uint16_t strategy;
    DPMI_REG r = {0};

    //try DOS alloc with UMB link
    r.h.ah = 0x58;
    r.h.al = 0x02;  //get
    DPMI_CallRealModeINT(0x21, &r);
    UMBlinkstate = r.h.al;  //backup state

    r.h.ah = 0x58;
    r.h.al = 0x03;  //set
    r.w.bx = (uint16_t)UMB; //unlink UMB (LH will set UMB need set it back)
    DPMI_CallRealModeINT(0x21, &r);

    r.h.ah = 0x58;
    r.h.al = 0x0;   //get
    DPMI_CallRealModeINT(0x21, &r);
    strategy = r.w.ax; //back strategy

    r.h.ah = 0x58;
    r.h.al = 0x01;  //set
    //http://mirror.cs.msu.ru/oldlinux.org/Linux.old/docs/interrupts/int-html/rb-3008.htm
    r.w.bx = 0x82;  //try hi memory then low memory, last fit. DOS5.0+
    DPMI_CallRealModeINT(0x21,&r);
    if(alloc)
        result = DPMI_DOSMalloc((uint16_t)input);
    else
        DPMI_DOSFree(input);

    r.h.ah = 0x58;
    r.h.al = 0x01;
    r.w.bx = strategy;
    DPMI_CallRealModeINT(0x21, &r); //restore strategy

    r.h.ah = 0x58;
    r.h.al = 0x03;
    r.w.bx = UMBlinkstate;
    DPMI_CallRealModeINT(0x21, &r);
    return result;
}

uint32_t DPMI_HighMalloc(uint16_t size, BOOL UMB)
{
    //try XMS first. UMB support is only optional by XMS3.0
    uint16_t segment = UMB ? XMS_AllocUMB(size) : 0;
    if(segment)
        return 0xFFFF0000L | (uint32_t)segment;
    else //not supported, or UMB taken over by DOS (DOS=UMB in config.sys)
        return DPMI_DOSUMB(size, TRUE, UMB);
}

void DPMI_HighFree(uint32_t segment)
{
    if((segment&0xFFFF0000L) == 0xFFFF0000L)
        XMS_FreeUMB((uint16_t)segment);
    else
        DPMI_DOSUMB(segment, FALSE, TRUE);
}
