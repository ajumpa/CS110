#include <cassert>
#include <ctime>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#include <sched.h>
#include "subprocess.h"

using namespace std;

struct worker {
	worker() {}
	worker(char *argv[]) : sp(subprocess(argv, true, false)), available(false) {}
	subprocess_t sp;
	bool available;
};

static const size_t kNumCPUs = sysconf(_SC_NPROCESSORS_ONLN);
static vector<worker> workers(kNumCPUs);
static size_t numWorkersAvailable = 0;

static void markWorkersAsAvailable(int sig) {
  while (true)
  {
    pid_t pid = waitpid(-1, NULL, WNOHANG);
    if (pid <= 0) break;
    printf("%d ready for work!\n", pid);
  }
}

const string kExecutable = "./factor.py";
const string kArg = "--self-halting";
static void spawnAllWorkers() {
  cpu_set_t set;
  CPU_ZERO(&set);
  char *argv[] = {const_cast<char *>(kExecutable.c_str()), const_cast<char *>(kArg.c_str())};
  for (size_t i = 0; i < kNumCPUs; i++)
  {
    worker w = worker(argv);
    workers.push_back(w);

    CPU_SET(i,&set);
    if (sched_setaffinity(w.sp.pid, sizeof(set), &set) < 0)
      perror("sched_setaffinity");

    printf("Worker %d is set to run on CPU %ld\n", w.sp.pid, i);
  }
}

static size_t getAvailableWorker() {
  printf("There are %ld process online for this machine\n", kNumCPUs);
  for (size_t i = 0; i < kNumCPUs; i++)
  {
    if (workers[i].available)
      return i;
  }

  return -1;
}

static void broadcastNumbersToWorkers() {
	while (true) {
		string line;
		getline(cin, line);
		if (cin.fail()) break;
		size_t endpos;
		long long num = stoll(line, &endpos);
		if (endpos != line.size()) break;
	}
}

static void waitForAllWorkers() {

}

static void closeAllWorkers() {

}

int main(int argc, char *argv[]) {
	signal(SIGCHLD, markWorkersAsAvailable);
	spawnAllWorkers();
	broadcastNumbersToWorkers();
	waitForAllWorkers();
	closeAllWorkers();
	return 0;
}
