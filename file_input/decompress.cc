/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2016  GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
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

#include "decompress.hh"
#include "error.hh"
#include "config.hh"

#include "file_mmap.hh"
#include "pipe_buffer.hh"
#include "tcp_pipe_buffer.hh"

#include "thread_debug.hh"
#include "set_thread_name.hh"

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#define MIN_BUFFER_SIZE     0x01000000 //  8 MB prefetch buffer

#define DEFAULT_BUFFER_SIZE 0x04000000 // 64 MB prefetch buffer

size_t get_prefetch_size()
{
  size_t prefetch_size = MIN_BUFFER_SIZE;

  if (!_conf._input_buffer)
    prefetch_size = DEFAULT_BUFFER_SIZE;
  else
    {
      while (prefetch_size < _conf._input_buffer)
	prefetch_size *= 2;
    }

  return prefetch_size;
}

decompressor::decompressor()
{
}

decompressor::~decompressor()
{
  assert(!_fork._child); // want to get rid of the reap below
  reap();
}

void decompressor::start(int *fd_in,
			 const char *method,
			 const char *const *args,
			 const char *filename,
			 int *fd_out,
			 int fd_src)
{
  // From now on, we must either return a good file descriptor,
  // or return -1

  // Our job is to fork a process that does the job...
  // we'll be reading the data from that process' stdout

  // Code stolen from land02

  // We need stdout piped from cpp
  // And we give hime a broken stdin

  const char *argv[10];

  const char **darg = argv;

  *(darg++) = method;
  for (int i = 0; i < 7 && args[i]; i++)
    *(darg++) = args[i];
  if (filename)
    *(darg++) = filename;
  *(darg++) = NULL;

  //for (int i = 0; argv[i]; i++)
  //  printf ("%d: <%s>\n",i,argv[i]);

  _fork.fork(method,argv,fd_in,fd_out,-1,fd_src,-1,NULL,true);
  /* printf ("_fork._fd_in:%d\n",_fork._fd_in); */
  _fork._fd_in = -1; // caller will take care of closing the file!

  if (fd_src != -1)
    _fork._fd_out = fd_src;
  if (fd_out)
    _fork._fd_out = -1; // caller will take care of closing the file!

  // Now we can read the data...

  // Cleaning up follows in the close function

  // Note: the caller is responsible for closing the file
  // handle.  We're responsible for reaping the child process.

  // Note: the file must be closed BEFORE we reap the child, since
  // otherwise we may wait indefinately, since the child may be
  // waiting for us to read.  But if we close, it'll get a deadly
  // signal for writing to a closed file that is closed on the other end.

  char command[64];
  size_t n = 0;

  n = (size_t) snprintf (command,sizeof(command),"%s",method);
  for (int i = 0; i < 7 && args[i]; i++)
    n += (size_t) snprintf (command+n,sizeof(command)-n," %s",args[i]);

  if (filename)
    INFO(0,"Piping file through decompressor '%s'...",command);
  else
    INFO(0,"Piping input through decompressor '%s'...",command);
}

void decompressor::reap()
{
  if (!_fork._child)
    return;

  // close (pipe_out[0]); // by caller

  // Now lets get the exit status of the child

  // First ask him to die, if he did not already (this is
  // otherwise a beautiful way to lock us up, if we abort before
  // finishing reading for some reason)

  _fork.wait(true);

  // So child is dead, pipes are closed, we got the information.
  // Yippie!
}

void *decompress_relay_thread(void *info)
{
  // We could play around with splice(2), but if tee(2) did not work
  // (exist), then splice is also unheard of, so...  Directly do
  // read()/write() fallback.

  // Still, the memory management systems really like when one
  // operates with page-aligned buffers, so do that.  And also do the
  // calls page-aligned, i.e. correct for the magic inserted at the
  // beginning.

  drt_info *drt = (drt_info *) info;

#define DRT_BUF_SIZE (64 * 1024) // must be power of 2

  size_t page_size = (size_t) sysconf(_SC_PAGESIZE);
  size_t alloc = DRT_BUF_SIZE;

  alloc += page_size;

  char *buffer = (char *) malloc(alloc);
  char *aligned =
    (char *) ((((size_t) buffer) + page_size-1) & ~(page_size - 1));

  memcpy (aligned,drt->_magic,PEEK_MAGIC_BYTES);

  size_t available = PEEK_MAGIC_BYTES;
  size_t consumed = 0;
  /*
  for (size_t i = 0; i < available; i++)
    {
      printf ("%02x ",(int) (unsigned char) aligned[i]);
    }
  printf ("\n");
  */
  bool reached_eof = false;

  for ( ; !reached_eof || consumed != available; )
    {
      // First try to read the buffer full

      size_t toread = DRT_BUF_SIZE - (available - consumed);
      size_t offset = available & (DRT_BUF_SIZE - 1);
      size_t linear_left = DRT_BUF_SIZE - offset;

      if (linear_left < toread)
	toread = linear_left;

      if (!reached_eof && toread)
	{
	  ssize_t n = read(drt->_fd_read,aligned + offset,toread);

	  if (n == 0)
	    {
	      reached_eof = true;
	    }
	  if (n == -1)
	    {
	      if (errno == EINTR)
		n = 0;
	      else
		{
		  perror("read()");
		  exit(1);
		}
	    }
	  /*
	  printf ("read(%d) off:%d len:%d -> %d\n",
		  drt->_fd_read,(int) offset,(int) linear_left,(int) n);
	  */
	  available += (size_t) n;
	}

      size_t towrite = available - consumed;
      offset = consumed & (DRT_BUF_SIZE - 1);
      linear_left = DRT_BUF_SIZE - offset;

      if (linear_left < towrite)
	towrite = linear_left;

      if (towrite)
	{
	  /*
	  for (size_t i = 0; i < linear_left; i++)
	    {
	      printf ("%02x ",(int) (unsigned char) aligned[offset+i]);
	    }
	  printf ("\n");
	  */
	  ssize_t n = write(drt->_fd_write,aligned + offset,towrite);

	  if (n == -1)
	    {
	      if (errno == EINTR)
		n = 0;
	      else if (errno == EPIPE)
		{
		  // Further copying makes no sense.  Consumer died on us.
		  break;
		}
	      else
		{
		  perror("write()");
		  exit(1);
		}
	    }
	  /*
	  printf ("write(%d) off:%d len:%d -> %d\n",
		  drt->_fd_write,(int) offset,(int) towrite,(int) n);
	  */
	  consumed += (size_t) n;
	}
    }

  // We are done copying data.

  free(buffer);

  close(drt->_fd_write); // There will be no more data written.

  return NULL;
}

struct decompress_magic_cmd_args_t
{
  struct
  {
    unsigned char _len;
    unsigned char _b[4];
  } _magic;
  const char *_cmd;
  const char *_args[3];
} decompress_magic_cmd_args[] = {
  { { 3, { 'B',  'Z',  'h',  0    } } , "bunzip2", { "-c",NULL } },
  { { 2, { 0x1f, 0x8b, 0,    0    } } , "gunzip",  { "-c",NULL } },
  { { 4, { '7',  'z',  0xbc, 0xaf } } , "7za",     { "e","-so",NULL } },
  // FF 4C 5A 4D 41 00 = 0xff"LZMA"0x00
  { { 4, { 0xff, 'L',  'Z',  'M'  } } , "lzma",    { "-d","-c",NULL } },
  { { 4, { 0x89, 'L',  'Z',  'O'  } } , "lzop",    { "-d","-c",NULL } },
  // FD 37 7a 58 5a 00 = 0xff"7zXZ"0x00
  { { 4, { 0xfd, '7',  'z',  'X'  } } , "unxz",    { "-d","-c",NULL } },
};

/* Plan for finding out decompressing engine (if any).  Goal:
 *
 * - If no decrompressor needed - do not destroy the input.
 * - If decompressing from file, let decompressor do the file reading.
 * - If decompressing from pipe, let the decomprssor get the direct pipe.
 *
 * First we need to peek the four first bytes of the file (compressed
 * stream magic):
 *
 * - If file, just read will do.  But, we assume it is a pipe (no harm).
 * - If pipe, do a tee() on the pipe to a temporary pipe for getting
 *   the magic data.
 * - If tee fails, do the normal read.
 *
 * Then investigate magic.  If not compressed:
 *
 * - If tee() succeeded, continue reading pipe from original pipe.
 * - If file supports mmap, use that.
 * - If any (both) of the above failed, read from pipe, but first use
 *   the (non-magic) bytes stolen.
 *
 * If compressed:
 *
 * - If tee() succeeded, give away original pipe.
 * - If file, use that.
 * - If any of the above failed, we set up another pipe and first
 *   dump the stolen magic.  Then we relay the data into that pipe
 *   (using a small thread, using splice() if available, otherwise
 *   read()/write()).
 *
 */

void decompress(decompressor **handler,
		drt_info     **relay_info,
		int *fd,
		const char *filename,
		unsigned char *push_magic,size_t *push_magic_len)
{
  ssize_t n;
  bool untouched = true;

  // There is no major need to try seeking instead of tee.  If seeking
  // works, it is anyhow a file (no pipe), so relay will not be
  // needed.  If relay is needed, seeking will anyhow fail.

#ifdef HAVE_TEE
  if (lseek(*fd,0,SEEK_SET) != (off_t) -1)
#endif
    {
      // Well, the seek solution is quite easy, and avoids the
      // creation of a throw-away pipe.

      n = retry_read(*fd,push_magic,PEEK_MAGIC_BYTES);

      if (lseek(*fd,0,SEEK_SET) == (off_t) -1)
	{
#ifdef HAVE_TEE
	  WARNING("(Second) seeking failed after reading decompress magic, using fallback.");
#endif
	  untouched = false;
	}
    }
#ifdef HAVE_TEE
  else
    {
      // Creat a pipe to tee() into.

      int pipe_tee[2];

      if (pipe(pipe_tee) == -1)
	ERROR("Error creating pipe(2) for decompress magic peek.");

      if (tee(*fd,pipe_tee[1],PEEK_MAGIC_BYTES,0) != PEEK_MAGIC_BYTES)
	{
	  /*
	    WARNING("Failed to tee(2) data for decompress magic peek, "
	            "using fallback solution.");
	  */

	  n = retry_read(*fd,push_magic,PEEK_MAGIC_BYTES);
	  untouched = false;
	}
      else if ((n = retry_read(pipe_tee[0],push_magic,PEEK_MAGIC_BYTES)) !=
	       PEEK_MAGIC_BYTES)
	{
	  WARNING("Failed to read blind-pipe for decompress magic peek, "
		  "using fallback solution.");

	  n = retry_read(*fd,push_magic,PEEK_MAGIC_BYTES);
	  untouched = false;
	}
      /*
	printf ("tee pipe: %d %d\n",pipe_tee[0],pipe_tee[1]);
      */
      // Close extra pipe

      close(pipe_tee[0]);
      close(pipe_tee[1]);
    }
#endif

  if (n != PEEK_MAGIC_BYTES)
    {
      // Retrieving magic failed!
      return;
    }
  /*
  printf ("magic %02x.%02x.%02x.%02x : %d\n",
	  push_magic[0],
	  push_magic[1],
	  push_magic[2],
	  push_magic[3],
	  n);
  */
  // Investigate magic

  for (uint i = 0; i < countof(decompress_magic_cmd_args); i++)
    {
      const decompress_magic_cmd_args_t &dmca =
	decompress_magic_cmd_args[i];

      if (memcmp(push_magic,dmca._magic._b,(size_t) dmca._magic._len) == 0)
	{
	  // It seems to be a compressed file,

	  *handler = new decompressor();

	  int fd_copy = -1;
	  int fd_out = -1;

	  if (filename)
	    {
	      // We'll be reading from the real file.
	      close (*fd);
	      *fd = -1;
	    }

	  if (!filename && !untouched)
	    {
	      fd_copy = *fd;
	      *fd = -1; // will be overwritten below
	    }
	  /*
	  printf ("filename: %s untouched: %d fd_copy:%d *fd:%d\n",
		  filename,untouched,fd_copy,*fd);
	  */
	  (*handler)->start(fd,
			    dmca._cmd,dmca._args,
			    filename,
			    // get a file descriptor that we should write to
			    !filename && !untouched ? &fd_out : NULL,
			    // incoming pipe was unharmed, use it
			    !filename && untouched  ? *fd : -1);
	  /*
	  printf ("filename: %s untouched: %d fd_out:%d *fd:%d\n",
		  filename,untouched,fd_out,*fd);
	  */
	  if (!filename && !untouched)
	    {
	      // We have to copy the data from fd_copy to fd_out.
	      // fd_out will not be closed by forked_child.  Thus we
	      // need to close both ourselves.

	      *relay_info = new drt_info;

	      (*relay_info)->_fd_read  = fd_copy;
	      (*relay_info)->_fd_write = fd_out;

	      memcpy((*relay_info)->_magic,push_magic,
		     PEEK_MAGIC_BYTES);
	      /*
	      printf ("Fire of relay: %d -> %d\n",fd_copy,fd_out);
	      */
#ifndef HAVE_PTHREAD
#error pthreads is now required for UCESB, or some fixing is needed.
#error Please contact the author.
#endif
	      if (pthread_create(&(*relay_info)->_thread,NULL,
				 decompress_relay_thread,*relay_info) != 0)
		{
		  perror("pthread_create()");
		  exit(1);
		}

	      set_thread_name((*relay_info)->_thread, "RELAY", 5);
	    }

	  // !filename && untouched : fd_dest closed by forked_child
	  // (by special request in start())

	  return;
	}
    }

  // So no decompressor.  Since we anyhow need fallback handling in
  // case seeking failed, we do not attempt seeking at all.  Simply
  // give the stolen data back to whomever is after us.

  if (!untouched)
    *push_magic_len = PEEK_MAGIC_BYTES;
}

ssize_t retry_read(int fd,void *buf,size_t count)
{
  ssize_t total = 0;

  for ( ; ; )
    {
      ssize_t nread = read(fd,buf,count);

      if (!nread)
	break;

      if (nread == -1)
	{
	  if (errno == EINTR)
	    nread = 0;
	  else
	    return nread;
	}

      count -= (size_t) nread;
      total += nread;

      if (!count)
	break;
      buf = ((char*) buf)+nread;
    }

  return total;
}

size_t full_read(int fd,void *buf,size_t count,bool eof_at_start)
{
  ssize_t nread = read(fd,buf,count);

  if (nread == 0 && eof_at_start)
    return 0;

  // End of file (if read failed first time).  We can't have this check
  // inside the loop as a end of file inside the loop means we managed
  // to read a partial element.

  size_t total = 0;

  for ( ; ; )
    {
      if (nread == -1)
	{
	  if (errno == EINTR)
	    nread = 0;
	  else
	    {
	      perror("read");
	      ERROR("Error reading file.");
	    }
	}

      count -= (size_t) nread;
      total += (size_t) nread;

      if (!count)
	break;
      buf = ((char*) buf)+nread;

      nread = read(fd,buf,count);

      if (!nread)
	ERROR("Reached unexpected end of file");
    }

  return total;
}





data_input_source::data_input_source()
{
  _remote_cat = NULL;
  _decompressor = NULL;
  _relay_info = NULL;
}

#ifdef USE_LMD_INPUT
#include "lmd_input.hh"
#include "lmd_input_tcp.hh"

void data_input_source::connect(const char *name,int type
#ifdef USE_PTHREAD
				,thread_block *block_reader
#endif
#ifdef USE_THREADING
				,thread_block *blocked_next_file
				,int           wakeup_next_file
#endif
				)
{
  assert(!_input._input); // want to get rid of the close below
  close();

  // depending on the type of input, get ourselves the appropriate
  // input style

  lmd_input_tcp *server = NULL;

  switch (type)
    {
    case LMD_INPUT_TYPE_EVENT:
      server = new lmd_input_tcp_event();

      break;
    case LMD_INPUT_TYPE_STREAM:
      server = new lmd_input_tcp_stream();

      break;
    case LMD_INPUT_TYPE_TRANS:
      server = new lmd_input_tcp_transport();

      break;
    default:
      assert(false);
      return;
    }

  size_t minsize = server->connect(name);

  // ok, so we have some kind of connection.  (actually, this part may
  // have waited for some time to get initial info from the server.
  // as well, as errors from that won't bother us)

  // set up a tcp_pipe_buffer to handle the data transport

  tcp_pipe_buffer *tpb = new tcp_pipe_buffer();

  TDBG("attempting tcp pipe %p",tpb);

  // We also need to push any magic data that we read from the input file
  // in case it could not seek back to the beginning

#if USE_THREADING
  tpb->set_next_file(blocked_next_file,wakeup_next_file);
#endif

  size_t prefetch_size = get_prefetch_size();

  // Three times the size required.  We need twice to handle events
  // fragmented between streams (ucesb generated).  And then another
  // one to decouple the reading from the unpacking.

  while (prefetch_size < minsize * 3)
    prefetch_size *= 2;

  INFO(0,"Server data: %zd kiB chunks; prefetch buffer: %zd MiB.",
       minsize >> 10, prefetch_size >> 20);

  tpb->init(server,prefetch_size
#ifdef USE_PTHREAD
	   ,block_reader
#endif
	   );

  tpb->set_filename(name);

  _input._input = tpb;
  _input._cur   = 0;
}
#endif



void data_input_source::open(const char *filename
#ifdef USE_PTHREAD
			     ,thread_block *block_reader
#endif
#ifdef USE_THREADING
			     ,thread_block *blocked_next_file
			     ,int           wakeup_next_file
#endif
			     ,bool no_mmap
			     ,bool rfioinput
			     ,bool srminput)
{
  assert(!_input._input); // want to get rid of the close below
  close();

  // if name is a dash "-" or a dash followed by digits "-n" then we
  // read input from that file descriptor.  As we're then reading from
  // a pipe, don't even try to open using a decompressor, as it will
  // not be able to work (we use them using direct filenames, not
  // piping into them, which of course ne could do, but it's too much
  // pain) It's harmless to try mmaping, altough it won't work... :)

  const char *decompress_filename = filename;

  int fd = -1;

  if (rfioinput)
    {
#ifdef USE_RFIO
      // We need to fork a "../rfiocmd/rfcat FILENAME" and then get the output
      // from that pipe.

      _remote_cat = new forked_child;

      char *cattool = argv0_replace(RFCAT_PREFIX "rfcat");

      const char *argv[3];

      argv[0] = cattool;
      argv[1] = filename;
      argv[2] = NULL;

      _remote_cat->fork(cattool,argv,&fd,NULL);
      _remote_cat->_fd_in = -1; // caller will take care of closing the file!

      INFO(0,"Opened remote RFIO input using %s from %s, process...",
	   cattool,filename);

      free(cattool);

      decompress_filename = NULL;
#else
      assert(false);
#endif
    }
  else if (srminput)
    {
      // We need to fork a "arccp FILENAME -" and then get the output
      // from that pipe.

      _remote_cat = new forked_child;

      const char *cattool = "arccp";

      const char *argv[4];

      argv[0] = cattool;
      argv[1] = filename;
      argv[2] = "-";
      argv[3] = NULL;

      _remote_cat->fork(cattool,argv,&fd,NULL);
      _remote_cat->_fd_in = -1; // caller will take care of closing the file!

      INFO(0,"Opened remote SRM input using %s from %s, process...",
	   cattool,filename);

      decompress_filename = NULL;
    }
  else
    {
      if (filename[0] == '-')
	{
	  if (filename[1] == '\0')
	    fd = STDIN_FILENO;
	  else if(isdigit(filename[1]))
	    fd = atoi(filename+1);

	  if (fd != -1)
	    {
	      // Try to read 0 bytes from the file descriptor, just to see
	      // that it is valid

	      char dummy;
	      ssize_t ret;

	      ret = read(fd,&dummy,0);

	      // don't care about EINTR, which would be the only valid
	      // reason to retry...

	      if (ret != 0)
		{
		  perror("read");
		  ERROR("Attempt to read from file descriptor %d failed.",fd);
		}

	      INFO(0,"Opened input from file descriptor %d, process...",fd);

	      decompress_filename = NULL; // we are a pipe
	    }
	}

      if (fd == -1)
	{
	  fd = ::open(filename,O_RDONLY);

	  if (fd == -1)
	    {
	      perror("open");
	      ERROR("Error opening file '%s'.",filename);
	    }
	  INFO(0,"Opened input from file '%s', process...",filename);
	}
    }

  unsigned char push_magic[PEEK_MAGIC_BYTES];
  size_t push_magic_len = 0;

  // See it it was an compressed file, if so, run the
  // decompressor as a separate process

  decompress(&_decompressor,&_relay_info,&fd,decompress_filename,
	     push_magic,&push_magic_len);

  // Ok, whatever happened, _fd is still a file-handle to a file that
  // we want to read.  Either it is now pointing to a pipe from a
  // child, or it already from the beginning pointed to a pipe
  // (e.g. mkfifo) or it points to a file.  We'd prefer to read the
  // file using mmap if possible

  // mmap is not exactly working for pipes, so do not even try that

  if (!_decompressor && !no_mmap)
    {
      file_mmap *mm = new file_mmap();

#if USE_THREADING
      mm->set_next_file(blocked_next_file,wakeup_next_file);
#endif

      mm->init(fd);

      // Try to see if mmap()ing works, if not, we need to use some
      // other method.

      TDBG("attempting mmap %p",mm);
      /*
      fprintf (stderr,"attempting mmap %p\n",mm);
      */
      buf_chunk chunks[2];

      if (mm->map_range(0,0x1000,chunks) != 0)
	{
	/*
	  for (int i = 0; i < (1<<30); i += 0x1000)
	    {
	      // printf ("release -> %08x\n",i);
	      mm->release_to(i);
	      // printf ("map -> %08x .. %d\n",i,0x1000);
	      if (mm->map_range(i,i+0x1000,chunks) == 0)
		{
		  printf ("ended at %d\n",i);
		  break;
		}
	    }
	  goto fail_mmap;
	*/


	  mm->set_filename(filename);

	  _input._input = mm;
	  _input._cur   = 0;
	}
      else
	{
	fail_mmap:
	  mm->_fd = -1; // do not let the mmap destroy our input file descriptor
	  // mmap()ing failed.  We need to buffer the data ourselves.
	  delete mm;
	}
      /*
      fprintf (stderr,"done mmap %p\n",mm);
      */
  }

 opened_pipe:

  if (!_input._input)
    {
      // We need to read the file normally, but will do our own
      // buffering

      pipe_buffer *pb = new pipe_buffer();

      TDBG("attempting pipe %p",pb);

      // We also need to push any magic data that we read from the input file
      // in case it could not seek back to the beginning

#if USE_THREADING
      pb->set_next_file(blocked_next_file,wakeup_next_file);
#endif
      /*
      printf ("magic %02x.%02x.%02x.%02x : %d\n",
	      push_magic[0],
	      push_magic[1],
	      push_magic[2],
	      push_magic[3],
	      (int) push_magic_len);
      */

      size_t prefetch_size = get_prefetch_size();

      pb->init(fd,push_magic,push_magic_len,
	       prefetch_size
#ifdef USE_PTHREAD
	       ,block_reader
#endif
	       );

      pb->set_filename(filename);

      _input._input = pb;
      _input._cur   = 0;
    }

  // responsibility for fd is no longer ours (but one of the
  // _decompressor, and/or then of _input._input)

  return;
}

#if 0
/* This code is no longer used, as we use the external 'rfcat' process.
 * It is left for the moment.  However, it is strongly suggested that
 * special external reading is performed with external processes if some
 * external library need to be linked to do the communication.
 */
#ifdef USE_RFIO
void data_input_source::open_rfio(const char *filename
#ifdef USE_PTHREAD
				  ,thread_block *block_reader
#endif
#ifdef USE_THREADING
				  ,thread_block *blocked_next_file
				  ,int           wakeup_next_file
#endif
				  )
{
  assert(!_input._input); // want to get rid of the close below
  close();

  // We first open the file for query only to check it's media status.
  // If we'd open if for real, the tape robot would immediately start to
  // stage it, which is what we'd like to avoid.  (We refuse reading
  // files that are not staged)

  int fd = rfio_file_open(filename);

  // No need checking media again.  'Damage' (i.e. possible staging)
  // has already been done.

  INFO(0,"Opened input from rfio file '%s', process...",filename);

  rfio_pipe_buffer *rpb = new rfio_pipe_buffer();

  TDBG("attempting rfio pipe %p",pb);

#if USE_THREADING
  rpb->set_next_file(blocked_next_file,wakeup_next_file);
#endif

  size_t prefetch_size = get_prefetch_size();
  
  rpb->init(fd/*,magic*/,prefetch_size
#ifdef USE_PTHREAD
	   ,block_reader
#endif
	   );

  rpb->set_filename(filename);

  _input._input = rpb;
  _input._cur   = 0;

  return;
}
#endif
#endif

void data_input_source::close()
{
  bool boom = false;

  // Shut buffer down before we close the file (pipe)

  // if (_decompressor)
  //   _decompressor->join();

  try {
    _input.close();
  } catch (error &e) {
    boom = true;
  }

  /*
  if (_fd != -1)
    {
      if (::close(_fd) == -1)
	perror("close");
    }
  _fd = -1;
  */

  // Shut decompressor down (wait for child process after pipe is closed)

  if (_decompressor)
    {
      try {
	_decompressor->reap();
      } catch (error &e) {
	boom = true;
      }

      delete _decompressor;
      _decompressor = NULL;
    }

  // We try to reap the relay_thread after killing the decompressor.
  // That way (unless it already got the sigpipe or died itself due to
  // writing all information), we need not specially make it exit.

  if (_relay_info)
    {
      // We had an relay_thread

      if (pthread_join(_relay_info->_thread,NULL) != 0)
	{
	  perror("pthread_join()");
	  exit(1);
	}

      // The write pipe has already been closed by the relay thread.

      ::close (_relay_info->_fd_read);

      delete _relay_info;

      _relay_info = NULL;
    }

  // And then reap any remote cat tool

  if (_remote_cat)
    {
      try {
	_remote_cat->wait(true);
      } catch (error &e) {
	boom = true;
      }

      delete _remote_cat;
      _remote_cat = NULL;
    }

  try {
    _input.destroy();
  } catch (error &e) {
    boom = true;
  }
  if (boom)
    throw error();
}

void data_input_source::take_over(data_input_source &src)
{
  _decompressor = src._decompressor;
  src._decompressor = NULL;
  _remote_cat = src._remote_cat;
  src._remote_cat = NULL;
  _relay_info = src._relay_info;
  src._relay_info = NULL;

  _input.take_over(src._input);
}
