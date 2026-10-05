#include <unistd.h>
#include <stdio.h>

const char *LOG_FILENAME = "log";
static int glob_counter= 0;
int main() {

  int pid = getpid();
  // printf("%d",pid);

  // Запускает таймер и раз в 300 мс увеличивает счетчик на 1
  // тут наверное нужно форкнуть и завести статик переменную
  // а в форкнутом процессе просто в цикле sleep на 300 мсек и увеличить переменку
}
