//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2008 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: fpu control

void asm_fpu_setround_near(unsigned int *);

void pds_fpu_setround_near(void)
{
 unsigned int controlword;
#ifdef __WATCOMC__
 #pragma aux asm_fpu_setround_near=\
  "fstcw word ptr [eax]"\
  "and word ptr [eax],0xf3ff"\
  "fldcw word ptr [eax]"\
  parm[eax] modify[];
 asm_fpu_setround_near(&controlword);
#elif defined(DJGPP)
asm(
    "lea %0, %%eax \n\t"
    "fstcw (%%eax) \n\t"
    "andw $0xf3ff, (%%eax) \n\t"
    "fldcw (%%eax)"
    :"+m"(controlword)::"eax"
);
#endif
}

void asm_fpu_setround_chop(unsigned int *);

void pds_fpu_setround_chop(void)
{
 unsigned int controlword;
#ifdef __WATCOMC__
 #pragma aux asm_fpu_setround_chop=\
  "fstcw word ptr [eax]"\
  "or word ptr [eax],0x0c00"\
  "fldcw word ptr [eax]"\
  parm[eax] modify[];
 asm_fpu_setround_chop(&controlword);
#elif defined(DJGPP)
asm(
    "lea %0, %%eax \n\t"
    "fstcw (%%eax) \n\t"
    "orw $0x0c00, (%%eax) \n\t"
    "fldcw (%%eax)"
    :"+m"(controlword)::"eax"
);
#endif
}
