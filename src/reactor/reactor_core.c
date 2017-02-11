#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <poll.h>

#include <dynamic.h>

#include "reactor_user.h"
#include "reactor_core.h"

typedef struct reactor_core reactor_core;
struct reactor_core
{
  vector polls;
  vector users;
};

static __thread struct reactor_core core = {0};

static void reactor_core_grow(size_t size)
{
  size_t current;

  current = vector_size(&core.polls);
  if (size > current)
    {
      vector_insert_fill(&core.polls, current, size - current, (struct pollfd[]) {{.fd = -1}});
      vector_insert_fill(&core.users, current, size - current, (reactor_user[]) {{0}});
    }
}

static void reactor_core_shrink(void)
{
  while (vector_size(&core.polls) && (((struct pollfd *) vector_back(&core.polls))->fd == -1))
    {
      vector_pop_back(&core.polls);
      vector_pop_back(&core.users);
    }
}

void reactor_core_construct()
{
  vector_construct(&core.polls, sizeof (struct pollfd));
  vector_construct(&core.users, sizeof (reactor_user));
}

void reactor_core_destruct()
{
  vector_destruct(&core.polls);
  vector_destruct(&core.users);
}

void reactor_core_register(int fd, reactor_user_callback *callback, void *state, int events)
{
  reactor_core_grow(fd + 1);
  *(struct pollfd *) reactor_core_poll(fd) = (struct pollfd) {.fd = fd, .events = events};
  *(reactor_user *) reactor_core_user(fd) = (reactor_user) {.callback = callback, .state = state};
}

void reactor_core_deregister(int fd)
{
  *(struct pollfd *) reactor_core_poll(fd) = (struct pollfd) {.fd = -1};
  reactor_core_shrink();
}

void *reactor_core_poll(int fd)
{
  return vector_at(&core.polls, fd);
}

void *reactor_core_user(int fd)
{
  return vector_at(&core.users, fd);
}

int reactor_core_run(void)
{
  int e;
  size_t i;
  struct pollfd *pollfd;

  while (vector_size(&core.polls))
    {
      e = poll(vector_data(&core.polls), vector_size(&core.polls), -1);
      if (e == -1)
        return -1;

      for (i = 0; i < vector_size(&core.polls); i ++)
        {
          pollfd = vector_at(&core.polls, i);
          if (pollfd->revents)
            reactor_user_dispatch(vector_at(&core.users, i), REACTOR_CORE_EVENT_POLL, pollfd);
        }
    }

  return 0;
}
