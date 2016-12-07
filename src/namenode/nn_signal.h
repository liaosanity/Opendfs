#ifndef NN_SIGNAL_H
#define NN_SIGNAL_H

#define SIGNAL_RECONF     SIGHUP
#define SIGNAL_QUIT       SIGQUIT
#define SIGNAL_KILL       SIGKILL
#define SIGNAL_TERMINATE  SIGTERM
#define SIGNAL_TEST_STORE (__SIGRTMIN + 10)

int signal_setup();

#endif

