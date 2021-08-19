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

#include "lmd_output.hh"

#include "error.hh"
#include "colourtext.hh"

#include "endian.hh"
#include "event_loop.hh"
#include "parse_util.hh"

#include "../common/strndup.hh"

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/time.h>
#include <time.h>

// Write LMD files.
//
// Due to some (mucho-monumental) brain-damages in the way the LMD
// files are handled (mainly the lack of an endianess marker for the
// data itself, we need to make some precautions.  (The endianess of
// the headers is known, and further it is assumed that that buffer
// has been written in the same endianess.  So far so good.  Trouble
// comes as certain programs 'transporting' these buffers take upon
// themselves to swap them into their native byte order and then let
// them continue.  Which is a disaster, since the byteswapping is on
// 32-bit boundaries.  16-bit data will be scrambled.  64-bit data
// would also be cooked.
//
// This program will _not_ be doing any 32-bit aligned byte-swapping
// unless we know what data we operate on.  Therefore, any data that
// is output will be output in the same order we got it.  To make that
// possible, we'll (if for any reason our input data changed endianess
// between events), create a new buffer in the new endianess.
//
// Second issue: since it seems that it is not supported to put event
// data into the file header buffer (although all necessary fields are
// there), we'll avoid doing that.  Now this would not be such a
// problem, if it had not been for the fact that some programs
// (e.g. eventapi) assumes all buffers to have the same size.  So the
// file header buffer must be as large as the following ones, so
// wasting space.  Another similar problem is then that we cannot trim
// the last buffer output...

#define min(a,b) ((a)<(b)?(a):(b))

lmd_output_buffered::lmd_output_buffered()
{
  _cur_buf_start = NULL;
  _cur_buf_length = 0;

  _cur_buf_left = 0;

  _buffer_header.l_buf = 1;
  _last_bufno = _buffer_header.l_buf;
  _last_replays = _replays = 0;

  _write_native = true;
  _compact_buffers = false; // to be compatible with eventapi, who
			    // cannot handle last buffer chopped
}

lmd_output_file::lmd_output_file()
{
  _fd_handle = -1;
  _fd_write = -1;

  _cur_filename = NULL;
  _lmd_log = NULL;

  _event_cut = 0;
  _last_eventno = (uint32) -1;

  _write_protect = false;

  // 64 k ( fragmentation no good...) (I could have used 128 k buffers
  // (and saved some headers, but MBS format for no good reason uses
  // _signed_ short integers to store lengths) (has since been fixed
  // to use full 32-bit lengths, but as default use a safe size)
  _buf_size = 0x10000;

  _has_file_header = false;

  _compression_level = 6;
}


lmd_output_file::~lmd_output_file()
{
  assert(_fd_handle == -1); // we should not have to have the close_file below
  close_file(); // must be before we free the buffers

  delete[] _cur_filename;
  delete _lmd_log;
}

void lmd_output_file::close()
{
  close_file();
}

void lmd_output_file::close_file()
{
  bool boom = false;

  // close the file if open, and if we're responsible
  // (we're not responsible if it is stdin)

  // write the last buffer

  // INFO(0,"Writing last buffer.");

  try {
    send_buffer();
  } catch (error &e) {
    boom = true;
  }

  try {
    // First wait for the compressor to finish (if there)
    _compressor.wait(false);
  } catch (error &e) {
    boom = true;
  }

  // Then close the output
  if (_fd_handle != -1)
    {
      if (_write_protect)
	{
	  struct stat st;
	  if (fstat(_fd_handle,&st) != 0)
	    perror("fstat");
	  else if (fchmod(_fd_handle,
			  (st.st_mode &
			   ~(mode_t) (S_IWUSR | S_IWGRP | S_IWOTH))) != 0)
	    perror("fchmod");
	}
      if (::close(_fd_handle) != 0)
	perror("close");

      report_open_close(false);
    }
  _fd_handle = -1;
  _fd_write = -1;

  free (_cur_buf_start);
  _cur_buf_start = NULL;

  _cur_buf_left = 0;

  delete[] _cur_filename;
  _cur_filename = NULL;

  if (boom)
    throw error();
}

uint32 parse_compression_level(const char* post) {
  char* end;
  uint32 level = (uint32) strtol(post, &end, 10);

  if (*end != 0)
    ERROR("compression level request malformed: %s", post);

  if (level > 9)
    ERROR("compression level to large - max is 9: %s", post);

  if (level < 1)
    ERROR("compression level to small - min is 1: %s", post);

  return level;
}

void lmd_out_common_options()
{
  printf ("native              Native byte order (this machine).\n");
  printf ("big                 Big endian byte order.\n");
  printf ("little              Little endian byte order.\n");
  printf ("incl=               (Sub)event inclusion; name or tag list: tag1=N[:tag2=N...]\n");
  printf ("                    Subev tags: type,subtype,[proc]id,[sub]crate,ctrl|control\n");
  printf ("                    Event tags: trig,wr_id\n");
  printf ("excl=               Subevent exclusion.  (See incl= above.)\n");
  printf ("skipempty           Skip events with no subevents.\n");
}

void lmd_out_file_usage()
{
  printf ("\n");
  printf ("LMD file output (--output) options:\n");
  printf ("\n");
  lmd_out_common_options();
  printf ("compact             Minimize buffer padding.\n");
  printf ("bufsize=N           Buffer size [ki|Mi|Gi].\n");
  printf ("size=N              File size limit [ki|Mi|Gi].\n");
  printf ("events=N            Event limit [k|M].\n");
  printf ("eventcut=N          Limit events / file [k|M].\n");
  printf ("clevel=N            Compression level 1-9. Default 6.\n");
  printf ("newnum              Avoid using existing file numbers.\n");
  printf ("wp                  Write protect file.\n");

  printf ("\n");
}

#define MATCH_C_PREFIX(prefix,post) (strncmp(request,prefix,strlen(prefix)) == 0 && *(post = request + strlen(prefix)) != '\0')
#define MATCH_C_ARG(name) (strcmp(request,name) == 0)

bool parse_lmd_out_common(char *request,
			  bool allow_selections,
			  bool allow_compact,
			  lmd_output_buffered *out)
{
  char *post;

  if (MATCH_C_ARG("native"))
    out->_write_native = true;
  else if (MATCH_C_ARG("big"))
    out->_write_native = (__BYTE_ORDER == __BIG_ENDIAN);
  else if (MATCH_C_ARG("little"))
    out->_write_native = (__BYTE_ORDER == __LITTLE_ENDIAN);
  else if (allow_compact && MATCH_C_ARG("compact"))
    out->_compact_buffers = true;
  else if (allow_selections && MATCH_C_PREFIX("incl=",post))
    out->_select.parse_request(post,true);
  else if (allow_selections && MATCH_C_PREFIX("excl=",post))
    out->_select.parse_request(post,false);
  else if (MATCH_C_ARG("skipempty"))
    out->_select._omit_empty_payload = true;
  else
    return false;

  return true;
}

lmd_output_file *parse_open_lmd_file(const char *command, bool allow_selections)
{
  lmd_output_file *out_file = new lmd_output_file();

  // chop off any options of the filename
  // native, net, big, little
  // compact

  const char *req_end;

  while ((req_end = strchr(command,',')) != NULL)
    {
      char *request = strndup(command,(size_t) (req_end-command));
      char *post;

      if (parse_lmd_out_common(request, allow_selections, true, out_file))
	;
      else if (MATCH_C_ARG("help"))
	{
	  lmd_out_file_usage();
	  exit(0);
	}
      else if (MATCH_C_PREFIX("bufsize=",post))
	{
	  out_file->_buf_size = (uint32) parse_size_postfix(post,"kMG","BufSize",true);
	  if (out_file->_buf_size % 1024)
	    ERROR("Buffer size (%d) must be a multuple of 1024.",out_file->_buf_size);
	}
      else if (MATCH_C_PREFIX("size=",post))
	out_file->_limit_size = parse_size_postfix(post,"kMG","Size",false);
      else if (MATCH_C_PREFIX("events=",post))
	out_file->_limit_events = (uint32) parse_size_postfix(post,"kM","Events",true);
      else if (MATCH_C_PREFIX("eventcut=",post))
	out_file->_event_cut = (uint32) parse_size_postfix(post,"kM","Eventcut",true);
      else if (MATCH_C_PREFIX("clevel=",post))
	out_file->_compression_level = parse_compression_level(post);
      else if (MATCH_C_ARG("newnum"))
	out_file->_avoid_used_number = true;
      else if (MATCH_C_ARG("wp"))
	out_file->_write_protect = true;
      else if (allow_selections && MATCH_C_ARG("log"))
	out_file->_lmd_log = new logfile("ucesb.log");
      else if (allow_selections && MATCH_C_PREFIX("log=",post))
	out_file->_lmd_log = new logfile(post);
      else
	ERROR("Unrecognised option for LMD output: %s",request);

      free(request);
      command = req_end+1;
    }

  if (strcmp(command,"help") == 0)
    {
      lmd_out_file_usage();
      exit(0);
    }

  const char *filename = command;

  if (strcmp(filename,"-") == 0)
    out_file->open_stdout();
  else if (out_file->_limit_size != (uint64) -1 ||
      out_file->_limit_events != (uint32) -1 ||
      out_file->_event_cut != 0 ||
      out_file->_avoid_used_number)
    out_file->parse_open_first_file(filename);
  else
    out_file->open_file(filename);

  return out_file;
}


void lmd_output_file::new_file(const char *filename)
{
  close_file();
  open_file(filename);

  if (_has_file_header)
    write_file_header(&_file_header_extra);

  _sticky_store.write_events(this);
}

void lmd_output_file::open_stdout()
{
  // Write the output to stdout...

  if (_limit_size != (uint64) -1 ||
      _limit_events != (uint32) -1||
      _event_cut)
    ERROR("Cannot write data to stdout with file size or "
	  "event number limits.");

  // We currently have the following output file descriptors open
  // STDOUT_FILENO (1)  for stdout
  // STDERR_FILENO (2)  for stderr
  //
  // And with each of them associated (FILE *) handles that are used
  // by printf etc for writing to stdout and stderr

  // Now, we would like to write our data to the file to which
  // descriptor number 1 points right now, but any printf- output
  // that will go to the associated (FILE *) we would like to go
  // to stderr instead.  Since we cannot reassociate a file handle
  // (in this case stdout) with another file descriptor, we need
  // to do some dup juggling.

  // Side-effects: anyone writing to file descriptor 2 in the
  // future, will mess up the output data file.  Stuff that goes
  // via stdio file handle will be fine.  Since both stdout and
  // stderr file handle buffered data will go to descriptor 2, it
  // may look like a mess there.  Not worse than what it normally
  // looks on the screen though...

  // Make a copy of STDOUT_FILENO so we can write to the output file
  // in the pipeline

  if ((_fd_handle = dup(STDOUT_FILENO)) == -1)
    {
      perror("dup");
      ERROR("Failed to duplicate STDOUT.");
    }

  // The duplicate STDERR to also be used by stdout

  if (dup2(STDERR_FILENO,STDOUT_FILENO) == -1)
    {
      perror("dup2");
      ERROR("Failed to duplicate STDERR to STDOUT.");
    }

  // Re-evaluate terminals
  colourtext_prepare();

  _fd_write = _fd_handle;

  _may_change_file = false; // To avoid closing of our one output.
}


void lmd_output_file::open_file(const char* filename)
{
  assert(_fd_handle == -1); // we should not have the close_file below
  close_file();

  if ((_fd_handle = open(filename,
			 O_WRONLY | O_CREAT | O_TRUNC
#ifdef O_LARGEFILE
			 | O_LARGEFILE
#endif
			 ,
			 S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) == -1)
    {
      perror("open");
      ERROR("Failed to open file '%s' for writing.",filename);
    }
  INFO(0,"Opened output file '%s'",filename);

  assert (!_cur_filename);
  _cur_filename = new char[strlen(filename)+1];
  strcpy(_cur_filename, filename);

  report_open_close(true);

  char level[10];
  snprintf(level, 10, "-%d", _compression_level);


  const char *argv_gzip[3] = { "gzip", level, NULL };
  const char *argv_bzip2[3] = { "bzip2", level, NULL };
  const char *argv_xz[3] = { "xz", level, NULL };
  const char **argv = NULL;

  size_t n = strlen(filename);

  if (n > 3 && strcmp(filename+n-3,".gz") == 0)
    argv = argv_gzip;
  else if (n > 4 && strcmp(filename+n-4,".bz2") == 0)
    argv = argv_bzip2;
  else if (n > 3 && strcmp(filename+n-3,".xz") == 0)
    argv = argv_xz;

  if (argv)
    {
      _compressor.fork(argv[0],argv,NULL,&_fd_write,_fd_handle,-1,-1,NULL,true);
      INFO(0,"Piping output through compressor '%s %s'",argv[0],argv[1]);
    }
  else
    _fd_write = _fd_handle;
}



void lmd_output_file::write_buffer(size_t count, bool has_sticky)
{
  UNUSED(has_sticky);
  
  full_write(_fd_write,_cur_buf_start,count);

  add_size(count);

  _total_written += count;
}

void lmd_output_file::get_buffer()
{
  if (!_cur_buf_start)
    {
      // allocate a buffer

      if (!_cur_buf_length)
	_cur_buf_length = _buf_size; // 64 k ( fragmentation no good...) (I
				     // could have used 128 k buffers (and
				     // saved some headers, but MBS format
				     // for no good reason uses _signed_
				     // short integers to store lengths)

      assert (_cur_buf_length > sizeof (_buffer_header)); // or no space for buffer header

      if (!(_cur_buf_start = (uchar*) malloc (_cur_buf_length)))
	ERROR("Memory allocation failed!");

      //printf ("_file_buffer : %8p .. %8p\n",_file_buffer,_file_buffer+_file_buffer_size);
    }

  _stream_left = _stream_left_max = INT_MAX;
}




void lmd_output_buffered::mark_close_buffer()
{
  assert(_buffer_header.l_evt == 0);
  _buffer_header.l_evt = (uint32_t) -1;
}

void lmd_output_buffered::send_buffer(size_t lie_about_used_when_large_dlen)
{
  // Write the current buffer to file.
  // It's really very simple.  In principle nothing can go wrong.

  // We are called when there is no buffer available (first call).  or
  // when it's full, or when someone wants to write it to disk anyway
  // (e.g. on close).  Note that we should only be called if there will
  // be additions to the new buffer created, or the file will be
  // closed/released.  Otherwise we'll write an unfilled buffer and
  // create a new one (unnecessarily, but still a proper buffer.

  // first: is there anything to write, then write it

  if (!_cur_buf_start)
    return;

  // se if we can shrink the buffer (it must always be a factor of two,
  // but we might be able to shrink it)

  // fixup: do not mess up _buffer_size, instead copy value
  // to buffer header and mess with that one.
  // this way any following buffers will not be affected by this
  // gymnastics

  size_t count = _cur_buf_length;

  if (_compact_buffers)
    {
      size_t too_much = (_cur_buf_left / 1024) * 1024;

      count -= too_much;

      if (too_much)
	INFO(0,"Decreasing buffer size by: %d",(int) too_much);
    }

  // now we're ready to write!

  // first byte swap the buffer header, then write

  _buffer_header.l_dlen = (sint32) DLEN_FROM_BUFFER_SIZE(count);

  size_t used_size = _cur_buf_usable - _cur_buf_left;

  if (lie_about_used_when_large_dlen)
    {
      size_t max_count = BUFFER_SIZE_FROM_DLEN(LMD_BUF_HEADER_MAX_IUSED_DLEN);

      if (count > max_count)
	{
	  used_size = lie_about_used_when_large_dlen;

	  INFO(0,"Writing file header used size: %zd+%zd "
	       "(instead of buffer %zd).",
	       sizeof (s_bufhe_host), used_size, count);

	  count = used_size + sizeof (s_bufhe_host);

	  if (count > max_count)
	    ERROR("Cannot reduce written buffer size (%zd) to max dlen (%zd).",
		  count, max_count);
	}
    }

  _buffer_header.l_free[2] =
    (sint32) IUSED_FROM_BUFFER_USED(used_size);

  if (count <= BUFFER_SIZE_FROM_DLEN(LMD_BUF_HEADER_MAX_IUSED_DLEN))
    _buffer_header.i_used = (sint16) _buffer_header.l_free[2];

  /*
    printf ("dlen: %04x used: %4x  count:%4x  usable:%4x  left: %4x\n",
    _buffer_header.l_dlen,
    _buffer_header.i_used,
    count,
    _cur_buf_usable,
    _cur_buf_left);
  */
  // INFO(0,"%d from %d-%d=%d.",_buffer_header.i_used,_cur_buf_usable,_cur_buf_left,_cur_buf_usable - _cur_buf_left);

  // Make sure the buffer header ends up in network order
  // I.e., we need to swap if we are not big endian

  bool has_sticky =
    (_buffer_header.i_type    == LMD_BUF_HEADER_HAS_STICKY_TYPE &&
     _buffer_header.i_subtype == LMD_BUF_HEADER_HAS_STICKY_SUBTYPE);

  if (!_write_native)
    byteswap_32(_buffer_header);

  memcpy(_cur_buf_start,&_buffer_header,sizeof(_buffer_header));

  // make sure any unused space is full of zeros

  memset(_cur_buf_ptr,0,_cur_buf_left);

  // INFO(0,"Writing %d bytes.",count);

  // printf ("write (%8x, %d)\n",count,_cur_buf_left);

  write_buffer(count, has_sticky);
}

void lmd_output_buffered::new_buffer(size_t lie_about_used_when_large_dlen)
{
  int old_buffer_no = _buffer_header.l_buf;

  send_buffer(lie_about_used_when_large_dlen);

  // create a new empty buffer!, of size _buffer_size
  // it is not allowed to change _buffer_size outside this function
  // and assume that the size of the buffer will be changed!

  get_buffer();

  // fill in the buffer header with some reasonable values

  // _cur_buffer_header = (s_bufhe_host*) _buffer;

  _cur_buf_ptr  = _cur_buf_start  + sizeof (_buffer_header);
  _cur_buf_left = _cur_buf_length - sizeof (_buffer_header);

  memset (&_buffer_header,0,sizeof (_buffer_header));

  _buffer_header.l_buf = old_buffer_no + 1; // this will give continuity errors when reading several files, that were created independently... (reader should not care about troubles when recieving a file header)

  // marker for swapping order
  // network order 0x0000001 : data written in network (big endian order) (scrambled?)
  // network order 0x0100000 : data written little endian order           (scrambled?)

  // For lmdx files:
  // network order 0x0000003 : headers written in network order
  //                           data written in host order (host == big endian)
  // network order 0x0200001 : headers written in network order
  //                           data written in host order (host == little endian)
  // so,           0x0000002 : marker for data endianess
  // hmmm....

  /*
  //#if BYTE_ORDER == BIG_ENDIAN
  _buffer_header.l_free[0] = 0x00000003; // We actually write all data network endian (actually as bytes)
  //#else
  //_buffer_header.l_free[0] = 0x02000001;
  //#endif
  */

  _buffer_header.l_free[0] = 0x00000001;

  // set the current time (of current file writing)

  struct timeval tv;

  gettimeofday(&tv,NULL);

  _buffer_header.l_time[0] = (sint32) tv.tv_sec;
  _buffer_header.l_time[1] = (sint32) (tv.tv_usec / 1000);

  // mark it as buffer header

  _buffer_header.i_type    = LMD_BUF_HEADER_10_1_TYPE;
  _buffer_header.i_subtype = LMD_BUF_HEADER_10_1_SUBTYPE;

  _cur_buf_usable = _cur_buf_left;

  //printf ("_cur_buf_ptr : %8p .. %8p\n",_cur_buf_ptr,_cur_buf_ptr+_cur_buf_left);

  // We intend to write this much more data to the file...

  ///////

  if (do_sticky_replay())
    {
#if DEBUG_REPLAYS
      printf ("Sticky replay (buf %d).\n",
	      _buffer_header.l_buf);
#endif
      _replays++;
      
      // Before we continue with normal operation, we can (and shall)
      // now inject the sticky events needed for replay.

      mark_replay_stream(true);
      
      _sticky_store.write_events(this);

      // Make sure the entire stream is ejected!

      while (_stream_left != _stream_left_max)
	new_buffer();

      mark_replay_stream(false);      
    }
}

void copy_to_buffer(void *dest, const void *src,
		    size_t length, bool swap_32)
{
  if (swap_32)
    {
      assert(!(length & 3));

      uint32 *d32 = (uint32 *) dest;
      uint32 *s32  = (uint32 *) src;

      for (size_t i = length / 4; i; --i) {
        uint32 tmp = *(s32++);
	*(d32++) = bswap_32(tmp);
      }
    }
  else
    memcpy(dest,src,length);
}

void lmd_output_buffered::copy_to_buffer(const void *data,
					 size_t length,bool swap_32)
{
  assert(_cur_buf_left >= length);

  ::copy_to_buffer(_cur_buf_ptr, data, length, swap_32);

  _cur_buf_ptr += length;
  _cur_buf_left -= length;
  _stream_left -= length;
}

void lmd_output_buffered::write_event(const lmd_event_out *event,
				      bool sticky_replay,
				      bool discard_revoke)
{
  if (!sticky_replay && // do not add from the replay
      event->_header.i_type == LMD_EVENT_STICKY_TYPE &&
      event->_header.i_subtype == LMD_EVENT_STICKY_SUBTYPE)
    _sticky_store.insert(event, discard_revoke);
  
  // We're only dealing with the topmost layer of event data, i.e. the
  // _header and the chunks.  Since these are always valid (after
  // having successfully retrieved an event, we cannot fail).  (we may
  // of course emit an (internally) broken event, but that's another
  // story)

  // get the total size of the event (data)

  size_t event_data_length = event->get_length();

  /*
  printf ("*** WRITE: %zd chunks, %zd bytes ***\n",
	  event->_chunk_end - event->_chunk_start,
	  event_data_length);
  */

  // so write an event to file.

  // we always write fragments if possible.  As a reader by spec should
  // be able to deal with them, we may as well use them and reduce the
  // I/O (by not emitting so much junk at the end of buffers) (in
  // principle it's easy to deactivate it...)

  // the event should be in the _event object

  // first of all, flush the output buffer if the space left is less
  // than an event header! (which can not be divided into fragements!

  if (_cur_buf_left <= sizeof (lmd_event_10_1_host))
    {
      // check with <=, as there is no need to fragment anything without also
      // writing some data, as the event will need a new header in the next
      // buffer anyway

      new_buffer();
    }

  // so, we should eject a header and as much data as can be fitted into
  // the buffer, and while the buffer is not large enough, write buffers
  // and fragment

  // if we need to fragment, the real event length should be stored in
  // _buffer_header.l_free[1], and _event._header.l_dlen should
  // contain how much of the event is in this buffer.  Any buffer
  // containing a beginning of an event (an unfinshed event), should
  // have _buffer_header.h_begin set, and the following buffer should
  // have _buffer_header.h_end set.

  if (sizeof(lmd_event_header_host) + event_data_length > _stream_left)
    {
      // The event won't fit into the stream (which only applies when
      // running a stream server, since for the LMD files we do not
      // care about the stream limits at all, we fragment as far as
      // needed...)

      if (sizeof(lmd_event_header_host) + event_data_length > _stream_left_max)
	{
	  // The event will never fit into a stream

	  ERROR("Event is too large for the stream (%d > %d).",
		(int) (sizeof(lmd_event_header_host) + event_data_length),
		(int) _stream_left_max);
	}

      // All buffers of this stream must be flushed.  Flush buffers
      // until stream left is stream max, which is when we have turned
      // around.  We do it via new_buffer etc, since that is the one
      // keeping track of the buffer numbers.

      while (_stream_left != _stream_left_max)
	new_buffer();
    }

  size_t event_size_left = event_data_length;
  const buf_chunk_swap *chunk_cur = event->_chunk_start;
  size_t offset_cur  = 0;

  while (sizeof(lmd_event_header_host) + event_size_left > _cur_buf_left)
    {
      assert (_cur_buf_left > sizeof(lmd_event_header_host)); // or you have a bug: buffers too small

      // we need to write the start of an fragment

      size_t event_size_write = _cur_buf_left - sizeof(lmd_event_header_host);

      assert (event_size_left > event_size_write); // we should really be a fragment
      assert ((event_size_write & 1) == 0); // should be factor of 2

      _buffer_header.l_free[1] = // or should we use event_size_left?
	(sint32) DLEN_FROM_EVENT_DATA_LENGTH(event_data_length);

      // first a header

      lmd_event_header_host header_write;

      header_write = event->_header;
      header_write.l_dlen =
	(sint32) DLEN_FROM_EVENT_DATA_LENGTH(event_size_write);

      copy_to_buffer(&header_write,sizeof(lmd_event_header_host),
		     !_write_native);

      // then the data

      for ( ; ; )
	{
	  size_t chunk_write = chunk_cur->_length - offset_cur;

	  if (chunk_write > _cur_buf_left)
	    {
	      // Chunk larger than space left

	      chunk_write = _cur_buf_left;

	      copy_to_buffer(chunk_cur->_ptr + offset_cur,chunk_write,
			     !(_write_native ^ chunk_cur->_swapping));

	      offset_cur += chunk_write;
	      break; // we cannot write more
	    }
	  else
	    {
	      // Chunk smaller (or equal) to space left

	      copy_to_buffer(chunk_cur->_ptr + offset_cur,chunk_write,
			     !(_write_native ^ chunk_cur->_swapping));

	      // next time, next chunk

	      chunk_cur++;
	      offset_cur = 0;
	    }
	}

      assert (_cur_buf_left == 0);
      event_size_left -= event_size_write;

      //INFO(0,"Fragmenting:  writing %d bytes, %d bytes left (%d total).",
      //	   event_size_write,event_size_left,_event._event_size);

      if (event->_header.i_type == LMD_EVENT_STICKY_TYPE &&
	  event->_header.i_subtype == LMD_EVENT_STICKY_SUBTYPE)
	{
	  _buffer_header.i_type    = LMD_BUF_HEADER_HAS_STICKY_TYPE;
	  _buffer_header.i_subtype = LMD_BUF_HEADER_HAS_STICKY_SUBTYPE;
	}
      assert(_buffer_header.l_evt != (uint32_t) -1);
      _buffer_header.l_evt++; // we write another fragment

      _buffer_header.h_begin = 1; // this buffer has an unfinished event
      new_buffer();
      _buffer_header.h_end = 1;   // and the new buffer will have the continuation
    }

  // write whatever is left

  // first a header

  assert (!(event_size_left & 0x01)); // we should be a factor of 2!

  lmd_event_header_host header_write;

  header_write = event->_header;
  header_write.l_dlen = (sint32) DLEN_FROM_EVENT_DATA_LENGTH(event_size_left);

  copy_to_buffer(&header_write,sizeof(lmd_event_header_host),!_write_native);

  // then the data, first the buffer that may have already been copied from...

  // then the remaining full ones

  for ( ; chunk_cur < event->_chunk_end; chunk_cur++)
    {
      size_t chunk_write = chunk_cur->_length - offset_cur;

      copy_to_buffer(chunk_cur->_ptr + offset_cur,chunk_write,
		     !(_write_native ^ chunk_cur->_swapping));

      offset_cur = 0;
    }

  if (event->_header.i_type == LMD_EVENT_STICKY_TYPE &&
      event->_header.i_subtype == LMD_EVENT_STICKY_SUBTYPE)
    {
      _buffer_header.i_type    = LMD_BUF_HEADER_HAS_STICKY_TYPE;
      _buffer_header.i_subtype = LMD_BUF_HEADER_HAS_STICKY_SUBTYPE;
    }
  assert(_buffer_header.l_evt != (uint32_t) -1);
  _buffer_header.l_evt++; // we write another fragment

  while (flush_buffer())
    new_buffer();

  // we're done!
}

void lmd_output_file::event_no_seen(sint32 seventno)
{
  if (!_event_cut)
    return;

  uint32 eventno = (uint32) seventno;

  // Change if we crossed the boundary,
  // or the event numbers wrapped,
  // or we crossed the boundary by very many events :-)

  if ((eventno % _event_cut < _last_eventno % _event_cut) ||
      eventno < _last_eventno ||
      eventno > _last_eventno + _event_cut)
    if (_last_eventno != (uint32) -1 ||
	_cur_events) // do not change on startup
      change_file();

  _last_eventno = eventno;
}

void lmd_output_file::write_event(const lmd_event_out *event,
				  bool sticky_replay,
				  bool discard_revoke)
{
  UNUSED(discard_revoke);
  
  lmd_output_buffered::write_event(event, sticky_replay, true);

  add_events(); // we've added an event
  if (!sticky_replay) // Do not change file while replaying
    can_change_file(); // now would be a good time to change file
}


void lmd_output_file::write_file_header(const s_filhe_extra_host *file_header_extra)
{
  // make sure we get a new buffer

  new_buffer();

  // mark it as file header

  _buffer_header.i_type    = LMD_FILE_HEADER_2000_1_TYPE;
  _buffer_header.i_subtype = LMD_FILE_HEADER_2000_1_SUBTYPE;

  // allocate and set to zero the extra space...

  assert (_cur_buf_left >= sizeof (s_filhe_extra_host));
  // or no space for file header

  s_filhe_extra_host write_filhe_extra;

  memcpy(&write_filhe_extra,
	 file_header_extra,//&file_header->_file_extra,
	 sizeof (s_filhe_extra_host));

  // Clean away any garbage left over...
  zero_padding(write_filhe_extra);

  if (!_write_native)
    byteswap_32(write_filhe_extra);

  copy_to_buffer(&write_filhe_extra,
		 sizeof (s_filhe_extra_host),false/*!_write_native*/);

  // The space used by the file header does not count
  _cur_buf_usable = _cur_buf_left;

  // Do not write data to this buffer!  (TODO: do not create the
  // following buffer if actually no events are generated.)

  // Reduce the buffer size to 32k.  Seems go4 expects file header To
  // not be longer than 32k.  No, we will not write shorter file
  // headers, as all following buffers then become misaligned...  If
  // one has a buffer size - stick to it!  Mini-savings are
  // ill-advised.  Update: it is worse than that.  When writing large
  // buffers, only the actually used part is written, and the size is
  // recorded in the i_used member.  The l_dlen member lies about the
  // file header length and gives the length of following buffers.
  // This must be so, as the eventapi is so ill-designed that it does
  // not read the headers of data buffers to figure out their length,
  // but instead uses the length given in the first buffer (or
  // previous, who knows...?).

  size_t file_header_used_size =
    sizeof(write_filhe_extra) -
    sizeof(write_filhe_extra.s_strings) +
    (min(30,write_filhe_extra.filhe_lines)) *
    sizeof(write_filhe_extra.s_strings[0]);

  new_buffer(file_header_used_size);
}

void
lmd_output_file::set_file_header(const s_filhe_extra_host *file_header_extra,
				 const char *add_comment)
{
  // If file is now (i.e. no buffer yet) write file header.
  // Also keep file header to be written in case of renewed file.
  // No file header written for TCP transports.  (done by beaing virtual)

  // Header is cleaned up by write_file_header, so we only
  // need to add what we want (the comment).

  if (file_header_extra)
    _file_header_extra = *file_header_extra;
  else
    memset(&_file_header_extra,0,sizeof(_file_header_extra));

  size_t comment_index;

  if (_file_header_extra.filhe_lines <
      (int) countof(_file_header_extra.s_strings))
    comment_index = _file_header_extra.filhe_lines++;
  else
    {
      // overwrite last line
      comment_index = countof(_file_header_extra.s_strings)-1;
    }

  strncpy((char*) _file_header_extra.s_strings[comment_index].string,
	  add_comment,
	  sizeof(_file_header_extra.s_strings[comment_index].string));
  
  _file_header_extra.s_strings[comment_index].string_l =
    (uint16) strlen(add_comment);
  
  if (_file_header_extra.s_strings[comment_index].string_l >
      sizeof(_file_header_extra.s_strings[comment_index].string))
    _file_header_extra.s_strings[comment_index].string_l =
      sizeof(_file_header_extra.s_strings[comment_index].string);

  _has_file_header = true;

  if (!_cur_buf_start)
    write_file_header(&_file_header_extra);
}

void lmd_output_file::report_open_close(bool open)
{
  time_t rawtime;
  struct tm * timeinfo;
  char t[20];

  ::report_open_close(open, _cur_filename, _cur_size, _cur_events);

  if (!_lmd_log)
    return;

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(t,20,"%F %T",timeinfo);

  char *output = new char[200+strlen(_cur_filename)];

  if (open)
    sprintf(output,
	    "%-19s  %-5s  %s\n",
	    t, "Open", _cur_filename);
  else
    sprintf(output,
	    "%-19s  %-5s  %-71s  Size:%11llu  Events:%10u\n",
	    t, "Close", _cur_filename, _cur_size, _cur_events);

  _lmd_log->append(output);

  delete[] output;
}

void lmd_output_file::print_status(double elapsed)
{
  double bufrate =
    (double) (_buffer_header.l_buf - _last_bufno) * 1.e-3 / elapsed;
  _last_bufno = _buffer_header.l_buf;
  double datarate =
    (double) (_total_written - _last_written) * 1.e-6 / elapsed;
  _last_written = _total_written;
  double replayrate =
    (double) (_replays - _last_replays) / elapsed;
  _last_replays = _replays;
  
  fprintf (stderr,
	   "\nFile: %s%.1f%sMB/s (%s%.1f%skbuf/s, replay %s%.1f%sbuf/s)   \r",
	   CT_ERR(BOLD_MAGENTA),
	   datarate,
	   CT_ERR(NORM),
	   CT_ERR(BOLD),
	   bufrate,
	   CT_ERR(NORM),
	   CT_ERR(BOLD),
	   replayrate,
	   CT_ERR(NORM));
}

lmd_event_out::lmd_event_out()
{
  memset(&_header,0,sizeof(_header));
  memset(&_info,0,sizeof(_info));

  _chunk_start = _chunk_end = _chunk_alloc = NULL;

  _buf_start = _buf_end = _buf_alloc = NULL;

  _free_buf = NULL;
}

lmd_event_out::~lmd_event_out()
{
  clear(); // free old _free_buf entries

  free(_chunk_start);
  free(_buf_start);
}

void lmd_event_out::realloc_chunks()
{
  // We need to reallocate

  size_t n = (size_t) (_chunk_alloc - _chunk_start);
  size_t m = 2 * n + 4;

  _chunk_start = (buf_chunk_swap*) realloc(_chunk_start,
					   sizeof(buf_chunk_swap) * m);

  if (!_chunk_start)
    ERROR("Memory allocation failure.");

  _chunk_end   = _chunk_start + n;
  _chunk_alloc = _chunk_start + m;
}

void lmd_event_out::realloc_buf(size_t more)
{
  // We need to reallocate

  size_t n = (size_t) (_buf_alloc - _buf_start);
  size_t m = 2 * n + 2 * more;

  free_ptr *fp = (free_ptr *) malloc(sizeof(free_ptr));

  if (!fp)
    ERROR("Memory allocation failure.");

  fp->_ptr = _buf_start;
  fp->_next = _free_buf;
  _free_buf = fp;

  // printf ("TO-free: %p (alloc: %zd)\n",_buf_start,m);

  _buf_start = (char*) malloc(sizeof(char) * m);

  if (!_buf_start)
    ERROR("Memory allocation failure.");

  _buf_end   = _buf_start + n;
  _buf_alloc = _buf_start + m;
}

void lmd_event_out::clear()
{
  _chunk_end = _chunk_start;
  _buf_end = _buf_start;

  for (free_ptr *fp = _free_buf; fp; )
    {
      // printf ("NOW-free: %p\n",fp->_ptr);

      free(fp->_ptr);
      free_ptr *ffp = fp;
      fp = fp->_next;
      free(ffp);
    }

  _free_buf = NULL;
}

#define ANOTHER_CHUNK(ptr,length,swapping) { \
  if (UNLIKELY(_chunk_end >= _chunk_alloc))  \
    realloc_chunks();                        \
  _chunk_end->_ptr      = ptr;               \
  _chunk_end->_length   = length;            \
  _chunk_end->_swapping = swapping;          \
  /*printf ("chunk: %zd (%p) (c%p)\n",length,ptr,_chunk_end); */ \
  _chunk_end++;                              \
}

#define ADD_TO_BUF(dest,ptr,length) {			  \
  assert((length & 3) == 0);				  \
  if (UNLIKELY(_buf_alloc - _buf_end < (ssize_t) length)) \
    realloc_buf(length);				  \
    /*printf ("buf: %zd (%p)\n",length,_buf_end); */	  \
  dest = _buf_end;					  \
  memcpy(_buf_end,ptr,length);				  \
  _buf_end += length;					  \
}

void lmd_event_out::add_chunk(const void *ptr,size_t len,bool swapping)
{
  ANOTHER_CHUNK((const char *) ptr,len,swapping);
}

void lmd_event_out::copy_header(const lmd_event *event,
				bool combine_event)
{
  /*
  printf ("Copy header: comb: %d (# %d)\n",
	  combine_event,event->_header._info.l_count);
  */

  if (combine_event)
    {
      assert(_chunk_end != _chunk_start);

      // Dummy and event counter we keep from first event.

      // Do we have a better trigger number.
      // Better defined as being higher.

      if (event->_header._info.i_trigger > _info.i_trigger)
	_info.i_trigger = event->_header._info.i_trigger;
    }
  else
    {
      assert(_chunk_end == _chunk_start);

      // Copy directly from the event, ignoring the subevents

      _header = event->_header._header;

      // The second part of the header is handled as a chunk

      _info = event->_header._info;

      ANOTHER_CHUNK((const char *) &_info,
		    sizeof(lmd_event_info_host),
		    0/*native*/);
    }
}

void lmd_event_out::copy_all(const lmd_event *event,
			     bool combine_event)
{
  copy_header(event,combine_event);

  // The part of a chunk that makes up the second part of the header
  // must be ignored

  size_t to_ignore = sizeof(lmd_event_info_host);

  for (const buf_chunk* c = event->_chunks_ptr; c < event->_chunk_end; c++)
    {
      printf ("ignore: %zd\n",to_ignore);

      if (c->_length > to_ignore)
	{
	  /*
	  ANOTHER_CHUNK(c->_ptr + to_ignore,
			c->_length - to_ignore,
			event->_swapping);
	  */

	  char *buf;

	  ADD_TO_BUF(buf,c->_ptr + to_ignore,(size_t) (c->_length - to_ignore));
	  ANOTHER_CHUNK(buf,c->_length - to_ignore,event->_swapping);
	}

      if (to_ignore > 0)
	{
	  if (c->_length >= to_ignore)
	    to_ignore = 0;
	  else
	    to_ignore -= c->_length;
	}
    }
}

void lmd_event_out::copy(const lmd_event *event,
			 const select_event *select,
			 bool combine_event,
			 bool emit_header)
{
  // Copy only subevents requested, so we first make sure they have
  // been located

  assert(event->_status & LMD_EVENT_LOCATE_SUBEVENTS_ATTEMPT);

  if (emit_header)
    copy_header(event,combine_event);

  // Then look at the subevents

  for (int i = 0; i < event->_nsubevents; i++)
    {
      lmd_subevent           *subevent_info   = &event->_subevents[i];
      lmd_subevent_10_1_host *subevent_header = &subevent_info->_header;

      // Do we want it?

      if (!(event->_status & LMD_EVENT_IS_STICKY) &&
	  !select->accept_subevent(subevent_header))
	continue;

      /*****************************************/

      // Subevent is wanted.

      // Treat the header we have in host native format as one chunk :-)

      char *buf_subevent_header;

      ADD_TO_BUF(buf_subevent_header,
		 subevent_header,
		 sizeof(*subevent_header));

      ANOTHER_CHUNK(buf_subevent_header,
		    sizeof(*subevent_header),
		    0/*native*/);

      if (LIKELY(subevent_info->_data != NULL))
	{
	  // data is in one chunk

	  // we already checked for space for first chunk

	  size_t length;

	  if ((event->_status & LMD_EVENT_IS_STICKY) &&
	      subevent_header->_header.l_dlen ==
	      LMD_SUBEVENT_STICKY_DLEN_REVOKE)
	    length = 0;
	  else
	    length =
	      SUBEVENT_DATA_LENGTH_FROM_DLEN((size_t) subevent_info->_header._header.l_dlen);

	  // ANOTHER_CHUNK(subevent_info->_data,length,event->_swapping);

	  if (length)
	    {
	      char *buf;

	      ADD_TO_BUF(buf,subevent_info->_data,length);
	      ANOTHER_CHUNK(buf,length,event->_swapping);
	    }
	}
      else
	{
	  // Copy all the data chunks

	  buf_chunk *frag = subevent_info->_frag;
	  size_t size0 = frag->_length - subevent_info->_offset;

	  // we already checked for space for first chunk

	  // ANOTHER_CHUNK(frag->_ptr,size0,event->_swapping);

	  char *buf;

	  ADD_TO_BUF(buf,frag->_ptr,size0);
	  ANOTHER_CHUNK(buf,size0,event->_swapping);

	  // No guard against stciky revoke (size -1), as we had large size
	  size_t length =
	    SUBEVENT_DATA_LENGTH_FROM_DLEN((size_t) subevent_info->_header._header.l_dlen) - size0;
	  frag++;

	  size_t size = frag->_length;

	  while (UNLIKELY(size < length))
	    {
	      // ANOTHER_CHUNK(frag->_ptr,size,event->_swapping);

	      ADD_TO_BUF(buf,frag->_ptr,size);
	      ANOTHER_CHUNK(buf,size,event->_swapping);

	      length -= size;

	      frag++;
	      size = frag->_length;
	    }

	  // ANOTHER_CHUNK(frag->_ptr,length,event->_swapping);

	  ADD_TO_BUF(buf,frag->_ptr,length);
	  ANOTHER_CHUNK(buf,length,event->_swapping);
	}
    }
}

bool lmd_event_out::has_subevents() const
{
  /* The first thing the event has is one chunk with the second part
   * of the header.  If there is more data than that, then there are
   * subevents.
   */

  /* First chunk shall be the header, if anything. */

  assert (_chunk_start == _chunk_end ||
	  _chunk_start[0]._length == sizeof(lmd_event_info_host));

  /* Second chunk, if any, shall not be empty. */

  assert (_chunk_start + 1 >= _chunk_end ||
	  _chunk_start[1]._length > 0);

  /* More than one (header) chunk? */

  return (_chunk_start + 1 < _chunk_end);
}

size_t lmd_event_out::get_length() const
{
  size_t length = 0;

  for (const buf_chunk_swap* c = _chunk_start; c < _chunk_end; c++)
    length += c->_length;

  return length;
}

void lmd_event_out::write(void *dest) const
{
  char *p = (char *) dest;
 
  p += sizeof(lmd_event_header_host);

  for (const buf_chunk_swap* c = _chunk_start; c < _chunk_end; c++)
    {
      // printf ("%3zx [%zd]\n", p - (char *) dest, c->_length);
      ::copy_to_buffer(p,
		       c->_ptr, c->_length,
		       c->_swapping);
      p += c->_length;
    }

  size_t event_data_length = p - (char *) dest - sizeof(lmd_event_header_host);
    
  lmd_event_header_host header_write;
  header_write = _header;
  header_write.l_dlen =
	(sint32) DLEN_FROM_EVENT_DATA_LENGTH(event_data_length);
  
  // printf ("%3x [%zd]\n", 0, sizeof(lmd_event_header_host));
  ::copy_to_buffer(dest,
		   &header_write,sizeof(lmd_event_header_host),
		   false);
}

void lmd_event_out::dump_debug()
{
  fprintf (stderr, "-- Event: --\n");
  fprintf (stderr, "t/s: %04x/%04x\n", _header.i_type, _header.i_subtype);
  fprintf (stderr, "d: %04x  t: %04x  cnt: %08x\n",
	   _info.i_dummy, _info.i_trigger, _info.l_count);

  for (const buf_chunk_swap* c = _chunk_start; c < _chunk_end; c++)
    {
      const char *p = c->_ptr;
      fprintf(stderr, "\n");
      for (size_t i = 0; i < c->_length; i += sizeof (uint32_t))
	fprintf (stderr, "%3zx: %08x\n", i, *((const uint32_t *) (p+i)));
    }

  fprintf (stderr, "------------\n");
}
