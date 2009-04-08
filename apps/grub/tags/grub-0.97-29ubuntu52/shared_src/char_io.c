/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 1996   Erich Boleyn  <erich@uruk.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#define _CHAR_IO_C

#include "shared.h"

#ifndef NO_FANCY_STUFF
int
getkey(void)
{
  buf_drive = -1;
  return asm_getkey();
}
#endif  /* NO_FANCY_STUFF */


void
print_error(void)
{
  if (errnum > ERR_NONE && errnum < MAX_ERR_NUM)
#ifndef NO_FANCY_STUFF
    /* printf("\7\n %s\n", err_list[errnum]); */
    printf("\n %s\n", err_list[errnum]);
#else /* NO_FANCY_STUFF */
    printf("Error: %d\n", errnum);
#endif  /* NO_FANCY_STUFF */

  errnum = ERR_NONE;
}


char *
convert_to_ascii(char *buf, int c, ...)
{
  unsigned long num = *((&c) + 1), mult = 10;
  char *ptr = buf;

  if (c == 'x')
    mult = 16;

  if ((num & 0x80000000uL) && c == 'd')
    {
      num = (~num)+1;
      *(ptr++) = '-';
      buf++;
    }

  do
    {
      int dig = num % mult;
      *(ptr++) = ( (dig > 9) ? dig + 'a' - 10 : '0' + dig );
    }
  while (num /= mult);

  /* reorder to correct direction!! */
  {
    char *ptr1 = ptr-1;
    char *ptr2 = buf;
    while (ptr1 > ptr2)
      {
	int c = *ptr1;
	*ptr1 = *ptr2;
	*ptr2 = c;
	ptr1--;
	ptr2++;
      }
  }

  return ptr;
}


void
printf(char *format, ... )
{
  int *dataptr = (int *) &format;
  char c, *ptr, str[16];

  dataptr++;

  while (c = *(format++))
    {
      if (c != '%')
	putchar(c);
      else
	switch (c = *(format++))
	  {
	  case 'd': case 'u': case 'x':
	    *convert_to_ascii(str, c, *((unsigned long *) dataptr++)) = 0;

	    ptr = str;

	    while (*ptr)
	      putchar(*(ptr++));
	    break;

	  case 'c': putchar((*(dataptr++))&0xff); break;

	  case 's':
	    ptr = (char *)(*(dataptr++));

	    while (c = *(ptr++))
	      putchar(c);
	    break;
	  }
    }
}


#ifndef NO_FANCY_STUFF

void
init_page(void)
{
  cls();

  printf("\n    GRUB  version %s  (%dK lower / %dK upper memory)\n\n",
	 version_string, mbi.mem_lower, mbi.mem_upper);
}


/* don't use this with a maxlen greater than 1600 or so!  the problem
   is that it depends on the whole thing fitting on the screen at once,
   and the whole screen is about 2000 characters, minus the prompt...
   so want to leave space for an error line, etc.
   maxlen should be at least 1, and we shouldn't have a NULL prompt or
   cmdline...  the cmdline must be a valid string at the start */

int
get_cmdline(char *prompt, char *commands, char *cmdline, int maxlen)
{
  int ystart, yend, xend, lpos, c;
  int plen = 0;
  int llen = 0;

  /* nested function definition for code simplicity XXX GCC only, I think */
  void cl_print(char *str)
    {
      while (*str != 0)
	{
	  putchar(*(str++));
	  if (++xend > 78)
	    {
	      xend = 0;
	      putchar(' ');
	      if (yend == (getxy() & 0xFF))
		ystart--;
	      else
		yend++;
	    }
	}
    }
  /* nested function definition for code simplicity XXX GCC only, I think */
  void cl_setcpos(void)
    {
      yend = ((lpos+plen) / 79) + ystart;
      xend = ((lpos+plen) % 79);
      gotoxy(xend, yend);
    }

  /* nested function definition for initial command-line printing */
  void cl_init()
    {
      /* distinguish us from other lines and error messages! */
      putchar('\n');

      /* print full line and set position here */
      ystart = (getxy() & 0xFF);
      yend = ystart;
      xend = 0;
      cl_print(prompt);
      cl_print(cmdline);
      cl_setcpos();
    }

  /* nested function definition for erasing to the end of the line */
  void cl_kill_to_end()
    {
      int i;
      cmdline[lpos] = 0;
      for (i = lpos; i <= llen; i++)
	{
	  if (i && ((i + plen) % 79) == 0)
	    putchar(' ');
	  putchar(' ');
	}
      llen = lpos;
      cl_setcpos();
    }

  while (prompt[plen])
    plen++;
  while (cmdline[llen])
    llen++;
  /* XXX limiting maxlen to 1600 */
  if (maxlen > 1600)
    {
      maxlen = 1600;
      if (llen > 1599)
	{
	  llen = 1599;
	  cmdline[1600] = 0;
	}
    }
  lpos = llen;

  cl_init();

  while (ASCII_CHAR(c = getkey()) != '\n' &&  ASCII_CHAR(c) != '\r')
    {
      switch (c)
	{
	case KEY_LEFT:
	  c = 2;
	  break;
	case KEY_RIGHT:
	  c = 6;
	  break;
	case KEY_HOME:
	  c = 1;
	  break;
	case KEY_END:
	  c = 5;
	  break;
	case KEY_DELETE:
	  c = 8;
	default:
	}

      c = ASCII_CHAR(c);

      switch (c)
	{
	case 27:  /* ESC immediately return 1*/
	  return 1;
	case 9:   /* TAB lists completions */
	  {
	    int i, j = 0, llen_old = llen;

	    while (cmdline[j] && cmdline[j] != '=')
	      j++;

	    /* since the command line cannot have a '\n', we're OK to use c */
	    c = cmdline[lpos];

	    cl_kill_to_end();

	    /* goto part after line here */
	    yend = ((llen+plen) / 79) + ystart;
	    gotoxy(0, yend); putchar('\n');

	    if (lpos > j)
	      {
		for (i = lpos; i > 0 && cmdline[i-1] != ' '; i--);
		if (i <= j)
		  i = j+1;
		/* print possible completions */
		print_completions(cmdline+i);
	      }
	    else if (commands)
	      printf(commands);
	    else
	      break;

	    /* restore command-line */
	    cmdline[lpos] = c;
	    llen = llen_old;
	    cl_init();
	  }
	  break;
	case 1:   /* C-a go to beginning of line */
	  lpos = 0;
	  cl_setcpos();
	  break;
	case 5:   /* C-e go to end of line */
	  lpos = llen;
	  cl_setcpos();
	  break;
	case 6:   /* C-f forward one character */
	  if (lpos < llen)
	    {
	      lpos++;
	      cl_setcpos();
	    }
	  break;
	case 2:   /* C-b backward one character */
	  if (lpos > 0)
	    {
	      lpos--;
	      cl_setcpos();
	    }
	  break;
	case 4:   /* C-d delete character under cursor */
	  if (lpos == llen)
	    break;
	  lpos++;
	  /* fallthrough is on purpose! */
	case 8:   /* C-h backspace */
	  if (lpos > 0)
	    {
	      int i;
	      for (i = lpos-1; i < llen; i++)
		cmdline[i] = cmdline[i+1];
	      i = lpos;
	      lpos = llen - 1;
	      cl_setcpos();
	      putchar(' ');
	      lpos = i - 1;  /* restore lpos and decrement */
	      llen--;
	      cl_setcpos();
	      if (lpos != llen)
		{
		  cl_print(cmdline+lpos);
		  cl_setcpos();
		}
	    }
	  break;
	case 21:   /* C-u kill to beginning of line */
	  if (lpos == 0)
	    break;
	  {
	    int i;
	    for (i = 0; i < (llen-lpos); i++)
	      cmdline[i] = cmdline[lpos+i];
	  }
	  lpos = llen-lpos;
	  cl_setcpos();
	  /* fallthrough on purpose! */
	case 11:   /* C-k kill to end of line */
	  if (lpos < llen)
	    {
	      cl_kill_to_end();
	      if (c == 21)
		{
		  lpos = 0;
		  cl_setcpos();
		  cl_print(cmdline);
		  cl_setcpos();
		}
	    }
	  break;
	default:   /* insert printable character into line */
	  if (llen < (maxlen-1) && c >= ' ' && c <= '~')
	    {
	      if (lpos == llen)
		{
		  cmdline[lpos] = c;
		  cmdline[lpos+1] = 0;
		  cl_print(cmdline+lpos);
		  lpos++;
		}
	      else
		{
		  int i;
		  for (i = llen; i >= lpos; i--)
		    cmdline[i+1] = cmdline[i];
		  cmdline[lpos] = c;
		  cl_setcpos();
		  cl_print(cmdline+lpos);
		  lpos++;
		  cl_setcpos();
		}
	      llen++;
	    }
	}
    }

  /* goto part after line here */
  yend = ((llen+plen) / 79) + ystart;
  gotoxy(0, yend); putchar('\n');

  /* remove leading spaces */
  /* use c and lpos as indexes now */
  for (lpos = 0; cmdline[lpos] == ' '; lpos++);

  if (lpos != 0)
    {
      c = 0;
      do
	{
	  cmdline[c] = cmdline[lpos];
	  c++;
	  lpos++;
	}
      while (cmdline[lpos]);
    }

  cmdline[c] = 0;

  return 0;
}

#endif /* NO_FANCY_STUFF */


int
get_based_digit(int c, int base)
{
  int digit = -1;

  /* make sure letter in the the range we can check! */
  c = tolower(c);

  /*
   *  Is it in the range between zero and nine?
   */
  if (base > 0 && c >= '0' && c <= '9' && c < (base + '0'))
    {
      digit = c - '0';
    }

  /*
   *  Is it in the range used by a letter?
   */
  if (base > 10 && c >= 'a' && c <= 'z' && c < ((base - 10) + 'a'))
    {
      digit = c - 'a' + 10;
    }

  return digit;
}


int
safe_parse_maxint(char **str_ptr, int *myint_ptr)
{
  register char *ptr = *str_ptr;
  register int myint = 0, digit;
  int mult = 10, found = 0;

  /*
   *  Is this a hex number?
   */
  if (*ptr == '0' && tolower(*(ptr+1)) == 'x')
    {
      ptr += 2;
      mult = 16;
    }

  while ((digit = get_based_digit(*ptr, mult)) != -1)
    {
      found = 1;
      if (myint > ((MAXINT - digit)/mult))
	{
	  errnum = ERR_NUMBER_PARSING;
	  return 0;
	}
      myint = (myint * mult) + digit;
      ptr++;
    }

  if (!found)
    {
      errnum = ERR_NUMBER_PARSING;
      return 0;
    }

  *str_ptr = ptr;
  *myint_ptr = myint;

  return 1;
}


int
tolower(int c)
{
  if (c >= 'A' && c <= 'Z')
    return (c + ('a' - 'A'));

  return c;
}


int
isspace(int c)
{
  if (c == ' ' || c == '\t' || c == '\n')
    return 1;

  return 0;
}


int
strncat(char *s1, char *s2, int n)
{
  int i = -1;

  while (++i < n && s1[i] != 0);

  while (i < n && (s1[i++] = *(s2++)) != 0);

  s1[n-1] = 0;

  if (i >= n)
    return 0;

  s1[i] = 0;

  return 1;
}


int
strcmp(char *s1, char *s2)
{
  while (*s1 == *s2)
    {
      if (!*(s1++))
	return 0;
      s2++;
    }

  if (*s1 == 0)
    return -1;

  return 1;
}


char *
strstr(char *s1, char *s2)
{
  char *ptr, *tmp;

  while (*s1)
    {
      ptr = s1;
      tmp = s2;

      while (*s1 && *s1++ == *tmp++);

      if (tmp > s2 && !*(tmp-1))
	return ptr;
    }

  return 0;
}


int
memcheck(int start, int len)
{
  if ( (start < 0x1000) || (start < 0x100000
			   && (mbi.mem_lower * 1024) < (start+len))
       || (start >= 0x100000
	   && (mbi.mem_upper * 1024) < ((start-0x100000)+len)) )
    errnum = ERR_WONT_FIT;

  return (!errnum);
}


int
bcopy(char *from, char *to, int len)
{
  if (memcheck((int)to, len))
    {
      if ((to >= from+len) || (to <= from))
	{
	  while (len > 3)
	    {
	      len -= 4;
	      *(((unsigned long *)to)++) = *(((unsigned long *)from)++);
	    }
	  while (len-- > 0)
	    *(to++) = *(from++);
	}
      else
	{
	  while (len-- > 0)
	    to[len] = from[len];
	}
    }

  return (!errnum);
}


int
bzero(char *start, int len)
{
  if (memcheck((int)start, len))
    {
      while (len-- > 0)
	*(start++) = 0;
    }

  return (!errnum);
}

