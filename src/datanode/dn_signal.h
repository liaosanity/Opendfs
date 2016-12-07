#ifndef DN_SIGNAL_H
#define DN_SIGNAL_H

#define SIGNAL_RECONF     SIGHUP
#define SIGNAL_QUIT       SIGQUIT
#define SIGNAL_KILL       SIGKILL
#define SIGNAL_TERMINATE  SIGTERM
#define SIGNAL_TEST_STORE (__SIGRTMIN + 10)

int dn_signal_setup();

#endif

