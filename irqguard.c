#include <string.h>
#include <assert.h>
#include "irqguard.h"
#include "sbemu/dpmi/dpmi.h"
#include "sbemu/pic.h"

static BOOL IRQGUARD_Installed;
static uint32_t IRQGUARD_DOSMEM;
static DPMI_ISR_HANDLE IRQGUARD_Handle;
static uint8_t IRQGUARD_IRQ = 0xFF;
#define IRQGUARD_BYTE_LOC 4

static void __NAKED IRQGUARD_Handler()
{
    _ASM_BEGIN16
        _ASM(cmp byte ptr cs:[4], 1) //IRQGUARD_BYTE_LOC: guard enabled?
        _ASM(je skip)
        _ASM(pushf) //calling iret
        _ASM(call dword ptr cs:[0]) //call handler
    _ASMLBL(skip:)
        _ASM(iret)
    _ASM_END16
}
static void __NAKED IRQGUARD_HandlerEnd() {}

BOOL IRQGUARD_Install(uint8_t irq)
{
    if(IRQGUARD_Installed && irq == IRQGUARD_IRQ)
        return TRUE;

    const uint16_t datasize = IRQGUARD_BYTE_LOC+1; //far ptr + guard byte
    if(!IRQGUARD_Installed)
    {
        assert(IRQGUARD_DOSMEM == 0);
        uint32_t codesize = (uintptr_t)&IRQGUARD_HandlerEnd - (uintptr_t)&IRQGUARD_Handler;
        IRQGUARD_DOSMEM = DPMI_HighMalloc((codesize+datasize+15)>>4, TRUE);
        if(IRQGUARD_DOSMEM == 0)
        {
            assert(FALSE);
            return FALSE;
        }
        DPMI_CopyLinear(DPMI_SEGOFF2L(IRQGUARD_DOSMEM, datasize), DPMI_PTR2L(&IRQGUARD_Handler), codesize);
    }    

    if(IRQGUARD_Installed && irq != IRQGUARD_IRQ)
        //restore old handler
        DPMI_UninstallRealModeISR_Direct(&IRQGUARD_Handle);
    
    int vec = PIC_IRQ2VEC(irq);
    CLIS();
    //install handler
    //need install to DPMI because PM games may install RM handler (rm mode sound card drivers) to DPMI
    //if rawIVT is used, it will prevent DPMI and PM games to process it
    DPMI_InstallRealModeISR_Direct(vec, (uint16_t)(IRQGUARD_DOSMEM&0xFFFF), datasize, &IRQGUARD_Handle, FALSE);
    //far ptr
    DPMI_CopyLinear(DPMI_SEGOFF2L(IRQGUARD_DOSMEM, 0), DPMI_PTR2L(&IRQGUARD_Handle.old_rm_offset), 2);
    DPMI_CopyLinear(DPMI_SEGOFF2L(IRQGUARD_DOSMEM, 2), DPMI_PTR2L(&IRQGUARD_Handle.old_rm_cs), 2);
    //init guard byte
    IRQGUARD_Installed = TRUE;
    IRQGUARD_IRQ = irq;
    IRQGUARD_Disable();
    STIL();
}

BOOL IRQGUARD_Uninstall()
{
    if(IRQGUARD_Installed)
    {
        assert(IRQGUARD_Handle.old_rm_offset != 0);
        assert(IRQGUARD_Handle.old_rm_cs != 0);
        DPMI_UninstallRealModeISR_Direct(&IRQGUARD_Handle);
        memset(&IRQGUARD_Handle, 0, sizeof(IRQGUARD_Handle));
        DPMI_HighFree(IRQGUARD_DOSMEM);
        IRQGUARD_DOSMEM = 0;
        IRQGUARD_Installed = 0;
        IRQGUARD_IRQ = 0xFF;
        return TRUE;
    }
    return FALSE;
}

void IRQGUARD_Enable()
{
    if(IRQGUARD_Installed)
        DPMI_StoreB(DPMI_SEGOFF2L(IRQGUARD_DOSMEM, IRQGUARD_BYTE_LOC), 1);
}

void IRQGUARD_Disable()
{
    if(IRQGUARD_Installed)
        DPMI_StoreB(DPMI_SEGOFF2L(IRQGUARD_DOSMEM, IRQGUARD_BYTE_LOC), 0);
}