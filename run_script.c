#include "common.h"
#include "config.h"
#include "run_script.h"
#include "event.h"
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/wait.h>
#include <cap-ng.h>

int sigfd = -1;

static int read_sigfd(
    int fd,
    int events,
    void *data)
{
  int st;
  pid_t pid;
  struct signalfd_siginfo ssi;
  memset(&ssi, 0, sizeof(ssi));

  if (read(fd, &ssi, sizeof(ssi)) != sizeof(ssi)) {
    warn("read sigfd");
    return -1;
  }

  if (ssi.ssi_signo != SIGCHLD) {
    warn("Invalid signal handler");
    return -1;
  }

  pid = wait(&st);
  if (pid < 0) {
    warn("wait");
    return -1;
  }

  return 0;
}


static void close_sigfd(
    int fd,
    void *data)
{
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGCHLD);
  if (sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
    err(EXIT_FAILURE, "setup_signalhandler sigprocmask");
  close(sigfd);

  return;
}

void run_script_init(
    void)
{
  sigset_t set;
  int fd;

  sigemptyset(&set);
  sigaddset(&set, SIGCHLD);
  if (sigprocmask(SIG_BLOCK, &set, NULL) < 0)
    err(EXIT_FAILURE, "setup_signalhandler sigprocmask");

  fd = signalfd(-1, &set, SFD_CLOEXEC|SFD_NONBLOCK);
  if (fd < 0)
    err(EXIT_FAILURE, "setup_signalhandler signalfd");
  if (event_add_fd(fd, read_sigfd, close_sigfd, NULL, EPOLLIN) < 0)
    err(EXIT_FAILURE, "setup_signalhandler epoll_add_fd");

  sigfd = fd;
  return;
}

void run_script(
    void)
{
  int status;
  pid_t pid;
  config_t *co = config_get();
  if (co->run_script == "")
    return;

  pid = fork();
  if (pid) 
    return;

  chdir("/dev/shm");
  /* Might silently fail. Be OK with this */
  capng_clear(CAPNG_SELECT_BOTH);
  capng_apply(CAPNG_SELECT_BOTH);
  capng_lock();

  if (execl(co->run_script, co->run_script) < 0)
    err(EXIT_FAILURE, "exec(%s)", co->run_script);

  return;
}
