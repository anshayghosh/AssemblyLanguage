/*program: ax.  assemble and execute CSC258 assembly language programs*/
/* written 2003 September 2.  author: E.Hehner. */
/* modified 2004 March 7 by Hehner */
/*   to fix UNIX input problem, endian problem, and allow tabs */
/* modified 2005 January 21 by Hehner to make padding uniform */
/*   and to get rid of message on normal return */
/* modified 2005 November 14 by Hehner to fix OUT instruction */
/* modified 2005 December 13 by Hehner to add CFI instruction */
/* modified 2007 June 22 by Hehner to change CFI and CIF instructions */
/*   so that they load AC, and to give better trace output */
/* modified 2007 June 24 by Hehner to allow absolute addresses */
/* modified 2007 December 19 by Hehner to add A format, */
/*   remove most quotes, allow blank lines, and improve tracing */
/* modified 2010 February 10 by Hehner to reverse C format */
/* modified 2010 November 4 by Hehner to randomize initial RAM and AC */

#define MaxLineLen 100
#define MaxIdLen 20
#define RamSize 5000
#define LabTabSize 500

#include <signal.h>     /*UNIX*/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
FILE *fpin, *fopen();
char *filename;
char line[MaxLineLen+2];  /* one line of source */

/*op-codes*/
#define NumOps 22
char ops[NumOps+1][4] = {"LDA", "STA", "ADD", "SUB", "MUL",
  "DIV", "MOD", "FLA", "FLS", "FLM", "FLD", "CIF", "CFI", "AND",
  "IOR", "XOR", "BUN", "BZE", "BSA", "BIN", "INP", "OUT"};

/*hardware*/
union {unsigned int u; signed int i; float f;} RAM[RamSize], AC;
unsigned char IRop;  unsigned int IRadr;  /* IR in two parts */
unsigned int PC;
unsigned char E;

int firstfreeRAMadr; /* initialized to 0 */
int numsourcelines[RamSize+1];/*number of source before and with each*/
                              /* object line, initialized to all 0s */
unsigned char err;  /* error during assemble, initialized to 0 */

void assemble (void) /*initializes RAM and PC*/
{ struct {char id[MaxIdLen+1]; unsigned char def; unsigned int adr;}
    labeltable[LabTabSize]
  = {{"main", 0, RamSize}, {"opsys", 1, RamSize}};
  int lastlabel = 1;  int thislabel;
  int aRAMadr, bRAMadr;
  int h, i, j, k; /*positions in line*/
  int op;
  unsigned int uu, vv;

  /* start with random values in RAM and AC */
  for(aRAMadr=0; aRAMadr<RamSize; aRAMadr++) RAM[aRAMadr].u = rand();
  AC.u = rand();

  while (fgets(line, MaxLineLen+2, fpin)!=NULL)
  { if (firstfreeRAMadr>=RamSize)
      {printf("assemble: memory size too small\n"); exit(0);}
    numsourcelines[firstfreeRAMadr]++;

    i=0; label: while(line[i]==' ' || line[i]=='\t') i++;
    if(line[i]=='\r' || line[i]=='\n') continue; /*restart the loop*/
    j=i; while(line[j]!=' ' && line[j]!='\t' && line[j]!=':'
               && line[j]!='\'' && line[j]!='\r' && line[j]!='\n') j++;
    k=j; while(line[k]==' ' || line[k]=='\t') k++;
    if(j>i && line[k]==':') /*we have a label*/
    { if(j-i>MaxIdLen)
        {printf("assemble: label too long\n%s\n", line); err=1; continue;}
      h=0; while(i<j) {labeltable[lastlabel+1].id[h] = line[i]; h++; i++;}
      labeltable[lastlabel+1].id[h] = '\0';
      thislabel = 0;
      while(strcmp(labeltable[thislabel].id, labeltable[lastlabel+1].id)!=0)
        thislabel++;
      if(thislabel<=lastlabel) /*it was already there*/
      { if(labeltable[thislabel].def)
          {printf("assemble: duplicate label\n%s\n", line); err=1; continue;}
        /*fixup forward addresses*/
        aRAMadr = labeltable[thislabel].adr;
        while(aRAMadr!=RamSize)
        { bRAMadr = RAM[aRAMadr].u & 0x00FFFFFF;
          RAM[aRAMadr].u = (RAM[aRAMadr].u & 0xFF000000) | firstfreeRAMadr;
          aRAMadr = bRAMadr;
        } /*end while*/
      } else lastlabel = thislabel;
      labeltable[thislabel].def = 1;
      labeltable[thislabel].adr = firstfreeRAMadr;
      i = k+1; goto label;
    } /*end of label processing*/

    if(i==j) {printf("assemble: bad format\n%s\n", line); err=1; continue;}
    /*now i<j<=k
      and (line[i] is not blank nor tab nor colon nor quote nor return nor newline)
      and (line[j] is blank or tab or colon or quote or return or newline)
      and (line[k] is not blank nor tab nor colon) */
    /*process instruction or data*/
    if(j-i==3) /*op-code*/
    { h=0; while(i<j) {ops[NumOps][h] = line[i];  h++;  i++;}
      ops[NumOps][h] = '\0';
      op = 0; while(strcmp(ops[op], ops[NumOps])!=0) op++;
      if(op==NumOps)
        {printf("assemble: bad format\n%s\n", line); err=1; continue;}
      RAM[firstfreeRAMadr].u = op * 0x01000000;
      address: /*find address starting at line[k]*/
      if('0'<=line[k] && line[k]<='9') /*we have an absolute address*/
      { aRAMadr = line[k] - '0';  k++;
        while('0'<=line[k] && line[k]<='9')
        { aRAMadr = aRAMadr*10 + line[k] - '0';  k++; }
        RAM[firstfreeRAMadr].u += aRAMadr; /*no check for too big address*/
      } else
      { i=j=k;
        while(line[j]!=' ' && line[j]!='\t' && line[j]!=':'
              && line[j]!='\'' && line[j]!='\r' && line[j]!='\n') j++;
        if(i==j){printf("assemble: bad format\n%s\n", line); err=1; continue;}
        /*we have an identifier address*/
        if(j-i>MaxIdLen)
          {printf("assemble: identifier too long\n%s\n", line); err=1; continue;}
        h=0; while(i<j) {labeltable[lastlabel+1].id[h] = line[i]; h++; i++;}
        labeltable[lastlabel+1].id[h] = '\0';
        thislabel = 0;
        while(strcmp(labeltable[thislabel].id, labeltable[lastlabel+1].id)!=0)
          thislabel++;
        if(thislabel<=lastlabel) /*it was already there*/
          if(labeltable[thislabel].def)
            RAM[firstfreeRAMadr].u += labeltable[thislabel].adr;
          else /*it's another forward reference*/
          { RAM[firstfreeRAMadr].u += labeltable[thislabel].adr;
            labeltable[thislabel].adr = firstfreeRAMadr;
          }
        else /*it's a new forward reference*/
        { labeltable[thislabel].def = 0;
          labeltable[thislabel].adr = firstfreeRAMadr;
          RAM[firstfreeRAMadr].u += RamSize;
          lastlabel = thislabel;
        }
      } firstfreeRAMadr++;
    } else if(j-i==1) /*data or W*/
    { if(line[i]=='I')
      { if(sscanf(&line[k], "%d", &RAM[firstfreeRAMadr].i)!=1)
          {printf("assemble: bad format\n%s\n", line); err=1; continue;}
        firstfreeRAMadr++;
      } else if(line[i]=='F')
      { if(sscanf(&line[k], "%e", &RAM[firstfreeRAMadr].f)!=1)
          {printf("assemble: bad format\n%s\n", line); err=1; continue;}
        firstfreeRAMadr++;
      } else if(line[i]=='C' && line[k]=='\'')
      { uu = 0;  i = j = k+1;  nextchar:
        if(line[j]=='\'') {RAM[firstfreeRAMadr].u = uu; firstfreeRAMadr++;}
        else if (j-i<4)
             { vv = line[j]; for(k=i; k<j; k++) vv *= 256;
               uu += vv; j++; goto nextchar;
             }
        else {printf("assemble: bad format\n%s\n", line); err=1; continue;}
      } else if(line[i]=='B' && (line[k]=='0' || line[k]=='1'))
      { uu = 0;  i = j = k;  nextbit:
        if (j-i<32 && line[j]=='0') {uu = uu*2;  j++;  goto nextbit;}
        else if (j-i<32 && line[j]=='1') {uu = uu*2 + 1;  j++;  goto nextbit;}
        else {RAM[firstfreeRAMadr].u = uu; firstfreeRAMadr++;}
      } else if(line[i]=='H' && (   (line[k]>='0' && line[k]<='9')
                                 || (line[k]>='A' && line[k]<='F')))
      { if(sscanf(&line[k], "%X", &RAM[firstfreeRAMadr].u)!=1)
          {printf("assemble: bad format\n%s\n", line); err=1; continue;}
        firstfreeRAMadr++;
      } else if(line[i]=='A') {RAM[firstfreeRAMadr].u = 0;  goto address;}
      else if(line[i]=='W')
      { if(sscanf(&line[k], "%d", &uu)!=1)
          {printf("assemble: bad format\n%s\n", line); err=1; continue;}
        firstfreeRAMadr += uu;
      } else {printf("assemble: bad format\n%s\n", line); err=1; continue;}
    } else {printf("assemble: bad format\n%s\n", line); err=1; continue;}
  } /*end while*/

  /*check that all labels are defined*/
  for(thislabel=0;  thislabel<=lastlabel;  thislabel++)
    if(!labeltable[thislabel].def)
    { printf("assemble: unresolved address\n%s\n", labeltable[thislabel].id);
      err=1; continue;
    }
  PC = labeltable[0].adr; /*set PC to main*/
} /*end assemble*/

void execute (char trace) /*executes instructions in RAM starting at the
                            address in PC and ending normally with a branch
                            to RamSize (where the operating system should be)
                            or abnormally with an illegal op-code
                            or an address past the end of memory*/
/* arithmetic overflow fails to make E=1 */
{ char cc;  /*used for reading into*/
  int a, b;  /* used for printing memory */
  if(trace=='y')
  { printf("To execute one instruction, press x.\n");
    printf("To see RAM, press m.\n");
    printf("To see AC, press a.\n");
    printf("PC and IR are shown automatically.\n");
    printf("All numbers are shown in hexadecimal.\n");
  }
  while (1)
  { if(PC==RamSize) return;
    if(PC>RamSize)
      {printf("\nexecute: instruction address past end of memory\n"); return;}
    IRop = RAM[PC].u / 0x01000000;  IRadr = RAM[PC].u & 0x00FFFFFF;

    if(trace=='y')
    { Q: printf("\ninstruction at address %06X: %s %06X (x/m/a): ",
                PC, ops[IRop], IRadr);
         cc = getchar ( );
         if(cc=='m')
         { fpin = fopen(filename, "r");
           printf("\naddress  memory   original source\n");
           for(a=0; a<firstfreeRAMadr; a++)
           { for (b=1; b<numsourcelines[a];  b++)
             { fgets(line, MaxLineLen+2, fpin);
               printf("                  %s", line);
             }
             printf("%06X: %08X", a, RAM[a].u);
             if(numsourcelines[a]==0) putchar('\n');
             else { fgets(line, MaxLineLen+2, fpin);
                    printf ("  %s", line);
                  }
           }
           for (b=0; b<numsourcelines[firstfreeRAMadr]; b++)
           { fgets(line, MaxLineLen+2, fpin);
             printf("                  %s", line);
           }
           fclose(fpin);  goto Q;
         } else if(cc=='a')
         { printf("AC = %08X", AC.u);  goto Q;
         } else if(cc!='x')
         { printf("What? "); goto Q;
         } 
    } /* end of trace processing, except for INP and OUT instructions */

    if(IRadr>=RamSize && (IRop<=15 || IRop==18  || IRop==19))
      {printf("\nexecute: data address past end of memory\n"); return;}
    switch(IRop)
    { case 0:  /*LDA*/ AC = RAM[IRadr]; PC++;  break;
      case 1:  /*STA*/ RAM[IRadr] = AC;  PC++;  break;
      case 2:  /*ADD*/ E = 0;  AC.i += RAM[IRadr].i;  PC++;  break;
      case 3:  /*SUB*/ E = 0;  AC.i -= RAM[IRadr].i;  PC++;  break;
      case 4:  /*MUL*/ E = 0;  AC.i *= RAM[IRadr].i;  PC++;  break;
      case 5:  /*DIV*/ if (RAM[IRadr].i==0) E = 1;
                       else{E=0; AC.i /= RAM[IRadr].i;}
                       PC++;  break;
      case 6:  /*MOD*/ if (RAM[IRadr].i==0) E = 1;
                       else{E=0; AC.i %= RAM[IRadr].i;}
                       PC++;  break;
      case 7:  /*FLA*/ E = 0;  AC.f += RAM[IRadr].f;  PC++;  break;
      case 8:  /*FLS*/ E = 0;  AC.f -= RAM[IRadr].f;  PC++;  break;
      case 9:  /*FLM*/ E = 0;  AC.f *= RAM[IRadr].f;  PC++;  break;
      case 10: /*FLD*/ if (RAM[IRadr].f==0.0) E = 1;
                       else{E=0; AC.f /= RAM[IRadr].f;}
                       PC++;  break;
      case 11: /*CIF*/ AC.f = (float) RAM[IRadr].i;  PC++;  break;
      case 12: /*CFI*/ if (RAM[IRadr].i >= 0.0) AC.i = (int) (RAM[IRadr].f + 0.5);
                       else AC.i = (int) (RAM[IRadr].f - 0.5);
                       PC++;  break;
      case 13: /*AND*/ AC.u &= RAM[IRadr].u;  E = (AC.u!=0);  PC++;  break;
      case 14: /*IOR*/ AC.u |= RAM[IRadr].u;  E = (AC.u!=0);  PC++;  break;
      case 15: /*XOR*/ AC.u ^= RAM[IRadr].u;  E = (AC.u!=0);  PC++;  break;
      case 16: /*BUN*/ PC = IRadr;  break;
      case 17: /*BZE*/ if (E) PC++; else PC = IRadr;  break;
      case 18: /*BSA*/ RAM[IRadr].u = PC+1;  PC = IRadr + 1;  break;
      case 19: /*BIN*/ PC = RAM[IRadr].u & 0x00FFFFFF;  break;
      case 20: /*INP*/ if(trace=='y') printf("\nwaiting for input: ");
                       AC.u = getchar ( );  PC++;  break;/*waits; no branch*/
      case 21: /*OUT*/ if(trace=='y') printf("\noutput: ");
                       putchar (AC.u % 256);  PC++;  break; /* no branch */
      default: {printf("\nexecute: illegal op-code\n"); return;}
    } /*end switch*/
  } /*end while*/
} /*end execute*/

void finish (int i) {system("stty -cbreak echo"); exit(0);} /*UNIX*/

int main (int count, char *argument[])
{ char trace;
  if (count!=2) {printf("%s\n", "usage: ax file"); exit(0);}
  filename = argument[1];
  if ((fpin = fopen(filename, "r")) == NULL)
  {printf("%s\n", "can't open file"); exit(0);}
  assemble ();
  fclose(fpin);
  if(err) exit(0);
  QT: printf("Trace execution? (y/n): "); trace = getchar(); getchar();
      if (trace!='y' && trace!='n') { printf("What? "); goto QT; }
  signal(SIGINT, finish);        /*UNIX*/
  system("stty cbreak -echo");   /*UNIX*/
  execute (trace);
  system("stty -cbreak echo");   /*UNIX*/
  printf("\n");
  exit(0);
} /*end main*/
