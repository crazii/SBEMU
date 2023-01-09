//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2011 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: string handling

//#define MPXPLAY_USE_DEBUGF 1
#define MPXPLAY_DEBUG_OUTPUT stdout

#include <string.h>
#include "mpxplay.h"

#ifdef MPXPLAY_WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#endif

unsigned int pds_strcpy(char *dest,char *src)
{
 char *begin;
 if(!dest)
  return 0;
 if(!src){
  *dest=0;
  return 0;
 }
 begin=src;
 do{
  char c=*src;
  *dest=c;
  if(!c)
   break;
  dest++;src++;
 }while(1);
 return (src-begin); // returns the lenght of string, not the target pointer!
}

unsigned int pds_strmove(char *dest,char *src)
{
 unsigned int len,count;
 if(!dest)
  return 0;
 if(!src){
  *dest=0;
  return 0;
 }
 if(dest<src)
  return pds_strcpy(dest,src);
 count=len=pds_strlen(src)+1;
 src+=len;
 dest+=len;
 do{
  src--;dest--;
  *dest=*src;
 }while(--count);
 return len; // returns the lenght of string
}

unsigned int pds_strncpy(char *dest,char *src,unsigned int maxlen)
{
 char *begin;
 if(!dest || !maxlen)
  return 0;
 if(!src){
  *dest=0;
  return 0;
 }
 begin=src;
 do{
  char c=*src;
  *dest=c;
  if(!c)
   break;
  dest++;src++;
 }while(--maxlen);
 return (src-begin); // returns the lenght of string, not the target pointer!
}

unsigned int pds_strcat(char *strp1,char *strp2)
{
 if(!strp1 || !strp2)
  return 0;
 return pds_strcpy(&strp1[pds_strlen(strp1)],strp2);
}

static int pds_strchknull(char *strp1,char *strp2)
{
 register const unsigned char *s1 = (const unsigned char *) strp1;
 register const unsigned char *s2 = (const unsigned char *) strp2;

 if(!s1 || !s1[0])
  if(s2 && s2[0])
   return -1;
  else
   return 0;

 if(!s2 || !s2[0])
  if(s1 && s1[0])
   return 1;
  else
   return 0;

 return 2;
}

int pds_strcmp(char *strp1,char *strp2)
{
 register const unsigned char *s1 = (const unsigned char *) strp1;
 register const unsigned char *s2 = (const unsigned char *) strp2;
 unsigned char c1,c2;
 int retcode=pds_strchknull(strp1,strp2);
 if(retcode!=2)
  return retcode;

  do{
   c1 = (unsigned char) *s1++;
   c2 = (unsigned char) *s2++;
   if(!c1)
    break;
  }while (c1 == c2);

  return c1 - c2;
}

int pds_stricmp(char *strp1,char *strp2)
{
 register const unsigned char *s1 = (const unsigned char *) strp1;
 register const unsigned char *s2 = (const unsigned char *) strp2;
 unsigned char c1,c2;
 int retcode=pds_strchknull(strp1,strp2);
 if(retcode!=2)
  return retcode;

 do{
  c1 = (unsigned char) *s1++;
  c2 = (unsigned char) *s2++;
  if(!c1)
   break;
  if(c1>='a' && c1<='z')  // convert to uppercase
   c1-=32;                // c1-='a'-'A'
  if(c2>='a' && c2<='z')
   c2-=32;
 }while(c1 == c2);
 return (c1 - c2);
}

//faster (no pointer check), returns 1 if equal
unsigned int pds_stri_compare(char *strp1,char *strp2)
{
 char c1,c2;
 do{
  c1=*strp1;
  c2=*strp2;
  if(c1!=c2){
   if(c1>='a' && c1<='z')  // convert to uppercase
    c1-=32;                // c1-='a'-'A'
   if(c2>='a' && c2<='z')
    c2-=32;
   if(c1!=c2)
    return 0;
  }
  strp1++;strp2++;
 }while(c1 && c2);
 return 1;
}

int pds_strricmp(char *str1,char *str2)
{
 char *pstr1=str1,*pstr2=str2;
 int retcode=pds_strchknull(str1,str2);
 if(retcode!=2)
  return retcode;

 while(pstr1[0]!=0)
  pstr1++;
 while(pstr1[0]==0 || pstr1[0]==32)
  pstr1--;
 if(pstr1<=str1)
  return 1;
 while(pstr2[0]!=0)
  pstr2++;
 while(pstr2[0]==0 || pstr2[0]==32)
  pstr2--;
 if(pstr2<=str2)
  return -1;
 while(pstr1>=str1 && pstr2>=str2){
  char c1=pstr1[0];
  char c2=pstr2[0];
  if(c1>='a' && c1<='z')  // convert to uppercase
   c1-=32;
  if(c2>='a' && c2<='z')
   c2-=32;
  if(c1!=c2){
   if(c1<c2)
    return -1;
   else
    return 1;
  }
  pstr1--;pstr2--;
 }
 return 0;
}

int pds_strlicmp(char *str1,char *str2)
{
 char c1,c2;
 int retcode=pds_strchknull(str1,str2);
 if(retcode!=2)
  return retcode;

 do{
  c1=*str1;
  c2=*str2;
  if(!c1 || !c2)
   break;
  if(c1!=c2){
   if(c1>='a' && c1<='z')  // convert to uppercase
    c1-=32;
   if(c2>='a' && c2<='z')
    c2-=32;
   if(c1!=c2){
    if(c1<c2)
     return -1;
    else
     return 1;
   }
  }
  str1++;str2++;
 }while(1);
 return 0;
}

int pds_strncmp(char *strp1,char *strp2,unsigned int counter)
{
 char c1,c2;
 int retcode=pds_strchknull(strp1,strp2);
 if(retcode!=2)
  return retcode;
 if(!counter)
  return 0;
 do{
  c1=*strp1;
  c2=*strp2;
  if(c1!=c2)
   if(c1<c2)
    return -1;
   else
    return 1;
  strp1++;strp2++;
 }while(c1 && c2 && --counter);
 return 0;
}

int pds_strnicmp(char *strp1,char *strp2,unsigned int counter)
{
 char c1,c2;
 int retcode=pds_strchknull(strp1,strp2);
 if(retcode!=2)
  return retcode;
 if(!counter)
  return 0;
 do{
  c1=*strp1;
  c2=*strp2;
  if(c1!=c2){
   if(c1>='a' && c1<='z')
    c1-=32;
   if(c2>='a' && c2<='z')
    c2-=32;
   if(c1!=c2){
    if(c1<c2)
     return -1;
    else
     return 1;
   }
  }
  strp1++;strp2++;
 }while(c1 && c2 && --counter);
 return 0;
}

unsigned int pds_strlen(char *strp)
{
 char *beginp;
 if(!strp || !strp[0])
  return 0;
 beginp=strp;
 do{
  strp++;
 }while(*strp);
 return (unsigned int)(strp-beginp);
}

unsigned int pds_strlenc(char *strp,char seek)
{
 char *lastnotmatchp,*beginp;

 if(!strp || !strp[0])
  return 0;

 lastnotmatchp=NULL;
 beginp=strp;
 do{
  if(*strp!=seek)
   lastnotmatchp=strp;
  strp++;
 }while(*strp);

 if(!lastnotmatchp)
  return 0;
 return (unsigned int)(lastnotmatchp-beginp+1);
}

/*unsigned int pds_strlencn(char *strp,char seek,unsigned int len)
{
 char *lastnotmatchp,*beginp;

 if(!strp || !strp[0] || !len)
  return 0;

 lastnotmatchp=NULL;
 beginp=strp;
 do{
  if(*strp!=seek)
   lastnotmatchp=strp;
  strp++;
 }while(*strp && --len);

 if(!lastnotmatchp)
  return 0;
 return (unsigned int)(lastnotmatchp-beginp+1);
}*/

char *pds_strchr(char *strp,char seek)
{
 if(!strp)
  return NULL;
 do{
  char c=strp[0];
  if(c==seek)
   return strp;
  if(!c)
   break;
  strp++;
 }while(1);
 return NULL;
}

char *pds_strrchr(char *strp,char seek)
{
 char *foundp=NULL,curr;

 if(!strp)
  return foundp;

 curr=*strp;
 if(!curr)
  return foundp;
 do{
  if(curr==seek)
   foundp=strp;
  strp++;
  curr=*strp;
 }while(curr);
 return foundp;
}

char *pds_strnchr(char *strp,char seek,unsigned int len)
{
 if(!strp || !strp[0] || !len)
  return NULL;
 do{
  if(*strp==seek)
   return strp;
  strp++;
 }while(*strp && --len);
 return NULL;
}

char *pds_strstr(char *s1,char *s2)
{
 if(s1 && s2 && s2[0]){
  char c20=*s2;
  do{
   char c1=*s1;
   if(!c1)
    break;
   if(c1==c20){        // search the first occurence
    char *s1p=s1,*s2p=s2;
    do{                 // compare the strings (part of s1 with s2)
     char c2=*(++s2p);
     if(!c2)
      return s1;
     c1=*(++s1p);
     if(!c1)
      return NULL;
     if(c1!=c2)
      break;
    }while(1);
   }
   s1++;
  }while(1);
 }
 return NULL;
}

char *pds_strstri(char *s1,char *s2)
{
 if(s1 && s2 && s2[0]){
  char c20=*s2;
  if(c20>='a' && c20<='z')  // convert to uppercase (first character of s2)
   c20-=32;
  do{
   char c1=*s1;
   if(!c1)
    break;
   if(c1>='a' && c1<='z')  // convert to uppercase (current char of s1)
    c1-=32;
   if(c1==c20){        // search the first occurence
    char *s1p=s1,*s2p=s2;
    do{                 // compare the strings (part of s1 with s2)
     char c2;
     s2p++;
     c2=*s2p;
     if(!c2)
      return s1;
     s1p++;
     c1=*s1p;
     if(!c1)
      return NULL;
     if(c1>='a' && c1<='z')  // convert to uppercase
      c1-=32;
     if(c2>='a' && c2<='z')  // convert to uppercase
      c2-=32;
     if(c1!=c2)
      break;
    }while(1);
   }
   s1++;
  }while(1);
 }
 return NULL;
}

unsigned int pds_strcutspc(char *src)
{
 char *dest,*dp;

 if(!src)
  return 0;

 dest=src;

 while(src[0] && (src[0]==32))
  src++;

 if(!src[0]){
  dest[0]=0;
  return 0;
 }
 if(src>dest){
  char c;
  dp=dest;
  do{
   c=*src++; // move
   *dp++=c;  //
  }while(c);
  dp-=2;
 }else{
  while(src[1])
   src++;
  dp=src;
 }
 while((*dp==32) && (dp>=dest))
  *dp--=0;

 if(dp<dest)
  return 0;

 return (dp-dest+1);
}

// convert %HH (%hexa) strings to single chars (url address)
void pds_str_url_decode(char *strbegin)
{
 char *src, *dest;
 if(!strbegin)
  return;
 src=dest=strbegin;
 do{
  char c=*src++;
  if(!c)
  {
   *dest = 0;
   break;
  }
  if(c == '%'){
   c = pds_atol16(src);
   if(c || ((src[0]=='0') && (src[1]=='0')))
    src += 2;
   else
    c = *src;
  }
  *dest++ = c;
 }while(1);
}

// convert non chars (control codes) to spaces, cut spaces from the begin and end
unsigned int pds_str_clean(char *strbegin)
{
 char *str;
 if(!strbegin)
  return 0;
 str=strbegin;
 do{
  char c=*str;
  if(!c)
   break;
  if(c<0x20){
   c=0x20;
   *str=c;
  }
  str++;
 }while(1);
 return pds_strcutspc(strbegin);
}

void pds_str_conv_forbidden_chars(char *str,char *fromchars,char *tochars)
{
 if(!str || !str[0] || !fromchars || !fromchars[0] || !tochars || !tochars[0] || (pds_strlen(fromchars)!=pds_strlen(tochars)))
  return;

 do{
  char c=*str,*f,*t;
  if(!c)
   break;
  f=fromchars;
  t=tochars;
  do{
   if(c==*f){
    *str=*t;
    break;
   }
   f++,t++;
  }while(*f);
  str++;
 }while(1);
}

unsigned int pds_str_extendc(char *str,unsigned int newlen,char c)
{
 unsigned int currlen=pds_strlen(str);
 if(currlen<newlen){
  str+=currlen;
  do{
   *str++=c;
  }while((++currlen)<newlen);
 }
 return currlen;
}

unsigned int pds_str_fixlenc(char *str,unsigned int newlen,char c)
{
 pds_str_extendc(str,newlen,c);
 str[newlen]=0;
 return newlen;
}

// cut string at min(newlen, pos_of_limit_c)
int pds_str_limitc(char *src, char *dest, unsigned int newlen, char limit_c)
{
 unsigned int pos;
 char *end;
 int len;
 if(!dest)
  return -1;
 if(!newlen){
  *dest=0;
  return -1;
 }
 len = pds_strlen_mpxnative(src);
 if(len <= newlen){
  if(dest==src)
   return -1;
  return pds_strcpy(dest,src);
 }

 pos = pds_strpos_mpxnative(src, newlen);
 if(dest != src)
  pds_strncpy(dest, src, pos);
 dest[pos] = 0;
 end = pds_strrchr(dest, limit_c);
 if(end && ((end - dest) > (newlen >> 1))) { // FIXME: not utf8 calculation
  *end = 0;
  len = end - dest;
 }else
  len = -1;

 return len;
}

// get N. word of the string, beginning with 0
/*char *pds_str_getwordn(char *str, unsigned int wordcount)
{
 if(!str || !wordcount)
  return str;
 do{
  char c = *str;
  if(!c)
   break;
  if(c != ' '){
   str++;
   continue;
  }
  if(!(--wordcount))
   return str;
  while(*str  == ' ')
   str++;
 }while(1);
 return str;
}*/

unsigned int pds_utf8_str_centerize(char *str,unsigned int maxlen,unsigned int ispath)
{
 unsigned int lenc=pds_strlen(str), len8;
 if(!maxlen)
  return maxlen;
 if(!lenc)
  return lenc;
#ifdef MPXPLAY_UTF8
 len8=pds_utf8_strlen((mpxp_uint8_t *)str);
#else
 len8=lenc;
#endif
 if(len8<=maxlen){
  unsigned int sp1=(maxlen-len8)/2,sp2=maxlen-len8-sp1;
  if(sp1){
   pds_strmove(&str[sp1],str);
   pds_memset(&str[0],0x20,sp1);
  }
  if(sp2)
   pds_memset(&str[sp1+lenc],0x20,sp2);
 }else if(ispath && (len8>maxlen)){
  char *de=pds_strchr(str,PDS_DIRECTORY_SEPARATOR_CHAR);
  if(!de)
   de=pds_strchr(str,PDS_DIRECTORY_SEPARATOR_CHRO);
  if(de){
   unsigned int i=de-str+4;
   pds_strncpy(de+1,"...",3);
   if(len8>=(maxlen-i))
    pds_strmove(&str[i],&str[pds_utf8_strpos(str,len8-(maxlen-i))]);
  }
 }
 str[pds_utf8_strpos(str,maxlen)]=0;
 return len8;
}

void pds_listline_slice(char **listparts,char *cutchars,char *listline)
{
 unsigned int i=0,ccn=pds_strlen(cutchars);
 char *lastpart=listline;
 listparts[0]=listline;
 do{
  char *nextpart=pds_strchr(lastpart,cutchars[i++]);
  if(nextpart){
   *nextpart++=0;
   listparts[i]=nextpart;
   lastpart=nextpart;
  }
 }while(i<ccn);
}

void mpxplay_newfunc_string_listline_slice(char **listparts, unsigned int maxparts, char cutchar, char *listline)
{
 char *lastpart;
 unsigned int i;
 mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "mpxplay_newfunc_string_listline_slice START %s %d %c", listline, maxparts, cutchar );
 if(!listparts || !maxparts)
  return;
 pds_memset(listparts, 0, maxparts * sizeof(*listparts));
 if(!cutchar || !listline || !listline[0])
  return;
 lastpart = listline;
 pds_strcutspc(lastpart);
 if(!lastpart[0])
  return;
 i = 0;
 do{
  char *nextelem = pds_strchr(lastpart, cutchar);
  mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "mpxplay_newfunc_string_listline_slice i:%d next:%s last:%s", i, nextelem, lastpart);
  listparts[i] = lastpart;
  if(!nextelem)
   break;
  *nextelem++ = 0;
  pds_strcutspc(lastpart);
  lastpart = nextelem;
 }while(++i < maxparts);
}

// does str contains letters only?
unsigned int pds_chkstr_letters(char *str)
{
 if(!str || !str[0])
  return 0;
 do{
  char c=*str;
  if(!((c>='a') && (c<='z')) && !((c>='A') && (c<='Z'))) // found non-US letter char
   return 0;
  str++;
 }while(*str);
 return 1;
}

// does str contains uppercase chars only?
unsigned int pds_chkstr_uppercase(char *str)
{
 if(!str || !str[0])
  return 0;
 do{
  char c=*str;
  if((c>='a') && (c<='z')) // found lowercase char
   return 0;
  if(c>=128) // found non-us char
   return 0;
  str++;
 }while(*str);
 return 1;
}

//convert all lower-case letters to upper case (us-ascii only)
void pds_str_uppercase(char *str)
{
 if(!str || !str[0])
  return;
 do{
  char c = *str;
  if((c >= 'a') && (c <= 'z')){ // found lowercase char
   c = 'A' + (c - 'a');
   *str = c;
  }
  str++;
 }while(*str);
}

//convert all upper-case letters to lower case (us-ascii only)
void pds_str_lowercase(char *str)
{
 if(!str || !str[0])
  return;
 do{
  char c = *str;
  if((c >= 'A') && (c <= 'Z')){ // found uppercase char
   c = 'a' + (c - 'A');
   *str = c;
  }
  str++;
 }while(*str);
}

//-------------------------------------------------------------------------------------------------------

static unsigned int dekadlim[10]={1,10,100,1000,10000,100000,1000000,10000000,100000000,1000000000};

unsigned int pds_log10(long value)
{
 unsigned int dekad=1;
 while((value>=dekadlim[dekad]) && (dekad<10))
  dekad++;
 return dekad;
}

void pds_ltoa(int value,char *ltoastr)
{
 unsigned int dekad=pds_log10(value);
 do{
  *ltoastr++=(value/dekadlim[dekad-1])%10+0x30;
 }while(--dekad);
 *ltoastr=0;
}

/*void pds_ltoa16(int value,char *ltoastr)
{
 static int dekadlim[9]={1,0x10,0x100,0x1000,0x10000,0x100000,0x1000000,0x10000000,0x100000000};
 int dekad;

 dekad=1;
 while(value>dekadlim[dekad] && dekad<9)
  dekad++;
 do{
  int number=(value/dekadlim[dekad-1])%16;
  if(number>9)
   ltoastr[0]=number-10+'A';
  else
   ltoastr[0]=number+'0';
  dekad--;
  ltoastr++;
 }while(dekad>0);
 ltoastr[0]=0;
}*/

long pds_atol(char *strp)
{
 long number=0;
 unsigned int negative=0;

 if(!strp || !strp[0])
  return number;

 while(*strp==' ')
  strp++;

 if(*strp=='-'){
  negative=1;
  strp++;
 }else{
  if(*strp=='+')
   strp++;
 }

 do{
  if((strp[0]<'0') || (strp[0]>'9'))
   break;
  number=(number<<3)+(number<<1);     // number*=10;
  number+=(unsigned long)strp[0]-'0';
  strp++;
 }while(1);
 if(negative)
  number=-number;
 return number;
}

mpxp_int64_t pds_atoi64(char *strp)
{
 mpxp_int64_t number=0;
 unsigned int negative=0;

 if(!strp || !strp[0])
  return number;

 while(*strp==' ')
  strp++;

 if(*strp=='-'){
  negative=1;
  strp++;
 }else{
  if(*strp=='+')
   strp++;
 }

 do{
  if((strp[0]<'0') || (strp[0]>'9'))
   break;
  number=(number<<3)+(number<<1);     // number*=10;
  number+=(unsigned long)strp[0]-'0';
  strp++;
 }while(1);
 if(negative)
  number=-number;
 return number;
}

long pds_atol16(char *strp)
{
 unsigned long number=0;

 if(!strp || !strp[0])
  return number;

 while(*strp==' ')
  strp++;

 if(*((unsigned short *)strp)==(((unsigned short)'x'<<8)|(unsigned short)'0')) // C format
  strp+=2;
 else if(*strp=='#') // HTML format
  strp++;

 do{
  char c=*strp++;
  if(c>='0' && c<='9')
   c-='0';
  else
   if(c>='a' && c<='f')
    c-=('a'-10);
   else
    if(c>='A' && c<='F')
     c-=('A'-10);
    else
     break;

  number<<=4;     // number*=16;
  number+=(unsigned long)c;
 }while(1);

 return number;
}

void pds_str_to_hexs(char *src,char *dest,unsigned int destlen)
{
 if(!src || !dest || !destlen)
  return;
 do{
  unsigned char cl=*((unsigned char *)src),ch;
  if(!cl)
   break;
  ch=cl>>4;
  if(ch<=9)
   ch+='0';
  else
   ch+='A'-10;
  *((unsigned char *)dest)=ch;
  dest++;
  cl&=0x0f;
  if(cl<=9)
   cl+='0';
  else
   cl+='A'-10;
  *((unsigned char *)dest)=cl;
  dest++;
  src++;
  destlen-=2;
 }while(destlen>=2);
 *dest=0;
}

void pds_hexs_to_str(char *src,char *dest,unsigned int destlen)
{
 unsigned int i=0,d=0;
 if(!src || !dest || !destlen)
  return;
 do{
  unsigned char c=*((unsigned char *)src++);
  if(!c)
   break;
  if((c>='0') && (c<='9'))
   c-='0';
  else if((c>='A') && (c<='F'))
   c-='A'-10;
  else if((c>='a') && (c<='a'))
   c-='a'-10;
  else
   break;
  d=(d<<4)|c;
  if(i&1){
   *dest++=d;
   destlen--;
   d=0;
  }
  i++;
 }while(destlen>1);
 *dest=0;
}

//-----------------------------------------------------------------------
// some filename/path routines (string handling only, no DOS calls)
#define PDS_GETFILENAME_MODE_VIRTNAME (1 << 0) // get short virtual name (without http:// pretag) if no filename

static char *pds_getfilename_from_fullname_modeselect(char *fullname, unsigned int getfilename_mode)
{
 char *filenamep, *beginp, *virtual_drive = NULL;

 if(!fullname)
  return fullname;

 beginp = pds_strchr(fullname,':');
 if(beginp)
  beginp++;
 else
  beginp=fullname;
 if( ((beginp[0]==PDS_DIRECTORY_SEPARATOR_CHAR) && (beginp[1]==PDS_DIRECTORY_SEPARATOR_CHAR)) // virtual drive (like http://)
  || ((beginp[0]==PDS_DIRECTORY_SEPARATOR_CHRO) && (beginp[1]==PDS_DIRECTORY_SEPARATOR_CHRO)))
 {
  while((*beginp == PDS_DIRECTORY_SEPARATOR_CHAR) || (*beginp == PDS_DIRECTORY_SEPARATOR_CHRO))
   beginp++;
  virtual_drive = beginp;
 }
 if(!*beginp)
  return NULL;
 filenamep = pds_strrchr(beginp, PDS_DIRECTORY_SEPARATOR_CHAR);
 if(!filenamep)
  filenamep = pds_strrchr(beginp, PDS_DIRECTORY_SEPARATOR_CHRO);  // for http
 if(filenamep && filenamep[1])  // normal path/filename
  filenamep++;
 else if(virtual_drive && (getfilename_mode & PDS_GETFILENAME_MODE_VIRTNAME))  // no filename, use http address
  filenamep = virtual_drive;
 else if(virtual_drive)
  filenamep = NULL;
 else if(filenamep)
  filenamep++;
 else{
  filenamep = fullname;
  if((fullname[1]==':') && fullname[2]) // cut drive
   filenamep += 2;
 }

 return filenamep;
}

// returns NULL if no filename found
char *pds_getfilename_from_fullname(char *fullname)
{
 return pds_getfilename_from_fullname_modeselect(fullname, 0);
}

// returns fullname if no filename found
char *pds_getfilename_any_from_fullname(char *fullname)
{
 char *fn = pds_getfilename_from_fullname_modeselect(fullname, PDS_GETFILENAME_MODE_VIRTNAME);
 if(!fn || !*fn)
  fn = fullname;
 return fn;
}

// returns extension ptr
static char *pds_getfilename_noext_select(char *strout, char *fullname, unsigned int any)
{
 char *filename, *extension;

 if(!strout)
  return NULL;
 if(!fullname || !fullname[0]){
  *strout=0;
  return NULL;
 }

 if(any)
  filename = pds_getfilename_any_from_fullname(fullname);
 else
  filename = pds_getfilename_from_fullname(fullname);

 pds_strcpy(strout, filename);

 if(any){
  extension = pds_filename_get_extension(fullname);
  if(extension)
   extension = pds_strrchr(strout,'.');
 }else{
  extension = pds_strrchr(strout,'.');
 }
 if(extension)
  *extension++ = 0;

 return extension;
}

char *pds_getfilename_noext_from_fullname(char *strout,char *fullname)
{
 return pds_getfilename_noext_select(strout, fullname, 0);
}

char *pds_getfilename_any_noext_from_fullname(char *strout,char *fullname)
{
 return pds_getfilename_noext_select(strout, fullname, 1);
}

// cut at the last '\' char
char *pds_getpath_from_fullname(char *path,char *fullname)
{
 char *filenamep;

 if(!path)
  return path;

 if(!fullname){
  *path = 0;
  return fullname;
 }

 if(path != fullname)
  pds_strcpy(path, fullname);

 filenamep = pds_getfilename_from_fullname(path);
 if(!filenamep)
  filenamep = path;
 if(filenamep > path){
  *(filenamep - 1) = 0;
  filenamep = fullname + (filenamep - path);
 }else
  filenamep = NULL;

 return filenamep;
}

// cut at the last '\' char of real/root path
char *pds_getpath_nowildcard_from_filename(char *path,char *fullname)
{
 char *filenamep;

 if(!path)
  return path;
 if(!fullname){
  *path=0;
  return fullname;
 }

 if(path != fullname)
  pds_strcpy(path,fullname);

 do{
  filenamep = pds_getfilename_from_fullname(path);
  if(!filenamep)
   break;
  *(filenamep - 1) = 0;
 }while(pds_filename_wildcard_chk(path));

 if(filenamep >= path)
  filenamep = fullname + (filenamep - path);

 return filenamep;
}

char *pds_filename_get_extension_from_shortname(char *filename)
{
 char *ext;
 if(!filename || !filename[0])
  return NULL;
 ext = pds_strrchr(filename,'.');
 if(ext)
  ext++;
 return ext;
}

char *pds_filename_get_extension(char *fullname)
{
 return pds_filename_get_extension_from_shortname(pds_getfilename_from_fullname(fullname));
}

unsigned int pds_filename_conv_slashes_to_local(char *filename)
{
 unsigned int found=0;
 char c;
 if(!filename)
  return found;

 do{
  c=*filename;
  if(c==PDS_DIRECTORY_SEPARATOR_CHRO){
   c=PDS_DIRECTORY_SEPARATOR_CHAR;
   *filename=c;
   found=1;
  }
  filename++;
 }while(c);
 return found;
}

/*unsigned int pds_filename_conv_slashes_to_unxftp(char *filename)
{
 unsigned int found=0;
#if (PDS_DIRECTORY_SEPARATOR_CHAR!=PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP)
 char c;
 if(!filename)
  return found;

 do{
  c=*filename;
  if(c==PDS_DIRECTORY_SEPARATOR_CHAR){
   c=PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP;
   *filename=c;
   found=1;
  }
  filename++;
 }while(c);
#endif
 return found;
}*/

// !!! without path
void pds_filename_conv_forbidden_chars(char *filename)
{
 char c,*s,*d;
 s=d=filename;
 while((c=*s)){
  if((c=='?') || (c=='*') || (c==':') || (c=='|') || (c<0x20)){// || (c>254)){
   s++;
   continue;
  }
  if((c=='/') || (c=='\\'))
   c=',';
  else if(c=='\"')
   c='\'';
  else if(c=='<')
   c='[';
  else if(c=='>')
   c=']';
  *d=c;
  s++;d++;
 }
 *d=0;
}

int pds_getdrivenum_from_path(char *path)
{
 // a=0 b=1 c=2 ...
 if(path && path[0] && path[1]==':'){
  char d=path[0];
  if(d>='a' && d<='z')
   return (d-'a');
  if(d>='A' && d<='Z')
   return (d-'A');
  if(d>='0' && d<='7')         // !!!
   return ((d-'0') + ('Z'-'A'+1)); // for remote drives
 }
 return -1;
}

unsigned int pds_path_is_dir(char *path) // does path seem to be a directory?
{
 char *p;

 if(!path || !path[0])
  return 0;
 if(path[1]==':' && pds_getdrivenum_from_path(path)>=0) // d:
#if defined(MPXPLAY_WIN32) && defined(MPXPLAY_GUI_QT)
  if(path[2]==0 || (((path[2]==PDS_DIRECTORY_SEPARATOR_CHAR) || (path[2]==PDS_DIRECTORY_SEPARATOR_CHRO)) && path[3]==0))
#else
  if(path[2]==0 || (path[2]==PDS_DIRECTORY_SEPARATOR_CHAR && path[3]==0))
#endif
   return 1;
 if(path[0]=='.' && path[1]=='.' && path[2]==0) // ..
  return 1;
 if(path[0]=='.' && path[1]==0)                 // .
  return 1;
#if defined(MPXPLAY_WIN32) && defined(MPXPLAY_GUI_QT)
 if((path[1]==0) && ((path[0]==PDS_DIRECTORY_SEPARATOR_CHAR) || (path[0]==PDS_DIRECTORY_SEPARATOR_CHRO)))
#else
 if(path[0]==PDS_DIRECTORY_SEPARATOR_CHAR && path[1]==0) // backslash only
#endif
  return 1;
 p=pds_strrchr(path,PDS_DIRECTORY_SEPARATOR_CHAR); // backslash at end
#if defined(MPXPLAY_WIN32) && defined(MPXPLAY_GUI_QT)
 p=pds_strrchr(path,PDS_DIRECTORY_SEPARATOR_CHRO);
#endif
 if(p && p[1]==0)
  return 1;
 if(pds_filename_wildcard_chk(path)) // directory name/path may not contain wildcards
  return 0;

 return 1;
}

// !!! "d:" is also accepted as full-path
unsigned int pds_filename_check_absolutepath(char *path)
{
 char *dd=pds_strnchr(path,':',PDS_DIRECTORY_DRIVESTRLENMAX); // for non local drives too (like ftp:)
 if(dd && (!dd[1] || (dd[1]==PDS_DIRECTORY_SEPARATOR_CHAR_DOSWIN) || (dd[1]==PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP)))
  return 1;
 return 0;
}

//remove ".." from filename (ie: d:\temp\..\track01.mp3 -> d:\track01.mp3)
unsigned int pds_filename_remove_relatives(char *filename)
{
 char *fn,*dd,*next,*prev,*curr;
 char currdirstr[4]={PDS_DIRECTORY_SEPARATOR_CHAR,'.',PDS_DIRECTORY_SEPARATOR_CHAR,0};
 char updirstr[4]={PDS_DIRECTORY_SEPARATOR_CHAR,'.','.',0};

 if(!filename)
  return 0;

 fn=filename;
 do{
  dd=pds_strstr(fn,currdirstr); // "\.\"
  if(!dd)
   break;
  pds_strcpy(dd,dd+2); // "\.\" -> "\"
  fn=dd;
 }while(1);

 fn=filename;
 do{
  dd=pds_strstr(fn,updirstr); // "\.."
  if(!dd)
   break;
  prev=NULL;
  curr=dd-1;
  while(curr>=filename){      // search prev "\"
   if(*curr==PDS_DIRECTORY_SEPARATOR_CHAR){
    prev=curr;
    break;
   }
   curr--;
  }
  next=dd+3; // next "\" or eol
  if(!prev)
   prev=dd;
  if(!*next)   //
   prev++;     // "\.." -> "\"
  pds_strcpy(prev,next);
  fn=prev;
 }while(1);
 return pds_strlen(filename);
}

unsigned int pds_filename_build_fullpath(char *destbuf,char *currdir,char *filename)
{
 unsigned int len;
 char *p;

 if(!destbuf)
  return 0;
 if(!currdir || !filename){
  *destbuf=0;
  return 0;
 }

 if(pds_filename_check_absolutepath(filename)){
  pds_strcpy(destbuf,filename);
  return pds_filename_remove_relatives(destbuf);
 }

 if(!currdir[0]){
  *destbuf=0;
  return 0;
 }

 len=pds_strcpy(destbuf,currdir);

 if((filename[0]==PDS_DIRECTORY_SEPARATOR_CHAR_DOSWIN) || (filename[0]==PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP)){
  p=pds_strchr(destbuf,filename[0]);
  if(p){
   if(p[1]==filename[0]) // "//"
    p++;
   *p=0;
   len=p-destbuf;
  }
 }else{
  if((destbuf[len-1]!=PDS_DIRECTORY_SEPARATOR_CHAR_DOSWIN) && (destbuf[len-1]!=PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP) && filename[0])
   len+=pds_strcpy(&destbuf[len],PDS_DIRECTORY_SEPARATOR_STR);
 }
 pds_strcpy(&destbuf[len],filename);
 return pds_filename_remove_relatives(destbuf);
}

unsigned int pds_filename_assemble_fullname(char *destbuf,char *path,char *name)
{
 unsigned int len;
 if(!destbuf)
  return 0;
 if(path && path[0]){
  if(destbuf==path)
   len=pds_strlen(path);
  else
   len=pds_strcpy(destbuf,path);
  if((destbuf[len-1]!=PDS_DIRECTORY_SEPARATOR_CHAR_DOSWIN) && (destbuf[len-1]!=PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP))
   len+=pds_strcpy(&destbuf[len],PDS_DIRECTORY_SEPARATOR_STR);
  len+=pds_strcpy(&destbuf[len],pds_getfilename_from_fullname(name));
 }else
  len=pds_strcpy(destbuf,name);
 return len;
}

// not only for filenames
unsigned int pds_filename_wildcard_chk(char *filename)
{
 if(!filename)
  return 0;
 if( (pds_strlicmp(filename,"http:")==0)
  || (pds_strlicmp(filename,"mms:")==0)
  || (pds_strlicmp(filename,"rtsp:")==0)) // !!! dirty hack (http address/filename may contain '?' char)
  return 0;
 if(pds_strchr(filename,'?') || pds_strchr(filename,'*'))
  return 1;
 return 0;
}

unsigned int pds_filename_wildcard_cmp(char *fullname,char *mask)
{
 unsigned int match;
 char *fn,*fe,*mn,*me;

 if(!fullname || !fullname[0] || !mask || !mask[0])
  return 0;
 if(pds_strcmp(mask,PDS_DIRECTORY_ALLFILE_STR)==0 || pds_strcmp(mask,"*.?*")==0)
  return 1;

 fn=pds_getfilename_from_fullname(fullname);
 if(!fn)
  return 0;
 fe=pds_strrchr(fn,'.');
 if(fe){
  fe++;
  if(!fe[0])
   fe=NULL;
 }

 mn=mask;
 me=pds_strrchr(mn,'.');
 if(me){
  me++;
  if(!me[0])
   me=NULL;
 }

 if((!fe && me) || (fe && !me))
  return 0;

 //fprintf(stdout,"fn:%s fe:%s me:%s\n",fn,fe,me);

 match=1;
 while(fn[0] && (!fe || (fn<fe))){ // check filename (without extension)
  //fprintf(stdout,"fn:%c mn:%c\n",fn[0],mn[0]);
  if(!mn[0] || (me && mn>=me)){
   match=0;
   break;
  }
  if(mn[0]=='*')  // ie: track*
   break;
  if(mn[0]!='?'){
   char cf,cm;
   cf=fn[0];
   if(cf>='a' && cf<='z')
    cf-='a'-'A';
   cm=mn[0];
   if(cm>='a' && cm<='z')
    cm-='a'-'A';
   if(cf!=cm){
    match=0;
    break;
   }
  }
  fn++;mn++;
 }

 if(!fn[0] && mn[0] && (mn[0]!='*')) // mask is longer than filename
  match=0;
 if(me && (mn<me) && (mn[0]!='*')) // mask is longer than filename
  match=0;

 if(match && fe && me){ // check extension
  //fprintf(stdout,"fe:%c me:%c\n",fe[0],me[0]);
  do{
   if(!me[0]){
    if(fe[0])
     match=0;
    break;
   }
   if(me[0]=='*')
    break;
   if(!fe[0]){
    match=0;
    break;
   }
   if(me[0]!='?'){
    char cf,cm;
    cf=fe[0];
    if(cf>='a' && cf<='z')
     cf-='a'-'A';
    cm=me[0];
    if(cm>='a' && cm<='z')
     cm-='a'-'A';
    if(cf!=cm){
     match=0;
     break;
    }
   }
   fe++;me++;
  }while(1);
 }

 return match;
}

#ifndef MPXPLAY_UTF8
// search string in an other one, using wildcards (doesn't handle dot in filename)
unsigned int pds_wildcard_is_strstri(char *str,char *mask)
{
 unsigned int match;
 char ms;

 if(!mask || !mask[0])
  return 0;
 if(mask[0]=='*' && !mask[1])
  return 1;
 if(!str || !str[0])
  return 0;

 match=1;
 ms=*mask;
 if(ms>='a' && ms<='z')  // convert to uppercase (first character of mask)
  ms-=32;
 do{
  char c1=*str;
  if(!c1){
   match=0;
   break;
  }
  if(c1>='a' && c1<='z')  // convert to uppercase (current char of s1)
   c1-=32;
  if((c1==ms) || (ms=='?')){        // search the first occurence
   char *s1p=str,*s2p=mask;
   do{                 // compare the strings (part of str with mask)
    char c2=*(++s2p);
    if(!c2){
     match=2;
     break;
    }
    if(c2=='*'){
     if(s2p[1])
      match=0;
     else
      match=2;
     break;
    }
    c1=*(++s1p);
    if(!c1){
     match=0;
     break;
    }
    if(c2!='?'){
     if(c1>='a' && c1<='z')  // convert to uppercase
      c1-=32;
     if(c2>='a' && c2<='z')  // convert to uppercase
      c2-=32;
     if(c1!=c2)
      break;
    }
   }while(1);
  }
  str++;
 }while(match==1);
 return match;
}

// create a new filename from filename+mask (modification)
unsigned int pds_filename_wildcard_rename(char *destname,char *srcname,char *mask)
{
 char *fnd,*fns,*fne,*fed,*fes,*fee,*mn,*me;

 if(!destname)
  return 0;
 if(!srcname || !srcname[0] || !mask || !mask[0]){
  *destname=0;
  return 0;
 }

 if(!pds_filename_wildcard_chk(mask)){ // mask does not contain wildcards
  pds_strcpy(destname,mask);           // then use mask as a new filename
  return 1;
 }

 pds_strcpy(destname,srcname);

 if(pds_strcmp(mask,"*.*")==0 || pds_strcmp(mask,"*.?*")==0)
  return 1;

 fns=pds_getfilename_from_fullname(srcname);
 if(!fns)
  return 0;
 fes=pds_strrchr(fns,'.');
 if(fes){
  fne=fes;                 // end of 1st part (before extension)
  fes++;
 }else
  fne=fes=fns+pds_strlen(fns); // end of srcfilename (no extension)

 fnd=pds_getfilename_from_fullname(destname);
 if(!fnd)
  return 0;
 fed=pds_strrchr(fnd,'.');
 if(fed)
  fed++;

 mn=mask;
 me=pds_strrchr(mn,'.');
 if(me)
  me++;

 do{ // check filename (without extension)
  //fprintf(stdout,"1. fns:%c fnd:%c mn:%c %8.8X %8.8X\n",fns[0],fnd[0],mn[0],mn,me);
  if(!mn[0] || (me && mn>=(me-1))){ // end of first part of the mask
   break;
  }
  if(mn[0]=='*'){  // ie: track*
   if(fns<fne)
    fnd+=(fne-fns); // skip non-modified chars
   break;
  }
  if(mn[0]=='?'){
   if(fns>=fne) // mask run out from filename (without extension)
    break;
   *fnd=*fns; // copy char from src to dest
  }else
   *fnd=*mn;  // modify/insert filename with char from mask
  //fprintf(stdout,"3. fns:%c fnd:%c mn:%c\n",fns[0],fnd[0],mn[0]);
  fns++;fnd++;mn++;
 }while(1);

 fnd[0]=fnd[1]=0;// close filename
 fed=&fnd[1];    // new extension pos (leave space for dot)

 //if(fes || me){ // check extension
 if(me && me[0]){
  fee=fes+pds_strlen(fes); // end of extension (src)
  do{
   //fprintf(stdout,"1. fes:%c fed:%c me:%c\n",fes[0],fed[0],me[0]);
   if(me[0]=='*'){  // ie: .m*
    while(*fes)     // no more modification
     *fed++=*fes++; // copy left chars from src to dest
    break;
   }

   if(me[0]=='?'){
    if(fes>=fee)// mask run out from srcfilename
     break;     // finish
    *fed=*fes;  // copy char from src to dest
   }else
    *fed=*me;   // modify extension with char from mask
   //fprintf(stdout,"3. fes:%c fed:%c me:%c\n",fes[0],fed[0],me[0]);
   fed++;fes++;me++;
  }while(me[0]);

  *fed=0;
 }

 if(fed>&fnd[1]) // filename has extension
  fnd[0]='.';    // insert dot in filename before extension

 return 1;
}
#endif // !MPXPLAY_UTF8

//-------------------------------------------------------------------------

unsigned int pds_UTF16_strlen(mpxp_uint16_t *strp)
{
 mpxp_uint16_t *beginp;
 if(!strp || !strp[0])
  return 0;
 beginp=strp;
 do{
  strp++;
 }while(*strp);
 return (unsigned int)(strp-beginp);
}

//-------------------------------------------------------------------------

#ifdef MPXPLAY_UTF8
extern unsigned short mapping_unicode_cp437[256];

unsigned int pds_str_CP437_to_UTF16LE(mpxp_wchar_t *utf16,char *src,unsigned dest_buflen)
{
 unsigned int len_out=0;
 dest_buflen>>=1;
 if(!utf16 || !src || !dest_buflen)
  return len_out;
 do{
  unsigned char c=*src++;
  if(!c)
   break;
  *utf16++=mapping_unicode_cp437[c];
  len_out++;
 }while(len_out<dest_buflen);
 utf16[0]=0;
 return len_out;
}

mpxp_wchar_t pds_cvchar_CP437_to_UTF16LE(mpxp_wchar_t c)
{
 if(c>=0x0100)
  return c;
 return mapping_unicode_cp437[c];
}

unsigned int pds_strn_UTF8_to_UTF16LE_u8bl(mpxp_uint16_t *utf16,mpxp_uint8_t *utf8,unsigned int dest_buflen,int dest_strlen,unsigned int *ut8_blen)
{
 unsigned int index_out=0,destbuflen_save=dest_buflen;
 mpxp_uint8_t *ut8_begin=utf8;

 dest_buflen>>=1; // bytes to wchars
 if(!utf16 || !dest_buflen)
  goto err_out_u8u16;
 utf16[0]=0;
 if(!utf8 || (dest_strlen==0))
  goto err_out_u8u16;

 if(dest_strlen<0){
  dest_strlen=0x7fffffff;
  dest_buflen--; // for trailing zero
 }

 //if((utf8[0]==0xef) && (utf8[1]==0xbb) && (utf8[2]==0xbf))
 // utf8+=3;

 do{
  unsigned short unicode;
  unsigned int codesize;
  unsigned char c;

  c=utf8[0];
  if(!c)
   break;

  codesize=0;

  if(c&0x80){
   if((c&0xe0)==0xe0){
    unicode = (c&0x0F) << 12;
    c = utf8[1];
    if(c&0x80){
     unicode |= (c&0x3F) << 6;
     c = utf8[2];
     if(c&0x80){
      unicode |= (c&0x3F);
      codesize=3;
     }
    }
   }else{
    unicode = (c&0x3F) << 6;
    c = utf8[1];
    if(c&0x80){
     unicode |= (c&0x3F);
     codesize=2;
    }
   }
  }

  if(codesize){
   PDS_PUTB_LE16(&utf16[0],unicode);
   utf8+=codesize;
  }else{
   PDS_PUTB_LE16(&utf16[0],(mpxp_uint16_t)utf8[0]);
   utf8++;
  }
  utf16++;
  index_out++;
 }while((index_out<dest_buflen) && (index_out<dest_strlen));

 if((index_out<destbuflen_save) && (index_out<dest_strlen))
  utf16[0]=0;

err_out_u8u16:
 if(ut8_blen)
  *ut8_blen=utf8-ut8_begin;
 return index_out;
}

unsigned int pds_str_UTF8_to_UTF16LE(mpxp_uint16_t *utf16,mpxp_uint8_t *utf8,unsigned int dest_buflen)
{
 return pds_strn_UTF8_to_UTF16LE_u8bl(utf16,utf8,dest_buflen,-1,NULL);
}

// gives back binary length of utf8 string too (if lenu16==lenbu8 then no utf8 code in the string)
unsigned int pds_str_UTF8_to_UTF16LE_get_u8bl(mpxp_uint16_t *utf16,mpxp_uint8_t *utf8,unsigned int dest_buflen,unsigned int *ut8_blen)
{
 return pds_strn_UTF8_to_UTF16LE_u8bl(utf16,utf8,dest_buflen,-1,ut8_blen);
}

unsigned int pds_strn_UTF16LE_to_UTF8(mpxp_uint8_t *utf8,mpxp_uint16_t *utf16,unsigned int dest_buflen,int dest_strlen)
{
 unsigned int bytes_out=0,chars_out=0,destbuflen_save=dest_buflen;

 if(!utf8 || !utf16 || !dest_buflen || (dest_strlen==0))
  return bytes_out;

 if(dest_strlen<0){
  dest_strlen=0x7fffffff;
  dest_buflen--; // for trailing zero
 }

 do{
  unsigned short wc=PDS_GETB_LEU16(utf16++);

  if(!wc)
   break;

  if(wc < (1<<7)){
   utf8[0] = wc;
   utf8++;
   bytes_out++;
  }else if(wc < (1<<11)){
   if((bytes_out+2)>dest_buflen)
    break;
   utf8[0] = 0xc0 | (wc >> 6);
   utf8[1] = 0x80 | (wc & 0x3f);
   utf8+=2;
   bytes_out+=2;
  }else{
   if((bytes_out+3)>dest_buflen)
    break;
   utf8[0] = 0xe0 | (wc >> 12);
   utf8[1] = 0x80 | ((wc >> 6) & 0x3f);
   utf8[2] = 0x80 | (wc & 0x3f);
   utf8+=3;
   bytes_out+=3;
  }
  chars_out++;
 }while((bytes_out<dest_buflen) && (chars_out<dest_strlen));

 if((bytes_out<destbuflen_save) && (chars_out<dest_strlen))
  utf8[0]=0;

 return bytes_out;
}

unsigned int pds_str_UTF16LE_to_UTF8(mpxp_uint8_t *utf8,mpxp_uint16_t *utf16,unsigned int dest_buflen)
{
 return pds_strn_UTF16LE_to_UTF8(utf8,utf16,dest_buflen,-1);
}

unsigned int pds_utf8_strlen(mpxp_uint8_t *utf8)
{
 unsigned int index_out=0;

 if(!utf8)
  return index_out;

 do{
  unsigned int codesize;
  unsigned char c;

  c=utf8[0];
  if(!c)
   break;

  codesize=1;

  if(c&0x80){
   if((c&0xe0)==0xe0){
    if(utf8[1]&0x80)
     if(utf8[2]&0x80)
      codesize=3;
   }else{
    if(utf8[1]&0x80)
     codesize=2;
   }
  }
  utf8+=codesize;
  index_out++;
 }while(1);

 return index_out;
}

// gives back the byte position of pos. utf8-char (to cut utf8 strings correctly)
unsigned int pds_utf8_strpos(mpxp_uint8_t *utf8,unsigned int pos)
{
 mpxp_uint8_t *utf8_begin=utf8;
 unsigned int index_out=0;

 if(!utf8 || !pos)
  return 0;

 do{
  unsigned int codesize;
  unsigned char c;

  c=utf8[0];
  if(!c)
   break;

  codesize=1;

  if(c&0x80){
   if((c&0xe0)==0xe0){
    if(utf8[1]&0x80)
     if(utf8[2]&0x80)
      codesize=3;
   }else{
    if(utf8[1]&0x80)
     codesize=2;
   }
  }
  utf8+=codesize;
  index_out++;
 }while(index_out<pos);

 return (unsigned int)(utf8-utf8_begin);
}

// mixed mode stricmp is bad at sort (else have to be good)
/*int pds_utf8_stricmp(char *str1,char *str2)
{
 int retcode;
 unsigned int len1,len2,u8blen1,u8blen2;
 mpxp_wchar_t u16buf1[MAX_PATHNAMEU16*2],u16buf2[MAX_PATHNAMEU16*2];// !!! static size
 retcode=pds_strchknull(str1,str2);
 if(retcode!=2)
  return retcode;
 len1=pds_str_UTF8_to_UTF16LE_get_u8bl(u16buf1,str1,sizeof(u16buf1),&u8blen1);
 len2=pds_str_UTF8_to_UTF16LE_get_u8bl(u16buf2,str2,sizeof(u16buf2),&u8blen2);
 if((len1==u8blen1) && (len2==u8blen2)) // ??? there are only US-chars in the strings
  return pds_stricmp(str1,str2);
 switch(CompareStringW(LOCALE_USER_DEFAULT,NORM_IGNORECASE,u16buf1,-1,u16buf2,-1)){
  case CSTR_LESS_THAN:return -1;
  case CSTR_GREATER_THAN:return 1;
 }
 if(len1!=len2){
  if(len1<len2)
   return -1;
  return 1;
 }
 return 0;
}*/

int pds_utf8_stricmp(char *str1,char *str2)
{
 int retcode;
 unsigned int len1,len2;
 mpxp_wchar_t u16buf1[MAX_STRLEN/2],u16buf2[MAX_STRLEN/2];// !!! static size
 retcode=pds_strchknull(str1,str2);
 if(retcode!=2)
  return retcode;
 len1=pds_str_UTF8_to_UTF16LE(u16buf1,str1,sizeof(u16buf1));
 len2=pds_str_UTF8_to_UTF16LE(u16buf2,str2,sizeof(u16buf2));
 switch(CompareStringW(LOCALE_USER_DEFAULT,NORM_IGNORECASE|SORT_STRINGSORT,u16buf1,-1,u16buf2,-1)){
  case CSTR_LESS_THAN:return -1;
  case CSTR_GREATER_THAN:return 1;
 }
 if(len1!=len2){
  if(len1<len2)
   return -1;
  return 1;
 }
 return 0;
}

// returns 1 if found (not the str pointer)
unsigned int pds_utf8_is_strstri(char *str1,char *str2)
{
 unsigned int len1,len2,u8blen;
 mpxp_wchar_t *u16b1i,u16buf1[MAX_STRLEN/sizeof(mpxp_wchar_t)],u16buf2[MAX_STRLEN/sizeof(mpxp_wchar_t)/2];// !!! static size
 len2=pds_str_UTF8_to_UTF16LE_get_u8bl(u16buf2,str2,sizeof(u16buf2),&u8blen);
 if(!len2)
  return 0;
 if(len2==u8blen) // no utf8 code in the 2. string
  return (mpxp_ptrsize_t)pds_strstri(str1,str2);
 len1=pds_str_UTF8_to_UTF16LE_get_u8bl(u16buf1,str1,sizeof(u16buf1),&u8blen);
 if((len1<len2) || (len1==u8blen)) // no utf8 code in the 1. string (but there is in the 2.)
  return 0;
 if(!LCMapStringW(LOCALE_USER_DEFAULT,LCMAP_LOWERCASE,u16buf1,-1,u16buf1,sizeof(u16buf1)/sizeof(mpxp_wchar_t)))
  return 0;
 if(!LCMapStringW(LOCALE_USER_DEFAULT,LCMAP_LOWERCASE,u16buf2,-1,u16buf2,sizeof(u16buf2)/sizeof(mpxp_wchar_t)))
  return 0;
 u16b1i=&u16buf1[0];
 len1-=len2;
 do{
  unsigned int l2=len2;
  mpxp_wchar_t *u1=u16b1i,*u2=&u16buf2[0];
  do{
   if(u1[0]!=u2[0])
    break;
   u1++;u2++;
  }while(--l2);
  if(!l2)
   return 1;
  if(!len1)
   break;
  u16b1i++;
  len1--;
 }while(1);
 return 0;
}

//--------------------------------------------------------------------------

unsigned int pds_utf8_filename_wildcard_cmp(mpxp_uint8_t *fullname,mpxp_uint8_t *mask)
{
 unsigned int match;//,lenf,lenm;
 mpxp_wchar_t *fn,*fe,*mn,*me;
 mpxp_wchar_t u16_filename[MAX_PATHNAMEU16],u16_mask[MAX_PATHNAMEU16/2];

 if(!fullname || !fullname[0] || !mask || !mask[0])
  return 0;
 if(pds_strcmp(mask,PDS_DIRECTORY_ALLFILE_STR)==0 || pds_strcmp(mask,"*.?*")==0)
  return 1;

 fn=(mpxp_wchar_t *)pds_getfilename_from_fullname(fullname);
 if(!fn)
  return 0;

 if(!pds_filename_wildcard_chk(mask)){
  if(pds_utf8_stricmp((mpxp_uint8_t *)fn,mask)==0)
   return 1;
  return 0;
 }

 if(!pds_str_UTF8_to_UTF16LE(u16_filename,(mpxp_uint8_t *)fn,sizeof(u16_filename)))
  return 0;
 if(!pds_str_UTF8_to_UTF16LE(u16_mask,mask,sizeof(u16_mask)))
  return 0;

 if(!LCMapStringW(LOCALE_USER_DEFAULT,LCMAP_LOWERCASE,u16_filename,-1,u16_filename,sizeof(u16_filename)/sizeof(mpxp_wchar_t)))
  return 0;
 if(!LCMapStringW(LOCALE_USER_DEFAULT,LCMAP_LOWERCASE,u16_mask,-1,u16_mask,sizeof(u16_mask)/sizeof(mpxp_wchar_t)))
  return 0;

 fn=&u16_filename[0];
 fe=pds_wchar_strrchr(fn,'.');
 if(fe){
  fe++;
  if(!fe[0])
   fe=NULL;
 }

 mn=&u16_mask[0];
 me=pds_wchar_strrchr(mn,'.');
 if(me){
  me++;
  if(!me[0])
   me=NULL;
 }

 if((!fe && me) || (fe && !me))
  return 0;

 match=1;
 while(fn[0] && (!fe || (fn<fe))){ // check filename (without extension)
  if(!mn[0] || (me && mn>=me)){
   match=0;
   break;
  }
  if(mn[0]=='*')  // ie: track*
   break;
  if(mn[0]!='?'){
   if(fn[0]!=mn[0]){
    match=0;
    break;
   }
  }
  fn++;mn++;
 }

 if(!fn[0] && mn[0] && (mn[0]!='*')) // mask is longer than filename
  match=0;
 if(me && (mn<me) && (mn[0]!='*')) // mask is longer than filename
  match=0;

 if(match && fe && me){ // check extension
  do{
   if(!me[0]){
    if(fe[0])
     match=0;
    break;
   }
   if(me[0]=='*')
    break;
   if(!fe[0]){
    match=0;
    break;
   }
   if(me[0]!='?'){
    if(fe[0]!=me[0]){
     match=0;
     break;
    }
   }
   fe++;me++;
  }while(1);
 }

 return match;
}

unsigned int pds_utf8_wildcard_is_strstri(mpxp_uint8_t *str,mpxp_uint8_t *mask)
{
 unsigned int match;
 mpxp_wchar_t ms,*sp,*mp,u16_str[MAX_STRLEN/sizeof(mpxp_wchar_t)],u16_mask[MAX_STRLEN/8];

 if(!mask || !mask[0])
  return 0;
 if(mask[0]=='*' && !mask[1])
  return 1;
 if(!str || !str[0])
  return 0;

 if(!pds_str_UTF8_to_UTF16LE(u16_str,str,sizeof(u16_str)))
  return 0;
 if(!pds_str_UTF8_to_UTF16LE(u16_mask,mask,sizeof(u16_mask)))
  return 0;

 if(!LCMapStringW(LOCALE_USER_DEFAULT,LCMAP_LOWERCASE,u16_str,-1,u16_str,sizeof(u16_str)/sizeof(mpxp_wchar_t)))
  return 0;
 if(!LCMapStringW(LOCALE_USER_DEFAULT,LCMAP_LOWERCASE,u16_mask,-1,u16_mask,sizeof(u16_mask)/sizeof(mpxp_wchar_t)))
  return 0;

 match=1;
 sp=&u16_str[0];
 mp=&u16_mask[0];
 ms=*mp;
 do{
  mpxp_wchar_t c1=*sp;
  if(!c1){
   match=0;
   break;
  }
  if((c1==ms) || (ms=='?')){        // search the first occurence
   mpxp_wchar_t *s1p=sp,*s2p=mp;
   do{                 // compare the strings (part of str with mask)
    mpxp_wchar_t c2=*(++s2p);
    if(!c2){
     match=2;
     break;
    }
    if(c2=='*'){
     if(s2p[1])
      match=0;
     else
      match=2;
     break;
    }
    c1=*(++s1p);
    if(!c1){
     match=0;
     break;
    }
    if(c2!='?'){
     if(c1!=c2)
      break;
    }
   }while(1);
  }
  sp++;
 }while(match==1);
 return match;
}

unsigned int pds_utf8_filename_wildcard_rename(mpxp_uint8_t *destname,mpxp_uint8_t *srcname,mpxp_uint8_t *mask)
{
 mpxp_wchar_t *fnd,*fns,*fne,*fed,*fes,*fee,*mn,*me;
 mpxp_wchar_t u16dest[MAX_STRLEN/sizeof(mpxp_wchar_t)],u16src[MAX_STRLEN/sizeof(mpxp_wchar_t)],u16mask[MAX_STRLEN/8];
 char srcpath[MAX_PATHNAMEU08];

 if(!destname)
  return 0;
 if(!srcname || !srcname[0] || !mask || !mask[0]){
  *destname=0;
  return 0;
 }

 if(!pds_filename_wildcard_chk(mask)){ // mask does not contain wildcards
  pds_strcpy(destname,mask);           // then use mask as a new filename
  return 1;
 }

 pds_strcpy(destname,srcname);

 if(pds_strcmp(mask,"*.*")==0 || pds_strcmp(mask,"*.?*")==0)
  return 1;

 fns=(mpxp_wchar_t *)pds_getpath_from_fullname(srcpath,srcname);
 if(!fns)
  return 0;
 if(!pds_str_UTF8_to_UTF16LE(u16src,(mpxp_uint8_t *)fns,sizeof(u16src)))
  return 0;
 fns=&u16src[0];
 fes=pds_wchar_strrchr(fns,'.');
 if(fes){
  fne=fes;                 // end of 1st part (before extension)
  fes++;
 }else
  fne=fes=fns+pds_wchar_strlen(fns); // end of srcfilename (no extension)

 fnd=(mpxp_wchar_t *)pds_getfilename_from_fullname(destname);
 if(!fnd)
  return 0;
 if(!pds_str_UTF8_to_UTF16LE(u16dest,(mpxp_uint8_t *)fnd,sizeof(u16src)))
  return 0;
 fnd=&u16dest[0];
 fed=pds_wchar_strrchr(fnd,'.');
 if(fed)
  fed++;

 if(!pds_str_UTF8_to_UTF16LE(u16mask,mask,sizeof(u16mask)))
  return 0;
 mn=&u16mask[0];
 me=pds_wchar_strrchr(mn,'.');
 if(me)
  me++;

 do{ // check filename (without extension)
  if(!mn[0] || (me && mn>=(me-1))){ // end of first part of the mask
   break;
  }
  if(mn[0]=='*'){  // ie: track*
   if(fns<fne)
    fnd+=(fne-fns); // skip non-modified chars
   break;
  }
  if(mn[0]=='?'){
   if(fns>=fne) // mask run out from filename (without extension)
    break;
   *fnd=*fns; // copy char from src to dest
  }else
   *fnd=*mn;  // modify/insert filename with char from mask
  fns++;fnd++;mn++;
 }while(1);

 fnd[0]=fnd[1]=0;// close filename
 fed=&fnd[1];    // new extension pos (leave space for dot)

 if(me && me[0]){ // check extension
  fee=fes+pds_wchar_strlen(fes); // end of extension (src)
  do{
   if(me[0]=='*'){  // ie: .m*
    while(*fes)     // no more modification
     *fed++=*fes++; // copy left chars from src to dest
    break;
   }

   if(me[0]=='?'){
    if(fes>=fee)// mask run out from srcfilename
     break;     // finish
    *fed=*fes;  // copy char from src to dest
   }else
    *fed=*me;   // modify extension with char from mask
   fed++;fes++;me++;
  }while(me[0]);

  *fed=0;
 }

 if(fed>&fnd[1]) // filename has extension
  fnd[0]='.';    // insert dot in filename before extension

 if(!pds_str_UTF16LE_to_UTF8((mpxp_uint8_t *)&u16mask[0],u16dest,sizeof(u16mask)))
  return 0;

 pds_filename_assemble_fullname(destname,srcpath,(mpxp_uint8_t *)&u16mask[0]);

 return 1;
}

//----------------------------------------------------------------------

unsigned int pds_wchar_strcpy(mpxp_uint16_t *dest,mpxp_uint16_t *src)
{
 mpxp_uint16_t *begin;
 if(!dest || !src)
  return 0;
 begin=src;
 do{
  *dest=*src;
  if(!src[0])
   break;
  dest++;src++;
 }while(1);
 return (unsigned int)(src-begin); // returns the lenght of string, not the target pointer!
}

unsigned int pds_wchar_strmove(mpxp_wchar_t *dest,mpxp_wchar_t *src)
{
 unsigned int len,count;
 if(!dest || !src)
  return 0;
 if(dest<src)
  return pds_wchar_strcpy(dest,src);
 count=len=pds_wchar_strlen(src)+1;
 src+=len;
 dest+=len;
 do{
  src--;dest--;
  *dest=*src;
 }while(--count);
 return len; // returns the lenght of string
}

mpxp_wchar_t *pds_wchar_strrchr(mpxp_wchar_t *strp,mpxp_wchar_t seek)
{
 mpxp_wchar_t *foundp=NULL,curr;

 if(!strp)
  return foundp;

 curr=*strp;
 if(!curr)
  return foundp;
 do{
  if(curr==seek)
   foundp=strp;
  strp++;
  curr=*strp;
 }while(curr);
 return foundp;
}

#endif // MPXPLAY_UTF8
