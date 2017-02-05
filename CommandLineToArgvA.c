#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
char strArgc[256]={0};

/*************************************************************************
 * CommandLineToArgvA            [SHELL32.@]
 *
 * We must interpret the quotes in the command line to rebuild the argv
 * array correctly:
 * - arguments are separated by spaces or tabs
 * - quotes serve as optional argument delimiters
 *   '"a b"'   -> 'a b'
 * - escaped quotes must be converted back to '"'
 *   '\"'      -> '"'
 * - consecutive backslashes preceding a quote see their number halved with
 *   the remainder escaping the quote:
 *   2n   backslashes + quote -> n backslashes + quote as an argument delimiter
 *   2n+1 backslashes + quote -> n backslashes + literal quote
 * - backslashes that are not followed by a quote are copied literally:
 *   'a\b'     -> 'a\b'
 *   'a\\b'    -> 'a\\b'
 * - in quoted strings, consecutive quotes see their number divided by three
 *   with the remainder modulo 3 deciding whether to close the string or not.
 *   Note that the opening quote must be counted in the consecutive quotes,
 *   that's the (1+) below:
 *   (1+) 3n   quotes -> n quotes
 *   (1+) 3n+1 quotes -> n quotes plus closes the quoted string
 *   (1+) 3n+2 quotes -> n+1 quotes plus closes the quoted string
 * - in unquoted strings, the first quote opens the quoted string and the
 *   remaining consecutive quotes follow the above rule.
 */
LPSTR* WINAPI CommandLineToArgvA_wine(LPSTR lpCmdline, int* numargs)
{
  DWORD argc;
  LPSTR  *argv;
  LPSTR s;
  LPSTR d;
  LPSTR cmdline;
  int qcount,bcount;

  if(!numargs || *lpCmdline==0)
    {
      SetLastError(ERROR_INVALID_PARAMETER);
      return NULL;
    }

  /* --- First count the arguments */
  argc=1;
  s=lpCmdline;
  /* The first argument, the executable path, follows special rules */
  if (*s=='"')
    {
      /* The executable path ends at the next quote, no matter what */
      s++;
      while (*s)
        if (*s++=='"')
          break;
    }
  else
    {
      /* The executable path ends at the next space, no matter what */
      while (*s && *s!=' ' && *s!='\t')
        s++;
    }
  /* skip to the first argument, if any */
  while (*s==' ' || *s=='\t')
    s++;
  if (*s)
    argc++;

  /* Analyze the remaining arguments */
  qcount=bcount=0;
  while (*s)
    {
      if ((*s==' ' || *s=='\t') && qcount==0)
        {
          /* skip to the next argument and count it if any */
          while (*s==' ' || *s=='\t')
            s++;
          if (*s)
            argc++;
          bcount=0;
        }
      else if (*s=='\\')
        {
          /* '\', count them */
          bcount++;
          s++;
        }
      else if (*s=='"')
        {
          /* '"' */
          if ((bcount & 1)==0)
            qcount++; /* unescaped '"' */
          s++;
          bcount=0;
          /* consecutive quotes, see comment in copying code below */
          while (*s=='"')
            {
              qcount++;
              s++;
            }
          qcount=qcount % 3;
          if (qcount==2)
            qcount=0;
        }
      else
        {
          /* a regular character */
          bcount=0;
          s++;
        }
    }

  /* Allocate in a single lump, the string array, and the strings that go
   * with it. This way the caller can make a single LocalFree() call to free
   * both, as per MSDN.
   */
  argv=LocalAlloc(LMEM_FIXED, (argc+1)*sizeof(LPSTR)+(strlen(lpCmdline)+1)*sizeof(char));
  if (!argv)
    return NULL;
  cmdline=(LPSTR)(argv+argc+1);
  strcpy(cmdline, lpCmdline);

  /* --- Then split and copy the arguments */
  argv[0]=d=cmdline;
  argc=1;
  /* The first argument, the executable path, follows special rules */
  if (*d=='"')
    {
      /* The executable path ends at the next quote, no matter what */
      s=d+1;
      while (*s)
        {
          if (*s=='"')
            {
              s++;
              break;
            }
          *d++=*s++;
        }
    }
  else
    {
      /* The executable path ends at the next space, no matter what */
      while (*d && *d!=' ' && *d!='\t')
        d++;
      s=d;
      if (*s)
        s++;
    }
  /* close the executable path */
  *d++=0;
  /* skip to the first argument and initialize it if any */
  while (*s==' ' || *s=='\t')
    s++;
  if (!*s)
    {
      /* There are no parameters so we are all done */
      argv[argc]=NULL;
      *numargs=argc;
      return argv;
    }

  /* Split and copy the remaining arguments */
  argv[argc++]=d;
  qcount=bcount=0;
  while (*s)
    {
      if ((*s==' ' || *s=='\t') && qcount==0)
        {
          /* close the argument */
          *d++=0;
          bcount=0;

          /* skip to the next one and initialize it if any */
          do {
            s++;
          } while (*s==' ' || *s=='\t');
          if (*s)
            argv[argc++]=d;
        }
      else if (*s=='\\')
        {
          *d++=*s++;
          bcount++;
        }
      else if (*s=='"')
        {
          if ((bcount & 1)==0)
            {
              /* Preceded by an even number of '\', this is half that
               * number of '\', plus a quote which we erase.
               */
              d-=bcount/2;
              qcount++;
            }
          else
            {
              /* Preceded by an odd number of '\', this is half that
               * number of '\' followed by a '"'
               */
              d=d-bcount/2-1;
              *d++='"';
            }
          s++;
          bcount=0;
          /* Now count the number of consecutive quotes. Note that qcount
           * already takes into account the opening quote if any, as well as
           * the quote that lead us here.
           */
          while (*s=='"')
            {
              if (++qcount==3)
                {
                  *d++='"';
                  qcount=0;
                }
              s++;
            }
          if (qcount==2)
            qcount=0;
        }
      else
        {
          /* a regular character */
          *d++=*s++;
          bcount=0;
        }
    }
  *d='\0';
  argv[argc]=NULL;
  *numargs=argc;

  return argv;
}

/*************************************************************************
 * CommandLineToArgvA
 *
 * https://github.com/ola-ct/actilog/blob/master/actiwin/CommandLineToArgvA.cpp
 */
PCHAR *CommandLineToArgvA_ola(PCHAR CmdLine, int *_argc) {
  PCHAR *argv;
  PCHAR _argv;
  ULONG len;
  ULONG argc;
  CHAR a;
  ULONG i, j;

  BOOLEAN in_QM;
  BOOLEAN in_TEXT;
  BOOLEAN in_SPACE;

  len = strlen(CmdLine);
  i = ((len + 2) / 2) * sizeof(PVOID) + sizeof(PVOID);

  argv = (PCHAR *)LocalAlloc(LMEM_FIXED, i + (len + 2) * sizeof(CHAR));

  _argv = (PCHAR)(((PUCHAR)argv) + i);

  argc = 0;
  argv[argc] = _argv;
  in_QM = FALSE;
  in_TEXT = FALSE;
  in_SPACE = TRUE;
  i = 0;
  j = 0;

  while (a = CmdLine[i]) {
    if (in_QM) {
      if (a == '\"') {
        in_QM = FALSE;
      } else {
        _argv[j] = a;
        j++;
      }
    } else {
      switch (a) {
      case '\"':
        in_QM = TRUE;
        in_TEXT = TRUE;
        if (in_SPACE) {
          argv[argc] = _argv + j;
          argc++;
        }
        in_SPACE = FALSE;
        break;
      case ' ':
      case '\t':
      case '\n':
      case '\r':
        if (in_TEXT) {
          _argv[j] = '\0';
          j++;
        }
        in_TEXT = FALSE;
        in_SPACE = TRUE;
        break;
      default:
        in_TEXT = TRUE;
        if (in_SPACE) {
          argv[argc] = _argv + j;
          argc++;
        }
        _argv[j] = a;
        j++;
        in_SPACE = FALSE;
        break;
      }
    }
    i++;
  }
  _argv[j] = '\0';
  argv[argc] = NULL;

  (*_argc) = argc;
  return argv;
}

/*************************************************************************
 * CommandLineToArgvA TINYCLIB version
 *
 * http://www.wheaty.net/
 */
#define _MAX_CMD_LINE_ARGS  128
// argv stored in below buffer
char * _ppszArgv[_MAX_CMD_LINE_ARGS+1];
int CommandLineToArgvA_wheaty(LPSTR pszSysCmdLine) {
  int argc;
  int cbCmdLine;
  PSTR pszCmdLine;

  // Set to no argv elements, in case we have to bail out
  _ppszArgv[0] = 0;

  // First get a pointer to the system's version of the command line, and
  // figure out how long it is.
  cbCmdLine = lstrlen( pszSysCmdLine );

  // Allocate memory to store a copy of the command line.  We'll modify
  // this copy, rather than the original command line.  Yes, this memory
  // currently doesn't explicitly get freed, but it goes away when the
  // process terminates.
  pszCmdLine = (PSTR)HeapAlloc( GetProcessHeap(), 0, cbCmdLine+1 );
  if ( !pszCmdLine )
    return 0;

  // Copy the system version of the command line into our copy
  lstrcpy( pszCmdLine, pszSysCmdLine );

  if ( '"' == *pszCmdLine )   // If command line starts with a quote ("),
    {                           // it's a quoted filename.  Skip to next quote.
      pszCmdLine++;

      _ppszArgv[0] = pszCmdLine;  // argv[0] == executable name

      while ( *pszCmdLine && (*pszCmdLine != '"') )
        pszCmdLine++;

      if ( *pszCmdLine )      // Did we see a non-NULL ending?
        *pszCmdLine++ = 0;  // Null terminate and advance to next char
      else
        return 0;           // Oops!  We didn't see the end quote
    }
  else    // A regular (non-quoted) filename
    {
      _ppszArgv[0] = pszCmdLine;  // argv[0] == executable name

      while ( *pszCmdLine && (' ' != *pszCmdLine) && ('\t' != *pszCmdLine) )
        pszCmdLine++;

      if ( *pszCmdLine )
        *pszCmdLine++ = 0;  // Null terminate and advance to next char
    }

  // Done processing argv[0] (i.e., the executable name).  Now do th
  // actual arguments

  argc = 1;

  while ( 1 )
    {
      // Skip over any whitespace
      while ( *pszCmdLine && (' ' == *pszCmdLine) || ('\t' == *pszCmdLine) )
        pszCmdLine++;

      if ( 0 == *pszCmdLine ) // End of command line???
        return argc;

      if ( '"' == *pszCmdLine )   // Argument starting with a quote???
        {
          pszCmdLine++;   // Advance past quote character

          _ppszArgv[ argc++ ] = pszCmdLine;
          _ppszArgv[ argc ] = 0;

          // Scan to end quote, or NULL terminator
          while ( *pszCmdLine && (*pszCmdLine != '"') )
            pszCmdLine++;

          if ( 0 == *pszCmdLine )
            return argc;

          if ( *pszCmdLine )
            *pszCmdLine++ = 0;  // Null terminate and advance to next char
        }
      else                        // Non-quoted argument
        {
          _ppszArgv[ argc++ ] = pszCmdLine;
          _ppszArgv[ argc ] = 0;

          // Skip till whitespace or NULL terminator
          while ( *pszCmdLine && (' '!=*pszCmdLine) && ('\t'!=*pszCmdLine) )
            pszCmdLine++;

          if ( 0 == *pszCmdLine )
            return argc;

          if ( *pszCmdLine )
            *pszCmdLine++ = 0;  // Null terminate and advance to next char
        }

      if ( argc >= (_MAX_CMD_LINE_ARGS) )
        return argc;
    }
  return argc;
}

// http://stackoverflow.com/a/13281447/4612829
// NO memory alloc, just modified source command line string
enum { kMaxArgs = 64 };
char *simpleArgv[kMaxArgs];
int CommandLineToArgvA_simple(LPSTR commandLine) {
  int argc=0;
  char *p2 = strtok(commandLine, " ");
  while (p2 && argc < kMaxArgs - 1) {
    simpleArgv[argc++] = p2;
    p2 = strtok(0, " ");
  }
  simpleArgv[argc] = 0;
  return argc;
}

//***************************************************
// TEST CODE
//***************************************************

void test_simple(void) {
  int argc = CommandLineToArgvA_simple(GetCommandLineA());
  for (int i = 0; i<argc; i++) {
    sprintf(strArgc, "simple: arg %i / %i", i+1, argc);
    MessageBox(NULL, simpleArgv[i], strArgc, MB_OK);
  }
  // since modified source command line, no need to free memory
}

void test_ola(void) {
  int argc;
  LPSTR * argv = CommandLineToArgvA_ola(GetCommandLineA(), &argc);
  for (int i = 0; i<argc; i++) {
    sprintf(strArgc, "ola: arg %i / %i", i+1, argc);
    MessageBox(NULL, argv[i], strArgc, MB_OK);
  }
  LocalFree(argv);
}

void test_wine(void) {
  int argc;
  LPSTR * argv = CommandLineToArgvA_wine(GetCommandLineA(), &argc);
  for (int i = 0; i<argc; i++) {
    sprintf(strArgc, "wine: arg %i / %i", i+1, argc);
    MessageBox(NULL, argv[i], strArgc, MB_OK);
  }
  LocalFree(argv);
}

void test_wheaty(void) {
  int argc = CommandLineToArgvA_wheaty(GetCommandLineA());
  for (int i = 0; i<argc; i++) {
    sprintf(strArgc, "wheaty: arg %i / %i", i+1, argc);
    MessageBox(NULL, _ppszArgv[i], strArgc, MB_OK);
  }
  // since use global _ppszArgv, no need free memory
}

/* int main(int argc, char *argv[]) { */
int WINAPI WinMain(HINSTANCE currentInstance, HINSTANCE previousInstance, LPSTR commandLine, int showMode) {
  MessageBox(NULL, GetCommandLineA(), "Command line is", MB_OK);

  test_wheaty();
  test_ola();
  test_wine();

  // since it's modified, have to be tested lastly
  test_simple();

  return 0;
}
