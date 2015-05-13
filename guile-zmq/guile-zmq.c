/* guile-zmq
 * Copyright (C) 2011, 2012 Andy Wingo <wingo@pobox.com>
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
	#if 0
	/* -- MSW -- disabled -- we don't allow scheme to shutdown our context! */
  void *ctx = (void*)SCM_SMOB_DATA (obj);
  if (ctx)
    { fprintf (stderr, "terminating %p\n", ctx);
      SCM_SET_SMOB_DATA (obj, 0);
      zmq_term (ctx);
    }
  #endif
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
	#if 0
	/* -- MSW -- disabled -- we don't allowe scheme to close our sockets implictly */
  void *sock = (void*)SCM_SMOB_DATA (obj);

  if (sock)
    {
      zmq_close (sock);
      SCM_SET_SMOB_DATA (obj, 0);
    }
  
  #endif
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

SCM_DEFINE (scm_zmq_wrap_context, "zmq-wrap-context", 0, 1, 0,
		(SCM ptr),
		"Wrap a foreign pointer representing a ZMQ Context in a scheme variable for use with the library.")
#define FUNC_NAME s_scm_zmq_wrap_context
{
	void *ctx;
	if (SCM_UNBNDP(ptr))
		scm_zmq_error(FUNC_NAME);

	ctx = scm_to_pointer(ptr);
	if (!ctx)
		scm_zmq_error(FUNC_NAME);

  return scm_from_zmq_context (ctx);
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


SCM_DEFINE (scm_zmq_msg_send, "zmq-msg-send", 2, 1, 0,
	    (SCM socket, SCM vec, SCM flags),
	    "Send @var{vec}, a vector of bytevectors, as one frame per bv over @var{socket}.")
#define FUNC_NAME s_scm_zmq_msg_send
{
  void *s;
  int cflags, i, len, curflag;
  SCM bv;
  
  s = scm_to_zmq_socket (socket);
  SCM_VALIDATE_VECTOR (2, vec);
  if (SCM_UNBNDP (flags))
    cflags = 0;
  else
    cflags = scm_to_int (flags);

  /* we're controlling setting of ZMQ_SNDMORE ourselves */
  cflags &= ~ZMQ_SNDMORE;
  
  len = scm_to_int(scm_vector_length(vec));
  for (i = 0; i < len; ++i) {
	  bv = scm_vector_ref(vec, scm_from_int(i));
	  zmq_msg_t msg;
	  if (zmq_msg_init_size (&msg, SCM_BYTEVECTOR_LENGTH (bv)))
		  scm_zmq_error (FUNC_NAME);

	  memcpy (zmq_msg_data (&msg), SCM_BYTEVECTOR_CONTENTS (bv), 
			  SCM_BYTEVECTOR_LENGTH (bv));

	  curflag = cflags;
	  if (i != len-1)
		  curflag |= ZMQ_SNDMORE;
	  if (zmq_msg_send (&msg, s, curflag) < 0) {
		  int errno_save = errno;
		  zmq_msg_close (&msg);
		  errno = errno_save;
		  scm_zmq_error (FUNC_NAME);
	  } 
	  zmq_msg_close (&msg);
  }

  return scm_from_int(len);
}
#undef FUNC_NAME


SCM_DEFINE (scm_zmq_msg_recv, "zmq-msg-recv", 1, 1, 0,
	    (SCM socket, SCM flags),
	    "Receive a message from @var{socket}.")
#define FUNC_NAME s_scm_zmq_msg_recv
{
  void *s;
  int cflags, rc, thereismore=1;
  SCM ret, msg_contents;
  
  s = scm_to_zmq_socket (socket);
  if (SCM_UNBNDP (flags))
    cflags = 0;
  else
    cflags = scm_to_int (flags);
  
  ret = SCM_EOL;
  do {
	  zmq_msg_t msg;
	  if (zmq_msg_init (&msg))
		  scm_zmq_error (FUNC_NAME);

	  rc=zmq_msg_recv (&msg, s, cflags);
	  if (rc < 0 && !(cflags & ZMQ_DONTWAIT && errno == EAGAIN))
		  scm_zmq_error (FUNC_NAME);
	  else if (rc < 0 && (cflags & ZMQ_DONTWAIT && errno == EAGAIN))
		  scm_throw(scm_from_locale_symbol("zmq-error"), scm_list_1(scm_from_locale_symbol("EAGAIN")));

	  msg_contents = scm_c_make_bytevector (zmq_msg_size (&msg));
	  memcpy (SCM_BYTEVECTOR_CONTENTS (msg_contents), zmq_msg_data (&msg),
			  zmq_msg_size (&msg));
	  ret = scm_cons(msg_contents, ret);
	  thereismore = zmq_msg_more(&msg);
	  zmq_msg_close (&msg);
  } while (thereismore);

  ret = scm_vector(scm_reverse(ret));

  return ret;
}
#undef FUNC_NAME

SCM_DEFINE (scm_zmq_poll, "zmq-poll", 1, 1, 0,
		(SCM pollitems, SCM timeout),
		"Poll items in @var{pollitems}, waiting at most @var{timeout} ms (-1 = eternally, default = 0) for described activity.")
#define FUNC_NAME s_scm_zmq_poll
{
	SCM pollitem, iter, pi_socket, pi_fdesc, pi_mode;
	long t_o;
	int i,nitems = 0, il, pi_type=0, pik = 0;
	zmq_pollitem_t *pi = NULL;

	if (SCM_UNBNDP(timeout)) 
		t_o=0L;
	else
		t_o = scm_to_long(timeout);

	if (!scm_is_pair(pollitems))
		scm_zmq_error(FUNC_NAME);

	nitems = scm_to_int(scm_length(pollitems));
	if (!nitems)
		scm_zmq_error(FUNC_NAME);

	pi = calloc(nitems, sizeof(zmq_pollitem_t));

	/* look for (socket? fdesc? mode) lists
	 * either (list #f fdesc mode) or (list socket #f mode)
	 */
	for (i=0, iter=pollitems; i < nitems && scm_is_pair(iter) && !scm_is_null(iter); iter=scm_cdr(iter), ++i, ++pik) {
		pollitem = scm_car(iter);
		pi_type = 0;
		if (!scm_is_pair(pollitem) || scm_is_null(pollitem))
			scm_zmq_error(FUNC_NAME);
		il=scm_to_int(scm_length(pollitem));
		if (il != 3)
			scm_zmq_error(FUNC_NAME);
		pi_socket = scm_car(pollitem);
		pi_fdesc = scm_cadr(pollitem);
		pi_mode = scm_caddr(pollitem);	
		if (scm_is_true(scm_boolean_p(pi_socket))) {
			if (scm_is_true(pi_socket))
				scm_zmq_error(FUNC_NAME);
			else
				pi_type = 1; /* fdesc */
		}
		if (scm_is_true(scm_boolean_p(pi_fdesc))) {
			if (scm_is_true(pi_fdesc))
				scm_zmq_error(FUNC_NAME);
			else
				pi_type = 2; /* socket */
		}
		switch (pi_type) {
			case 1: /* listen on passed fdesc */
				if (!scm_is_integer(pi_fdesc))
					scm_zmq_error(FUNC_NAME);
				pi[pik].fd = scm_to_int(pi_fdesc);
				break;
			case 2: /* listen on passed (wrapped) zmq socket */
				if (!SCM_SMOB_PREDICATE(scm_tc16_zmq_socket, pi_socket))
					scm_zmq_error(FUNC_NAME);
				pi[pik].socket = scm_to_zmq_socket(pi_socket);
				break;
			default:
				scm_zmq_error(FUNC_NAME);
				break;
		}
		if (!scm_is_integer(pi_mode))
			scm_zmq_error(FUNC_NAME);
		pi[pik].events = scm_to_int(pi_mode);
	}

	il=0;
	for (i=0; i < nitems; ++i) {
		if (pi[i].fd > 0 || pi[i].socket != NULL) {
			il=1;
			break;
		}
	}
	if (!il)
		scm_zmq_error(FUNC_NAME);

	i=zmq_poll(pi, nitems, t_o);

	if (i < 0) {
		scm_zmq_error(FUNC_NAME);
	}
	if (i == 0) {
		free(pi);
		return SCM_EOL;
	}

	pollitem = scm_make_list(scm_from_int(i), SCM_EOL);
	pik=0;
	for (il=0; il < nitems; ++il) {
		if (pi[il].revents != 0) { /* something happened here */
			pi_mode = scm_from_int(pi[il].revents);
			pi_fdesc = (pi[il].fd  ? scm_from_int(pi[il].fd) : SCM_BOOL_F);
			pi_socket = (pi[il].socket ? scm_from_zmq_socket(pi[il].socket) : SCM_BOOL_F);
			iter = scm_list_3(pi_socket, pi_fdesc, pi_mode);
			scm_list_set_x(pollitem, scm_from_int(pik), iter);
			++pik;
		}
	}

	free(pi);
	return pollitem;
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

    scm_c_define ("ZMQ_AFFINITY", scm_from_int (ZMQ_AFFINITY));
    scm_c_define ("ZMQ_IDENTITY", scm_from_int (ZMQ_IDENTITY));
    scm_c_define ("ZMQ_SUBSCRIBE", scm_from_int (ZMQ_SUBSCRIBE));
    scm_c_define ("ZMQ_UNSUBSCRIBE", scm_from_int (ZMQ_UNSUBSCRIBE));
    scm_c_define ("ZMQ_RATE", scm_from_int (ZMQ_RATE));
    scm_c_define ("ZMQ_RECOVERY_IVL", scm_from_int (ZMQ_RECOVERY_IVL));
    scm_c_define ("ZMQ_SNDBUF", scm_from_int (ZMQ_SNDBUF));
    scm_c_define ("ZMQ_RCVBUF", scm_from_int (ZMQ_RCVBUF));
    scm_c_define ("ZMQ_RCVMORE", scm_from_int (ZMQ_RCVMORE));
    scm_c_define ("ZMQ_FD", scm_from_int (ZMQ_FD));
    scm_c_define ("ZMQ_EVENTS", scm_from_int (ZMQ_EVENTS));
    scm_c_define ("ZMQ_TYPE", scm_from_int (ZMQ_TYPE));
    scm_c_define ("ZMQ_LINGER", scm_from_int (ZMQ_LINGER));
    scm_c_define ("ZMQ_BACKLOG", scm_from_int (ZMQ_BACKLOG));
    scm_c_define ("ZMQ_RECONNECT_IVL_MAX", scm_from_int (ZMQ_RECONNECT_IVL_MAX));

	scm_c_define ("ZMQ_POLLIN", scm_from_int(ZMQ_POLLIN));
	scm_c_define ("ZMQ_POLLOUT", scm_from_int(ZMQ_POLLOUT));
	scm_c_define ("ZMQ_POLLERR", scm_from_int(ZMQ_POLLERR));
	scm_c_define ("ZMQ_DONTWAIT", scm_from_int(ZMQ_DONTWAIT));

    
    scm_c_define ("ZMQ_NOBLOCK", scm_from_int (ZMQ_NOBLOCK));
    scm_c_define ("ZMQ_SNDMORE", scm_from_int (ZMQ_SNDMORE));

    initialized = 1;
}
