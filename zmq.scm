;; guile-zmq
;; Copyright (C) 2011 Andy Wingo <wingo at pobox dot com>

;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU Lesser General Public License as
;; published by the Free Software Foundation; either version 3 of the
;; License, or (at your option) any later version.
;;                                                                  
;; This program is distributed in the hope that it will be useful, but
;; WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
;; Lesser General Public License for more details.
;;                                                                  
;; You should have received a copy of the GNU Lesser General Public
;; License along with this program; if not, contact:
;;
;; Free Software Foundation, Inc.     Voice:  +1-617-542-5942
;; 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652
;; Boston, MA  02110-1301,  USA       gnu@gnu.org

;;; Commentary:
;;
;; This is the zmq wrapper for Guile.
;;
;; See the zmq documentation for more details.
;;
;;; Code:

(define-module (zmq)
  #:use-module (zmq config)
  #:use-module (rnrs bytevectors)
  #:export (zmq-version

            zmq-init
            zmq-term

            zmq-socket
            zmq-close
            ZMQ_PAIR
            ZMQ_PUB
            ZMQ_SUB
            ZMQ_REQ
            ZMQ_REP
            ZMQ_XREQ
            ZMQ_XREP
            ZMQ_PULL
            ZMQ_PUSH
            ZMQ_XPUB
            ZMQ_XSUB
            ZMQ_UPSTREAM
            ZMQ_DOWNSTREAM

            zmq-bind
            zmq-connect

            zmq-setsockopt
            zmq-getsockopt
            ZMQ_HWM
            ZMQ_SWAP
            ZMQ_AFFINITY
            ZMQ_IDENTITY
            ZMQ_SUBSCRIBE
            ZMQ_UNSUBSCRIBE
            ZMQ_RATE
            ZMQ_RECOVERY_IVL
            ZMQ_MCAST_LOOP
            ZMQ_SNDBUF
            ZMQ_RCVBUF
            ZMQ_RCVMORE
            ZMQ_FD
            ZMQ_EVENTS
            ZMQ_TYPE
            ZMQ_LINGER
            ZMQ_RECONNECT_IVL
            ZMQ_BACKLOG
            ZMQ_RECOVERY_IVL_MSEC
            ZMQ_RECONNECT_IVL_MAX
            ZMQ_MAXMSGSIZE

            zmq-recv
            zmq-send
            ZMQ_NOBLOCK
            ZMQ_SNDMORE))

;; This will export many things
(dynamic-call "scm_init_zmq" (dynamic-link *zmq-lib-path*))

(define (zmq-setsockopt socket name val)
  (%zmq-setsockopt socket name
                   (cond
                    ((bytevector? val) val)
                    ((string? val) (string->utf8 val))
                    ((and (integer? val) (exact? val))
                     (let ((bv (make-bytevector 8)))
                       (bytevector-u64-native-set! bv 0 val)
                       bv))
                    (else
                     (error "bad val" val)))))

(define (zmq-getsockopt socket name)
  (error "unclear how to do this, yo."))

#;
(if (not (member *zmq-documentation-path* documentation-files))
    (set! documentation-files (cons *zmq-documentation-path*
                                    documentation-files)))
