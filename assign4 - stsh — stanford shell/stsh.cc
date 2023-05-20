#include "stsh-parser/stsh-parse.h"
#include "stsh-parser/stsh-readline.h"
#include "stsh-parser/stsh-parse-exception.h"
#include "stsh-signal.h"
#include "stsh-job-list.h"
#include "stsh-job.h"
#include "stsh-process.h"
#include <cstring>
#include <iostream>
#include <string>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>  // for fork
#include <signal.h>  // for kill
#include <sys/wait.h>
using namespace std;

static STSHJobList joblist; // the one piece of global data we need so signal handlers can access it

/**
 *  * Function: handleBuiltin
 *  * * -----------------------
 *  * Examines the leading command of the provided pipeline to see if
 *  * it's a shell builtin, and if so, handles and executes it.  handleBuiltin
 *  * returns true if the command is a builtin, and false otherwise.
 *   */
static const string kSupportedBuiltins[] = {"quit", "exit", "fg", "bg", "slay", "halt", "cont", "jobs"};
static const size_t kNumSupportedBuiltins = sizeof(kSupportedBuiltins)/sizeof(kSupportedBuiltins[0]);
static bool handleBuiltin(const pipeline& pipeline) {
  const string& command = pipeline.commands[0].command;
  auto iter = find(kSupportedBuiltins, kSupportedBuiltins + kNumSupportedBuiltins, command);
  if (iter == kSupportedBuiltins + kNumSupportedBuiltins) return false;
  size_t index = iter - kSupportedBuiltins;

  switch (index) {
    case 0:
    case 1: exit(0);
    case 2: foreground();
    case 7: cout << joblist; break;
    default: throw STSHException("Internal Error: Builtin command not supported."); // or not implemented yet
  }

  return true;
}

static void foreground()
{

}

static void childStatusHandler(int sig)
{
  while(true)
  {
    int status;
    pid_t pid = waitpid(-1, &status, WUNTRACED|WCONTINUED|WNOHANG);
    if (pid <= 0) break;
    if (joblist.containsProcess(pid))
    {
      STSHJob& job = joblist.getJobWithProcess(pid);
      STSHProcess& process = job.getProcess(pid);
      if (WIFEXITED(status) || WIFSIGNALED(status))
        process.setState(kTerminated);
      if (WIFSTOPPED(status))
        process.setState(kStopped); 
      if (WIFCONTINUED(status))
        process.setState(kRunning);
      joblist.synchronize(job);
    }
  }
}

// TODO: HANDLE JOBS BEING BROUGHT BACK TO FOREGROUND
static void contHandler(int sig)
{
  
}

static void stopHandler(int sig)
{
  if (joblist.hasForegroundJob())
  {
    STSHJob& job = joblist.getForegroundJob();
    job.setState(kBackground);
    kill(job.getGroupID(), SIGTSTP);
    joblist.synchronize(job);
  }
}

static void intHandler(int sig)
{
  if (joblist.hasForegroundJob())
  {
    STSHJob& job = joblist.getForegroundJob();
    kill(job.getGroupID(), SIGKILL);
    joblist.synchronize(job);
  }
}

/**
 *  * Function: installSignalHandlers
 *  * -------------------------------
 *  * Installs user-defined signals handlers for four signals
 *  * (once you've implemented signal handlers for SIGCHLD, 
 *  * SIGINT, and SIGTSTP, you'll add more installSignalHandler calls) and 
 *  * ignores two others.
 *  */
static void installSignalHandlers() {
  installSignalHandler(SIGQUIT, [](int sig) { exit(0); });
  installSignalHandler(SIGTTIN, SIG_IGN);
  installSignalHandler(SIGTTOU, SIG_IGN);
  signal(SIGCHLD, childStatusHandler);
  signal(SIGINT, intHandler);
  signal(SIGTSTP, stopHandler);
  signal(SIGCONT, contHandler);
}

/* Add job to joblist with list of processes that are waiting to run
*/
static size_t addToJobList(const vector<STSHProcess>& processes, STSHJobState state) {
  STSHJob& job = joblist.addJob(state);
  for (const STSHProcess& process : processes) job.addProcess(process);
  return job.getNum();
}

/**
 * Create a list of processes from a parsed command
*/
static vector<STSHProcess> createProcesses(const pipeline& p)
{
  vector<STSHProcess> jobProcesses;
  for (size_t i = 0; i < p.commands.size(); i++)
  {
    char *argv[kMaxArguments + 2];
    argv[0] = (char *) p.commands[i].command;
    
    size_t j;
    for (j = 0; j <= kMaxArguments || p.commands[i].tokens[j] == NULL; j++) {
      argv[j+1] = p.commands[i].tokens[j];
    }

    pid_t pid = fork();
    if (pid == 0)
    {
     execvp(argv[0], argv);
    }
    STSHProcess process = STSHProcess(pid, p.commands[i], kWaiting);
    jobProcesses.push_back(process);
  }
  return jobProcesses;
}

static void syncJobList(size_t newJobNum)
{
  sigset_t mask, oldMask;
  sigemptyset(&oldMask);
  sigaddset(&oldMask, SIGCHLD);
  sigprocmask(SIG_BLOCK, &oldMask, &mask);


  // THIS IS WEIRD NEEDS TO BE DONE RIGHT
  STSHJob& job = joblist.getJob(newJobNum);
  while(job.getState() == kForeground)  
  {
    sigsuspend(&mask);
    STSHProcess& process = job.getProcesses().front();
    setpgid(process.getID(), 0);
    process.setState(kRunning);
    joblist.synchronize(job);
  }
  sigprocmask(SIG_UNBLOCK, &oldMask, NULL);
}

/**
 *  * Function: createJob
 *  * -------------------
 *  * Creates a new job on behalf of the provided pipeline.
 *  */
static void createJob(const pipeline& p) {
  vector<STSHProcess> jobProcesses = createProcesses(p);
  //bool inForeground = !joblist.hasForegroundJob();
  //enum STSHJobState newJobState = inForeground ? kForeground : kBackground;
  size_t newJobNum = addToJobList(jobProcesses, kForeground);
  syncJobList(newJobNum);
}

/**
 *  * Function: main
 *  * --------------
 *  * Defines the entry point for a process running stsh.
 *  * The main function is little more than a read-eval-print
 *  * loop (i.e. a repl).  
 *  */
int main(int argc, char *argv[]) {
  pid_t stshpid = getpid();
  installSignalHandlers();
  rlinit(argc, argv);
  while (true) {
    string line;
    if (!readline(line)) break;
    if (line.empty()) continue;
    try {
      pipeline p(line);
      bool builtin = handleBuiltin(p);
      if (!builtin) createJob(p);
    } catch (const STSHException& e) {
      cerr << e.what() << endl;
      if (getpid() != stshpid) exit(0); // if exception is thrown from child process, kill it
    }
  }

  return 0;
}
