/*
 *  info.c
 *
 *  $Id$
 *
 *  Information functions
 *
 *  The iODBC driver manager.
 *  
 *  Copyright (C) 1995 by Ke Jin <kejin@empress.com> 
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>

#include <sql.h>
#include <sqlext.h>

#include <dlproc.h>

#include <herr.h>
#include <henv.h>
#include <hdbc.h>
#include <hstmt.h>

#include <itrace.h>

#include <stdio.h>
#include <ctype.h>

extern char *_iodbcdm_getinifile (char *buf, int size);

#define SECT1			"ODBC Data Sources"
#define SECT2			"Default"
#define MAX_ENTRIES		1024

static int
stricmp (const char *s1, const char *s2)
{
  int cmp;

  while (*s1)
    {
      if ((cmp = toupper (*s1) - toupper (*s2)) != 0)
	return cmp;
      s1++;
      s2++;
    }
  return (*s2) ? -1 : 0;
}

static int
SectSorter (const void *p1, const void *p2)
{
  char **s1 = (char **) p1;
  char **s2 = (char **) p2;

  return stricmp (*s1, *s2);
}


SQLRETURN SQL_API
SQLDataSources (
    SQLHENV henv,
    SQLUSMALLINT fDir,
    SQLCHAR FAR * szDSN,
    SQLSMALLINT cbDSNMax,
    SQLSMALLINT FAR * pcbDSN,
    SQLCHAR FAR * szDesc,
    SQLSMALLINT cbDescMax,
    SQLSMALLINT FAR * pcbDesc)
{
  GENV (genv, henv);
  char *path;
  char buf[1024];
  FILE *fp;
  int i;
  static int cur_entry = -1;
  static int num_entries = 0;
  static char **sect = NULL;

  if (!IS_VALID_HENV (genv))
    {
      return SQL_INVALID_HANDLE;
    }
  CLEAR_ERRORS (genv);

  /* check argument */
  if (cbDSNMax < 0 || cbDescMax < 0)
    {
      PUSHSQLERR (genv->herr, en_S1090);

      return SQL_ERROR;
    }

  if (fDir != SQL_FETCH_FIRST && fDir != SQL_FETCH_NEXT)
    {
      PUSHSQLERR (genv->herr, en_S1103);

      return SQL_ERROR;
    }

  if (cur_entry < 0 || fDir == SQL_FETCH_FIRST)
    {
      cur_entry = 0;
      num_entries = 0;


      /* 
       *  Open the odbc.ini file
       */
      path = (char *) _iodbcdm_getinifile (buf, sizeof (buf));
      if ((fp = fopen (path, "r")) == NULL)
	{
	  return SQL_NO_DATA_FOUND;
	}
      /*
       *  Free old section list
       */
      if (sect)
	{
	  for (i = 0; i < MAX_ENTRIES; i++)
	    if (sect[i])
	      free (sect[i]);
	  free (sect);
	}
      if ((sect = (char **) calloc (MAX_ENTRIES, sizeof (char *))) == NULL)
	{
	  PUSHSQLERR (genv->herr, en_S1011);
	  fclose (fp);

	  return SQL_ERROR;
	}

      /*
       *  Build a dynamic list of sections
       */
      while (1)
	{
	  char *str, *p;

	  str = fgets (buf, sizeof (buf), fp);

	  if (str == NULL)
	    break;

	  if (*str == '[')
	    {
	      str++;
	      for (p = str; *p; p++)
		if (*p == ']')
		  *p = '\0';

	      if (!strcmp (str, SECT1))
		continue;
	      if (!strcmp (str, SECT2))
		continue;

	      /*
	       *  Add this section to the comma separated list
	       */
	      if (num_entries >= MAX_ENTRIES)
		break;		/* Skip the rest */

	      sect[num_entries++] = (char *) strdup (str);
	    }
	}
      fclose (fp);

      /*
       *  Sort all entries so we can present a nice list
       */
      if (num_entries > 1)
	qsort (sect, num_entries, sizeof (char *), SectSorter);
    }

  /*
   *  Try to get to the next item
   */
  if (cur_entry >= num_entries)
    {
      cur_entry = 0;		/* Next time, start all over again */
      return SQL_NO_DATA_FOUND;
    }

  /*
   *  Copy DSN information 
   */
  STRNCPY (szDSN, sect[cur_entry], cbDSNMax);

  /*
   *  And find the description that goes with this entry
   */
  _iodbcdm_getkeyvalbydsn (sect[cur_entry], strlen (sect[cur_entry]),
      "Description", szDesc, cbDescMax);

  /*
   *  Next record
   */
  cur_entry++;

  return SQL_SUCCESS;
}


SQLRETURN SQL_API
SQLDrivers (
    SQLHENV henv,
    SQLUSMALLINT fDir,
    SQLCHAR FAR * szDrvDesc,
    SQLSMALLINT cbDrvDescMax,
    SQLSMALLINT FAR * pcbDrvDesc,
    SQLCHAR FAR * szDrvAttr,
    SQLSMALLINT cbDrvAttrMax,
    SQLSMALLINT FAR * pcbDrvAttr)
{
  GENV (genv, henv);

  if (!IS_VALID_HENV (genv))
    {
      return SQL_INVALID_HANDLE;
    }
  CLEAR_ERRORS (genv);

  if (cbDrvDescMax < 0 || cbDrvAttrMax < 0 || cbDrvAttrMax == 1)
    {
      PUSHSQLERR (genv->herr, en_S1090);

      return SQL_ERROR;
    }

  if (fDir != SQL_FETCH_FIRST && fDir != SQL_FETCH_NEXT)
    {
      PUSHSQLERR (genv->herr, en_S1103);

      return SQL_ERROR;
    }
  if (!szDrvDesc || !szDrvAttr || !cbDrvDescMax || !cbDrvAttrMax)
    {
      PUSHSQLERR (genv->herr, en_01004);
      return SQL_SUCCESS_WITH_INFO;
    }

/*********************/
  return SQL_NO_DATA_FOUND;
}


SQLRETURN SQL_API
SQLGetInfo (
    SQLHDBC hdbc,
    SQLUSMALLINT fInfoType,
    SQLPOINTER rgbInfoValue,
    SQLSMALLINT cbInfoValueMax,
    SQLSMALLINT FAR * pcbInfoValue)
{
  CONN (pdbc, hdbc);
  ENV_t FAR *penv;
  STMT_t FAR *pstmt = NULL;
  STMT_t FAR *tpstmt;
  HPROC hproc;
  SQLRETURN retcode = SQL_SUCCESS;

  DWORD dword;
  int size = 0, len = 0;
  char buf[16] =
  {'\0'};

  if (!IS_VALID_HDBC (pdbc) || pdbc->henv == SQL_NULL_HENV)
    {
      return SQL_INVALID_HANDLE;
    }
  CLEAR_ERRORS (pdbc);

  if (cbInfoValueMax < 0)
    {
      PUSHSQLERR (pdbc->herr, en_S1090);

      return SQL_ERROR;
    }

#if (ODBCVER < 0x0300)
  if (				/* fInfoType < SQL_INFO_FIRST || */
      (fInfoType > SQL_INFO_LAST
	  && fInfoType < SQL_INFO_DRIVER_START))
    {
      PUSHSQLERR (pdbc->herr, en_S1096);

      return SQL_ERROR;
    }
#endif
  if (fInfoType == SQL_ODBC_VER)
    {
      sprintf (buf, "%02d.%02d",
	  (ODBCVER) >> 8, 0x00FF & (ODBCVER));


      if (rgbInfoValue != NULL
	  && cbInfoValueMax > 0)
	{
	  len = STRLEN (buf);

	  if (len > cbInfoValueMax - 1)
	    {
	      len = cbInfoValueMax - 1;
	      PUSHSQLERR (pdbc->herr, en_01004);

	      retcode = SQL_SUCCESS_WITH_INFO;
	    }

	  STRNCPY (rgbInfoValue, buf, len);
	  ((char FAR *) rgbInfoValue)[len] = '\0';
	}

      if (pcbInfoValue != NULL)
	{
	  *pcbInfoValue = (SWORD) len;
	}

      return retcode;
    }

  if (pdbc->state == en_dbc_allocated || pdbc->state == en_dbc_needdata)
    {
      PUSHSQLERR (pdbc->herr, en_08003);

      return SQL_ERROR;
    }

  switch (fInfoType)
    {
    case SQL_DRIVER_HDBC:
      dword = (DWORD) (pdbc->dhdbc);
      size = sizeof (dword);
      break;

    case SQL_DRIVER_HENV:
      penv = (ENV_t FAR *) (pdbc->henv);
      dword = (DWORD) (penv->dhenv);
      size = sizeof (dword);
      break;

    case SQL_DRIVER_HLIB:
      penv = (ENV_t FAR *) (pdbc->henv);
      dword = (DWORD) (penv->hdll);
      size = sizeof (dword);
      break;

    case SQL_DRIVER_HSTMT:
      if (rgbInfoValue != NULL)
	{
	  pstmt = *((STMT_t FAR **) rgbInfoValue);
	}

      for (tpstmt = (STMT_t FAR *) (pdbc->hstmt);
	  tpstmt != NULL;
	  tpstmt = tpstmt->next)
	{
	  if (tpstmt == pstmt)
	    {
	      break;
	    }
	}

      if (tpstmt == NULL)
	{
	  PUSHSQLERR (pdbc->herr, en_S1009);

	  return SQL_ERROR;
	}

      dword = (DWORD) (pstmt->dhstmt);
      size = sizeof (dword);
      break;

    default:
      break;
    }

  if (size)
    {
      if (rgbInfoValue != NULL)
	{
	  *((DWORD *) rgbInfoValue) = dword;
	}

      if (pcbInfoValue != NULL)
	{
	  *(pcbInfoValue) = (SWORD) size;
	}

      return SQL_SUCCESS;
    }

  hproc = _iodbcdm_getproc (pdbc, en_GetInfo);

  if (hproc == SQL_NULL_HPROC)
    {
      PUSHSQLERR (pdbc->herr, en_IM001);

      return SQL_ERROR;
    }

  CALL_DRIVER (hdbc, pdbc, retcode, hproc, en_GetInfo,
      (pdbc->dhdbc, fInfoType, rgbInfoValue, cbInfoValueMax, pcbInfoValue));

  if (retcode == SQL_ERROR
      && fInfoType == SQL_DRIVER_ODBC_VER)
    {
      STRCPY (buf, "01.00");

      if (rgbInfoValue != NULL
	  && cbInfoValueMax > 0)
	{
	  len = STRLEN (buf);

	  if (len < cbInfoValueMax - 1)
	    {
	      len = cbInfoValueMax - 1;
	      PUSHSQLERR (pdbc->herr, en_01004);
	    }

	  STRNCPY (rgbInfoValue, buf, len);
	  ((char FAR *) rgbInfoValue)[len] = '\0';
	}

      if (pcbInfoValue != NULL)
	{
	  *pcbInfoValue = (SWORD) len;
	}

      /* what should we return in this case ???? */
    }

  return retcode;
}

#if (ODBCVER >= 0x0300)
static const int FunctionNumbers[] =
{
    SQL_API_SQLALLOCHANDLE,
    SQL_API_SQLBINDPARAM,
    SQL_API_SQLCLOSECURSOR,
    SQL_API_SQLCOPYDESC,
    SQL_API_SQLENDTRAN,
    SQL_API_SQLFREEHANDLE,
    SQL_API_SQLGETCONNECTATTR,
    SQL_API_SQLGETDESCFIELD,
    SQL_API_SQLGETDESCREC,
    SQL_API_SQLGETDIAGFIELD,
    SQL_API_SQLGETDIAGREC,
    SQL_API_SQLGETENVATTR,
    SQL_API_SQLGETSTMTATTR,
    SQL_API_SQLSETCONNECTATTR,
    SQL_API_SQLSETDESCFIELD,
    SQL_API_SQLSETDESCREC,
    SQL_API_SQLSETENVATTR,
    SQL_API_SQLSETSTMTATTR,
    SQL_API_SQLFETCHSCROLL,
    SQL_API_SQLBULKOPERATIONS,
    SQL_API_SQLALLOCENV,
    SQL_API_SQLALLOCCONNECT,
    SQL_API_SQLCONNECT,
    SQL_API_SQLDRIVERCONNECT,
    SQL_API_SQLBROWSECONNECT,
    SQL_API_SQLDATASOURCES,
    SQL_API_SQLDRIVERS,
    SQL_API_SQLGETINFO,
    SQL_API_SQLGETFUNCTIONS,
    SQL_API_SQLGETTYPEINFO,
    SQL_API_SQLSETCONNECTOPTION,
    SQL_API_SQLGETCONNECTOPTION,
    SQL_API_SQLSETSTMTOPTION,
    SQL_API_SQLGETSTMTOPTION,
    SQL_API_SQLALLOCSTMT,
    SQL_API_SQLPREPARE,
    SQL_API_SQLBINDPARAMETER,
    SQL_API_SQLPARAMOPTIONS,
    SQL_API_SQLGETCURSORNAME,
    SQL_API_SQLSETCURSORNAME,
    SQL_API_SQLSETSCROLLOPTIONS,
    SQL_API_SQLSETPARAM,
    SQL_API_SQLEXECUTE,
    SQL_API_SQLEXECDIRECT,
    SQL_API_SQLNATIVESQL,
    SQL_API_SQLDESCRIBEPARAM,
    SQL_API_SQLNUMPARAMS,
    SQL_API_SQLPARAMDATA,
    SQL_API_SQLPUTDATA,
    SQL_API_SQLROWCOUNT,
    SQL_API_SQLNUMRESULTCOLS,
    SQL_API_SQLDESCRIBECOL,
    SQL_API_SQLCOLATTRIBUTES,
    SQL_API_SQLCOLATTRIBUTE,
    SQL_API_SQLBINDCOL,
    SQL_API_SQLFETCH,
    SQL_API_SQLEXTENDEDFETCH,
    SQL_API_SQLGETDATA,
    SQL_API_SQLSETPOS,
    SQL_API_SQLMORERESULTS,
    SQL_API_SQLERROR,
    SQL_API_SQLCOLUMNPRIVILEGES,
    SQL_API_SQLCOLUMNS,
    SQL_API_SQLFOREIGNKEYS,
    SQL_API_SQLPRIMARYKEYS,
    SQL_API_SQLPROCEDURECOLUMNS,
    SQL_API_SQLPROCEDURES,
    SQL_API_SQLSPECIALCOLUMNS,
    SQL_API_SQLSTATISTICS,
    SQL_API_SQLTABLEPRIVILEGES,
    SQL_API_SQLTABLES,
    SQL_API_SQLFREESTMT,
    SQL_API_SQLCANCEL,
    SQL_API_SQLTRANSACT,
    SQL_API_SQLDISCONNECT,
    SQL_API_SQLFREECONNECT,
    SQL_API_SQLFREEENV
};

#define FUNCTION_NUMBERS_SIZE sizeof(FunctionNumbers)/sizeof(int)

#define SQL_ODBC3_SET_FUNC_ON(pfExists, uwAPI) \
	*( ((UWORD*) (pfExists)) + ((uwAPI) >> 4) ) |= (1 << ((uwAPI) & 0x000F))

#define SQL_ODBC3_SET_FUNC_OFF(pfExists, uwAPI) \
	*( ((UWORD*) (pfExists)) + ((uwAPI) >> 4) ) &= !(1 << ((uwAPI) & 0x000F))

#endif

SQLRETURN SQL_API
SQLGetFunctions (
    SQLHDBC hdbc,
    SQLUSMALLINT fFunc,
    SQLUSMALLINT FAR * pfExists)
{
  CONN (pdbc, hdbc);
  HPROC hproc;
  SQLRETURN retcode;

  if (!IS_VALID_HDBC (pdbc))
    {
      return SQL_INVALID_HANDLE;
    }
  CLEAR_ERRORS (pdbc);


  if (pdbc->state == en_dbc_allocated
      || pdbc->state == en_dbc_needdata)
    {
      PUSHSQLERR (pdbc->herr, en_S1010);

      return SQL_ERROR;
    }

  if (pfExists == NULL)
    {
      return SQL_SUCCESS;
    }

#if (ODBCVER >= 0x0300)
  /* thease are pure DM functions */
  if (fFunc == SQL_API_SQLGETENVATTR ||
      fFunc == SQL_API_SQLSETENVATTR)
    {
      *pfExists = SQL_TRUE;
      return SQL_SUCCESS;
    }

  if (((GENV_t FAR *) pdbc->genv)->odbc_ver == SQL_OV_ODBC2)
#endif
    if (fFunc > SQL_EXT_API_LAST)
      {
	PUSHSQLERR (pdbc->herr, en_S1095);

	return SQL_ERROR;
      }
#if (ODBCVER >= 0x0300)
  if (((ENV_t FAR *) pdbc->henv)->dodbc_ver == SQL_OV_ODBC2)
    {
      switch (fFunc)
	{
	case SQL_API_ALL_FUNCTIONS:
	case SQL_API_ODBC3_ALL_FUNCTIONS:
	  break;

	  /* Mapped ODBC3 app -> ODBC2 drvr fucntions */
	case SQL_API_SQLALLOCHANDLE:
	case SQL_API_SQLFREEHANDLE:
	case SQL_API_SQLSETCONNECTATTR:
	case SQL_API_SQLGETCONNECTATTR:
	case SQL_API_SQLGETSTMTATTR:
	case SQL_API_SQLSETSTMTATTR:
	case SQL_API_SQLCOLATTRIBUTE:
	case SQL_API_SQLENDTRAN:
	case SQL_API_SQLBULKOPERATIONS:
	case SQL_API_SQLFETCHSCROLL:
	case SQL_API_SQLGETDIAGREC:
	case SQL_API_SQLGETDIAGFIELD:
	  *pfExists = SQL_TRUE;
	  return SQL_SUCCESS;

	case SQL_API_SQLBINDPARAM:
	  fFunc = SQL_API_SQLBINDPARAMETER;
	  break;

	default:
	  if (fFunc > SQL_EXT_API_LAST)
	    {
	      *pfExists = SQL_FALSE;
	      return SQL_SUCCESS;
	    }
	  break;
	}
    }
#endif

  if (fFunc == SQL_API_SQLDATASOURCES || fFunc == SQL_API_SQLDRIVERS)
    {
      *pfExists = (UWORD) 1;
      return SQL_SUCCESS;
    }

  hproc = _iodbcdm_getproc (pdbc, en_GetFunctions);

  if (hproc != SQL_NULL_HPROC)
    {
      CALL_DRIVER (hdbc, pdbc, retcode, hproc, en_GetFunctions,
	  (pdbc->dhdbc, fFunc, pfExists));

      return retcode;
    }

  if (fFunc == SQL_API_SQLSETPARAM)
    {
      fFunc = SQL_API_SQLBINDPARAMETER;
    }

#if (ODBCVER >= 0x0300)
  if (fFunc != SQL_API_ALL_FUNCTIONS && fFunc != SQL_API_ODBC3_ALL_FUNCTIONS)
#else
  if (fFunc != SQL_API_ALL_FUNCTIONS)
#endif
    {
      hproc = _iodbcdm_getproc (pdbc, fFunc);

      if (hproc == SQL_NULL_HPROC)
	{
	  *pfExists = (UWORD) 0;
	}
      else
	{
	  *pfExists = (UWORD) 1;
	}

      return SQL_SUCCESS;
    }

#if (ODBCVER >= 0x0300)
  if (fFunc == SQL_API_ODBC3_ALL_FUNCTIONS)
    {
      for (fFunc = 0; fFunc < FUNCTION_NUMBERS_SIZE; fFunc++)
	{
	  hproc = _iodbcdm_getproc (pdbc, FunctionNumbers[fFunc]);

	  if (hproc == SQL_NULL_HPROC)
	    {
	      SQL_ODBC3_SET_FUNC_OFF (pfExists, FunctionNumbers[fFunc]);
	    }
	  else
	    {
	      SQL_ODBC3_SET_FUNC_ON (pfExists, FunctionNumbers[fFunc]);
	    }
	}
    }
  else
#endif
    for (fFunc = 0; fFunc < 100; fFunc++)
      {
	hproc = _iodbcdm_getproc (pdbc, fFunc);

	if (hproc == SQL_NULL_HPROC)
	  {
	    pfExists[fFunc] = (UWORD) 0;
	  }
	else
	  {
	    pfExists[fFunc] = (UWORD) 1;
	  }
      }

  return SQL_SUCCESS;
}
