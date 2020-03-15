/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2016  Haakan T. Johansson  <f96hajo@chalmers.se>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301  USA
 */

#include "external_writer.hh"

#include "staged_ntuple.hh"

#include "error.hh"
#include "colourtext.hh"
#include "optimise.hh"

#ifdef __LAND02_CODE__ // i.e. compiling as UCESB - this *is* rotten!
#include "mc_def.hh"
#else
#include "util.hh"
#endif

#include "colourtext.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>

ext_writer_buf::ext_writer_buf()
{
  _size = 0;
  _reading = false;
}

ext_writer_shm_buf::ext_writer_shm_buf()
{
  _fd_mem = -1;
  _ptr = NULL; // shm
  _len = 0;
}

ext_writer_pipe_buf::ext_writer_pipe_buf()
{
  _mem = NULL; // !shm
  _cur = NULL;
  _begin_write = NULL;
}

external_writer::external_writer()
{
  _buf = NULL;
  _max_message_size = 0;
  _sort_u32_words = 0;
  _max_raw_words = 0;
}

external_writer::~external_writer()
{
  close();
  if (_buf)
    delete _buf;
}


int ext_writer_shm_buf::init_open()
{
#if defined(__NetBSD__) || defined(__APPLE__) || defined(EXTERNAL_WRITER_NO_SHM)
  // NetBSD has no shm_open().  Has shmat() etc, but for now: revert to pipe.
  // MacOSX seems not to like to ftruncate the shm after unlinking it, for now:
  // revert to pipe.
  return -1;
#else
  // First create ourselves a piece of shared memory for efficient
  // communication

  char tmpname[32];

  snprintf (tmpname,sizeof(tmpname),
	    "/ucesb_%d_%d",(int)time(NULL),(int)getpid());

  // Using mkdtemp or any of those functions here does not work, as
  // they try to create an actual file somewhere, whose name has not
  // much to do with the shared memory file.

  _fd_mem = shm_open(tmpname,O_RDWR | O_CREAT | O_EXCL,
		     S_IRUSR | S_IWUSR);

  if (_fd_mem == -1)
    {
      perror("shm_open");
      WARNING("Error creating shared memory segment (%s).",tmpname);
      return -1;
    }

  // Then, the first thing we do is to unlink it.  Thus, it will
  // go away as soon as we're done

  if (shm_unlink(tmpname) == -1)
    {
      WARNING("Error unlinking shared memory segment.");
      return -1;
    }

  // Set the size

  _len = EXTERNAL_WRITER_MIN_SHARED_SIZE;

  if (ftruncate(_fd_mem,(off_t) _len) == -1)
    {
      WARNING("Error setting size of shared memory segment.");
      return -1;
    }

  // Check that the external writer has a chance to get the size

  struct stat st;

  int r = fstat(_fd_mem, &st);

  if (r != 0)
    {
      perror("stat");
      WARNING("Error determining shared memory size.");
      return -1;
    }

  if (st.st_size != (off_t) _len)
    {
      WARNING("Shared memory size wrong.");
      return -1;
    }

  // We want the file descriptor to survive the later exec() call

  int flags = fcntl(_fd_mem,F_GETFD);

  if (fcntl(_fd_mem,F_SETFD,flags & ~FD_CLOEXEC) == -1)
    {
      WARNING("Error clearing close-on-exec for shared memory file handle.");
      return -1;
    }

  // And try to map it

  _ptr = (char *) mmap(0, _len, (PROT_READ | PROT_WRITE),
		       MAP_SHARED, _fd_mem, 0);

  if (_ptr == MAP_FAILED)
    {
      WARNING("Error mapping shared memory segment.");
      return -1;
    }

  // Ensure the pages get faulted in.
  memset(_ptr,0,_len);

  _ctrl = (external_writer_shm_control*) _ptr;
  _cur  = _begin = (char*) (_ctrl + 1);
  _end  = _ptr + _len;
  _size = (size_t) (_end - _begin);

  assert (_end > _begin); // or external_writer_shm_control too large

  _ctrl->_magic = EXTERNAL_WRITER_MAGIC;
  _ctrl->_len   = _len;
  // Set up such that we send a token when we've written something
  _ctrl->_need_consumer_wakeup = true;
  _ctrl->_wakeup_avail = 0;

  return _fd_mem;
#endif
}

void ext_writer_pipe_buf::alloc(uint32_t size)
{
  size_t off_cur = (size_t) (_cur         - _mem);
  size_t off_bw  = (size_t) (_begin_write - _mem);

  _mem = (char*) realloc(_mem,size);
  if (!_mem)
    ERROR("Failure allocating memory for pipe buffer.");
  _end = _mem + size;
  _size = size;

  _cur         = _mem + off_cur;
  _begin_write = _mem + off_bw;
  _write_mark  = _begin_write;

  _write_mark = _begin_write + (_end - _begin_write) / 4;
  if (_write_mark == _begin_write)
    _write_mark = _end;
}

void ext_writer_pipe_buf::init_alloc()
{
  // Allocate buffer

  alloc(EXTERNAL_WRITER_MIN_SHARED_SIZE);
}


void external_writer::init_x(unsigned int type,unsigned int opt,
			     const char *filename,const char *ftitle,
			     int server_port,int generate_header,
			     int timeslice,int timeslice_subdir,
			     int autosave)
{
  int fd_mem = -1;
  bool shm = !(opt & NTUPLE_OPT_WRITER_NO_SHM);

  if (shm)
    {
      ext_writer_shm_buf *ewsb = new ext_writer_shm_buf();

      fd_mem = ewsb->init_open();

      if (fd_mem == -1)
	{
	  delete ewsb;
	  INFO(0,"Reverting to pipe communication.");
	  shm = false;
	}
      else
	{
	  _buf = ewsb;
	  INFO(0,"Using shm communication.");
	}
    }
  // We may have reverted to !shm above
  if (!shm)
    {
      ext_writer_pipe_buf *ewpb = new ext_writer_pipe_buf();

      ewpb->init_alloc();
      _buf = ewpb;
      INFO(0,"Using pipe communication.");
    }

  // Then fork of the child process

  const char *ext_writer;
  const char *ext_writer_name;

#ifndef EXT_WRITER_PREFIX // UCESB has this defined
// as land02
#define EXT_WRITER_PREFIX "../util/hbook/"
#endif

  if (type & NTUPLE_TYPE_ROOT)
    {
      ext_writer = EXT_WRITER_PREFIX "root_writer";
      ext_writer_name = "root_writer";
    }
  else if (type & NTUPLE_TYPE_CWN)
    {
      ext_writer = EXT_WRITER_PREFIX "hbook_writer";
      ext_writer_name = "hbook_writer";
    }
  else if (type & (NTUPLE_TYPE_STRUCT | NTUPLE_TYPE_STRUCT_HH))
    {
      ext_writer = EXT_WRITER_PREFIX "struct_writer";
      ext_writer_name = "struct_writer";
    }
  else
    ERROR("No external writer for selected ntuple type.");

  // We generate all strings for the argv[] array, so they can all be
  // freed afterwards

  int fd_dest = -1;

  char tmp[1024];
  /*const*/ char *argv[25];
  int argc = 0;
  char *executable = NULL;

  char *fork_pipes = NULL;

  // gdb -batch -ex "run" -ex "bt"

  if (opt & NTUPLE_OPT_EXT_GDB)
    {
      executable = strdup("gdb");
      argv[argc++] = strdup("gdb");
      argv[argc++] = strdup("-batch");
      argv[argc++] = strdup("-ex");
      argv[argc++] = strdup("run");
      argv[argc++] = strdup("-ex");
      argv[argc++] = strdup("bt");
      argv[argc++] = strdup("--return-child-result");
      argv[argc++] = strdup("--args");
    }
  else if (opt & NTUPLE_OPT_EXT_VALGRIND)
    {
     executable = strdup("valgrind");
     argv[argc++] = strdup("valgrind");
    }
  else
    {
      argv[argc++] = strdup(ext_writer_name);
      executable = argv0_replace(ext_writer);
    }

  if (opt & (NTUPLE_OPT_EXT_GDB | NTUPLE_OPT_EXT_VALGRIND))
    {
      argv[argc++] = strdup(argv0_replace(ext_writer));
    }

  // fork_pipes points to XXXXX,XXXXX, so that actual numbers can be
  // filled out by the forker

  if (shm)
    {
      snprintf (tmp,sizeof(tmp),
		"--shm-forked=%d,XXXXX,XXXXX",fd_mem);
    }
  else
    {
      strcpy(tmp, "--forked=XXXXX,XXXXX");
    }
  argv[argc] = strdup(tmp);
  fork_pipes = argv[argc] + strlen(argv[argc]) - 11;
  argc++;

  int forcecolour = colourtext_getforce();

  if (forcecolour)
    {
      snprintf (tmp,sizeof(tmp),
		"--colour=%s",forcecolour > 0 ? "yes" : "no");
      argv[argc++] = strdup(tmp);
    }

  if (type & NTUPLE_TYPE_STRUCT_HH)
    {
      snprintf (tmp,sizeof(tmp),
		"--header=%s",filename);  argv[argc++] = strdup(tmp);
    }

  if (opt & NTUPLE_OPT_READER_INPUT)
    {
      if (type & (NTUPLE_TYPE_ROOT | NTUPLE_TYPE_CWN))
	{
	  snprintf (tmp,sizeof(tmp),
		    "--infile=%s",filename);  argv[argc++] = strdup(tmp);
	}
      if (type & NTUPLE_TYPE_STRUCT)
	{
	  snprintf (tmp,sizeof(tmp),
		    "--insrc=%s",filename);  argv[argc++] = strdup(tmp);
	}
    }
  else
    {
      if (type & (NTUPLE_TYPE_ROOT | NTUPLE_TYPE_CWN))
	{
	  snprintf (tmp,sizeof(tmp),
		    "--outfile=%s",filename); argv[argc++] = strdup(tmp);
	  snprintf (tmp,sizeof(tmp),
		    "--ftitle=%s",ftitle);    argv[argc++] = strdup(tmp);

	  if (timeslice)
	    {
	      snprintf (tmp,sizeof(tmp),
			"--timeslice=%d",timeslice);
	      if (timeslice_subdir)
		snprintf (tmp+strlen(tmp),sizeof(tmp)-strlen(tmp),
			  ":%d",timeslice_subdir);
	      argv[argc++] = strdup(tmp);
	    }

	  if (autosave)
	    {
	      snprintf (tmp,sizeof(tmp),
			"--autosave=%d",autosave);
	      argv[argc++] = strdup(tmp);
	    }
	}
      if (type & NTUPLE_TYPE_STRUCT)
	{
	  if (strcmp(filename,"-") == 0)
	    {
	      argv[argc++] = strdup("--stdout");

	      // Make a copy of STDOUT_FILENO so we can write to it from
	      // the forked process

	      if ((fd_dest = dup(STDOUT_FILENO)) == -1)
		{
		  perror("dup");
		  ERROR("Failed to duplicate STDOUT.");
		}

	      // Then duplicate STDERR to also be used by anything
	      // else that wants to go to stdout

	      if (dup2(STDERR_FILENO,STDOUT_FILENO) == -1)
		{
		  perror("dup2");
		  ERROR("Failed to duplicate STDERR to STDOUT.");
		}

#ifdef __LAND02_CODE__ // i.e. compiling as UCESB - this *is* rotten!
	      // Re-evaluate terminals
	      colourtext_prepare();
#endif
	    }
	  else
	    {
	      snprintf (tmp,sizeof(tmp),
			"--server=%d",server_port); argv[argc++] = strdup(tmp);
	    }
	}
    }

  argv[argc++] = NULL; // terminate

  int dummy_fd; // so they are not closed

  _buf->_fork.fork(executable,argv,
                   &dummy_fd,&dummy_fd,fd_dest,-1,fd_mem,fork_pipes);

  INFO(0,"Forked: %s\n",argv[0]);

  free (executable);
  for (int i = 0; argv[i]; i++)
    free(argv[i]);

  // Make sure the child runs and managed to initialize.  No need, if
  // it crashed, we'll know as soon as the pipes are full and we
  // cannot write/read from the control pipe.
}

void ext_writer_shm_buf::close()
{
  if (_ptr)
    {
      munmap(_ptr,_len);
      _ptr = NULL;
    }
  if (_fd_mem != -1)
    {
      if (::close(_fd_mem) == -1)
	ERROR("Error closing shared memory segment.");
      _fd_mem = -1;
    }
  _cur = NULL;
}

void ext_writer_pipe_buf::close()
{
  if (_mem)
    {
      free(_mem);
      _mem = NULL;
    }
  _cur = NULL;
}

void external_writer::close()
{
  if (!_buf->_reading && _buf->_cur)
    {
      // A final token message, done

      send_done();

      // Write whatever is to be written

      _buf->flush();

      // Wait for the successful close response

      ext_writer_shm_buf *ewsb = dynamic_cast<ext_writer_shm_buf *>(_buf);

      if (ewsb && ewsb->_ctrl)
	{
	  while (_buf->get_response() != EXTERNAL_WRITER_RESPONSE_DONE)
	    ;

	  // If everything has worked properly, it should have exactly
	  // consumed all our data

	  if (ewsb->_ctrl->_avail != ewsb->_ctrl->_done)
	    ERROR("External writer has not consumed all shared memory messages"
		  " (avail=%lld,done=%lld)",
		  (long long) ewsb->_ctrl->_avail,
		  (long long) ewsb->_ctrl->_done);
	}
    }

  _buf->close();
  // Now, we can shutdown our side

  if (!_buf->_fork._child)
    return;

  _buf->_fork.wait(false);
}

void ext_writer_buf::write_command(char cmd)
{
  full_write(_fork._fd_out,&cmd,sizeof(cmd));
}

char ext_writer_buf::get_response()
{
  char response;

  for ( ; ; )
    {
      ssize_t ret = read(_fork._fd_in,&response,sizeof(response));

      if (ret == -1)
	{
	  if (errno == EINTR)
	    continue;
	  else
	    {
              perror("read");
              ERROR("Error reading token from external writer.");
	    }
	}
      if (!ret)
	{
	  WARNING("Unexpected end of file from external writer.");
	  exit(1);
	}
      break;
    }
  if (response != EXTERNAL_WRITER_RESPONSE_WORK &&
      response != EXTERNAL_WRITER_RESPONSE_WORK_RD &&
      response != EXTERNAL_WRITER_RESPONSE_DONE)
    {
      // Some kind of error message.
      ERROR("Error from external writer (%d).",response);
    }
  return response;
}

void ext_writer_shm_buf::commit_buf_space(size_t space)
{
  /*
  printf ("c: %16p x (a:%08x d:%08x)\n",_cur,
	  _ctrl->_avail,_ctrl->_done);
  */
  /*
  printf ("COMMIT: %08x (%08x) %08x (%4x) [%08x]\n",
	  _ctrl->_avail,_ctrl->_done,_cur - (char*) _ctrl,space,
	  *((uint32_t *) _cur));
  */

  _ctrl->_avail += space;
  _cur += space;

  if (_cur == _end)
    _cur = _begin; // start over from beginning
  assert(_cur + sizeof (uint32_t) <= _end);

  // if the consumer wanted to be woken up...

  if (_ctrl->_need_consumer_wakeup &&
      (((int) _ctrl->_avail) - ((int) _ctrl->_wakeup_avail)) >= 0)
    {
      _ctrl->_need_consumer_wakeup = 0;
      SFENCE;

      flush();
    }
}

void ext_writer_shm_buf::flush()
{
  write_command(EXTERNAL_WRITER_CMD_SHM_WORK);
}

void ext_writer_pipe_buf::flush()
{
  if (_cur-_begin_write)
    {
      //printf ("write %d bytes (flush)\n",_cur-_begin_write);
      full_write(_fork._fd_out,_begin_write,
		 (size_t) (_cur-_begin_write));
      _begin_write = _cur;
      // wrote_pipe_buf();
    }
}

void ext_writer_pipe_buf::commit_buf_space(size_t space)
{
  _cur += space;
  if (_cur >= _write_mark)
    {
      //printf ("write %d bytes (partial)\n",_write_mark-_begin_write);
      full_write(_fork._fd_out,_begin_write,
		 (size_t) (_write_mark-_begin_write));
      _begin_write = _write_mark;
      // wrote_pipe_buf();

      if (_size >= 2*0x1000)
	_write_mark += ((_size / 4) & (size_t) ~(0x1000 - 1));
      else
	_write_mark += _size / 4;
    }
}

char *ext_writer_shm_buf::ensure_buf_space(uint32_t space)
{
  if (space > _size)
    ERROR("Requesting too much space in shared memory segment. (%d > %d)",
	  space,(uint32_t) _size);

  // Check that there is enough space available

 check_space:
  while (_ctrl->_avail + space - _ctrl->_done > _size)
    {
      MFENCE; // (_size >> 4) to get some hysteresis
      _ctrl->_wakeup_done = _ctrl->_avail - _size + space + (_size >> 4);
      MFENCE;
      _ctrl->_need_producer_wakeup = 1;
      MFENCE;
      // check again, it may have become available
      if (_ctrl->_avail + space - _ctrl->_done <= _size)
	break;
      // we've told we wanted to be woken up, and got nothing in
      // between.  We need to block

      get_response();
    }

  // Now, if we're having really bad luck, the linear space is not
  // long enough

  size_t lin_left = (size_t) (_end - _cur);

  if (lin_left < space)
    {
      assert ((_end - _cur) >= (ssize_t) sizeof(uint32_t));
      // put a marker that eats all the space
      *((uint32_t*) _cur) = htonl(EXTERNAL_WRITER_BUF_EAT_LIN_SPACE);
      //printf ("WASTE:  (%x)\n",lin_left);
      // waste the space
      commit_buf_space(lin_left);
      // we must check again that there is enough space free
      goto check_space;
    }
  return _cur;
}

char *ext_writer_pipe_buf::ensure_buf_space(uint32_t space)
{
  if (space > _size)
    ERROR("Requesting too much space in pipe memory. (%d > %d)",
	  space,(uint32_t) _size);

  if (_end - _cur >= (ssize_t) space)
    return _cur; // There is space enough

  // first we have to write out whatever was not written before

  if (_cur-_begin_write)
    {
      //printf ("write %d bytes (end-of-data)\n",_cur-_begin_write);
      full_write(_fork._fd_out,_begin_write,
		 (size_t) (_cur-_begin_write));
      // wrote_pipe_buf();
    }

  // so, now we should reset the _cur and _begin pointers to
  // maximise chance of alignment, we'll (if using more than one
  // page) align _begin such that what we wrote as a partial page
  // now, will get the remainder of the first page, and then the
  // next page becomes aligned (if the consumer operates with
  // entire pages)

  // TODO: should use page size known from system!

  restart_pointers();

  return _cur;
}

void ext_writer_pipe_buf::restart_pointers()
{
  if (_size >= 2*0x1000)
    {
      size_t page_off = (_cur - _mem) & (0x1000 - 1);
      _begin_write = _cur = _mem + page_off;
      _write_mark = _mem + ((_size / 4) & (size_t) ~(0x1000 - 1));
    }
  else
    {
      _begin_write = _cur = _mem;
      _write_mark = _mem + _size / 4;
    }
}

//
#define EWB_STRING_SPACE_FROM_LEN(len) (sizeof(external_writer_buf_string)+\
					((((len)+1) + 3) & (size_t) ~3))
#define EWB_STRING_SPACE(str) EWB_STRING_SPACE_FROM_LEN(strlen(str))

void *external_writer::insert_buf_string(void *ptr,const char *str)
{
  external_writer_buf_string *buf =
    (external_writer_buf_string*) ptr;

  uint32_t len = (uint32_t) strlen(str);
  size_t space = EWB_STRING_SPACE_FROM_LEN(len);

  buf->_length = htonl(len);
  memcpy(buf->_string,str,len);
  for ( ; len < space - sizeof(external_writer_buf_string); len++)
    buf->_string[len] = 0;

  return ((char *) ptr) + space;
}

void *external_writer::insert_buf_header(void *ptr,
					 uint32_t request,uint32_t length)
{
  external_writer_buf_header *header =
    (external_writer_buf_header*) ptr;

  header->_request = htonl(request | EXTERNAL_WRITER_REQUEST_HI_MAGIC);
  header->_length  = htonl(length);

  return (char *) (header+1);
}

void external_writer::send_file_open(/*const char *filename,
				     const char *ftitle,
				     int server_port,int generate_header*/
				     uint32_t sort_u32_words)
{
  uint32_t space = (uint32_t) (sizeof(external_writer_buf_header)+
    /*3**/2 * sizeof(uint32_t)/* +
      EWB_STRING_SPACE(filename)+EWB_STRING_SPACE(ftitle)*/);
  /*
  printf ("%d %d %d -> %d\n",
	  sizeof(external_writer_buf_header),
	  EWB_STRING_SPACE(filename),EWB_STRING_SPACE(ftitle),
	  space);
  */
  // space for header
  void *p = _buf->ensure_buf_space(space);

  p = insert_buf_header(p,EXTERNAL_WRITER_BUF_OPEN_FILE,space);

  uint32_t *i = (uint32_t*) p;
  *(i++) = htonl(EXTERNAL_WRITER_MAGIC); // put some magic for checking
  //*(i++) = htonl(server_port);
  //*(i++) = htonl(generate_header);
  *(i++) = htonl(sort_u32_words);
  _sort_u32_words = sort_u32_words;
  p = i;

  //p = insert_buf_string(p,filename);
  //p = insert_buf_string(p,ftitle);

  assert (p == _buf->_cur + space);

  _buf->commit_buf_space(space);
}

void external_writer::send_book_ntuple_x(int hid,
					 const char *id,const char *title,
					 const char *index_major,
					 const char *index_minor,
					 uint32_t struct_index,
					 uint32_t ntuple_index,
					 uint32_t max_raw_words)
{
  uint32_t space =
    (uint32_t) (sizeof(external_writer_buf_header) +
		(ntuple_index == 0 ? 4 : 3) * sizeof(uint32_t) +
		EWB_STRING_SPACE(id) + EWB_STRING_SPACE(title) +
		EWB_STRING_SPACE(index_major) + EWB_STRING_SPACE(index_minor));

  // space for header
  void *p = _buf->ensure_buf_space(space);

  p = insert_buf_header(p,EXTERNAL_WRITER_BUF_BOOK_NTUPLE,space);

  uint32_t *i = (uint32_t*) p;
  *(i++) = htonl(struct_index);
  *(i++) = htonl(ntuple_index);
  *(i++) = htonl(hid);
  if (ntuple_index == 0)
    {
      *(i++) = htonl(max_raw_words);
      _max_raw_words = max_raw_words;
    }
  p = i;

  p = insert_buf_string(p,id);
  p = insert_buf_string(p,title);
  p = insert_buf_string(p,index_major);
  p = insert_buf_string(p,index_minor);

  assert (p == _buf->_cur + space);

  _buf->commit_buf_space(space);
}

void external_writer::send_alloc_array(uint32_t size)
{
  uint32_t space = (uint32_t) (sizeof(external_writer_buf_header) +
			       sizeof(uint32_t));

  // space for header
  void *p = _buf->ensure_buf_space(space);

  p = insert_buf_header(p,EXTERNAL_WRITER_BUF_ALLOC_ARRAY,space);

  uint32_t *i = (uint32_t*) p;
  *(i++) = htonl(size);
  p = i;

  assert (p == _buf->_cur + space);

  _buf->commit_buf_space(space);
}

void external_writer::send_hbname_branch(const char *block,
					 uint32_t offset,
					 uint32_t length,
					 const char *var_name,
					 uint var_array_len,
					 const char *var_ctrl_name,
					 int var_type,
					 uint limit_min,uint limit_max)
{
  uint32_t space = (uint32_t) (sizeof(external_writer_buf_header)+
			       EWB_STRING_SPACE(block)+
			       6*sizeof(uint32_t)+
			       EWB_STRING_SPACE(var_name) +
			       EWB_STRING_SPACE(var_ctrl_name));

  // space for header
  void *p = _buf->ensure_buf_space(space);

  p = insert_buf_header(p,EXTERNAL_WRITER_BUF_CREATE_BRANCH,space);

  uint32_t *i = (uint32_t*) p;
  *(i++) = htonl(offset);
  *(i++) = htonl(length);
  *(i++) = htonl(var_array_len);
  *(i++) = htonl(var_type);
  if (var_type & EXTERNAL_WRITER_FLAG_HAS_LIMIT)
    {
      *(i++) = htonl(limit_min);
      *(i++) = htonl(limit_max);
    }
  else
    {
      *(i++) = htonl((uint32_t) -1);
      *(i++) = htonl((uint32_t) -1);
    }
  p = i;

  p = insert_buf_string(p,block);
  p = insert_buf_string(p,var_name);
  p = insert_buf_string(p,var_ctrl_name);

  assert (p == _buf->_cur + space);

  _buf->commit_buf_space(space);
}

void external_writer::send_setup_done(bool reader)
{
  uint32_t space = (uint32_t) (sizeof(external_writer_buf_header));

  // space for header
  void *p = _buf->ensure_buf_space(space);

  p = insert_buf_header(p,
			reader ? EXTERNAL_WRITER_BUF_SETUP_DONE_RD :
			EXTERNAL_WRITER_BUF_SETUP_DONE,space);

  assert (p == _buf->_cur + space);

  _buf->commit_buf_space(space);
  _buf->flush();

  // If we are to be a reader, then flush the buffer

  if (reader)
    {
      _buf->flush();
      _buf->ready_to_read();
    }
}

void external_writer::send_done()
{
  uint32_t space = sizeof(external_writer_buf_header);

  // space for header
  void *p = _buf->ensure_buf_space(space);

  p = insert_buf_header(p,EXTERNAL_WRITER_BUF_DONE,space);

  assert (p == _buf->_cur + space);

  _buf->commit_buf_space(space);
}

uint32_t *external_writer::prepare_send_fill_x(uint32_t size,
					       uint32_t struct_index,
					       uint32_t ntuple_index,
					       uint32_t *sort_u32,
					       uint32_t **raw,
					       uint32_t raw_words)
{
  uint32_t space =
    EXTERNAL_WRITER_SIZE_NTUPLE_FILL(size,_sort_u32_words,
				     _max_raw_words, raw_words);

  void *p = _buf->ensure_buf_space(space);

  p = insert_buf_header(p,EXTERNAL_WRITER_BUF_NTUPLE_FILL,space);

  uint32_t *i = (uint32_t*) p;
  for (uint32_t j = 0; j < _sort_u32_words; j++)
    *(i++) = htonl(sort_u32[j]);
  *(i++) = htonl(struct_index);
  *(i++) = htonl(ntuple_index);
  if (_max_raw_words)
    {
      assert(raw_words <= _max_raw_words);
      *(i++) = htonl(raw_words);
      assert(raw);
      // fprintf (stderr,"RAW DEST: %p\n", i);
      *raw = i; // give a pointer where to fill the data
      i += raw_words;
    }
  p = i;

  return (uint32_t*) p;
}

uint32_t *external_writer::prepare_send_offsets(uint32_t size)
{
  uint32_t space = (uint32_t) (sizeof(external_writer_buf_header) +
			       sizeof(uint32_t)) + size;

  void *o = _buf->ensure_buf_space(space);

  o = insert_buf_header(o,EXTERNAL_WRITER_BUF_ARRAY_OFFSETS,space);

  uint32_t *i = (uint32_t*) o;
  *(i++) = htonl(0); // Reserve space for the xor_sum check variable.
  o = i;

  return (uint32_t*) o;
}

void external_writer::send_offsets_fill(uint32_t *po)
{
  // Write the correct length

  external_writer_buf_header *header =
    (external_writer_buf_header*) _buf->_cur;

  uint32_t size = (uint32_t) (((char*) po) - _buf->_cur);

  assert(header->_request == htonl(EXTERNAL_WRITER_BUF_ARRAY_OFFSETS |
				   EXTERNAL_WRITER_REQUEST_HI_MAGIC) ||
	 header->_request == htonl(EXTERNAL_WRITER_BUF_NTUPLE_FILL |
				   EXTERNAL_WRITER_REQUEST_HI_MAGIC));
  assert(header->_length >= size); // or we wrote too much
  header->_length  = htonl(size);

  _buf->commit_buf_space(size);
}

void external_writer::send_flush()
{
  _buf->flush();
}

void ext_writer_shm_buf::resize_shm(uint32_t size)
{
  if (ftruncate(_fd_mem,size) == -1)
    {
      perror("ftruncate");
      ERROR("Error expanding size of shared memory segment.");
    }

  // then we need to remap
  /*
  printf ("l: %016p p: %016p ctrl: %016p b: %016p cur: %016p e: %016p / %08x\n",
	  _len,_ptr,_ctrl,_begin,_cur,_end,_size);
  */
  munmap(_ptr,_len);
  _ptr = (char *) mmap(0, size, (PROT_READ | PROT_WRITE),
		       MAP_SHARED, _fd_mem, 0);

  if (_ptr == MAP_FAILED)
    ERROR("Error mapping expanded shared memory segment.");

  // Ensure the _new_ pages get faulted in.
  memset(_ptr+_len,0,size-_len);

  size_t cur_offset = (size_t) (_cur - _begin);

  size_t expand = size - _len;

  _len = size;

  _ctrl = (external_writer_shm_control*) _ptr;
  _begin = (char*) (_ctrl + 1);
  _cur  = _begin + cur_offset;
  _end  = _ptr + _len;
  _size = (size_t) (_end - _begin);
  /*
  printf ("l: %016p p: %016p ctrl: %016p b: %016p cur: %016p e: %016p / %08x\n",
	  _len,_ptr,_ctrl,_begin,_cur,_end,_size);
  */
  assert (_end > _begin); // or external_writer_shm_control too large
  assert (_ctrl->_magic == EXTERNAL_WRITER_MAGIC);

  // _ctrl->_size is set by the reciever (as we cannot change it from
  // the initial value as it's being checked by the reciever, perhaps
  // after we did the resize)

  // Virtually consume all the available space.  Will be 'free'd' by
  // _ctrl->_done by the consumer when it has resized (i.e. consumed
  // messages up to this point)
  _ctrl->_avail += expand;
}

void ext_writer_pipe_buf::resize_alloc(uint32_t size)
{
  flush();
  alloc(size);
}

void external_writer::set_max_message_size(uint32_t size)
{
  // Also take header and ntuple index into size account

  size = EXTERNAL_WRITER_SIZE_NTUPLE_FILL(size,_sort_u32_words,
					  _max_raw_words, _max_raw_words);

  _max_message_size = size;

  // How much size do we then want?

  if (size < EXTERNAL_WRITER_MIN_SHARED_SIZE)
    size = EXTERNAL_WRITER_MIN_SHARED_SIZE;

  if (size < 0x10000) // 64k, will become 256k below
    size = 0x10000;

  size *= 4;
  // factor to give hysteresis in process/pipe swapping
  // also means we need not care about the space set aside
  // for the control structure


  // In chunks of the minimum size
  size = ((size + (EXTERNAL_WRITER_MIN_SHARED_SIZE-1)) &
	  (uint32_t) ~(EXTERNAL_WRITER_MIN_SHARED_SIZE-1));

  if (size < _buf->_size)
    return; // we never decrease

  ext_writer_shm_buf *ewsb = dynamic_cast<ext_writer_shm_buf *>(_buf);

  // We must first of all have space enough to write this important message

  uint32_t space = (uint32_t) (sizeof(external_writer_buf_header) +
			       2*sizeof(uint32_t));

  void *p = _buf->ensure_buf_space(space);

  p = insert_buf_header(p,
			EXTERNAL_WRITER_BUF_RESIZE,
			space);

  uint32_t *i = (uint32_t*) p;
  *(i++) = htonl(size);
  *(i++) = htonl(EXTERNAL_WRITER_MAGIC); // put some magic for testing (SHM)
  p = i;

  if (ewsb)
    {
      // Then, we must get the space, and remap our buffer
      ewsb->resize_shm(size);
      // Now that we have expanded, we can send the command
    }

  _buf->commit_buf_space(space);

  ext_writer_pipe_buf *ewpb = dynamic_cast<ext_writer_pipe_buf *>(_buf);

  if (ewpb)
    {
      // For the pipe, we (flush) and resize after having sent the
      // command (as it was prepared in the old buffer)
      ewpb->resize_alloc(size);
    }

  // printf ("resized SHM -> %d\n",size);
}

void ext_writer_shm_buf::ready_to_read()
{
  _cur = _begin;
  _reading = true;

  // As the other end still is reading messages, we can not fool
  // around with the avail and done messages yet.  We must wait for a
  // token, which is not the same as usually comes, so that we can
  // distinguish it from possible spurious old ones

  while (get_response() != EXTERNAL_WRITER_RESPONSE_WORK_RD)
    ;

  // We got the token, so we are now ready to move!  The client should have
  // reset donw for us.

  if (_ctrl->_done != 0)
    ERROR("SHM buffer done not 0 after direction reversal.");
}

char *ext_writer_shm_buf::ensure_read_space(uint32_t space)
{
  for ( ; ; )
    {
      while (_ctrl->_avail - _ctrl->_done < space)
	{
	  // Not enough message available.  Need to bang at the producer

	  MFENCE; // (_size >> 4) to get some hysteresis
	  _ctrl->_wakeup_avail = _ctrl->_done + (_size >> 4);
	  MFENCE;
	  _ctrl->_need_consumer_wakeup = 1;
	  MFENCE;
	  // check again, it may have become available
	  if (_ctrl->_avail - _ctrl->_done >= space)
	    break;
	  // we've told we wanted to be woken up, and got nothing in
	  // between.  We need to block

	  get_response();
	}

      // Enough message is available.  Now, did we by chance hit a
      // linear space consumer?

      assert(space >= sizeof(uint32_t));

      external_writer_buf_header *header =
	(external_writer_buf_header *) _cur;
      size_t lin_left = (size_t) (_end - _cur);

      uint32_t request = ntohl(header->_request);

      if ((request & EXTERNAL_WRITER_REQUEST_HI_MASK) !=
	  EXTERNAL_WRITER_REQUEST_HI_MAGIC)
	ERROR("Bad request, hi magic wrong (0x%08x).", request);

      request &= EXTERNAL_WRITER_REQUEST_LO_MASK;

      if (request == EXTERNAL_WRITER_BUF_EAT_LIN_SPACE)
	{
	  _ctrl->_done += lin_left;
	  _cur = _begin;
	  continue;
	}

      if (lin_left < sizeof(external_writer_buf_header))
	ERROR("Too little linear SHM space left for message (req=%d,size=%d).",
	      request,space);

      return _cur;
    }
}

void ext_writer_shm_buf::message_body_done(uint32_t *end)
{
  assert (_cur <= (char*) end);
  assert ((char*) end <= _end);

  _ctrl->_done += (size_t) (((char*) end) - _cur);
  _cur = (char*) end;

  if (_ctrl->_need_producer_wakeup &&
      (((int) _ctrl->_done) - ((int) _ctrl->_wakeup_done)) >= 0)
    {
      _ctrl->_need_producer_wakeup = 0;
      SFENCE;
      flush();
    }
}

#define READ_END _begin_write

void ext_writer_pipe_buf::ready_to_read()
{
  // We'll simply reuse the buffer space.  Reset the pointers to start
  // over at the beginning of the space.

  // _size
  // _mem : start of buffer
  // _end : end of buffer
  // _cur : start of message

  _cur = _mem;
  READ_END = _cur;

  _reading = true;
}

char *ext_writer_pipe_buf::ensure_read_space(uint32_t space)
{
  for ( ; ; )
    {
      for ( ; ; )
	{
	  if (_cur + space <= READ_END)
	    return _cur;

	  size_t max_read = (size_t) (_end - READ_END);

	  if (!max_read)
	    break;

	  ssize_t n = read(_fork._fd_in,READ_END,max_read);

	  if (n == -1)
	    {
	      if (errno == EINTR)
		continue;
	      else
		{
		  perror("read");
		  ERROR("Error reading from external reader.");
		}
	    }
	  if (!n)
	    {
	      WARNING("Unexpected end of file from external reader.");
	      exit(1);
	    }

	  READ_END += n;
	}

      // There is not enough space at the end of the buf space, we
      // must move the data back to the beginning!

      size_t left = (size_t) (READ_END - _cur);

      if (_size >= 2*0x1000)
	{
	  size_t page_off = (_cur - _mem) & (0x1000 - 1);
	  char *start = _mem + page_off;
	  if (_cur == start)
	    ERROR("Message too long to be handled by pipe buffer.");
	  //MSG("move to %d",page_off);
	  memmove(start,_cur,left);
	  _cur = start;
	}
      else
	{
	  if (_cur == _mem)
	    ERROR("Message too long for pipe buffer.");
	  //MSG("move to start");
	  memmove(_mem,_cur,left);
	  _cur = _mem;
	}
      READ_END = _cur + left;
    }
}

void ext_writer_pipe_buf::message_body_done(uint32_t *end)
{
  assert (_cur <= (char*) end);
  assert ((char*) end <= READ_END);
  _cur = (char*) end;
}

uint32_t external_writer::get_message(uint32_t **start,uint32_t **end)
{
  // First, we must get enough of the message to see its length

  // Now, at _cur there should be enough space for the header, if not,
  // we need to read more data

  char *p = _buf->ensure_read_space(sizeof (external_writer_buf_header));

  external_writer_buf_header *header = (external_writer_buf_header *) p;

  uint32_t request = ntohl(header->_request);
  uint32_t length  = ntohl(header->_length);

  if ((request & EXTERNAL_WRITER_REQUEST_HI_MASK) !=
      EXTERNAL_WRITER_REQUEST_HI_MAGIC)
    ERROR("Got bad request hi magic from external writer.");

  request &= EXTERNAL_WRITER_REQUEST_LO_MASK;

  // And then get the full message

  p = _buf->ensure_read_space(length);

  // We have the message.  Since only a repeated call to getmessage
  // can cause the space to be overwritten, there is no trouble with
  // incrementing _cur already now

  *start = (uint32_t *) (p + sizeof (external_writer_buf_header));
  *end   = (uint32_t *) (p + length);

  return request;
}

void external_writer::message_body_done(uint32_t *end)
{
  _buf->message_body_done(end);
}

uint32_t external_writer::max_h1i_size(size_t max_id_title_len,uint32_t bins)
{
  uint32_t space = (uint32_t) (sizeof(external_writer_buf_header) +
			       4*sizeof(uint32_t) +
			       2*EWB_STRING_SPACE_FROM_LEN(max_id_title_len) +
			       bins * sizeof(uint32_t));
  return space;
}

void external_writer::send_hist_h1i(int hid,const char *id,const char *title,
				    uint32_t bins,float low,float high,
				    uint32_t *data)
{
  uint32_t space = (uint32_t) (sizeof(external_writer_buf_header) +
			       4*sizeof(uint32_t) +
			       EWB_STRING_SPACE(id) + EWB_STRING_SPACE(title) +
			       (bins+2) * sizeof(uint32_t));

  // space for header
  void *p = _buf->ensure_buf_space(space);

  p = insert_buf_header(p,EXTERNAL_WRITER_BUF_HIST_H1I,space);

  uint32_t *i = (uint32_t*) p;
  *(i++) = htonl(bins);
  *(i++) = htonl(hid);
  *(i++) = htonl(external_write_float_as_uint32(low));
  *(i++) = htonl(external_write_float_as_uint32(high));
  p = i;

  p = insert_buf_string(p,id);
  p = insert_buf_string(p,title);

  i = (uint32_t*) p;
  for (uint32_t b = 0; b < bins+2; b++)
    *(i++) = htonl(data[b]);
  p = i;

  assert (p == _buf->_cur + space);

  _buf->commit_buf_space(space);
}

uint32_t external_writer::max_named_string_size(size_t max_id_len,
						size_t max_str_len)
{
  uint32_t space = (uint32_t) (sizeof(external_writer_buf_header) +
			       EWB_STRING_SPACE_FROM_LEN(max_id_len) +
			       EWB_STRING_SPACE_FROM_LEN(max_str_len));

  return space;
}

void external_writer::send_named_string(const char *id, const char *str)
{
  uint32_t space = (uint32_t) (sizeof(external_writer_buf_header) +
			       EWB_STRING_SPACE(id) +
			       EWB_STRING_SPACE(str));

  // space for header
  void *p = _buf->ensure_buf_space(space);

  p = insert_buf_header(p,EXTERNAL_WRITER_BUF_NAMED_STRING,space);

  p = insert_buf_string(p,id);
  p = insert_buf_string(p,str);

  assert (p == _buf->_cur + space);

  _buf->commit_buf_space(space);
}


void external_writer::silent_fork_pipe(ext_writer_buf *buf)
{
  // We are in a child process, that generates data to a common tree.
  // The data chunks that our owner produces are no longer sent to the
  // root_writer directly, but should be sent to the pipe @fd_out.
  // Our messages are mort likely interleaved with control structures
  // inserted using inject_message().

  // We are to destroy our controlling buffers and close any
  // associated file handles.  But not to write anything to them, or
  // read, or wait for any child.  (Since we do not own the child).  Then
  // (re)create a pipe-buffer object with an output pipe (nothing else).

  // Releasing any memory and/or mmapped areas.
  _buf->close();
  // We do not want to wait for the child below.
  _buf->_fork._child = 0;
  // Closing the file handled of the forked_child object
  _buf->_fork.wait(false);
  // And release
  delete _buf;

  // And assign the new pipe (wrap) structure

  _buf = buf;
}

void external_writer::inject_raw(void *buf,uint32_t len)
{
  void *p = _buf->ensure_buf_space(len);

  memcpy(p, buf, len);

  _buf->commit_buf_space(len);
}
