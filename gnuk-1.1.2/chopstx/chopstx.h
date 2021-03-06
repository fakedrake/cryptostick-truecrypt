/*
 * chopstx.h - Threads and only threads.
 *
 * Copyright (C) 2013 Flying Stone Technology
 * Author: NIIBE Yutaka <gniibe@fsij.org>
 *
 * This file is a part of Chopstx, a thread library for embedded.
 *
 * Chopstx is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Chopstx is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As additional permission under GNU GPL version 3 section 7, you may
 * distribute non-source form of the Program without the copy of the
 * GNU GPL normally required by section 4, provided you inform the
 * receipents of GNU GPL by a written offer.
 *
 */

typedef uint32_t chopstx_t;
typedef uint8_t chopstx_prio_t;

extern chopstx_t chopstx_main;

/* NOTE: This signature is different to PTHREAD's one.  */
chopstx_t
chopstx_create (uint32_t flags_and_prio,
		uint32_t stack_addr, size_t stack_size,
		void *(thread_entry) (void *), void *);
#define CHOPSTX_PRIO_BITS 8
#define CHOPSTX_DETACHED 0x10000
#define CHOPSTX_SCHED_RR 0x20000

#define CHOPSTX_PRIO_INHIBIT_PREEMPTION 248

void chopstx_usec_wait_var (uint32_t *arg);

void chopstx_usec_wait (uint32_t usec);

struct chx_spinlock {
  /* nothing for uniprocessor.  */
};

typedef struct chx_mtx {
  struct {
    struct chx_thread *next, *prev;
  } q;
  struct chx_spinlock lock;
  struct chx_thread *owner;
  struct chx_mtx *list;
} chopstx_mutex_t;

/* NOTE: This signature is different to PTHREAD's one.  */
void chopstx_mutex_init (chopstx_mutex_t *mutex);

void chopstx_mutex_lock (chopstx_mutex_t *mutex);

void chopstx_mutex_unlock (chopstx_mutex_t *mutex);

typedef struct chx_cond {
  struct {
    struct chx_thread *next, *prev;
  } q;
  struct chx_spinlock lock;
} chopstx_cond_t;

/* NOTE: This signature is different to PTHREAD's one.  */
void chopstx_cond_init (chopstx_cond_t *cond);

void chopstx_cond_wait (chopstx_cond_t *cond, chopstx_mutex_t *mutex);
void chopstx_cond_signal (chopstx_cond_t *cond);
void chopstx_cond_broadcast (chopstx_cond_t *cond);

typedef struct chx_intr {
  struct chx_intr *next;
  struct chx_spinlock lock;
  struct chx_thread *tp;
  uint32_t ready;
  uint8_t irq_num;
} chopstx_intr_t;

void chopstx_claim_irq (chopstx_intr_t *intr, uint8_t irq_num);
void chopstx_release_irq (chopstx_intr_t *intr);

void chopstx_intr_wait (chopstx_intr_t *intr);

/*
 * Library provides default implementation as weak reference.
 * User can replace it.
  */
void chx_fatal (uint32_t err_code) __attribute__((__weak__, __noreturn__));

void chopstx_join (chopstx_t, void **);
void chopstx_exit (void *retval) __attribute__((__noreturn__));


enum {
  CHOPSTX_ERR_NONE = 0,
  CHOPSTX_ERR_THREAD_CREATE,
  CHOPSTX_ERR_JOIN,
};

enum {
  CHOPSTX_EXIT_SUCCESS = 0,
  CHOPSTX_EXIT_CANCELED         = 256,
  CHOPSTX_EXIT_CANCELED_IN_SYNC = 257,
};

void chopstx_cancel (chopstx_t thd);
void chopstx_testcancel (void);

struct chx_cleanup {
  struct chx_cleanup *next;
  void (*routine) (void *);
  void *arg;
};

/* NOTE: This signature is different to PTHREAD's one.  */
void chopstx_cleanup_push (struct chx_cleanup *clp);
void chopstx_cleanup_pop (int execute);


void chopstx_wakeup_usec_wait (chopstx_t thd);

#define CHOPSTX_THREAD_SIZE 60
