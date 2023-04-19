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

int mapCostars(const string &player, map<string, set<film>> &costars, 
											 const vector<film>& credits, const imdb& db) {
	int n_costars = 0;
	for (int i = 0; i < (int) credits.size(); i++) {
		const film& movie = credits[i];
		vector<string> cast;
		db.getCast(movie, cast);
		for (int j = 0; j < (int) cast.size(); j++) {
			const string& costar = cast[j];
			if (costar != player) {
				n_costars++;
				costars[costar].insert(movie);
			}
		}
	}
	return n_costars;
}

static void search(const imdb& db, const string& a1, const string& a2) {
	vector<film> a1_films;
	if (!db.getCredits(a1, a1_films) || a1_films.size() == 0) {
		cout << "We're sorry, but " << a1
			<< " doesn't appear to be in our database." << endl;
		return;
	}

	vector<film> a2_films;
	if (!db.getCredits(a2, a2_films) || a2_films.size() == 0) {
		cout << "We're sorry, but " << a2
			<< " doesn't appear to be in our database." << endl;
		return;
	}

	map<string, set<film>> a1_immediate_costars;
	map<string, set<film>> a2_immediate_costars;
	map<string, set<film>> costars;
	string src;
	string dst;

	if ( mapCostars(a1, a1_immediate_costars, a1_films, db) 
			<= mapCostars(a2, a2_immediate_costars, a2_films, db))
	{
		src = a1, dst = a2;
		costars = a1_immediate_costars;
	} 
	else 
	{
		src = a2, dst = a1;
		costars = a2_immediate_costars;
	}

	list<string> Q;
	Q.push_back(src);
	map<string, set<film>>::const_iterator curr;
	while (!Q.empty()) {
		const string& costar = curr->first;
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
