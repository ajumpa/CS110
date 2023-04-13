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

/**
 * Anonymous helper function for STL lower_bounds.
 * Return true if actor record name at offset act_ix is lexigraphically
 * less than the searched name.
 * */ 
const bool imdb::compare_names(int act_ix, const std::string& player)
{
	int cmp = strcmp(&((char *) this->actorFile)[act_ix], player.c_str());
	return cmp < 0 ? true : false;
}

/*
	Binary search for actor name
	Get moviedata offset bytes from end of actor record
	populate film vecotr with film data from moviedata
*/
bool imdb::getCredits(const string& player, vector<film>& films) const { 
  int *lx = (int *) actorFile + 1;
	int *hx = lx + ((int *) this->actorFile)[0];
	
	int *pos = lower_bound(lx, hx, player, [this] (int act_ix, const std::string& player) {
		int cmp = strcmp(&((char *) this->actorFile)[act_ix], player.c_str());
		return cmp < 0 ? true : false;
	});

	const char *name = &((char *) this->actorFile)[*pos];

	if (strcmp(player.c_str(), name) != 0)
		return 0;
	
	printf("Found %s at offset %d\n", name, *pos);

	int *name_len = (int *) strlen(name);
	int *c = (int *) ( (*name_len % 2) ? (int *) 2 : (int *)1);
	short num_movies = ((int *) this->actorFile)[*pos + *name_len + *c];
	printf("Number of movies %d\n", num_movies);
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