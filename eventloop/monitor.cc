
#include "monitor.hh"
#include "optimise.hh"
#include "set_thread_name.hh"
#include "external_writer.hh"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

status_block _status_block;
external_writer *_mon_ew = NULL;

struct extwrite_mon_block
{
  uint  events;
  uint  multi_events;
  uint  errors;
};

#include "gen/extwrite_mon_block.hh"

void mon_block_update(status_block *handle, status_monitor *data)
{
  int copy;

  copy = (++handle->_copy_seq_done) & 1;

  /* Update the marker that we are now updating this copy.
   * (marker [0] does not match marker [1] during the copy.
   */

  handle->_copy_seq[copy][0] = handle->_copy_seq_done;

  SFENCE;

  memcpy (&handle->_copy[copy], data, sizeof (handle->_copy[copy]));

  SFENCE;

  /* Update is done, let marker[1] match. */

  handle->_copy_seq[copy][1] = handle->_copy_seq_done;

  //printf ("Copied status block to %d\n", copy);
}

void *monitor_thread(void *)
{
  for ( ; ; )
    {
      for (int attempt = 0; attempt < 2; attempt++)
	{
	  extwrite_mon_block mon;
	  status_monitor *src;
	  
	  /* Write info to external writer. */
	  int copy = _status_block._copy_seq_done & 1;
	  /* We check the copy seq value bafore and after using the
	   * data.  If mismatch at the end, we do not commit the
	   * stuff, but try again.
	   */
	  /* Note the reverse order: we check [1] first, while the
           * writer writes [0] first.
           */
	  int seq_before = _status_block._copy_seq[copy][1];
	  SFENCE;
	  src = &_status_block._copy[copy];

	  mon.events       = (uint) src->_events;
	  mon.multi_events = (uint) src->_multi_events;
	  mon.errors       = (uint) src->_errors;

	  SFENCE;	  
	  int seq_after = _status_block._copy_seq[copy][0];

	  if (seq_before == seq_after)
	    {
	      /* Good. */
	      //printf ("Good copy in %d.\n", copy);

	      send_fill_x_extwrite_mon_block(_mon_ew,mon);
	      _mon_ew->send_flush();
	      
	      break;
	    }
	  sleep (50000);
	}

      /* Wait for next update. */
      usleep(900000);
      /* Queue next update. */      
      _status_block._mon_update_seq++;
      SFENCE;
      /* Let's hope we get an update in 0.1 s time. */
      usleep(100000);
    }

  return NULL;
}

pthread_t _mon_thread;

void start_monitor_thread(int port)
{
  _status_block._mon_update_seq = 0;
  _status_block._update_seq = -1;
  /* Set up other monitor blocks to be invalid. */
  _status_block._copy_seq[0][0] = -2;
  _status_block._copy_seq[0][0] = -1;
  mon_block_update(&_status_block, &_status);

  /* *********************************************************************** */
  /* Create the external writer. */

  _mon_ew = new external_writer();

  _mon_ew->init_x(NTUPLE_TYPE_STRUCT | NTUPLE_CASE_KEEP,
		  NTUPLE_OPT_WRITER_NO_SHM,
		  "monitor.root","Monitor",
		  port,false,0,0,0);

  _mon_ew->send_file_open(0);

  _mon_ew->send_book_ntuple_x(1,"Monitor","Monitor");

  send_offsets_extwrite_mon_block(_mon_ew);

  /* *********************************************************************** */
 
  if (pthread_create(&_mon_thread,NULL,
		     monitor_thread,NULL) != 0)
    {
      perror("pthread_create()");
      exit(1);
    }

  set_thread_name(_mon_thread, "MON", 3);
}
