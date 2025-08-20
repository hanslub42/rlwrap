/*
 * All portions of code are copyright by their respective author/s.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *---------------------------------------------------------------------*/

#include "rlwrap.h"


/* The use of 'void' in the signatures of the ptytty_* functions avoids having
*  different signatures depending on USE_LIBPTYTTY
*/ 
   

#if USE_LIBPTYTTY /* Use libptytty (if configured with --with-libptytty) */

#include <libptytty.h>

int
ptytty_control_tty(int UNUSED(fd_tty), const void *ptytty)  /* called by child */
{
  if (! ptytty_make_controlling_tty((PTYTTY) ptytty))
      myerror(WARNING|USE_ERRNO, "Could not make slave a controlling terminal");
  return 0; /* ptty_make_controlling_tty() is already called in ptty_openpty */
}



int
ptytty_openpty(int *amaster, int *aslave, const void  **ptytty_ptr)
{
  PTYTTY ptytty = ptytty_create();
  DPRINTF0(DEBUG_TERMIO, "Using libptytty to obtain pty/tty pair");
  if (! ptytty_get(ptytty)) {
      myerror(FATAL|USE_ERRNO, "Could not create pty for  %s", program_name);
  }
  *amaster = ptytty_pty(ptytty);
  *aslave  = ptytty_tty(ptytty);
  *ptytty_ptr = (void *) ptytty ; 
  return 0;
}



#else  /*  if ! USE_LIBPTYTTY (i.e. if configured --without-libptytty), use rlwrap's original ancient implementation of
           the same functionality, shamelessly ripped out of rxvt-2.7.10 ((c) 1999-2001 Geoff Wing
           <gcw@pobox.com>) This could be useful on really old systems, where libptytty is not available, or not working
           for some reason.
          
*/






#include <stdio.h>
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#if defined(HAVE_STRING_H)
# include <string.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#if defined(PTYS_ARE_PTMX) && !defined(__CYGWIN32__)
# include <sys/stropts.h>       /* for I_PUSH */
#endif




/* ------------------------------------------------------------------------- *
 *                  GET PSEUDO TELETYPE - MASTER AND SLAVE                   *
 * ------------------------------------------------------------------------- */
/*
 * Returns pty file descriptor, or -1 on failure 
 * If successful, ttydev is set to the name of the slave device.
 * fd_tty _may_ also be set to an open fd to the slave device
 */



#define O_RWNN  O_RDWR | O_NDELAY | O_NOCTTY

static int
ptytty_get_pty(int *fd_tty, const void **ttydev)
{
  int pfd;

#ifdef PTYS_ARE_OPENPTY
  char tty_name[sizeof "/dev/pts/????\0"];

  if (openpty(&pfd, fd_tty, tty_name, NULL, NULL) != -1) {
    *ttydev = (void *) strdup(tty_name);
    return pfd;
  }
#endif

#ifdef PTYS_ARE__GETPTY
  *ttydev = (void *) _getpty(&pfd, O_RWNN, 0622, 0);
  if (*ttydev != NULL)
    return pfd;
#endif

#ifdef PTYS_ARE_GETPTY
  char *ptydev;

  while ((ptydev = getpty()) != NULL)
    if ((pfd = open(ptydev, O_RWNN, 0)) >= 0) {
      *ttydev = ptydev;
      return pfd;
    }
#endif

#if defined(HAVE_GRANTPT) && defined(HAVE_UNLOCKPT)
# if defined(PTYS_ARE_GETPT) || defined(PTYS_ARE_PTMX)
  {
    extern char *ptsname();

#  ifdef PTYS_ARE_GETPT
    pfd = getpt();
#  else
    pfd = open("/dev/ptmx", O_RWNN, 0);
#  endif
    if (pfd >= 0) {
      if (grantpt(pfd) == 0     /* change slave permissions */
          && unlockpt(pfd) == 0) {      /* slave now unlocked */
        *ttydev = ptsname(pfd); /* get slave's name */
        return pfd;
      }
      close(pfd);
    }
  }
# endif
#endif

#ifdef PTYS_ARE_PTC
  if ((pfd = open("/dev/ptc", O_RWNN, 0)) >= 0) {
    *ttydev = (void *) ttyname(pfd);
    return pfd;
  }
#endif

#ifdef PTYS_ARE_CLONE /* HP-UX */
  if ((pfd = open("/dev/ptym/clone", O_RWNN, 0)) >= 0) {
    *ttydev = (void *) ptsname(pfd);
    return pfd;
  }
#endif

#ifdef PTYS_ARE_NUMERIC
  {
    int idx;
    char *c1, *c2;
    char pty_name[] = "/dev/ptyp???";
    char tty_name[] = "/dev/ttyp???";

    c1 = &(pty_name[sizeof(pty_name) - 4]);
    c2 = &(tty_name[sizeof(tty_name) - 4]);
    for (idx = 0; idx < 256; idx++) {
      snprintf1(c1, strlen(c1), "%d", idx);
      snprintf1(c2, strlen(c2), "%d", idx);
      if (access(tty_name, F_OK) < 0) {
        idx = 256;
        break;
      }
      if ((pfd = open(pty_name, O_RWNN, 0)) >= 0) {
        if (access(tty_name, R_OK | W_OK) == 0) {
          *ttydev = (void *) strdup(tty_name);
          return pfd;
        }
        close(pfd);
      }
    }
  }
#endif
#ifdef PTYS_ARE_SEARCHED
  {
    const char *c1, *c2;
    char pty_name[] = "/dev/pty??";
    char tty_name[] = "/dev/tty??";

# ifndef PTYCHAR1
#  define PTYCHAR1      "pqrstuvwxyz"
# endif
# ifndef PTYCHAR2
#  define PTYCHAR2      "0123456789abcdef"
# endif
    for (c1 = PTYCHAR1; *c1; c1++) {
      pty_name[(sizeof(pty_name) - 3)] =
        tty_name[(sizeof(pty_name) - 3)] = *c1;
      for (c2 = PTYCHAR2; *c2; c2++) {
        pty_name[(sizeof(pty_name) - 2)] =
          tty_name[(sizeof(pty_name) - 2)] = *c2;
        if ((pfd = open(pty_name, O_RWNN, 0)) >= 0) {
          if (access(tty_name, R_OK | W_OK) == 0) {
            *ttydev = (void *) strdup(tty_name);
            return pfd;
          }
          close(pfd);
        }
      }
    }
  }
#endif
  return -1;
}



/* Returns tty file descriptor, or -1 on failure  */
static int
ptytty_get_tty(const char *ttydev)
{
  return open(ttydev, O_RDWR | O_NOCTTY, 0);
}


/* Make our tty a controlling tty so that /dev/tty points to us */
int
ptytty_control_tty(int fd_tty, const void *ttydev)
{

  

  DPRINTF3(DEBUG_TERMIO, "pid: %d, tty fd: %d, dev: %s", getpid(), fd_tty,
           (char *) ttydev);
  DPRINTF2(DEBUG_TERMIO, "tcgetpgrp(): %d  getpgrp(): %d", tcgetpgrp(fd_tty),
           getpgrp());



  
  /* ------------------- Become leader of our own session:   --------------------- */
# ifdef HAVE_SETSID
  {
    pid_t ATTR_ONLY_FOR_DEBUGGING ret = setsid();

    DPRINTF2(DEBUG_TERMIO, "setsid() returned %d %s", (int) ret,
             ERRMSG(ret < 0));
  }
# endif

  /* ------------------- Get controlling tty                 --------------------- */
#ifndef __QNX__  /* in QNX, the only way to get a controlling tty is at process creation time, using
                    qnx_spawn() instead of fork(). I'm too lazy to re-write my_pty_fork(), so I let rlwrap
                    soldier on without a controlling tty */ 
  {
    int fd;
    
# ifdef TIOCNOTTY
    fd = open("/dev/tty", O_RDWR | O_NOCTTY);
    DPRINTF1(DEBUG_TERMIO, "Voiding tty associations: previous=%s",
             fd < 0 ? "no" : "yes");
    if (fd >= 0) {
      int ATTR_ONLY_FOR_DEBUGGING ret = ioctl(fd, TIOCNOTTY, NULL);       /* void tty associations */

      DPRINTF2(DEBUG_TERMIO, "ioctl(..., TIOCNOTTY): %d %s", ret,
               ERRMSG(ret < 0));
      close(fd);
    }
# endif
    /* ---------------------------------------- */
    fd = open("/dev/tty", O_RDWR | O_NOCTTY);
    DPRINTF1(DEBUG_TERMIO,
             "ptytty_control_tty(): /dev/tty has controlling tty? %s",
             fd < 0 ? "no (good)" : "yes (bad)");
    if (fd >= 0)
      close(fd);                  /* ouch: still have controlling tty */

    /* ---------------------------------------- */

#ifdef HAVE_ISASTREAM
    if (isastream(fd_tty) == 1) {
#  if defined(I_SWROPT) 
        ioctl(fd_tty, I_SWROPT, 0);
#  endif      
#  if defined(PTYS_ARE_PTMX) && defined(I_PUSH)
    /*
     * Push STREAMS modules:
     *    ptem: pseudo-terminal hardware emulation module.
     *    ldterm: standard terminal line discipline.
     *    ttcompat: V7, 4BSD and XENIX STREAMS compatibility module.
     *
     * After we push the STREAMS modules, the first open() on the slave side
     * (i.e. the next section between the dashes giving us "tty opened OK")
     * should make the "ptem" (or "ldterm" depending upon either which OS
     * version or which set of manual pages you have) module give us a
     * controlling terminal.  We must already have close()d the master side
     * fd in this child process before we push STREAMS modules on because the
     * documentation is really unclear about whether it is any close() on
     * the master side or the last close() - i.e. a proper STREAMS dismantling
     * close() - on the master side which causes a hang up to be sent
     * through - Geoff Wing
     */
     
        DPRINTF0(DEBUG_TERMIO, "Pushing STREAMS modules");
        ioctl(fd_tty, I_PUSH, "ptem");
        ioctl(fd_tty, I_PUSH, "ldterm");
        ioctl(fd_tty, I_PUSH, "ttcompat");
      
#  endif
    }    
#endif        
    /* ---------------------------------------- */
# if defined(TIOCSCTTY)
    fd = ioctl(fd_tty, TIOCSCTTY, NULL);
    DPRINTF2(DEBUG_TERMIO, "ioctl(..,TIOCSCTTY): %d %s", fd, ERRMSG(fd < 0));
# elif defined(TIOCSETCTTY)
    fd = ioctl(fd_tty, TIOCSETCTTY, NULL);
    DPRINTF2(DEBUG_TERMIO, "ioctl(..,TIOCSETCTTY): %d %s", fd, ERRMSG(fd < 0));
# else
    fd = open(ttydev, O_RDWR);
    DPRINTF2(DEBUG_TERMIO, "tty open%s %s", (fd < 0 ? " failure" : "ed OK"),
             ERRMSG(fd < 0));
    if (fd >= 0)
      close(fd);
# endif
    /* ---------------------------------------- */
    fd = open("/dev/tty", O_WRONLY);
    DPRINTF2(DEBUG_TERMIO, "do we have controlling tty now: %s %s",
             (fd < 0 ? "no (fatal)" : "yes (good)"), ERRMSG(fd < 0));
    if (fd < 0)
      myerror(WARNING|USE_ERRNO, "Could not get controlling terminal for %s", program_name);  /* mywarn called from child */
    else
      close(fd);

    /* ---------------------------------------- */
    DPRINTF2(DEBUG_TERMIO, "tcgetpgrp(): %d  getpgrp(): %d", tcgetpgrp(fd_tty),
             getpgrp());
    /* ---------------------------------------- */
  }
#endif /* ! __QNX__ */
  return 0;
}




int
ptytty_openpty(int *amaster, int *aslave, const void **name)
{
  const void  *scrap = mysavestring("");

  *aslave = -1;
  *amaster = ptytty_get_pty(aslave, &scrap);
  if (*amaster < 0)
    myerror(FATAL|USE_ERRNO, "Could not open master pty");
  if (*aslave < 0)
    *aslave = ptytty_get_tty(scrap);
  if (*aslave < 0)
    myerror(FATAL|USE_ERRNO, "Could not open slave pty %s", scrap);
  else 
    if (name != NULL)
      *  name = scrap;

  return 0;
}

#endif /* HAVE_LIBPTYTTY */

