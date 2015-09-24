#include <cstdlib>
#include <cstdio>
#include <err.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char **argv)
{
  if (argc < 2) {
    fprintf(stderr, "command missing\n");
    exit(1);
  }
  int pipes[2];
  if (pipe(pipes) != 0) {
    err(1, "popen failed");
  }
  pid_t pid = fork();
  if (pid == -1) {
    err(1, "fork failed");
  } else if (pid == 0) {
    if (dup2(pipes[0], 0) != 0) {
      err(1, "dup failed");
    }
    if (execvp(argv[1], argv+1) != 0)
      err(1, "execvp failed");
  }
  int status;
  if (wait(&status) != pid) {
    err(1, "wait failed");
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    return WTERMSIG(status) & 128;
  } else {
    err(1, "unknown status from wait");
  }
}
