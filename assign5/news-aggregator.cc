/**
 * File: news-aggregator.cc
 * --------------------------------
 * Presents the implementation of the NewsAggregator class.
 */

#include "news-aggregator.h"
#include <iostream>
#include <iomanip>
#include <getopt.h>
#include <libxml/parser.h>
#include <libxml/catalog.h>
#include <ostream>
#include <thread>

#include "rss-feed.h"
#include "rss-feed-list.h"
#include "html-document.h"
#include "html-document-exception.h"
#include "rss-feed-exception.h"
#include "rss-feed-list-exception.h"
#include "utils.h"
#include "string-utils.h"
#include "ostreamlock.h"

using namespace std;

#define MAX_FEED_THREADS 8
#define MAX_THREADS_TO_LINK 10
#define MAX_EXECUTING_DOWNLOAD_THREADS 24

/**
 * Factory Method: createNewsAggregator
 * ------------------------------------
 * Factory method that spends most of its energy parsing the argument vector
 * to decide what rss feed list to process and whether to print lots of
 * of logging information as it does so.
 */
static const string kDefaultRSSFeedListURL = "small-feed.xml";
NewsAggregator *NewsAggregator::createNewsAggregator(int argc, char *argv[]) {
  struct option options[] = {
    {"verbose", no_argument, NULL, 'v'},
    {"quiet", no_argument, NULL, 'q'},
    {"url", required_argument, NULL, 'u'},
    {NULL, 0, NULL, 0},
  };
  
  string rssFeedListURI = kDefaultRSSFeedListURL;
  bool verbose = false;
  while (true) {
    int ch = getopt_long(argc, argv, "vqu:", options, NULL);
    if (ch == -1) break;
    switch (ch) {
    case 'v':
      verbose = true;
      break;
    case 'q':
      verbose = false;
      break;
    case 'u':
      rssFeedListURI = optarg;
      break;
    default:
      NewsAggregatorLog::printUsage("Unrecognized flag.", argv[0]);
    }
  }
  
  argc -= optind;
  if (argc > 0) NewsAggregatorLog::printUsage("Too many arguments.", argv[0]);
  return new NewsAggregator(rssFeedListURI, verbose);
}

/**
 * Method: buildIndex
 * ------------------
 * Initalizex the XML parser, processes all feeds, and then
 * cleans up the parser.  The lion's share of the work is passed
 * on to processAllFeeds, which you will need to implement.
 */
void NewsAggregator::buildIndex() {
  if (built) return;
  built = true; // optimistically assume it'll all work out
  xmlInitParser();
  xmlInitializeCatalog();
  processAllFeeds();
  xmlCatalogCleanup();
  xmlCleanupParser();
}

/**
 * Method: queryIndex
 * ------------------
 * Interacts with the user via a custom command line, allowing
 * the user to surface all of the news articles that contains a particular
 * search term.
 */
void NewsAggregator::queryIndex() const {
  static const size_t kMaxMatchesToShow = 15;
  while (true) {
    cout << "Enter a search term [or just hit <enter> to quit]: ";
    string response;
    getline(cin, response);
    response = trim(response);
    if (response.empty()) break;
    const vector<pair<Article, int> >& matches = index.getMatchingArticles(response);
    if (matches.empty()) {
      cout << "Ah, we didn't find the term \"" << response << "\". Try again." << endl;
    } else {
      cout << "That term appears in " << matches.size() << " article"
           << (matches.size() == 1 ? "" : "s") << ".  ";
      if (matches.size() > kMaxMatchesToShow)
        cout << "Here are the top " << kMaxMatchesToShow << " of them:" << endl;
      else if (matches.size() > 1)
        cout << "Here they are:" << endl;
      else
        cout << "Here it is:" << endl;
      size_t count = 0;
      for (const pair<Article, int>& match: matches) {
        if (count == kMaxMatchesToShow) break;
        count++;
        string title = match.first.title;
        if (shouldTruncate(title)) title = truncate(title);
        string url = match.first.url;
        if (shouldTruncate(url)) url = truncate(url);
        string times = match.second == 1 ? "time" : "times";
        cout << "  " << setw(2) << setfill(' ') << count << ".) "
             << "\"" << title << "\" [appears " << match.second << " " << times << "]." << endl;
        cout << "       \"" << url << "\"" << endl;
      }
    }
  }
}

/**
 * Private Constructor: NewsAggregator
 * -----------------------------------
 * Self-explanatory.  You may need to add a few lines of code to
 * initialize any additional fields you add to the private section
 * of the class definition.
 */
NewsAggregator::NewsAggregator(const string& rssFeedListURI, bool verbose): 
  log(verbose), rssFeedListURI(rssFeedListURI), built(false) 
{

}

const string& NewsAggregator::waitlink(const string& link)
{
  mapLock.lock();
  semaphore* up;
  auto linksem = linkLocks.find(link);
  if (linksem == linkLocks.end())
  {
    semaphore *s = new semaphore(MAX_THREADS_TO_LINK);
    linkLocks.insert({link, s});
    up = s;
  } else {
    up = linksem->second;
  }
  mapLock.unlock();
  up->wait();
  return link;
}

const string& NewsAggregator::signallink(const string& link)
{
  semaphore *up;
  mapLock.lock();
  auto linksem = linkLocks.find(link);
  mapLock.unlock();
  up = linksem->second;
  up->signal(on_thread_exit);
  return link;
}

void NewsAggregator::processArticle(size_t tid, semaphore &s, const string feedUrl, const Article& article)
{
  waitlink(getURLServer(feedUrl));
  s.signal(on_thread_exit);

  //cout << oslock << "Article thread " << tid << " looking up <" << article.url << " , " << article.title << endl << osunlock; 

  HTMLDocument document(article.url);
  try {
    document.parse();
  } catch (HTMLDocumentException e) {
    log.noteSingleArticleDownloadFailure(article);
    return;
  }

  indexLock.lock();
  index.add(article, document.getTokens());
  indexLock.unlock();   

  signallink(getURLServer(feedUrl));
}

void NewsAggregator::processFeed(size_t tid, semaphore &s, const string feedUrl, const string feedName)
{
  s.signal(on_thread_exit);
  //cout << oslock << "Thread " << tid << " looking up <" << feedUrl << " , " << feedName << endl << osunlock; 

  RSSFeed rssFeed(feedUrl);

  try {
    rssFeed.parse();
  } catch (RSSFeedException e) {
    log.noteSingleFeedDownloadFailure(feedUrl);
    return;
  }

  const vector<Article>& articles = rssFeed.getArticles();
  vector<thread> threads;
  semaphore permits(MAX_EXECUTING_DOWNLOAD_THREADS);

  for (auto const &article : articles)
  {
    permits.wait();

    threads.push_back(thread([this](size_t tid, semaphore& s, const string feedUrl, const Article& article) {
        processArticle(rand(), s, feedUrl, article);
    }, tid, ref(permits), feedUrl, ref(article)));

  }
  for (thread& t: threads) t.join();
}

/**
 * Private Method: processAllFeeds
 * -------------------------------
 * Downloads and parses the encapsulated RSSFeedList, which itself
 * leads to RSSFeeds, which themsleves lead to HTMLDocuemnts, which
 * can be collectively parsed for their tokens to build a huge RSSIndex.
 * 
 */
void NewsAggregator::processAllFeeds() 
{
  RSSFeedList rssFeedList(rssFeedListURI);

  try {
    rssFeedList.parse();
  } catch (RSSFeedListException e) {
    log.noteFullRSSFeedListDownloadFailureAndExit(rssFeedListURI);
  }

  const map<std::string, std::string>& feeds = rssFeedList.getFeeds();

  vector<thread> threads;
  semaphore permits(MAX_FEED_THREADS);
  size_t tid = 0;
  for (auto const &feed : feeds)
  {
    permits.wait();
    tid++;
    const string feedUrl = feed.first;
    const string feedName = feed.second;

    threads.push_back(thread([this](size_t tid, semaphore& s, const string feedUrl, const string feedName) {
        processFeed(tid, s, feedUrl, feedName);
    }, tid, ref(permits), feedUrl, feedName));
  }

  for (thread& t: threads) t.join();
}