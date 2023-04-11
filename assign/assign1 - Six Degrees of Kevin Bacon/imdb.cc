#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "imdb.h"

#include <string.h>
#include <algorithm>
#include <iterator>
using namespace std;

const char *const imdb::kActorFileName = "actordata";
const char *const imdb::kMovieFileName = "moviedata";
imdb::imdb(const string& directory) {
	const string actorFileName = directory + "/" + kActorFileName;
	const string movieFileName = directory + "/" + kMovieFileName;  
	actorFile = acquireFileMap(actorFileName, actorInfo);
	movieFile = acquireFileMap(movieFileName, movieInfo);
}

bool imdb::good() const {
	return !( (actorInfo.fd == -1) || 
			(movieInfo.fd == -1) ); 
}

imdb::~imdb() {
	releaseFileMap(actorInfo);
	releaseFileMap(movieInfo);
}


/*
	Binary search for actor name
	Get moviedata offset bytes from end of actor record
	populate film vecotr with film data from moviedata
*/
bool imdb::getCredits(const string& player, vector<film>& films) const { 
  int lx = 1;
	int mx = 0;
	int hx = ((int *) this->actorFile)[0];

	vector<int> a_ix;

	int ix = ((int *) this->actorFile)[999];
	char* name = &((char *) this->actorFile)[ix];
	printf("%s\n", name);


	/**
  while (lx != hx)
  {
		mx = lx + (hx-lx) / 2;
		int ix = ((int *) this->actorFile)[mx];
  }
  
	for (int i = 1; i < n_actors; i++) 
	{
		int act_ix = ((int *) this->actorFile)[i];
	}
   **/

	return 0;
}

bool imdb::getCast(const film& movie, vector<string>& players) const { 
	return 0;
}

const void *imdb::acquireFileMap(const string& fileName, struct fileInfo& info) {
	struct stat stats;
	stat(fileName.c_str(), &stats);
	info.fileSize = stats.st_size;
	info.fd = open(fileName.c_str(), O_RDONLY);
	return info.fileMap = mmap(0, info.fileSize, PROT_READ, MAP_SHARED, info.fd, 0);
}

void imdb::releaseFileMap(struct fileInfo& info) {
	if (info.fileMap != NULL) munmap((char *) info.fileMap, info.fileSize);
	if (info.fd != -1) close(info.fd);
}
