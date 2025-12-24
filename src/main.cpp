
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>
#include <unordered_set>
#include <unordered_map>
#include "ranker.h"
#include <chrono>
#include <mutex>
#include <thread>



namespace fs = std::filesystem;

struct Document {
    int id;
    std::string path;
    std::string content;
};

// STEP 3: Tokenizer
// -----------------
// Rules:
// - Converts all characters to lowercase
// - Treats any non-alphanumeric character as a delimiter
// - Splits on whitespace and punctuation
// - Ignores tokens with length < 2
//
// Notes:
// - Used for both document indexing and query processing
// - Token positions are preserved by the caller (important for positional index)

std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;
    current.reserve(16); // small optimization

    for (unsigned char ch : text) {
        char c = std::tolower(ch);

        if (std::isalnum(c)) {
            current.push_back(c);
        } else {
            if (current.size() >= 2) {
                tokens.push_back(current);
            }
            current.clear();
        }
    }

    // Push last token if present
    if (current.size() >= 2) {
        tokens.push_back(current);
    }

    return tokens;
}


// STEP 4: Stop word list
std::unordered_set<std::string> stopWords = {

    // articles
    "a","an","the",

    // pronouns
    "i","me","my","mine","myself",
    "you","your","yours","yourself","yourselves",
    "he","him","his","himself",
    "she","her","hers","herself",
    "it","its","itself",
    "we","us","our","ours","ourselves",
    "they","them","their","theirs","themselves",
    "one","ones","someone","anyone","everyone","nobody","nothing","something",

    // auxiliary & modal verbs
    "am","is","are","was","were",
    "be","been","being",
    "have","has","had","having",
    "do","does","did","doing",
    "will","would","shall","should",
    "can","could","may","might","must","ought",

    // common verb noise (base + inflections)
    "say","says","said","saying",
    "get","gets","got","getting",
    "make","makes","made","making",
    "go","goes","went","going",
    "know","knows","knew","knowing",
    "think","thinks","thought","thinking",
    "see","sees","saw","seeing",
    "come","comes","came","coming",
    "take","takes","took","taking",
    "use","uses","used","using",
    "find","finds","found","finding",
    "give","gives","gave","giving",
    "tell","tells","told","telling",
    "work","works","worked","working",
    "seem","seems","seemed","seeming",
    "try","tries","tried","trying",
    "leave","leaves","left","leaving",
    "call","calls","called","calling",
    "start","starts","started","starting",
    "end","ends","ended","ending",
    "show","shows","showed","showing",
    "play","plays","played","playing",
    "run","runs","ran","running",
    "move","moves","moved","moving",

    // conjunctions
    "and","or","but","if","while","because","as",
    "until","unless","although","though","whereas",
    "whether","nor","yet","so",

    // prepositions
    "of","to","in","on","at","by","for","with",
    "about","against","between","into","through",
    "during","before","after","above","below",
    "from","up","down","out","off","over","under",
    "within","without","across","behind","beyond",
    "near","along","among","around","toward","towards",

    // determiners & quantifiers
    "this","that","these","those",
    "each","every","either","neither",
    "some","any","no","none","all","both",
    "many","much","few","several","most","least",
    "such","same","other","another",

    // adverbs
    "not","only","very","too","quite",
    "so","then","there","here",
    "when","where","why","how",
    "again","once","ever","never",
    "already","still","often","sometimes","usually",

    // comparatives & intensifiers
    "more","most","less","least",
    "enough","rather","quite",

    // discourse / filler words
    "yes","no","ok","okay",
    "also","just","even","though",
    "however","therefore","thus","hence",
    "otherwise","meanwhile","furthermore",
    "moreover","nevertheless",

    // time & frequency
    "today","yesterday","tomorrow",
    "now","then","soon","later",
    "always","never","often","sometimes","usually",

    // question words
    "who","whom","whose",
    "which","what","when","where","why","how",

    // numbers (written)
    "zero","one","two","three","four","five","six","seven","eight","nine","ten",
    "first","second","third","fourth","fifth","sixth","seventh","eighth","ninth","tenth",

    // misc abbreviations & noise
    "etc","ie","eg","vs","via","per",

    // web / modern noise
    "http","https","www","com","org","net",

    // generic nouns often treated as noise
    "thing","things","stuff","something","anything","everything",
    "someone","anyone","everyone"
};
// Save positional index to disk
void saveIndex(
    const std::string& filename,
    const std::unordered_map<
        std::string,
        std::unordered_map<int, std::vector<int>>
    >& positionalIndex
) {
    std::ofstream out(filename);
    if (!out.is_open()) {
        std::cerr << "Failed to open index file for writing\n";
        return;
    }

    for (const auto& wordEntry : positionalIndex) {
        const std::string& word = wordEntry.first;

        for (const auto& docPair : wordEntry.second) {
            int docID = docPair.first;
            const std::vector<int>& positions = docPair.second;

            out << word << " " << docID;
            for (int pos : positions) {
                out << " " << pos;
            }
            out << "\n";
        }
    }

    out.close();
}

// Load positional index from disk
void loadIndex(
    const std::string& filename,
    std::unordered_map<
        std::string,
        std::unordered_map<int, std::vector<int>>
    >& positionalIndex
) {
    std::ifstream in(filename);
    if (!in.is_open()) {
        std::cerr << "Index file not found. Rebuilding index.\n";
        return;
    }

    positionalIndex.clear();

    std::string line;
    while (std::getline(in, line)) {
        std::istringstream iss(line);

        std::string word;
        int docID;
        iss >> word >> docID;

        int pos;
        while (iss >> pos) {
            positionalIndex[word][docID].push_back(pos);
        }
    }

    in.close();
}

const std::string INDEX_FILE = "positional_index.txt";

bool phraseMatchTwoWords(
    const std::vector<int>& p1,
    const std::vector<int>& p2
);

bool phraseMatchTwoWords(
    const std::vector<int>& p1,
    const std::vector<int>& p2
) {
    int i = 0;
    int j = 0;

    while (i < p1.size() && j < p2.size()) {
        if (p2[j] - p1[i] == 1) {
            return true;  // exact phrase match found
        } else if (p2[j] > p1[i]) {
            i++;
        } else {
            j++;
        }
    }

    return false; // no phrase match
}

// Mutex to protect shared index structures
std::mutex indexMutex;

/*
NOTE ON FALSE SHARING:
- Multiple threads may update nearby memory locations (e.g., docLength entries).
- This can cause cache line contention known as "false sharing".
- Not optimized here because workload is coarse-grained and impact is minimal.
- Would require padding or per-thread counters to eliminate fully.
*/


void indexDocuments(
    int start,
    int end,
    const std::vector<Document>& documents,
    std::unordered_map<
        std::string,
        std::unordered_map<int, std::vector<int>>
    >& globalIndex,
    std::unordered_map<int, int>& globalDocLength
);



void indexDocuments(
    int start,
    int end,
    const std::vector<Document>& documents,
    std::unordered_map<
        std::string,
        std::unordered_map<int, std::vector<int>>
    >& globalIndex,
    std::unordered_map<int, int>& globalDocLength
) {
 
    // Thread-local index to avoid lock contention.
// Each thread builds its own partial index without synchronization.
// This significantly improves performance compared to locking per token.


    // Thread-local structures
    std::unordered_map<
        std::string,
        std::unordered_map<int, std::vector<int>>
    > localIndex;

    std::unordered_map<int, int> localDocLength;

    for (int docID = start; docID < end; docID++) {

        const std::string& content = documents[docID].content;
        if (content.empty()) continue;

        auto tokens = tokenize(content);
        int position = 0;

        for (const auto& token : tokens) {
            if (stopWords.count(token)) continue;

            localIndex[token][docID].push_back(position);
            localDocLength[docID]++;
            position++;
        }
    }

    // Merge into global structures (single lock)
    // Merge local index into global index.
// This is the ONLY critical section.
// Lock is held briefly, once per thread, instead of once per token.

    {
        std::lock_guard<std::mutex> lock(indexMutex);

        for (auto& wordEntry : localIndex) {
            const std::string& word = wordEntry.first;

            for (auto& docEntry : wordEntry.second) {
                int docID = docEntry.first;
                auto& positions = docEntry.second;

                auto& globalPositions =
                    globalIndex[word][docID];

                globalPositions.insert(
                    globalPositions.end(),
                    positions.begin(),
                    positions.end()
                );
            }
        }

        for (auto& dl : localDocLength) {
            globalDocLength[dl.first] += dl.second;
        }
    }
}



int main() {
    fs::path dataDir = "data/10k";

    if (!fs::exists(dataDir) || !fs::is_directory(dataDir)) {
        std::cerr << "Data directory not found\n";
        return 1;
    }

    int docID = 0;
    std::vector<Document> documents;

    // Document length:
    // docID -> number of valid (non-stopword) tokens
    std::unordered_map<int, int> docLength;

    // Positional Index
    // word -> { docID -> [pos1, pos2, ...] }
    std::unordered_map<
        std::string,
        std::unordered_map<int, std::vector<int>>
    > positionalIndex;

    // Map document ID to file path
    std::unordered_map<int, std::string> docIdToName; 



/* ============================================================
   BUILD POSITIONAL INDEX
   PERFORMANCE MEASUREMENT
   ============================================================

   Design choice:
   - Parallelism is applied per-document, not per-word.

   Why per-document parallelism?
   - Each document can be tokenized independently.
   - Avoids contention on shared structures during parsing.
   - Scales well with number of documents and CPU cores.

   We deliberately avoid per-word parallelism because:
   - Words are highly shared across documents.
   - Fine-grained locking would destroy performance.
   ============================================================ */

// Decide number of threads
unsigned int numThreads = std::thread::hardware_concurrency();
if (numThreads == 0) {
    numThreads = 4;  // fallback
}

std::cout << "Using " << numThreads << " threads for indexing\n";

/* --------------------------------------------------
   1) LOAD DOCUMENTS (SINGLE-THREADED I/O)
   -------------------------------------------------- */
for (const auto& entry : fs::directory_iterator(dataDir)) {
    if (!entry.is_regular_file()) continue;

    std::string filePath = entry.path().string();
    std::ifstream file(entry.path());
    if (!file.is_open()) continue;

    std::string content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    documents.push_back({docID, filePath, content});
    docIdToName[docID] = filePath;
    docLength[docID] = 0;

    docID++;
}

/* --------------------------------------------------
   2) SINGLE-THREADED INDEXING (BASELINE)
   -------------------------------------------------- */
auto singleStart = std::chrono::high_resolution_clock::now();

indexDocuments(
    0,
    documents.size(),
    documents,
    positionalIndex,
    docLength
);

auto singleEnd = std::chrono::high_resolution_clock::now();

long long singleThreadTimeMs =
    std::chrono::duration_cast<std::chrono::milliseconds>(
        singleEnd - singleStart
    ).count();

std::cout << "Indexing time (single-thread): "
          << singleThreadTimeMs << " ms\n";

/* --------------------------------------------------
   3) RESET INDEX BEFORE MULTI-THREAD RUN
   -------------------------------------------------- */
positionalIndex.clear();
docLength.clear();

for (size_t i = 0; i < documents.size(); i++) {
    docLength[i] = 0;
}

/* --------------------------------------------------
   4) MULTI-THREADED INDEXING
   -------------------------------------------------- */
auto multiStart = std::chrono::high_resolution_clock::now();

int N = static_cast<int>(documents.size());
int chunkSize = (N + numThreads - 1) / numThreads;

std::vector<std::thread> threads;

for (unsigned int i = 0; i < numThreads; i++) {
    int start = i * chunkSize;
    int end   = std::min(start + chunkSize, N);

    if (start >= end) break;

    threads.emplace_back(
        indexDocuments,
        start,
        end,
        std::cref(documents),
        std::ref(positionalIndex),
        std::ref(docLength)
    );
}

for (auto& t : threads) {
    t.join();
}

auto multiEnd = std::chrono::high_resolution_clock::now();

long long multiThreadTimeMs =
    std::chrono::duration_cast<std::chrono::milliseconds>(
        multiEnd - multiStart
    ).count();

std::cout << "Indexing time (multi-threaded): "
          << multiThreadTimeMs << " ms\n";

/* --------------------------------------------------
   5) SPEEDUP REPORT
   -------------------------------------------------- */
if (singleThreadTimeMs > 0 && multiThreadTimeMs > 0) {
    double speedup =
        static_cast<double>(singleThreadTimeMs) /
        static_cast<double>(multiThreadTimeMs);

    std::cout << "Speedup: " << speedup << "x\n";
} else {
    std::cout << "(Note: dataset is small; indexing completes in < 1 ms)\n";
}





/* ===============================
   QUERY + TF-IDF RANKING + TOP-K
   =============================== */

std::cout << "\nEnter query: ";
std::string query;
std::getline(std::cin, query);

if (query.empty()) {
    std::cout << "Empty query. Please enter one or more words.\n";
    return 0;
}

/* ===============================
   STEP 4: Detect Phrase Query
   =============================== */
bool isPhraseQuery = false;

if (query.size() >= 2 && query.front() == '"' && query.back() == '"') {
    isPhraseQuery = true;
    query = query.substr(1, query.size() - 2);  // strip quotes
}

// Tokenize AFTER stripping quotes
auto queryTokens = tokenize(query);

/* ===============================
   PRESERVE ORDER FOR PHRASES
   =============================== */
std::vector<std::string> orderedQueryTokens;
for (const auto& token : queryTokens) {
    if (!stopWords.count(token)) {
        orderedQueryTokens.push_back(token);
    }
}

if (orderedQueryTokens.empty()) {
    std::cout << "No valid query terms after filtering stop words.\n";
    return 0;
}

/* ===============================
   STEP 6: PHRASE MATCHING (MULTI-WORD)
   =============================== */
if (isPhraseQuery && orderedQueryTokens.size() >= 2) {

    std::vector<int> matchingDocs;

    // Start from documents containing the FIRST word
    const std::string& firstWord = orderedQueryTokens[0];
    auto itFirst = positionalIndex.find(firstWord);

    if (itFirst == positionalIndex.end()) {
        std::cout << "No documents match the phrase.\n";
        return 0;
    }

    // For each candidate document
    for (const auto& docPair : itFirst->second) {
        int docID = docPair.first;
        bool matchesAll = true;

        // Check all consecutive word pairs
        for (size_t i = 0; i + 1 < orderedQueryTokens.size(); i++) {

            const std::string& w1 = orderedQueryTokens[i];
            const std::string& w2 = orderedQueryTokens[i + 1];

            auto it1 = positionalIndex.find(w1);
            auto it2 = positionalIndex.find(w2);

            if (it1 == positionalIndex.end() ||
                it2 == positionalIndex.end() ||
                it1->second.find(docID) == it1->second.end() ||
                it2->second.find(docID) == it2->second.end()) {
                matchesAll = false;
                break;
            }

            const auto& p1 = it1->second.at(docID);
            const auto& p2 = it2->second.at(docID);

            if (!phraseMatchTwoWords(p1, p2)) {
                matchesAll = false;
                break;
            }
        }

        if (matchesAll) {
            matchingDocs.push_back(docID);
        }
    }

    if (matchingDocs.empty()) {
        std::cout << "No documents match the phrase.\n";
    } else {
        std::cout << "Phrase match found in:\n";
        for (int docID : matchingDocs) {
            std::cout << "- " << docIdToName[docID] << "\n";
        }
    }

    return 0;  // ✅ stop here for phrase queries (ignore TF-IDF)
}


/* ===============================
   NORMAL TF-IDF RANKED QUERY
   =============================== */

// Deduplicate query terms ONLY for ranking
std::unordered_set<std::string> dedupedTokens(
    orderedQueryTokens.begin(),
    orderedQueryTokens.end()
);

std::vector<std::string> queryTokenVector(
    dedupedTokens.begin(),
    dedupedTokens.end()
);

// Read Top-K
std::cout << "Enter K (press Enter for default 5): ";
std::string kInput;
std::getline(std::cin, kInput);

int K = 5;
if (!kInput.empty()) {
    try {
        K = std::stoi(kInput);
        if (K <= 0) K = 5;
    } catch (...) {
        K = 5;
    }
}

auto rankedResults = rankDocuments(
    queryTokenVector,
    positionalIndex,
    docLength,
    documents.size(),
    K
);

if (rankedResults.empty()) {
    std::cout << "No query terms found in the index.\n";
} else {
    int rank = 1;
    for (const auto& p : rankedResults) {
        std::cout << "Rank " << rank << ": "
                  << docIdToName[p.first]
                  << " (score: " << p.second << ")\n";
        rank++;
    }
}


    return 0;
}

