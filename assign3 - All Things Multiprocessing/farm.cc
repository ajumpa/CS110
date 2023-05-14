#include <cassert>
#include <ctime>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <map>
#include <sys/wait.h>
#include <unistd.h>
#include <sched.h>
#include "subprocess.h"

#include <ext/stdio_filebuf.h>
#include <fstream>

using namespace std;
using namespace __gnu_cxx; // __gnu_cxx::stdio_filebuf -> stdio_filebuf

struct worker {
	worker() {}
	worker(char *argv[]) : sp(subprocess(argv, true, false)), available(false) {}
	subprocess_t sp;
	bool available;
};


static const size_t kNumCPUs = sysconf(_SC_NPROCESSORS_ONLN);
static vector<worker> workers(kNumCPUs);
static size_t numWorkersAvailable = 0;

static map<pid_t, int> pid_workers_ix;
static const string kExecutable = "./factor.py";
static const string kArg = "--self-halting";

static void markWorkersAsAvailable(int sig) {
  while (true)
  { 
    int status;
    pid_t pid = waitpid(-1, &status, WUNTRACED | WNOHANG);
    if (pid <= 0 || WIFCONTINUED(status) ) break;
    workers[pid_workers_ix[pid]].available = true;
    numWorkersAvailable++;
  }
}

static void spawnAllWorkers() {
  char *argv[] = {const_cast<char *>(kExecutable.c_str()), const_cast<char *>(kArg.c_str())};
  for (size_t i = 0; i < kNumCPUs; i++)
  {
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    worker w = worker(argv);
    workers[i] = w;
    pid_workers_ix.insert({w.sp.pid, i});
    CPU_SET(i,&cpu_set);

    if (sched_setaffinity(w.sp.pid, sizeof(cpu_set), &cpu_set) < 0)
      perror("sched_setaffinity");

    printf("Worker %d is set to run on CPU %ld\n", workers[i].sp.pid, pid_workers_ix[w.sp.pid]);
  }
}

static size_t getAvailableWorker() {
  sigset_t mask;
  sigemptyset(&mask);
  while(!numWorkersAvailable)
    sigsuspend(&mask);
  
  for (size_t i = 0; i < kNumCPUs; i++)
  {
    if (workers[i].available)
    {
      numWorkersAvailable--;
      workers[i].available = false;
      return i;
    }
  }

  return -1;
}

static void publishWordsToChild(int to, string number) {
	stdio_filebuf<char> outbuf(to, std::ios::out);
	ostream os(&outbuf);
	os << number + "\n" << endl;
}

static void broadcastNumbersToWorkers() {
	while (true) {
		string line;
		getline(cin, line);
		if (cin.fail()) break;
		size_t endpos;
		long long num = stoll(line, &endpos);
		if (endpos != line.size()) break;
    size_t i = getAvailableWorker();
    kill (workers[i].sp.pid, SIGCONT);
    dprintf(workers[i].sp.supplyfd, "%lld\n", num);
	}
}

static void waitForAllWorkers() {
  sigset_t mask, mask_o;
  sigemptyset(&mask_o);
  sigaddset(&mask_o, SIGCHLD);
  sigprocmask(SIG_BLOCK, &mask_o, &mask);
  while(numWorkersAvailable < kNumCPUs)
    sigsuspend(&mask);
  sigprocmask(SIG_UNBLOCK, &mask_o, NULL);
}

static void closeAllWorkers() {
  signal(SIGCHLD, SIG_DFL);

  for (worker w : workers)
  {
    kill(w.sp.pid, SIGCONT);
    close(w.sp.supplyfd);
  }

  for (worker w: workers)
  {
    waitpid(w.sp.pid, NULL, 0);
  }
}

int main(int argc, char *argv[]) {
	signal(SIGCHLD, markWorkersAsAvailable);
	spawnAllWorkers();
	broadcastNumbersToWorkers();
	waitForAllWorkers();
	closeAllWorkers();
	return 0;
}
