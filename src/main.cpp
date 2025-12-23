
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



int main() {
    fs::path dataDir = "data";

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

    /* ===============================
       BUILD POSITIONAL INDEX
       =============================== */
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

        auto tokens = tokenize(content);

        int position = 0;  // valid-token position only
        for (const auto& token : tokens) {

            if (stopWords.count(token)) {
                continue;
            }

            positionalIndex[token][docID].push_back(position);
            docLength[docID]++;
            position++;
        }

        docID++;
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

// Phrase query if wrapped in quotes
if (query.size() >= 2 && query.front() == '"' && query.back() == '"') {
    isPhraseQuery = true;

    // Strip surrounding quotes
    query = query.substr(1, query.size() - 2);
}

// Tokenize AFTER stripping quotes
auto queryTokens = tokenize(query);

// Remove stop words and duplicates
std::unordered_set<std::string> filteredQueryTokens;
for (const auto& token : queryTokens) {
    if (!stopWords.count(token)) {
        filteredQueryTokens.insert(token);
    }
}

if (filteredQueryTokens.empty()) {
    std::cout << "No valid query terms after filtering stop words.\n";
    return 0;
}

// Optional validation (REMOVE after testing)
/*
if (isPhraseQuery) {
    std::cout << "[Phrase query detected]\n";
} else {
    std::cout << "[Normal ranked query]\n";
}
*/

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

// Convert query tokens to vector
std::vector<std::string> queryTokenVector(
    filteredQueryTokens.begin(),
    filteredQueryTokens.end()
);

/* ===============================
   NORMAL RANKED QUERY PATH
   (Phrase matching comes next step)
   =============================== */
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

