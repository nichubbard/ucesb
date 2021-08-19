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

#ifndef __EXTERNAL_WRITER_HH__
#define __EXTERNAL_WRITER_HH__

#include "forked_child.hh"

#include "ext_file_writer.hh"

//#define NTUPLE_TYPE_RWN          0x0001  // deprecated
#define NTUPLE_TYPE_CWN          0x0002

#define NTUPLE_TYPE_ROOT         0x0004

#define NTUPLE_TYPE_STRUCT_HH    0x0008
#define NTUPLE_TYPE_STRUCT       0x0010

#define NTUPLE_TYPE_MASK   (/*NTUPLE_TYPE_RWN |*/ NTUPLE_TYPE_CWN | \
                            NTUPLE_TYPE_ROOT | \
			    NTUPLE_TYPE_STRUCT_HH | NTUPLE_TYPE_STRUCT)

#define NTUPLE_CASE_KEEP         0x0100
#define NTUPLE_CASE_UPPER        0x0200
#define NTUPLE_CASE_LOWER        0x0400
#define NTUPLE_CASE_H2ROOT       0x0800

#define NTUPLE_CASE_MASK   (NTUPLE_CASE_KEEP | \
                            NTUPLE_CASE_UPPER | NTUPLE_CASE_LOWER | \
                            NTUPLE_CASE_H2ROOT)

// Separate variable: (could reuse bits)

#define NTUPLE_OPT_WRITER_BITPACK  0x004000
#define NTUPLE_OPT_WRITER_NO_SHM   0x008000
#define NTUPLE_OPT_READER_INPUT    0x010000 // use as a reader!

#define NTUPLE_OPT_DUMP_RAW        0x020000
#define NTUPLE_OPT_EXT_GDB         0x040000
#define NTUPLE_OPT_EXT_VALGRIND    0x080000

class ext_writer_buf
{
public:
  ext_writer_buf();
  virtual ~ext_writer_buf() { }

public:
  size_t _size;    // max size of usable part of buffer
  char  *_cur;

  forked_child    _fork;

  bool   _reading;

public:
  virtual void flush() = 0;
  virtual void close() = 0;

  virtual void commit_buf_space(size_t space) = 0;
  virtual char *ensure_buf_space(uint32_t space) = 0;

public:
  void write_command(char cmd);
  char get_response();

public:
  virtual void ready_to_read() = 0;
  virtual char *ensure_read_space(uint32_t space) = 0;
  virtual void message_body_done(uint32_t *end) = 0;
};

class ext_writer_shm_buf
  : public ext_writer_buf
{
public:
  ext_writer_shm_buf();
  virtual ~ext_writer_shm_buf() { }

public:
  int    _fd_mem;
  char  *_ptr; // shm
  size_t _len;

  external_writer_shm_control *_ctrl;

public:
  char  *_begin;
  char  *_end;

public:
  int init_open();
  void resize_shm(uint32_t size);
  virtual void flush();
  virtual void close();

  virtual void commit_buf_space(size_t space);
  virtual char *ensure_buf_space(uint32_t space);

public:
  virtual void ready_to_read();
  virtual char *ensure_read_space(uint32_t space);
  virtual void message_body_done(uint32_t *end);
};

class ext_writer_pipe_buf
  : public ext_writer_buf
{
public:
  ext_writer_pipe_buf();
  virtual ~ext_writer_pipe_buf() { }

public:
  char  *_mem; // !shm
  char  *_end;

  char  *_begin_write;
  char  *_write_mark;

public:
  void restart_pointers();

public:
  void init_alloc();
  void alloc(uint32_t size);
  void resize_alloc(uint32_t size);
  virtual void flush();
  virtual void close();

  virtual void commit_buf_space(size_t space);
  virtual char *ensure_buf_space(uint32_t space);

public:
  virtual void ready_to_read();
  virtual char *ensure_read_space(uint32_t space);
  virtual void message_body_done(uint32_t *end);

public:
  // virtual void wrote_pipe_buf() { }
};


class external_writer
{
public:
  external_writer();
  ~external_writer();

public:
  ext_writer_buf *_buf;

  uint32_t _max_message_size;
  uint32_t _sort_u32_words;
  uint32_t _max_raw_words;

public:
  //void ensure_buf_space(uint32_t space);
  //void commit_buf_space(size_t space);
  void *insert_buf_header(void *ptr,
			  uint32_t request,uint32_t length);
  void *insert_buf_string(void *ptr,const char *str);

public:
  void init_x(unsigned int type,unsigned int opt,
	      const char *filename,const char *ftitle,
	      int server_port,int generate_header,
	      int timeslice,int timeslice_subdir,
	      int autosave,int ts_merge_window);
  void close();

public:
  void send_file_open(uint32_t sort_u32_words);
  void send_book_ntuple_x(int hid,
			  const char *id,const char *title,
			  const char *index_major = "",
			  const char *index_minor = "",
			  uint32_t struct_index = 0,
			  uint32_t ntuple_index = 0,
			  uint32_t max_raw_words = 0);
  void send_alloc_array(uint32_t size);
  void send_hbname_branch(const char *block,
			  uint32_t offset,uint32_t length,
			  const char *var_name,uint var_array_len,
			  const char *var_ctrl_name,int var_type,
			  uint limit_min = (uint) -1,
			  uint limit_max = (uint) -1);
  void set_max_message_size(uint32_t size);
  uint32_t *prepare_send_offsets(uint32_t size);
  void send_offsets_fill(uint32_t *po);
  void send_setup_done(bool reader = false);
  uint32_t *prepare_send_fill_x(uint32_t size,
				uint32_t struct_index = 0,
				uint32_t ntuple_index = 0,
				uint32_t *sort_u32 = NULL,
				uint32_t **raw = NULL,
				uint32_t raw_words = 0);
  void send_done();
  void send_flush(); /* Used when data is sent seldomly...  Hmmm */

  uint32_t max_h1i_size(size_t max_id_title_len,uint32_t bins);
  void send_hist_h1i(int hid,const char *id,const char *title,
		     uint32_t bins,float low,float high,
		     uint32_t *data);

  uint32_t max_named_string_size(size_t max_id_len,size_t max_str_len);
  void send_named_string(const char *id, const char *str);

public:
  uint32_t get_message(uint32_t **start,uint32_t **end);
  void message_body_done(uint32_t *end);

public:
  void silent_fork_pipe(ext_writer_buf *buf);
  void inject_raw(void *buf,uint32_t len);

};

inline uint32_t external_write_float_as_uint32(float src)
{
  union
  {
    float    _f;
    uint32_t _i;
  } value;
  value._f = src;
  return value._i;
}

/* Using the external_writer to create a custom ntuple/root/structure
 *
 * - Create an external_writer object.
 *
 * - Call init() to set the internal shm/pipe communication buffers up,
 *   and fork the appropriate external program, {hbook,root,struct}_writer.
 *
 *   @type         Type of output, combination of the NTUPLE_... values
 *                 defined in staged_ntuple.hh  (except for this, that
 *                 header file is not needed.)  Basically, one of the
 *                 NTUPLE_TYPE_... values, and on of NTUPLE_CASE_...
 *                 should be or|ed together.  The NTUPLE_WRITER_... are
 *                 not used by the external_writer.
 *
 *   @no_shm       Set to true to disable the shared memory communication,
 *                 should not be needed.
 *
 *   @filename     Name of the output file.  (Not used for NTUPLE_TYPE_STRUCT)
 *
 *   @ftitle       File title.  (part of hbook and root files).
 *
 *   @sort_u32_words  Number of words used for sorting multiple streams.
 *                    (Only for ntuple_index = 0).
 *
 *   @server_port  Used for NTUPLE_TYPE_STRUCT.
 *
 *   @generate_header  Used for NTUPLE_TYPE_STRUCT_HH.  Hmm...
 *
 *   @timeslice    Generate new output file every so often seconds.
 *
 *   @timeslice_subdir  New timeslice subdirectory every so often seconds.
 *
 *   @autosave     Autosave the tree every so often seconds.
 *
 * - Call send_file_open() to make it open the output file.
 *
 * - Call send_book_ntuple_x() to create the ntuple/tree object.
 *
 *   @hid           [NTUPLE] Ntuple id.
 *
 *   @id            [TREE] Name of the ntuple, use for root trees.
 *
 *   @title         Title of the ntuple/tree.
 *
 *   @index_major   For BuildIndex (if wanted).
 *
 *   @index_minor   See previous.
 *
 *   @ntuple_index  (Usually 0) For having several ntuples (of
 *                  same layout) in the output file.
 *
 *   @max_raw_words  Maximum size of raw data.
 *                   (Only for ntuple_index = 0).
 *
 *   @struct_index  (Usually/first 0) For having several ntuples (of
 *                  different layout) in the output file.
 *
 * - Call send_alloc_array() to allocate the staging array.
 *
 *   @size       This should have the length of the structure that
 *               you want to dump.  NOTE: it can (currently) only
 *               contain 32-bit entries, i.e. basically 32-bit int,
 *		 uint, and floats.  int32_t and uint32_t are
 *		 recommended.  It may be arrays whose length are
 *		 controlled by some earlier integer.
 *
 * - For each member of the ntuple/tree, call send_hbname_branch().
 *
 *   @block        [NTUPLE] Name of the block to put the variable in.
 *
 *   @offset       Offset of the item in the staging structure (bytes).
 *
 *   @length       Length of the item (bytes).
 *
 *   @var_name     [STRUCT] Variable name, without array or type.
 *
 *   @var_array_len [STRUCT] Array length, or -1.  Fixed (max) length of
 *                  array, i.e. how much to allocate statically.
 *
 *   @var_ctrl_name [STRUCT] Array length control member, or "".
 *
 *   @var_type      [STRUCT] Variable type, see ext_file_writer.hh
 *                  EXTERNAL_WRITER_FLAG_TYPE_{INT32,UINT32,FLOAT32}
 *
 *   @limit_min    [NTUPLE] Minimum value.  Flag needed:
 *                 var_type |= EXTERNAL_WRITER_FLAG_HAS_LIMIT).
 *
 *   @limit_max    [NTUPLE] Maximum value.  Flag needed:
 *                 var_type |= EXTERNAL_WRITER_FLAG_HAS_LIMIT).
 *
 * - Call set_max_message_size() to reallocate the communication
 *   shm/pipe with enough space to handle the worst case message.
 *
 *   @size          The maximum message length is protocol dependent
 *                  (see below), currently 2 32-bit word + 1 32-bit
 *                  words per item sent.  Also consider the offset
 *                  message.  Specified in bytes.
 *
 *                  It does not include any max_raw_words, which is
 *                  added internally.
 *
 * - Call prepare_send_offsets() to give the fill order and offsets of
 *   all items that are to be filled.  Returns a pointer to an array
 *   where to put the information
 *
 *   @size          The length of the offset array.  Equal to the number
 *                  of items.
 *
 *   The information consists of, for each data item:
 *
 *   uint32_t       offset   (offset in the staging array of the item)
 *
 *                           In case of zero-suppressed arrays, they
 *                           must be directly preceeded by the
 *                           controlling variable, which offset is to
 *                           be OR marked with 0x80000000.  This is
 *                           followed by two values, max_loops and
 *                           loop_size, describing the following arrays.
 *                           The items of the arrays are then to be
 *                           given in round-robin order).
 *
 *                           All @offset are to written with htonl(), to
 *                           avoid endianess issues.
 *
 *                           Items that are to be cleared with 0 (e.g.
 *                           integers), should have a marker 0x40000000.
 *                           Otherwise, they are cleared with NaN.
 *
 * - When the information has been filled, call send_offsets_fill() to
 *   send it..
 *
 *   @o             Points to the next byte after the filled data.
 *
 * - Call send_setup_done() to tell that we're ready to fill.
 *
 *   @reader        If it is suppose to be a reader and not a writer.
 *
 * - For each event to fill, first call prepare_send_fill() to
 *   reserve space for the data to send, and also get a pointer where
 *   to put the data.
 *
 *   @size          Maximum size that will be used for the protocol data.
 *                  Can safely use the value sent to set_max_message_size().
 *
 *   @struct_index  (Usually 0) which ntuple to fill.
 *
 *   @ntuple_index  (Usually 0) which ntuple to fill.
 *
 *   @sort_u32      Pointer to words used for sorting multiple streams.
 *
 *   @raw           Pointer to pointer (set during call) to location
 *                  where to write ancillary raw data of size @raw_words.
 *
 *   @raw_words     Size of raw data (bytes).
 *
 *   The protocol consists of:
 *
 *   uint32_t       struct_index  (usually zero, non-zero with multiple
 *                                ntuples (of different layout) in the
 *                                output file)
 *
 *   uint32_t       ntuple_index  (usually zero, non- with multiple
 *                                ntuples (of same layout) in the
 *                                output file)
 *
 *   uint32_t       sort_u32[sort_u32_words]  Words to sort on when receiving
 *                                            multiple streams.  Highest word
 *                                            first.
 *
 *   uint32_t       marker = 0x40000000
 *                              (used internally by the external writer,
 *                              when sending data by the STRUCT server)
 *
 *   For each data item:
 *
 *   uint32_t       value    (value, see writing_ntuple.hh for
 *                           type-punning of floats) Both @offset and
 *                           @value are to be written with htonl(), to
 *                           avoid endianess issues.
 *
 *                           The values are to be given in the same
 *                           order as the offsets to
 *                           prepare_send_offsets().  In case of
 *                           zero-suppressed arrays, only as many
 *                           rounds as the controlling item prescribes
 *                           should be given.
 *
 * - When the event data has been filled, call send_offsets_fill() to
 *   tell how mush data was generated.
 *
 *   @p             Points to the next byte after the filled data.
 *
 * - Call close() to close the file when finished.  Calls send_done().
 *
 * - Delete the external_writer object.  This will call close() if needed.
 *
 * Example: see example/ext_writer_test.cc
 */

#endif//__EXTERNAL_WRITER_HH__
