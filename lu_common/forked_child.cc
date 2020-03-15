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

#include "forked_child.hh"

#ifdef __LAND02_CODE__ // i.e. compiling as UCESB - this *is* rotten!
#include "error.hh"
#else
#include "../util/error.hh"
#endif

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#ifdef _POSIX_PRIORITY_SCHEDULING
#include <sched.h>
#endif

forked_child::forked_child()
{
  _child  = 0;
  _fd_in  = -1;
  _fd_out = -1;
  _argv0  = NULL;
}

forked_child::~forked_child()
{
  free(_argv0);
}

void
forked_child::fork(const char *file,const char *const argv[],
		   int *fd_in,
		   int *fd_out,
		   int fd_dest,int fd_src,
		   int fd_keep,char *fork_pipes,
		   bool ignore_sigint)
{
  // We need stdout piped from the process
  // And we give him a broken stdin

  int pipe_in[2] = { -1, -1 };
  int pipe_out[2] = { -1, -1 };

  if (((fork_pipes || fd_src == -1) && // do not create input pipe if we supply file handle
       pipe(pipe_out) == -1) ||
      ((fork_pipes || fd_dest == -1) && // do not create output pipe if we supply file handle
       pipe(pipe_in) == -1))
    ERROR("Error creating pipe(2)s for handling forked process.");

  _child = ::fork();

  if (_child == -1)
    ERROR("Error fork(3)ing child process.");

  if (fork_pipes)
    sprintf (fork_pipes,"%d,%d",
	     pipe_out[0],pipe_in[1]);

  if (!_child)
    {
      // This is the child process

      // File handle that the child reads (STDIN), parent writes

      if (fork_pipes || fd_src == -1)
	{
	  close (pipe_out[1]);   // Close writing
	  if (!fork_pipes)
	    {
	      dup2  (pipe_out[0],STDIN_FILENO); // Read from 0 (stdin)
	      close (pipe_out[0]);
	      pipe_out[0] = -1;
	    }
	}
      if (fd_src != -1)
	{
	  if (fd_src != STDIN_FILENO)
	    {
	      dup2  (fd_src,STDIN_FILENO);
	      close (fd_src);
	    }
	}

      // File handle that the child writes (STDOUT), parent reads

      if (fork_pipes || fd_dest == -1)
	{
	  close (pipe_in[0]);   // Close reading
	  if (!fork_pipes)
	    {
	      dup2  (pipe_in[1],STDOUT_FILENO); // Write to 1   (stdout)
	      close (pipe_in[1]);
	      pipe_in[1] = -1;
	    }
	}
      if (fd_dest != -1)
	{
	  if (fd_dest != STDOUT_FILENO)
	    {
	      dup2  (fd_dest,STDOUT_FILENO);
	      close (fd_dest);
	    }
	}

      // Then kill of all file handles.  We let the pipes stay open.
      // Let stderr go unharmed.  We also spare a special file handle
      // if given.  If the pipes were not moved to stdin/stdout, then
      // that was for a reason.  Let those survive too.  I.e.: never
      // kill file handles STDIN, STDOUT and STDERR.  (checking vs. -1
      // is a no-op)

      for (int i = getdtablesize() - 1; i >= 0; --i)
        if (i != STDIN_FILENO && i != STDOUT_FILENO && i != STDERR_FILENO &&
	    i != fd_keep      &&
	    i != pipe_in[1]   && i != pipe_out[0])
          close(i);

      unsigned int narg = 0;
      for ( ; argv[narg]; narg++)
	;
      char **nc_argv = (char **) malloc ((narg+1)*sizeof(char*));
      for (int i = 0; argv[i]; i++)
	nc_argv[i] = strdup(argv[i]);
      nc_argv[narg] = NULL;

      // Perhaps we should have used a different process group??
      // Would that (or this) lead to having now 'unkillable' children?
      if (ignore_sigint)
	signal(SIGINT, SIG_IGN);

      int ret = execvp(file,nc_argv);
      /*
		       "cpp",
		       "cpp",
		       "-DUSING_TCAL_PARAM",
		       "calib/calib.cc",NULL);
      */
      perror("exec");
      fprintf (stderr,"Failure starting '%s'.\n",file);
      _exit(ret);
    }

  // This is the parent process

  if (fork_pipes || fd_dest == -1)
    {
      close (pipe_in[1]); // Close writing (child will write)

      if (fd_in)
	*fd_in = _fd_in = pipe_in[0];
      else
	close (pipe_in[0]); // Close reading
    }

  if (fork_pipes || fd_src == -1)
    {
      close (pipe_out[0]); // Close reading (child will write)

      if (fd_out)
	*fd_out = _fd_out = pipe_out[1];
      else
	close (pipe_out[1]); // Close writing
    }

  _argv0 = strdup(argv[0]);
}

void forked_child::close_fds()
{
  // We get called directly when e.g. some other child is forked,
  // and it shall not have the communication file descriptors open.

  if (_fd_out != -1)
    {
      // First close the writing end of the pipe (unless done by
      // someone else)
      close (_fd_out);
      _fd_out = -1;
    }

  if (_fd_in != -1)
    {
      close (_fd_in);
      _fd_in = -1;
    }

}

void
forked_child::wait(bool terminate_child,
		   int *exit_status)
{
  if (exit_status)
    *exit_status = -1;

  // fprintf (stderr,"Waiting %d!\n",_child);

  // So the work is suppose to be done.
  // This also means that the child should be about to exit, since
  // we should always read to EOF!

  close_fds();

  if (!_child)
    return; // no child process, nothing to do

  // Now lets get the exit status of the child

  // First ask him to die, if he did not already (this is otherwise a
  // beautiful way to lock us up, if we abort before finishing reading
  // for some reason)

  int status;

  // INFO("Waiting for child %d...",_child);

  // Let's drop the idea of doing WNOHANG and a sleep.  The reason for
  // those games where such that if the child did not stop, then we
  // should be able to tell that it is not us, but a child that is
  // hanging...  Who cares, a hang is a hang.  Only sane way to play
  // those cames (since sleep(1) is sleeping for too long) is to use a
  // waitpid without WNOHANG, but instead to set up an timer
  // before-hand, which will interrupt the waitpid call and give us
  // back control...  Yuck!

  int wnohang = WNOHANG;

  for (int attempts = 0; ; attempts++)
    {
      int ret = waitpid (_child,&status,wnohang);

      // INFO(0,"Waited for child %d, ret=%d status=%d...",_child,ret,status);

      if (ret == _child)
	{
	  _child = 0;
	  break; // we're done waiting
	}

      if (ret == -1)
	{
	  if (errno == EINTR)
	    continue;
	  perror("waitpid");
	  ERROR("Failure waiting for child process (%s).",_argv0);
	}

      // When we get through to here, we've tried once with WNOHANG.
      // For no very good reason, but trying to be at least a little
      // bit nice, we'll first try (if available) to yield, hoping it
      // will clean itself away

#ifdef _POSIX_PRIORITY_SCHEDULING
      // TODO: this sleep can be replaced by an sched_yield?  at least
      // if the child runs at our priority or higher...  And if it
      // obeys the TERM signal...
      sched_yield();
#endif

      if (attempts == 1)
	{
	  // If we do not really need it to finish, and we already
	  // tried to be nice once, ask it to get away
	  if (terminate_child)
	    kill (_child,SIGTERM);
	  // After this, we'll wait until it goes away.
	  wnohang = 0;
	}
    }

  if (WIFEXITED(status))
    {
      // It exited!
      if (WEXITSTATUS(status) != 0)
	{
	  if (exit_status)
	    {
	      *exit_status = WEXITSTATUS(status);
	      // Someone will take care of the bad status.
	      return;
	    }

	  ERROR("Child process (%s) exited with error code=%d.  "
		"(Usually see error messages on lines above.)",
		_argv0,WEXITSTATUS(status));
	}
      else if (exit_status)
	*exit_status = 0;
      // So child is dead, pipes are closed, we got the information.
      // Yippie!
      return;
    }

  // It is (may be) ok of the child process died due to recieving
  // a sigpipe (since we might have closed the reading end before
  // end of file)

  if (WIFSIGNALED(status) &&
      (WTERMSIG(status) == SIGPIPE ||
       WTERMSIG(status) == SIGTERM))
    {
      // Either it got sigpipe, or it got a sigterm (by us)

      // WARNING("Decompressor got sigpipe.  Did we stop before end-of-file?");
      return;
    }

  ERROR("Child process (%s) did not exit cleanly, "
	"or something else bad happened.  "
	"This should not happen.  (exit=%d/%d sig=%d/%d)",_argv0,
	WIFEXITED(status),WEXITSTATUS(status),
	WIFSIGNALED(status),WTERMSIG(status));
}





size_t full_write(int fd,const void *buf,size_t count)
{
  size_t total = 0;

  for ( ; ; )
    {
      ssize_t nwrite = write(fd,buf,count);

      if (!nwrite)
	ERROR("Reached unexpected end of file while writing.");

      if (nwrite == -1)
	{
	  if (errno == EINTR)
	    nwrite = 0;
	  else
	    {
	      perror("write");
	      ERROR("Error writing file/pipe.");
	    }
	}

      count -= (size_t) nwrite;
      total += (size_t) nwrite;

      if (!count)
	break;
      buf = ((const char*) buf)+nwrite;
    }

  return total;
}



const char *main_argv0 = NULL;

char *argv0_replace(const char *filename)
{
  assert(main_argv0);

  if (filename[0] == '/') // absolute paths are not mangled
    return strdup(filename);

  // +2 = space for slash and trailing newline
  char *path = (char *) malloc(strlen(main_argv0) + strlen(filename) + 2);

  if (!path)
    ERROR("Memory allocation failure.");

  strcpy (path,main_argv0);

  // cut away the last component, i.e. anything past the trailing slash,
  // or everything, if nothing found

  char *p = strrchr(path,'/');

  if (p)
    *(p+1) = 0;
  else
    *path = 0;

  strcat(path,filename);

  return path;
}

