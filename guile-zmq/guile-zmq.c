/* guile-zmq
 * Copyright (C) 2011 Andy Wingo <wingo@pobox.com>
 *
 * guile-zmq.c: 0MQ for Guile
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *                                                                  
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *                                                                  
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, contact:
 *
 * Free Software Foundation, Inc.     Voice:  +1-617-542-5942
 * 51 Franklin St, Fifth Floor        Fax:    +1-617-542-2652
 * Boston, MA  02110-1301,  USA       gnu@gnu.org
 */

#include <libguile.h>

#include <zmq.h>

#include "guile-zmq.h"


static scm_t_bits scm_tc16_zmq_context;
static scm_t_bits scm_tc16_zmq_socket;

/* Map sockets to contexts, to keep contexts alive.  Also, allows atexit() to
   close sockets and terminate contexts, as is done with ports.  */
static SCM all_sockets = SCM_BOOL_F;
static scm_i_pthread_mutex_t all_sockets_lock = SCM_I_PTHREAD_MUTEX_INITIALIZER;


static void
register_socket (SCM sock, SCM ctx)
{
  scm_i_pthread_mutex_lock (&all_sockets_lock);
  scm_hashq_set_x (all_sockets, sock, ctx);
  scm_i_pthread_mutex_unlock (&all_sockets_lock);
}


static SCM scm_from_zmq_context (void *context)
{ 
  SCM_RETURN_NEWSMOB (scm_tc16_zmq_context, context);
}

static void* scm_to_zmq_context (SCM obj)
{ 
  void *ret;

  SCM_ASSERT (SCM_SMOB_PREDICATE (scm_tc16_zmq_context, obj),
              obj, 0, NULL);
  ret = (void*)SCM_SMOB_DATA (obj);

  if (!ret)
    scm_misc_error (NULL, "terminated zmq context: ~a", scm_list_1 (obj));

  return ret;
}

static size_t scm_zmq_context_free (SCM obj)
{
  void *ctx = (void*)SCM_SMOB_DATA (obj);
  if (ctx)
    { fprintf (stderr, "terminating %p\n", ctx);
      SCM_SET_SMOB_DATA (obj, 0);
      zmq_term (ctx);
    }
  return 0;
}


static SCM scm_from_zmq_socket (void *socket)
{ 
  SCM_RETURN_NEWSMOB (scm_tc16_zmq_socket, socket);
}

static void* scm_to_zmq_socket (SCM obj)
{ 
  void *ret;

  SCM_ASSERT (SCM_SMOB_PREDICATE (scm_tc16_zmq_socket, obj),
              obj, 0, NULL);
  ret = (void*)SCM_SMOB_DATA (obj);

  if (!ret)
    scm_misc_error (NULL, "closed zmq socket: ~a", scm_list_1 (obj));

  return ret;
}

static size_t scm_zmq_socket_free (SCM obj)
{
  void *sock = (void*)SCM_SMOB_DATA (obj);

  if (sock)
    {
      zmq_close (sock);
      SCM_SET_SMOB_DATA (obj, 0);
    }
  
  return 0;
}


static void
scm_zmq_error (const char *subr)
{
  int e = errno;
  
  scm_error (scm_from_locale_symbol ("zmq-error"),
             subr,
             zmq_strerror (e),
             SCM_EOL,
             scm_list_1 (scm_from_int (e)));
}


SCM_DEFINE (scm_zmq_version, "zmq-version", 0, 0, 0,
	    (void),
	    "Retrieves the version of the zmq library.")
#define FUNC_NAME s_scm_zmq_version
{
  int major = 0, minor = 0, patch = 0;
  zmq_version (&major, &minor, &patch);
  return scm_values (scm_list_3 (scm_from_int (major),
                                 scm_from_int (minor),
                                 scm_from_int (patch)));
}
#undef FUNC_NAME


SCM_DEFINE (scm_zmq_init, "zmq-init", 0, 1, 0,
            (SCM io_threads),
            "Create a ZeroMQ context with @var{io_threads} threads.")
#define FUNC_NAME s_scm_zmq_init
{
  int threads;
  void *ctx;

  if (SCM_UNBNDP (io_threads))
    threads = 1;
  else
    threads = scm_to_int (io_threads);

  ctx = zmq_init (threads);
  if (!ctx)
    scm_zmq_error (FUNC_NAME);
  
  return scm_from_zmq_context (ctx);
}
#undef FUNC_NAME


SCM_DEFINE (scm_zmq_term, "zmq-term", 1, 0, 0,
            (SCM context),
            "Terminate the ZeroMQ context @var{context}.")
#define FUNC_NAME s_scm_zmq_term
{
  void *ctx = scm_to_zmq_context (context);

  if (zmq_term (ctx))
    scm_zmq_error (FUNC_NAME);
  SCM_SET_SMOB_DATA (context, NULL);
  
  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME


SCM_DEFINE (scm_zmq_socket, "zmq-socket", 2, 0, 0,
	    (SCM context, SCM type),
	    "Create a new ZeroMQ of type @var{type} within context @var{context}.")
#define FUNC_NAME s_scm_zmq_socket
{
  void *ctx, *s;
  int typ;
  SCM ret;
  
  ctx = scm_to_zmq_context (context);
  typ = scm_to_int (type);

  s = zmq_socket (ctx, typ);
  if (!s)
    scm_zmq_error (FUNC_NAME);

  ret = scm_from_zmq_socket (s);
  register_socket (ret, context);

  return ret;
}
#undef FUNC_NAME


SCM_DEFINE (scm_zmq_close, "zmq-close", 1, 0, 0,
	    (SCM socket),
	    "Close @var{socket}.")
#define FUNC_NAME s_scm_zmq_close
{
  void *s;
  
  s = scm_to_zmq_socket (socket);

  if (zmq_close (s))
    scm_zmq_error (FUNC_NAME);
  SCM_SET_SMOB_DATA (socket, NULL);

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME


SCM_DEFINE (scm_zmq_setsockopt, "%zmq-setsockopt", 3, 0, 0,
	    (SCM socket, SCM opt, SCM val),
	    "Set @var{opt} to @var{val} on @var{socket}.")
#define FUNC_NAME s_scm_zmq_setsockopt
{
  void *s;
  int o;
  
  s = scm_to_zmq_socket (socket);
  o = scm_to_int (opt);
  SCM_VALIDATE_BYTEVECTOR (3, val);

  if (zmq_setsockopt (s, o, SCM_BYTEVECTOR_CONTENTS (val),
                      SCM_BYTEVECTOR_LENGTH (val)))
    scm_zmq_error (FUNC_NAME);

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME


SCM_DEFINE (scm_zmq_getsockopt, "%zmq-getsockopt", 3, 0, 0,
	    (SCM socket, SCM opt, SCM val),
	    "Get @var{opt} of @var{socket} into @var{val}.")
#define FUNC_NAME s_scm_zmq_getsockopt
{
  void *s;
  int o;
  size_t len;
  
  s = scm_to_zmq_socket (socket);
  o = scm_to_int (opt);
  SCM_VALIDATE_BYTEVECTOR (3, val);
  len = SCM_BYTEVECTOR_LENGTH (val);

  if (zmq_getsockopt (s, o, SCM_BYTEVECTOR_CONTENTS (val), &len))
    scm_zmq_error (FUNC_NAME);

  if (len != SCM_BYTEVECTOR_LENGTH (val))
    scm_misc_error (FUNC_NAME, "bad length of socket option: ~a",
                    scm_from_size_t (len));

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME


SCM_DEFINE (scm_zmq_bind, "zmq-bind", 2, 0, 0,
	    (SCM socket, SCM addr),
	    "Bind @var{socket} to @var{addr}.")
#define FUNC_NAME s_scm_zmq_bind
{
  void *s;
  char *caddr;
  
  scm_dynwind_begin (0);

  s = scm_to_zmq_socket (socket);
  caddr = scm_to_locale_string (addr);

  scm_dynwind_free (caddr);

  if (zmq_bind (s, caddr))
    scm_zmq_error (FUNC_NAME);

  scm_dynwind_end ();

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME


SCM_DEFINE (scm_zmq_connect, "zmq-connect", 2, 0, 0,
	    (SCM socket, SCM addr),
	    "Connect to @var{addr} on @var{socket}.")
#define FUNC_NAME s_scm_zmq_connect
{
  void *s;
  char *caddr;
  
  scm_dynwind_begin (0);

  s = scm_to_zmq_socket (socket);
  caddr = scm_to_locale_string (addr);

  scm_dynwind_free (caddr);

  if (zmq_connect (s, caddr))
    scm_zmq_error (FUNC_NAME);

  scm_dynwind_end ();

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME


SCM_DEFINE (scm_zmq_send, "zmq-send", 2, 1, 0,
	    (SCM socket, SCM bv, SCM flags),
	    "Send @var{bv} over @var{socket}.")
#define FUNC_NAME s_scm_zmq_send
{
  void *s;
  int cflags;
  zmq_msg_t msg;
  
  s = scm_to_zmq_socket (socket);
  SCM_VALIDATE_BYTEVECTOR (2, bv);
  if (SCM_UNBNDP (flags))
    cflags = 0;
  else
    cflags = scm_to_int (flags);
  
  if (zmq_msg_init_size (&msg, SCM_BYTEVECTOR_LENGTH (bv)))
    scm_zmq_error (FUNC_NAME);
  
  memcpy (zmq_msg_data (&msg), SCM_BYTEVECTOR_CONTENTS (bv),
          SCM_BYTEVECTOR_LENGTH (bv));
  
  if (zmq_send (s, &msg, cflags))
    {
      int errno_save = errno;
      zmq_msg_close (&msg);
      errno = errno_save;
      scm_zmq_error (FUNC_NAME);
    }

  zmq_msg_close (&msg);

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME


SCM_DEFINE (scm_zmq_recv, "zmq-recv", 1, 1, 0,
	    (SCM socket, SCM flags),
	    "Receive a message from @var{socket}.")
#define FUNC_NAME s_scm_zmq_recv
{
  void *s;
  int cflags;
  SCM ret;
  zmq_msg_t msg;
  
  s = scm_to_zmq_socket (socket);
  if (SCM_UNBNDP (flags))
    cflags = 0;
  else
    cflags = scm_to_int (flags);
  
  if (zmq_msg_init (&msg))
    scm_zmq_error (FUNC_NAME);
  
  if (zmq_recv (s, &msg, 0))
    scm_zmq_error (FUNC_NAME);

  ret = scm_c_make_bytevector (zmq_msg_size (&msg));
  memcpy (SCM_BYTEVECTOR_CONTENTS (ret), zmq_msg_data (&msg),
          zmq_msg_size (&msg));

  zmq_msg_close (&msg);

  return ret;
}
#undef FUNC_NAME




void
scm_init_zmq (void)
{
    static int initialized = 0;

    if (initialized)
        return;

    scm_tc16_zmq_context = scm_make_smob_type ("zmq-context", 0);
    scm_set_smob_free (scm_tc16_zmq_context, scm_zmq_context_free);

    scm_tc16_zmq_socket = scm_make_smob_type ("zmq-socket", 0);
    scm_set_smob_free (scm_tc16_zmq_socket, scm_zmq_socket_free);

    all_sockets = scm_make_weak_key_hash_table (SCM_UNDEFINED);

#ifndef SCM_MAGIC_SNARFER
#include "guile-zmq.x"
#endif

    scm_c_define ("ZMQ_PAIR", scm_from_int (ZMQ_PAIR));
    scm_c_define ("ZMQ_PUB", scm_from_int (ZMQ_PUB));
    scm_c_define ("ZMQ_SUB", scm_from_int (ZMQ_SUB));
    scm_c_define ("ZMQ_REQ", scm_from_int (ZMQ_REQ));
    scm_c_define ("ZMQ_REP", scm_from_int (ZMQ_REP));
    scm_c_define ("ZMQ_XREQ", scm_from_int (ZMQ_XREQ));
    scm_c_define ("ZMQ_XREP", scm_from_int (ZMQ_XREP));
    scm_c_define ("ZMQ_PULL", scm_from_int (ZMQ_PULL));
    scm_c_define ("ZMQ_PUSH", scm_from_int (ZMQ_PUSH));
    scm_c_define ("ZMQ_XPUB", scm_from_int (ZMQ_XPUB));
    scm_c_define ("ZMQ_XSUB", scm_from_int (ZMQ_XSUB));
    scm_c_define ("ZMQ_UPSTREAM", scm_from_int (ZMQ_UPSTREAM));
    scm_c_define ("ZMQ_DOWNSTREAM", scm_from_int (ZMQ_DOWNSTREAM));

    scm_c_define ("ZMQ_HWM", scm_from_int (ZMQ_HWM));
    scm_c_define ("ZMQ_SWAP", scm_from_int (ZMQ_SWAP));
    scm_c_define ("ZMQ_AFFINITY", scm_from_int (ZMQ_AFFINITY));
    scm_c_define ("ZMQ_IDENTITY", scm_from_int (ZMQ_IDENTITY));
    scm_c_define ("ZMQ_SUBSCRIBE", scm_from_int (ZMQ_SUBSCRIBE));
    scm_c_define ("ZMQ_UNSUBSCRIBE", scm_from_int (ZMQ_UNSUBSCRIBE));
    scm_c_define ("ZMQ_RATE", scm_from_int (ZMQ_RATE));
    scm_c_define ("ZMQ_RECOVERY_IVL", scm_from_int (ZMQ_RECOVERY_IVL));
    scm_c_define ("ZMQ_MCAST_LOOP", scm_from_int (ZMQ_MCAST_LOOP));
    scm_c_define ("ZMQ_SNDBUF", scm_from_int (ZMQ_SNDBUF));
    scm_c_define ("ZMQ_RCVBUF", scm_from_int (ZMQ_RCVBUF));
    scm_c_define ("ZMQ_RCVMORE", scm_from_int (ZMQ_RCVMORE));
    scm_c_define ("ZMQ_FD", scm_from_int (ZMQ_FD));
    scm_c_define ("ZMQ_EVENTS", scm_from_int (ZMQ_EVENTS));
    scm_c_define ("ZMQ_TYPE", scm_from_int (ZMQ_TYPE));
    scm_c_define ("ZMQ_LINGER", scm_from_int (ZMQ_LINGER));
    scm_c_define ("ZMQ_RECONNECT_IVL", scm_from_int (ZMQ_RECONNECT_IVL));
    scm_c_define ("ZMQ_BACKLOG", scm_from_int (ZMQ_BACKLOG));
    scm_c_define ("ZMQ_RECOVERY_IVL_MSEC", scm_from_int (ZMQ_RECOVERY_IVL_MSEC));
    scm_c_define ("ZMQ_RECONNECT_IVL_MAX", scm_from_int (ZMQ_RECONNECT_IVL_MAX));
    scm_c_define ("ZMQ_MAXMSGSIZE", scm_from_int (ZMQ_MAXMSGSIZE));
    
    scm_c_define ("ZMQ_NOBLOCK", scm_from_int (ZMQ_NOBLOCK));
    scm_c_define ("ZMQ_SNDMORE", scm_from_int (ZMQ_SNDMORE));

    initialized = 1;
}
