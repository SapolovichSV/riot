#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
const char *LOG_FILENAME = "log";
#define SHM_COUNTER "/counter"
#define SEM_COUNTER "/counter_sem"
#define PRIME_FLAG "/prime"
/* Написать на C\C++\Zig без использования сторонних библиотек программу для
 Linux, которая: 1) При старте открывает лог-файл и пишет в него строку со своим
 идентификатором процесса и временем запуска. 2) Запускает таймер и раз в 300 мс
 увеличивает счетчик на 1 3) Позволяет пользователю через интерфейс командной
 строки установить любое значение счетчика 4) Раз в 1 секунду пишет в лог-файл
 текущее время (дата, часы, минуты, секунды, миллисекунды), свой идентификатор
 процесса и значение счетчика
5) Раз в 3 секунды запускает 2 своих копии: a.
 Копия 1 записывает в тот же лог файл строку со своим идентификатором процесса,
 временем своего запуска, увеличивает текущее значение счетчика на 10,
 записывает в лог файл время выхода и завершается. b. Копия 2 записывает в тот
 же лог файл строку со своим идентификатором процесса, временем своего запуска,
 увеличивает текущее значение счетчика в 2 раза, через 2 секунды уменьшает
 значение счетчика в 2 раза, записывает в лог файл время выхода и завершается c.
 Если какая-либо копия к следующему циклу запуска еще не завершила свою работу,
 программа не запускает копию, а пишет в лог сообщение.
 6) Пользователь может
 запустить любое количество программ. В этом случае только одна из программ
 должна писать в лог текущее значение счетчика и порождать копии, но все
 запущенные программы должны модифицировать счетчик раз в 300 мс и по запросу
 пользователя. На проверку: исходные коды программы. */
void first_copy(int *counter, sem_t *sem_counter, int log_fd) {
  pid_t pid = getpid();
  time_t start_time = time(NULL);
  char *c_time = ctime(&start_time);
  char buf[1024];
  int len = snprintf(buf, sizeof(buf), "COPY_1_START pid = %d time= %s\n", pid,
                     c_time);
  write(log_fd, buf, len);

  sem_wait(sem_counter);
  *counter += 10;
  sem_post(sem_counter);

  time_t end_time = time(NULL);
  c_time = ctime(&start_time);
  len =
      snprintf(buf, sizeof(buf), "COPY_1_END pid = %d time %s\n", pid, c_time);
  write(log_fd, buf, len);
}
void second_copy(int *counter, sem_t *sem_counter, int log_fd) {

  pid_t pid = getpid();
  time_t start_time = time(NULL);
  char *c_time = ctime(&start_time);
  char buf[1024];
  int len = snprintf(buf, sizeof(buf), "COPY_2_START pid = %d time= %s\n", pid,
                     c_time);
  write(log_fd, buf, len);

  sem_wait(sem_counter);
  int old_counter = *counter;
  *counter = 2 * old_counter;
  sem_post(sem_counter);

  sleep(2);

  sem_wait(sem_counter);
  old_counter = *counter;
  *counter = old_counter / 2;
  sem_post(sem_counter);

  time_t end_time = time(NULL);
  c_time = ctime(&end_time);
  len =
      snprintf(buf, sizeof(buf), "COPY_2_END pid = %d time %s\n", pid, c_time);
  write(log_fd, buf, len);
}
void copies(int *counter, sem_t *sem_counter, int log_fd, int *first_pid,
            int *second_pid) {

  pid_t first_copy_pid = -1, second_copy_pid = -1;
  switch (first_copy_pid = fork()) {
  case 0:
    first_copy(counter, sem_counter, log_fd);
    exit(0);
  default:
    *first_pid = first_copy_pid;
    break;
  }
  switch (second_copy_pid = fork()) {
  case 0:
    second_copy(counter, sem_counter, log_fd);
    exit(0);
  default:
    *second_pid = second_copy_pid;
    break;
  }
}
void timer_counter(int *counter, sem_t *sem) {
  struct timespec ts = {
      .tv_sec = 0,
      .tv_nsec = 300 * 1000 * 1000,
  };

  while (1) {
    nanosleep(&ts, NULL);
    sem_wait(sem);
    (*counter)++;
    sem_post(sem);
  };
}
void input_handler(int *counter, sem_t *sem) {
  int new_counter;
  scanf("%d", &new_counter);
  sem_wait(sem);
  *counter = new_counter;
  sem_post(sem);
}
void logger(int *counter, sem_t *sem, int log_fd) {
  while (1) {
    sleep(1);
    pid_t pid = getpid();
    time_t now_time = time(NULL);
    char *c_time = ctime(&now_time);

    sem_wait(sem);
    int curr_counter = *counter;
    sem_post(sem);

    char buf[1024];
    int len = snprintf(buf, sizeof(buf), "time = %s pid = %d counter = %d\n",
                       c_time, pid, curr_counter);
    write(log_fd, buf, len);
  }
}
int main(int argc, char *argv[]) {
  int prime_flag = 1;
  shm_open(PRIME_FLAG, O_CREAT | O_EXCL, 0777);
  if (EEXIST == errno) {
    prime_flag = 0;
  }
  printf("prime = %d\n", prime_flag);

  int counter_fd = shm_open(SHM_COUNTER, O_CREAT | O_RDWR, 0777);

  if (counter_fd == -1) {
    // уже есть
    return -1;
  }
  if (prime_flag) {
    ftruncate(counter_fd, sizeof(int));
  }
  int *counter = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED,
                      counter_fd, 0);

  int sem_counter_fd = shm_open(SEM_COUNTER, O_CREAT | O_RDWR, 0777);
  if (sem_counter_fd == -1) {
    return -1;
  }
  if (prime_flag) {
    ftruncate(sem_counter_fd, sizeof(sem_t));
  }
  sem_t *sem_counter = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE,
                            MAP_SHARED, sem_counter_fd, 0);
  if (prime_flag) {
    sem_init(sem_counter, 1, 1);
  }

  // начальный логфайл
  time_t start_time = time(NULL);

  int log_fd = -1;
  if (prime_flag) {
    log_fd = open(LOG_FILENAME, O_RDWR | O_CREAT, 0777);
    if (log_fd == -1) {
      return 1;
    }

    pid_t pid = getpid();
    char buf[1024];
    char *time_str = ctime(&start_time);
    int len = snprintf(buf, sizeof(buf), "INIT_LOG %d %s\n", pid, time_str);

    write(log_fd, buf, len);
  }
  // конец начального логфайла

  // пунтк 2 timer 300 msek + counter
  switch (fork()) {
  case 0:
    timer_counter(counter, sem_counter);

  default:
    break;
  }

  if (prime_flag) {
    switch (fork()) {
    case 0:
      logger(counter, sem_counter, log_fd);

    default:
      break;
    }
  }

  if (prime_flag) {
    // пунтк 5 нач
    switch (fork()) {
    case 0:
      int first_copy_pid = -2, second_copy_pid = -2;
      while (1) {

        sleep(3);
        // первый запуск
        if ((first_copy_pid == -2) && (second_copy_pid == -2)) {
          copies(counter, sem_counter, log_fd, &first_copy_pid,
                 &second_copy_pid);

        } else {
          int first_result = waitpid(first_copy_pid, NULL, WNOHANG);
          int second_result = waitpid(second_copy_pid, NULL, WNOHANG);
          if (first_result == 0 || second_result == 0) {
            const char *msg = "CHILDS NOT DEAD\n";
            write(log_fd, msg, strlen(msg));
          } else {
            copies(counter, sem_counter, log_fd, &first_copy_pid,
                   &second_copy_pid);
          }
        }
      }
    default:
      break;
    }
  }
  // пунтк 5 кон
  while (1) {
    input_handler(counter, sem_counter);
  }
  if (prime_flag) {
    if (log_fd != -1) {
      close(log_fd);
    }
    munmap(sem_counter, sizeof(sem_t));
    close(sem_counter_fd);
    munmap(counter, sizeof(int));
    close(counter_fd);
  }
}
