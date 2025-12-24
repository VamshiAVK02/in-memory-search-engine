// ============================================================
// Standard library includes
// ============================================================

// I/O and strings
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>

// Containers
#include <unordered_set>
#include <unordered_map>

// Filesystem support
#include <filesystem>

// Timing and concurrency
#include <chrono>
#include <mutex>
#include <thread>

// Project headers
#include "ranker.h"

namespace fs = std::filesystem;

// ============================================================
// Document representation
// ============================================================
//
// Represents a single document in the corpus.
//
// - id      : unique numeric document identifier
// - path    : file path on disk
// - content : full text of the document
//
// Documents are loaded once (single-threaded I/O) and then
// indexed in parallel using per-document parallelism.
//
struct Document {
    int id;
    std::string path;
    std::string content;
};


// ============================================================
// Tokenizer
// ============================================================
//
// Purpose:
// - Normalizes text for indexing and querying
// - Produces consistent tokens across documents and queries
//
// Rules:
// - Converts all characters to lowercase
// - Treats any non-alphanumeric character as a delimiter
// - Splits on whitespace and punctuation
// - Ignores tokens with length < 2
//
// Notes:
// - Used for both document indexing and query processing
// - Token *positions* are tracked by the caller (important for
//   positional inverted index and phrase queries)
// - This function itself is stateless and thread-safe
//
std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;
    current.reserve(16);  // Small optimization to reduce reallocations

    for (unsigned char ch : text) {
        char c = static_cast<char>(std::tolower(ch));

        if (std::isalnum(c)) {
            current.push_back(c);
        } else {
            // Delimiter encountered: flush current token
            if (current.size() >= 2) {
                tokens.push_back(current);
            }
            current.clear();
        }
    }

    // Flush final token if present
    if (current.size() >= 2) {
        tokens.push_back(current);
    }

    return tokens;
}



/// STEP 4: Stop word list
// ---------------------
// Purpose:
// - Removes high-frequency, low-information words from indexing and queries
// - Improves ranking quality and reduces index size
//
// Notes:
// - Stop words are applied consistently during:
//   1) Document indexing
//   2) Query processing
// - Phrase queries preserve token order *after* stop-word removal

std::unordered_set<std::string> stopWords = {

    /* --------------------
       Articles
       -------------------- */
    "a", "an", "the",

    /* --------------------
       Pronouns
       -------------------- */
    "i","me","my","mine","myself",
    "you","your","yours","yourself","yourselves",
    "he","him","his","himself",
    "she","her","hers","herself",
    "it","its","itself",
    "we","us","our","ours","ourselves",
    "they","them","their","theirs","themselves",
    "one","ones","someone","anyone","everyone","nobody","nothing","something",

    /* --------------------
       Auxiliary & Modal Verbs
       -------------------- */
    "am","is","are","was","were",
    "be","been","being",
    "have","has","had","having",
    "do","does","did","doing",
    "will","would","shall","should",
    "can","could","may","might","must","ought",

    /* --------------------
       Common Verb Noise
       -------------------- */
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

    /* --------------------
       Conjunctions
       -------------------- */
    "and","or","but","if","while","because","as",
    "until","unless","although","though","whereas",
    "whether","nor","yet","so",

    /* --------------------
       Prepositions
       -------------------- */
    "of","to","in","on","at","by","for","with",
    "about","against","between","into","through",
    "during","before","after","above","below",
    "from","up","down","out","off","over","under",
    "within","without","across","behind","beyond",
    "near","along","among","around","toward","towards",

    /* --------------------
       Determiners & Quantifiers
       -------------------- */
    "this","that","these","those",
    "each","every","either","neither",
    "some","any","no","none","all","both",
    "many","much","few","several","most","least",
    "such","same","other","another",

    /* --------------------
       Adverbs
       -------------------- */
    "not","only","very","too","quite",
    "so","then","there","here",
    "when","where","why","how",
    "again","once","ever","never",
    "already","still","often","sometimes","usually",

    /* --------------------
       Comparatives & Intensifiers
       -------------------- */
    "more","most","less","least",
    "enough","rather","quite",

    /* --------------------
       Discourse / Filler Words
       -------------------- */
    "yes","no","ok","okay",
    "also","just","even","though",
    "however","therefore","thus","hence",
    "otherwise","meanwhile","furthermore",
    "moreover","nevertheless",

    /* --------------------
       Time & Frequency
       -------------------- */
    "today","yesterday","tomorrow",
    "now","then","soon","later",
    "always","never","often","sometimes","usually",

    /* --------------------
       Question Words
       -------------------- */
    "who","whom","whose",
    "which","what","when","where","why","how",

    /* --------------------
       Numbers (written)
       -------------------- */
    "zero","one","two","three","four","five","six","seven","eight","nine","ten",
    "first","second","third","fourth","fifth","sixth","seventh","eighth","ninth","tenth",

    /* --------------------
       Abbreviations & Noise
       -------------------- */
    "etc","ie","eg","vs","via","per",

    /* --------------------
       Web / Modern Noise
       -------------------- */
    "http","https","www","com","org","net",

    /* --------------------
       Generic Nouns (low semantic value)
       -------------------- */
    "thing","things","stuff",
    "something","anything","everything",
    "someone","anyone","everyone"
};

/* ============================================================
   POSitional Index Persistence (Optional Utility)
   ============================================================ */

void saveIndex(
    const std::string& filename,
    const std::unordered_map<
        std::string,
        std::unordered_map<int, std::vector<int>>
    >& positionalIndex
) {
    std::ofstream out(filename);
    if (!out) {
        std::cerr << "Error: Unable to open index file for writing\n";
        return;
    }

    for (const auto& [word, docMap] : positionalIndex) {
        for (const auto& [docID, positions] : docMap) {
            out << word << " " << docID;
            for (int pos : positions) {
                out << " " << pos;
            }
            out << '\n';
        }
    }
}

void loadIndex(
    const std::string& filename,
    std::unordered_map<
        std::string,
        std::unordered_map<int, std::vector<int>>
    >& positionalIndex
) {
    std::ifstream in(filename);
    if (!in) {
        std::cerr << "Index file not found. Rebuilding index...\n";
        return;
    }

    positionalIndex.clear();

    std::string word;
    int docID, pos;

    while (in >> word >> docID) {
        while (in.peek() == ' ') {
            in >> pos;
            positionalIndex[word][docID].push_back(pos);
        }
    }
}

const std::string INDEX_FILE = "positional_index.txt";

/* ============================================================
   PHRASE MATCHING (Two-Word Positional Merge)
   ============================================================ */

bool phraseMatchTwoWords(
    const std::vector<int>& p1,
    const std::vector<int>& p2
) {
    size_t i = 0, j = 0;

    while (i < p1.size() && j < p2.size()) {
        if (p2[j] == p1[i] + 1) {
            return true;  // exact adjacency match
        } else if (p2[j] > p1[i]) {
            ++i;
        } else {
            ++j;
        }
    }
    return false;
}

/* ============================================================
   MULTITHREADING INFRASTRUCTURE
   ============================================================ */

// Protects global index during merge
std::mutex indexMutex;

/*
NOTE ON FALSE SHARING:
- Multiple threads may update adjacent memory (docLength entries).
- This can cause cache-line contention ("false sharing").
- Not optimized here because updates are coarse-grained.
- Would require padding or per-thread buffers to eliminate fully.
*/

/* ============================================================
   DOCUMENT INDEXING WORKER (THREAD-SAFE)
   ============================================================ */

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
    /*
     Strategy:
     - Each thread builds its own local index (NO locks).
     - After processing its document range, it merges once
       into the shared global index (single lock).
     - This drastically reduces lock contention.
    */

    std::unordered_map<
        std::string,
        std::unordered_map<int, std::vector<int>>
    > localIndex;

    std::unordered_map<int, int> localDocLength;

    for (int docID = start; docID < end; ++docID) {

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

    // ---- Merge Phase (single critical section) ----
    {
        std::lock_guard<std::mutex> lock(indexMutex);

        for (auto& [word, docMap] : localIndex) {
            for (auto& [docID, positions] : docMap) {
                auto& globalPositions = globalIndex[word][docID];
                globalPositions.insert(
                    globalPositions.end(),
                    positions.begin(),
                    positions.end()
                );
            }
        }

        for (auto& [docID, len] : localDocLength) {
            globalDocLength[docID] += len;
        }
    }
}



int main() {
   /* ============================================================
   DATASET SETUP
   ============================================================ */

fs::path dataDir = "data/10k";

if (!fs::exists(dataDir) || !fs::is_directory(dataDir)) {
    std::cerr << "Data directory not found: " << dataDir << "\n";
    return 1;
}

// -------------------------------
// Document storage
// -------------------------------
int docID = 0;

// All documents loaded into memory (read-only after load)
std::vector<Document> documents;

// Document length:
// docID -> number of valid (non-stopword) tokens
std::unordered_map<int, int> docLength;

// Positional inverted index
// word -> { docID -> [pos1, pos2, ...] }
std::unordered_map<
    std::string,
    std::unordered_map<int, std::vector<int>>
> positionalIndex;

// Map document ID to file path (used for output)
std::unordered_map<int, std::string> docIdToName;


/* ============================================================
   BUILD POSITIONAL INDEX – PERFORMANCE MEASUREMENT
   ============================================================

   Design choice:
   - Parallelism is applied per-document, not per-word.

   Why per-document parallelism?
   - Each document can be tokenized independently.
   - Avoids fine-grained locking on shared word entries.
   - Scales well with number of documents and CPU cores.

   Why NOT per-word parallelism?
   - Words are shared across documents.
   - Would require locking for nearly every token.
   - Leads to severe contention and poor scalability.
   ============================================================ */

// Decide number of worker threads
unsigned int numThreads = std::thread::hardware_concurrency();
if (numThreads == 0) {
    numThreads = 4;  // Safe fallback
}

std::cout << "Using " << numThreads << " threads for indexing\n";


/* ------------------------------------------------------------
   1) LOAD DOCUMENTS (SINGLE-THREADED I/O)
   ------------------------------------------------------------
   - File I/O is intentionally single-threaded
   - Disk access does not scale well with threads
   - Keeps indexing phase purely CPU-bound
   ------------------------------------------------------------ */
for (const auto& entry : fs::directory_iterator(dataDir)) {
    if (!entry.is_regular_file()) {
        continue;
    }

    std::ifstream file(entry.path());
    if (!file.is_open()) {
        continue;
    }

    std::string content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    documents.push_back({
        docID,
        entry.path().string(),
        content
    });

    docIdToName[docID] = entry.path().string();
    docLength[docID] = 0;   // initialized, filled during indexing

    docID++;
}

/* ============================================================
   INDEX BUILD BENCHMARK
   (Single-thread vs Multi-thread)
   ============================================================ */

// --------------------------------------------------
// Measure TOTAL index build time (I/O + indexing)
// --------------------------------------------------
auto indexBuildStart = std::chrono::high_resolution_clock::now();

/* --------------------------------------------------
   1) SINGLE-THREADED INDEXING (BASELINE)
   -------------------------------------------------- */
positionalIndex.clear();
docLength.clear();
for (size_t i = 0; i < documents.size(); i++) {
    docLength[i] = 0;
}

auto singleStart = std::chrono::high_resolution_clock::now();

indexDocuments(
    0,
    static_cast<int>(documents.size()),
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
   2) MULTI-THREADED INDEXING
   -------------------------------------------------- */
positionalIndex.clear();
docLength.clear();
for (size_t i = 0; i < documents.size(); i++) {
    docLength[i] = 0;
}

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
   3) TOTAL INDEX BUILD TIME (I/O + INDEXING)
   -------------------------------------------------- */
auto indexBuildEnd = std::chrono::high_resolution_clock::now();

long long indexBuildTimeMs =
    std::chrono::duration_cast<std::chrono::milliseconds>(
        indexBuildEnd - indexBuildStart
    ).count();

std::cout << "Index build time: "
          << indexBuildTimeMs
          << " ms (" << documents.size() << " docs)\n";

/* --------------------------------------------------
   4) SPEEDUP REPORT
   -------------------------------------------------- */
if (singleThreadTimeMs > 0 && multiThreadTimeMs > 0) {
    double speedup =
        static_cast<double>(singleThreadTimeMs) /
        static_cast<double>(multiThreadTimeMs);

    std::cout << "Speedup: " << speedup << "x\n";
} else {
    std::cout << "(Dataset too small to measure speedup accurately)\n";
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

/* -------------------------------
   START QUERY TIMER
   ------------------------------- */
auto queryStart = std::chrono::high_resolution_clock::now();

/* ===============================
   DETECT PHRASE QUERY
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
   PHRASE QUERY PATH
   =============================== */
if (isPhraseQuery && orderedQueryTokens.size() >= 2) {

    std::vector<int> matchingDocs;

    const std::string& firstWord = orderedQueryTokens[0];
    auto itFirst = positionalIndex.find(firstWord);

    if (itFirst != positionalIndex.end()) {

        for (const auto& docPair : itFirst->second) {
            int docID = docPair.first;
            bool matchesAll = true;

            for (size_t i = 0; i + 1 < orderedQueryTokens.size(); i++) {
                const std::string& w1 = orderedQueryTokens[i];
                const std::string& w2 = orderedQueryTokens[i + 1];

                if (!positionalIndex.count(w1) ||
                    !positionalIndex.count(w2) ||
                    !positionalIndex[w1].count(docID) ||
                    !positionalIndex[w2].count(docID) ||
                    !phraseMatchTwoWords(
                        positionalIndex[w1][docID],
                        positionalIndex[w2][docID])) {
                    matchesAll = false;
                    break;
                }
            }

            if (matchesAll) {
                matchingDocs.push_back(docID);
            }
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

}
/* ===============================
   RANKED QUERY PATH (TF-IDF)
   =============================== */
else {

    std::unordered_set<std::string> dedupedTokens(
        orderedQueryTokens.begin(),
        orderedQueryTokens.end()
    );

    std::vector<std::string> queryTokenVector(
        dedupedTokens.begin(),
        dedupedTokens.end()
    );

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
}

/* -------------------------------
   END QUERY TIMER
   ------------------------------- */
auto queryEnd = std::chrono::high_resolution_clock::now();
long long queryTimeMs =
    std::chrono::duration_cast<std::chrono::milliseconds>(
        queryEnd - queryStart
    ).count();

std::cout << "Query latency: " << queryTimeMs << " ms\n";

    return 0;
}

