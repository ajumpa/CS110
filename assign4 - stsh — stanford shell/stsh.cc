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

// Built in functions
static void forgroundBuiltIn(char* input);

// Signal Handlers
static void childStatusHandler(int sig);
static void stopHandler(int sig);
static void intHandler(int sig);

// Control flow
static void runJob(size_t jobNum, STSHJobState state);
static size_t addToJobList(const vector<STSHProcess>& processes, STSHJobState state);
static void createJob(const pipeline& p);
static vector<STSHProcess> createProcesses(const pipeline& p);

static STSHJobList joblist; // the one piece of global data we need so signal handlers can access it

// TODO: Could add support so "fg " gets the last job created
// TODO: Make this cleaner
static void foregroundBuiltIn(char* input)
{
  size_t jobNum = 0;
  if (!input)
  {
    
  }
  else if (isdigit(input[0])) 
  {
    jobNum = stoi(input);
  }
  if (joblist.containsJob(jobNum))
  {
    STSHJob& job = joblist.getJob(jobNum);
    if (joblist.hasForegroundJob())
      raise(SIGTSTP);
    kill(job.getGroupID(), SIGCONT);
    runJob(jobNum, kForeground);
  }
  else 
    throw STSHException("fg: No such job");
}

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
    case 2: foregroundBuiltIn(pipeline.commands[0].tokens[0]);
    case 7: cout << joblist; break;
    default: throw STSHException("Internal Error: Builtin command not supported."); // or not implemented yet
  }

  return true;
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
      {
        process.setState(kRunning);
        cout << process.getID() << " is running" << endl;
      }
      joblist.synchronize(job);
    }
  }
}

// TODO: These could probably all be in one handler
static void contHandler(int sig)
{

}

static void stopHandler(int sig)
{
  if (joblist.hasForegroundJob())
    kill(joblist.getForegroundJob().getGroupID(), SIGTSTP);
}

static void intHandler(int sig)
{
  if (joblist.hasForegroundJob())
    kill(joblist.getForegroundJob().getGroupID(), SIGKILL);
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

/**
 * Add job to joblist with list of processes that are waiting to run
 * TODO: Need to properly set process groupIDs for jobs with one or more processes
 * TODO: Why "sleep n &" sets process to waiting and not running (it actually is running in background)
*/
static size_t addToJobList(vector<STSHProcess>& processes, STSHJobState state) 
{
  STSHJob& job = joblist.addJob(state);
  for (STSHProcess& process : processes)
  {
    job.addProcess(process);
    setpgid(process.getID(), 0);
  }
  kill(job.getGroupID(), SIGCONT);
  return job.getNum();
}

// Run job until completed or stopped
static void runJob(size_t jobNum, STSHJobState state)
{
  STSHJob& job = joblist.getJob(jobNum);
  job.setState(state);
  if (state == kForeground)
  {
    sigset_t mask, oldMask;
    sigemptyset(&oldMask);
    sigaddset(&oldMask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &oldMask, &mask);

    while(job.getState() == kForeground)  
    {
      sigsuspend(&mask);
    }
    sigprocmask(SIG_UNBLOCK, &oldMask, NULL);
  }
}

/**
 *  * Function: createJob
 *  * -------------------
 *  * Creates a new job on behalf of the provided pipeline.
 *  */
static void createJob(const pipeline& p)
{
  vector<STSHProcess> jobProcesses = createProcesses(p);
  STSHJobState state = p.background ? kBackground : kForeground;
  size_t newJobNum = addToJobList(jobProcesses, state);
  runJob(newJobNum, state);
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
