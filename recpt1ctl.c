#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

static void die(const char *msg)
{
  perror(msg);
  exit(EXIT_FAILURE);
}

static char *sockpath;

static void tune(int ch);

int main(int argc, char *argv[])
{
  if (argc < 3) {
    fprintf(stderr, "usage: %s sockpath tune ch\n", argv[0]);
    return 1;
  }
  sockpath = argv[1];
  const char *cmd = argv[2];

  if (strcmp(cmd, "tune") == 0) {
    if (argc != 4) {
      fprintf(stderr, "tune command requires channel\n");
      return 1;
    }
    const char *arg = argv[3];
    char *endptr = NULL;
    int ch = strtol(arg, &endptr, 10);
    if (*endptr != '\0') {
      fprintf(stderr, "invalid channel: %s\n", arg);
    } else {
      tune(ch);
    }
  } else {
    fprintf(stderr, "unknown command %s\n", cmd);
    return 1;
  }
  return 0;
}

static void tune(int ch)
{
  int fd = socket(PF_UNIX, SOCK_STREAM, 0);
  if (fd == -1) {
    die("socket");
  }
  struct sockaddr_un sun;
  sun.sun_family = AF_UNIX;
  snprintf(sun.sun_path, sizeof sun.sun_path, "%s", sockpath);
  socklen_t len = sizeof sun.sun_family + strlen(sun.sun_path);
  if (connect(fd, (struct sockaddr *)&sun, len) == -1) {
    die("cocnnect");
  }

  char buf[32];
  int n = snprintf(buf, sizeof buf, "tune %d\n",ch);
  send(fd, buf, n, 0);
}
