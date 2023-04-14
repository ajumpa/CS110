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
  const int *lx = (int *) actorFile + 1;
	const int *hx = lx + ((int *) this->actorFile)[0];
	
	const int *pos = lower_bound(lx, hx, player, [this] (int act_ix, const std::string& player) {
		int cmp = strcmp(&((char *) this->actorFile)[act_ix], player.c_str());
		return cmp < 0 ? true : false;
	});

	const char *name = &((char *) this->actorFile)[*pos];

	if (strcmp(player.c_str(), name) != 0)
		return 0;
	
	printf("Found %s at offset %d\n", name, *pos);

	int name_len = strlen(name);
	int c = ( (name_len % 2) ?  1 : 2);
	int ix = *pos+name_len+c;
	short num_movies = (short) ((unsigned char *) this->actorFile)[ix];

	printf("Number of movies %hi\n", num_movies);

	ix += 2;

	for (int i = 0; i <= (int) num_movies; i++)
	{
		int *movie_ix = (int *) ( (int *) this->actorFile)[ix+i*4];
		printf("%d \n", *movie_ix);

		/**
		 *  
		const char *movie_name = &((char *) this->movieFile)[*movie_ix];
		printf("HELLERRRR\n");
		printf("%s\n", &movie_name);
		 * 
		 */
	}
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