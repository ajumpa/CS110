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
	
	int ix = *pos + strlen(name) + ((strlen(name) % 2) ?  1 : 2);

	short num_movies = (short) ((char *) this->actorFile)[ix];
	ix += 2;
	ix += ix%4;

	int *offset_list = (int *) ((char *) actorFile + ix);
	for (int i = 0; i < (int) num_movies; i++)
	{
		int movie_offset = *(offset_list + i);
		char *title = ((char *) movieFile) + movie_offset;
		int year =  (int) *((char *) movieFile + movie_offset + strlen(title) + 1) + 1900;

		film new_film = film {title, year};
		films.push_back(new_film);
	}
	return 1;
}

bool imdb::getCast(const film& movie, vector<string>& players) const { 
	char *title = (char *) (movie.title).c_str();
  const int *lx = (int *) movieFile + 1;
	const int *hx = lx + ((int *) this->movieFile)[0];
	
	const int *pos = lower_bound(lx, hx, title, [this] (int movie_ix, const std::string& title) {
		int cmp = strcmp(&((char *) this->movieFile)[movie_ix], title.c_str());
		return cmp < 0 ? true : false;
	});

	const char *movie_name = &((char *) this->movieFile)[*pos];

	if (strcmp(title, movie_name) != 0)
		return 0;

	int ix = *pos + (strlen(movie_name)+1) + 1 + (((strlen(movie_name) + 2) % 2) ?  1 : 0);
	short num_actors = (short) ((char *) this->movieFile)[ix];
	ix += 2;
	ix += ix%4;

	int *offset_list = (int *) ((char *) movieFile + ix);
	for (int i = 0; i < (int) num_actors; i++)
	{
		int offset = *(offset_list + i);
		char *name = ((char *) actorFile) + offset;
		string str_name = std::string(name);
		players.push_back(str_name);
	}

	return 1;
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