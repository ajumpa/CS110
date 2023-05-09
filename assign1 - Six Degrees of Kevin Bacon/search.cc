#include <iostream>
#include <utility>
#include <vector>
#include "path.h"
#include <list>
#include <set>
#include <unordered_set>
#include "imdb.h"
#include <iomanip> // for setw formatter
#include <map>

using namespace std;

static const int kWrongArgumentCount = 1;
static const int kDatabaseNotFound = 2;

int numbCostars(const string &player, 
											const vector<film>& credits,
											const imdb& db)
{
	int n_costars = 0;
	for (int i = 0; i < (int) credits.size(); i++) {
		const film& movie = credits[i];
		vector<string> cast;
		db.getCast(movie, cast);
		for (int j = 0; j < (int) cast.size(); j++) {
			const string& costar = cast[j];
			if (costar != player) {
				n_costars++;
			}
		}
	}
	return n_costars;
}

static void search(const imdb& db, const string& src, const string& dst)
{
	vector<film> src_films;
	if (!db.getCredits(src, src_films) || src_films.size() == 0) {
		cout << "We're sorry, but " << src 
			<< " doesn't appear to be in our database." << endl;
		return;
	}

	vector<film> dst_films;
	if (!db.getCredits(dst, dst_films) || dst_films.size() == 0) {
		cout << "We're sorry, but " << dst
			<< " doesn't appear to be in our database." << endl;
		return;
	}

	set <string> v_actors;
	set <film> v_films;
	list <string> q_actors;	
	q_actors.push_back(src);

	while (!q_actors.empty()) {
		string actor = q_actors.front();
		q_actors.pop_front();

		v_actors.insert(actor);

		vector<film> credits;
		db.getCredits(actor, credits);

		for (int i = 0; i < (int) credits.size(); i++) {

			const film& movie = credits[i];

			if (!v_films.count(movie))
			{	
				v_films.insert(movie);
				vector<string> cast;
				db.getCast(movie, cast);
				for (int j = 0; j < (int) cast.size(); j++) {
					const string& costar = cast[j];
					if(!v_actors.count(costar))
					{
						q_actors.push_back(costar);	
					}

					if (costar == dst)
					{
						cout << actor << " was in " 
								<< movie.title << " (" << movie.year 
								<< ") with " << costar << endl;
						return;
					}
				}
			}
		}
	} 
}

int main(int argc, char *argv[]) {
	if (argc != 3) {
		cerr << "Usage: " << argv[0] << " <source-actor> <target-actor>" << endl;
		return kWrongArgumentCount;
	}
	imdb db(kIMDBDataDirectory);
	if (!db.good()) {
		cerr << "Data directory not found!  Aborting..." << endl; 
		return kDatabaseNotFound;
	}
  
  string src = argv[1];
  string dest = argv[2];
  
  search(db, argv[1], argv[2]);

  return 0;
}
