#ifndef __EXT_FILE_ERROR_HH__
#define __EXT_FILE_ERROR_HH__

#ifdef BUILD_LAND02
#include "../../lu_common/colourtext.hh"
#else
#include "../lu_common/colourtext.hh"
#endif

#define MSG(...) do {              \
  if (_config._forked)             \
    fprintf(stderr,"%s%s:%s ",CT_ERR(BLUE),_argv0,CT_ERR(DEF_COL));	\
  fprintf(stderr,__VA_ARGS__);     \
  fputc('\n',stderr);              \
} while (0)

#define WARN_MSG(...) do { \
  if (_config._forked)             \
    fprintf(stderr,"%s%s:%s ",CT_ERR(BLUE),_argv0,CT_ERR(DEF_COL));	\
  fprintf(stderr,"%s",CT_ERR(BLACK_BG_YELLOW));	   \
  fprintf(stderr,__VA_ARGS__);     \
  fprintf(stderr,"%s",CT_ERR(DEF_COL));	   \
  fputc('\n',stderr);              \
} while(0)

#define ERR_MSG(...) do { \
  if (_config._forked)             \
    fprintf(stderr,"%s%s:%s ",CT_ERR(BLUE),_argv0,CT_ERR(DEF_COL));	\
  fprintf(stderr,"%s",CT_ERR(WHITE_BG_RED));	   \
  fprintf(stderr,__VA_ARGS__);     \
  fprintf(stderr,"%s",CT_ERR(DEF_COL));	   \
  fputc('\n',stderr);              \
  exit(1);                \
} while(0)

#endif/*__EXT_FILE_ERROR_HH__*/
