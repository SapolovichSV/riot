#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
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
 процесса и значение счетчика 5) Раз в 3 секунды запускает 2 своих копии: a.
 Копия 1 записывает в тот же лог файл строку со своим идентификатором процесса,
 временем своего запуска, увеличивает текущее значение счетчика на 10,
 записывает в лог файл время выхода и завершается. b. Копия 2 записывает в тот
 же лог файл строку со своим идентификатором процесса, временем своего запуска,
 увеличивает текущее значение счетчика в 2 раза, через 2 секунды уменьшает
 значение счетчика в 2 раза, записывает в лог файл время выхода и завершается c.
 Если какая-либо копия к следующему циклу запуска еще не завершила свою работу,
 программа не запускает копию, а пишет в лог сообщение. 6) Пользователь может
 запустить любое количество программ. В этом случае только одна из программ
 должна писать в лог текущее значение счетчика и порождать копии, но все
 запущенные программы должны модифицировать счетчик раз в 300 мс и по запросу
 пользователя. На проверку: исходные коды программы. */
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
    int len = snprintf(buf, sizeof(buf), "time = %s pid = %d counter = %d",
                       c_time, pid, curr_counter);
    write(log_fd, buf, len);
  }
}
void time_pid_counter_logger(int *counter, sem_t *sem, int log_fd) {}
int main(int argc, char *argv[]) {
  int counter_fd = shm_open(SHM_COUNTER, O_CREAT | O_RDWR, 0777);

  if (counter_fd == -1) {
    // уже есть
    return -1;
  }
  ftruncate(counter_fd, sizeof(int));
  int *counter = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED,
                      counter_fd, 0);
  *counter = 0;

  int sem_counter_fd = shm_open(SEM_COUNTER, O_CREAT | O_RDWR, 0777);
  if (sem_counter_fd == -1) {
    return -1;
  }
  ftruncate(sem_counter_fd, sizeof(int));
  sem_t *sem_counter = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE,
                            MAP_SHARED, sem_counter_fd, 0);
  sem_init(sem_counter, 1, 1);

  // начальный логфайл
  time_t start_time = time(NULL);

  int prime_flag = 0;
  shm_open(PRIME_FLAG, O_CREAT | O_EXCL, 0777);
  if (EEXIST == errno) {
    prime_flag = 1;
  }

  int log_fd = -1;
  if (prime_flag) {
    log_fd = open(LOG_FILENAME, O_RDWR | O_CREAT, 0777);
    if (log_fd == -1) {
      return 1;
    }

    pid_t pid = getpid();
    char buf[1024];
    char *time_str = ctime(&start_time);
    int len = snprintf(buf, sizeof(buf), "%d %s", pid, time_str);

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


  switch (fork()) {
  case 0:
    logger(counter, sem_counter, log_fd);

  default:
    break;
  }

  while(1) {
    input_handler(counter, sem_counter);
  }
  if (log_fd != -1) {
    close(log_fd);
  }
  munmap(sem_counter, sizeof(sem_t));
  close(sem_counter_fd);
  munmap(counter, sizeof(int));
  close(counter_fd);
}
