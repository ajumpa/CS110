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
static void backgroundBuiltIn(char* input);
static void processBuiltIn(char* arg1, char* arg2, int sig);

// Signal Handlers
static void childStatusHandler(int sig);
static void stopHandler(int sig);
static void intHandler(int sig);

// Control flow
static void runJob(size_t jobNum, STSHJobState state);
static size_t addToJobList(const vector<STSHProcess>& processes, STSHJobState state);
static void createJob(const pipeline& p);
static void createProcesses(vector<STSHProcess>& jobProcesses, const pipeline& p);
static void stsh_wait(STSHJob& job);

static STSHJobList joblist; // the one piece of global data we need so signal handlers can access it

bool isNumber(char *s)
{
  int i=0;
  while(isdigit(s[i++]));
  return (i=strlen(s));
}

// TODO: Could add support so "fg " gets the last job created
// TODO: Make this cleaner
static void foregroundBuiltIn(char* input)
{
  size_t jobNum = 0;
  if (input && isNumber(input)) jobNum = stoi(input);
  
  if (joblist.containsJob(jobNum))
  {
    if (joblist.hasForegroundJob())
      raise(SIGTSTP);
    kill(-joblist.getJob(jobNum).getGroupID(), SIGCONT);
    runJob(jobNum, kForeground);
  }
  else 
    throw STSHException("fg: No such job");
}
static void backgroundBuiltIn(char* input)
{
  size_t jobNum = 0;
  if (input && isNumber(input)) jobNum = stoi(input);
  
  if (joblist.containsJob(jobNum))
    kill(-joblist.getJob(jobNum).getGroupID(), SIGCONT);
  else 
    throw STSHException("fg: No such job");
}

static void processBuiltIn(char *arg1, char *arg2, int sig)
{
  size_t num1 = NULL;
  size_t num2 = NULL;

  if (arg1 && isNumber(arg1)) num1 = stoi(arg1);
  if (arg2 && isNumber(arg2)) num2 = stoi(arg2); 

  if ((num1 && !num2) && joblist.containsProcess(num1))
    kill(num1, sig);
  else if(num1 > 0 && num2 >=0)
  {
    if (joblist.containsJob(num1) 
          && joblist.getJob(num1).getProcesses().size() > num2 )
      kill(joblist.getJob(num1).getProcesses()[num2].getID() ,sig);
    else
      throw STSHException("No such process");
  }
  else
    throw STSHException("No such process");
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
    case 2: foregroundBuiltIn(pipeline.commands[0].tokens[0]); break;
    case 3: backgroundBuiltIn(pipeline.commands[0].tokens[0]); break;
    case 4: processBuiltIn(pipeline.commands[0].tokens[0], pipeline.commands[0].tokens[1], SIGKILL); break;
    case 5: processBuiltIn(pipeline.commands[0].tokens[0], pipeline.commands[0].tokens[1], SIGTSTP); break;
    case 6: processBuiltIn(pipeline.commands[0].tokens[0], pipeline.commands[0].tokens[1], SIGCONT); break;
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
        process.setState(kRunning);
      joblist.synchronize(job);
    }
  }
}

static void contHandler(int sig)
{

}

static void stopHandler(int sig)
{
  if (joblist.hasForegroundJob())
    kill(-joblist.getForegroundJob().getGroupID(), SIGTSTP);
}

static void intHandler(int sig)
{
  if (joblist.hasForegroundJob())
    kill(-joblist.getForegroundJob().getGroupID(), SIGKILL);
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

static void readPipe(int read[2])
{
  if (close(read[1]) < 0)
    perror("close");
  if (dup2(read[0], STDIN_FILENO) < 0)
    perror("dup2");
}

static void writePipe(int write[2])
{
  if (close(write[0]) < 0)
    perror("close");
  if (dup2(write[1], STDOUT_FILENO) < 0)
    perror("dup2");
}

/**
 * Create a list of processes from a parsed command
*/
static void createProcesses(vector<STSHProcess>& jobProcesses, const pipeline& p)
{
  int nCommands = p.commands.size();
  size_t nPipes = nCommands-1;
  int fds[nPipes][2];
  pid_t groupPID;

  for (size_t i = 0; i < nCommands; i++)
  {
    char *argv[kMaxArguments + 2];
    argv[0] = (char *) p.commands[i].command;
    
    size_t j = 0;
    while(p.commands[i].tokens[j])
    {
      argv[j+1] = p.commands[i].tokens[j];
      j++;
    }
    argv[j+1] = NULL;

    // TODO: echo 12345 | ./conduit --delay 1 | ./conduit | ./conduit doesn't get past first "conduit"
    if (i < nPipes) pipe(fds[i]);
    pid_t pid = fork();
    if (pid == 0)
    {
      if (i > 0) readPipe(fds[i-1]);
      if (i < nPipes) writePipe(fds[i]);

      execvp(argv[0], argv);
      throw STSHException(strcat(argv[0],": command not found"));
    }

    if (i == 0) groupPID = pid;
    setpgid(pid, groupPID);
    STSHProcess process = STSHProcess(pid, p.commands[i], kWaiting);
    jobProcesses.push_back(process);
  }

  for (size_t i = 0; i < nPipes; i++)
  {
    if (close(fds[i][0]) < 0) perror("close");
    if (close(fds[i][1]) < 0) perror("close");
  }
}

/**
 * Add job to joblist with list of processes that are waiting to run
*/
static size_t addToJobList(vector<STSHProcess>& processes, STSHJobState state) 
{
  STSHJob& job = joblist.addJob(state);
  for (STSHProcess& process : processes)
  {
    job.addProcess(process);
  }
  return job.getNum();
}

static void stsh_wait(STSHJob& job)
{
    sigset_t mask, oldMask;
    sigemptyset(&oldMask);
    sigaddset(&oldMask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &oldMask, &mask);

    bool termCtr = (tcsetpgrp(STDIN_FILENO, job.getGroupID()) == 0) ? true : false;
    while(job.getState() == kForeground)  
    {
      sigsuspend(&mask);
    }
    sigprocmask(SIG_UNBLOCK, &oldMask, NULL);
    if (termCtr) tcsetpgrp(STDIN_FILENO, getpid());
}

// Run job until completed or stopped
static void runJob(size_t jobNum, STSHJobState state)
{
  STSHJob& job = joblist.getJob(jobNum);
  job.setState(state);
  for (STSHProcess& process : job.getProcesses()) 
    process.setState(kRunning);
  stsh_wait(job);
}

/**
 *  * Function: createJob
 *  * -------------------
 *  * Creates a new job on behalf of the provided pipeline.
 *  */
static void createJob(const pipeline& p)
{
  vector<STSHProcess> jobProcesses;
  createProcesses(jobProcesses, p);
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