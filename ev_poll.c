/*
 * libev poll fd activity backend
 *
 * Copyright (c) 2007 Marc Alexander Lehmann <libev@schmorp.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <poll.h>

void inline_size
pollidx_init (int *base, int count)
{
  while (count--)
    *base++ = -1;
}

static void
poll_modify (EV_P_ int fd, int oev, int nev)
{
  int idx;

  if (oev == nev)
    return;

  array_needsize (int, pollidxs, pollidxmax, fd + 1, pollidx_init);

  idx = pollidxs [fd];

  if (idx < 0) /* need to allocate a new pollfd */
    {
      pollidxs [fd] = idx = pollcnt++;
      array_needsize (struct pollfd, polls, pollmax, pollcnt, EMPTY2);
      polls [idx].fd = fd;
    }

  assert (polls [idx].fd == fd);

  if (nev)
    polls [idx].events =
        (nev & EV_READ ? POLLIN : 0)
        | (nev & EV_WRITE ? POLLOUT : 0);
  else /* remove pollfd */
    {
      pollidxs [fd] = -1;

      if (expect_true (idx < --pollcnt))
        {
          polls [idx] = polls [pollcnt];
          pollidxs [polls [idx].fd] = idx;
        }
    }
}

static void
poll_poll (EV_P_ ev_tstamp timeout)
{
  int i;
  int res = poll (polls, pollcnt, (int)ceil (timeout * 1000.));

  if (expect_false (res < 0))
    {
      if (errno == EBADF)
        fd_ebadf (EV_A);
      else if (errno == ENOMEM && !syserr_cb)
        fd_enomem (EV_A);
      else if (errno != EINTR)
        syserr ("(libev) poll");

      return;
    }

  for (i = 0; i < pollcnt; ++i)
    if (expect_false (polls [i].revents & POLLNVAL))
      fd_kill (EV_A_ polls [i].fd);
    else
      fd_event (
        EV_A_
        polls [i].fd,
        (polls [i].revents & (POLLOUT | POLLERR | POLLHUP) ? EV_WRITE : 0)
        | (polls [i].revents & (POLLIN | POLLERR | POLLHUP) ? EV_READ : 0)
      );
}

int inline_size
poll_init (EV_P_ int flags)
{
  backend_fudge  = 0.; /* posix says this is zero */
  backend_modify = poll_modify;
  backend_poll   = poll_poll;

  pollidxs = 0; pollidxmax = 0;
  polls    = 0; pollmax    = 0; pollcnt = 0;

  return EVBACKEND_POLL;
}

void inline_size
poll_destroy (EV_P)
{
  ev_free (pollidxs);
  ev_free (polls);
}

