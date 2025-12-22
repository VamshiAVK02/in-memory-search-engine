
#include <iostream>
#include <filesystem>
#include <fstream>
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

// STEP 3: 
// Tokenizer function
// - Converts text to lowercase
// - Removes punctuation
// - Splits on any non-alphanumeric character
// - Ignores words shorter than length 2
// Used for both document indexing and query processing
std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;

    for (char ch : text) {
        char c = std::tolower(static_cast<unsigned char>(ch));

        if (std::isalnum(static_cast<unsigned char>(c))) {
            current.push_back(c);
        } else {
            if (current.length() >= 2) {
                tokens.push_back(current);
            }
            current.clear();
        }
    }

    // Handle last token
    if (current.length() >= 2) {
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

void saveIndex(
    const std::string& filename,
    const std::unordered_map<std::string, std::unordered_map<int,int>>& invertedIndex
);

void loadIndex(
    const std::string& filename,
    std::unordered_map<std::string, std::unordered_map<int,int>>& invertedIndex
);

void saveIndex(
    const std::string& filename,
    const std::unordered_map<std::string, std::unordered_map<int,int>>& invertedIndex
) {
    std::ofstream out(filename);
    if (!out.is_open()) {
        std::cerr << "Failed to open index file for writing\n";
        return;
    }

    for (const auto& wordEntry : invertedIndex) {
        const std::string& word = wordEntry.first;

        for (const auto& docPair : wordEntry.second) {
            int docID = docPair.first;
            int freq  = docPair.second;

            out << word << " " << docID << " " << freq << "\n";
        }
    }

    out.close();
}

void loadIndex(
    const std::string& filename,
    std::unordered_map<std::string, std::unordered_map<int,int>>& invertedIndex
) {
    std::ifstream in(filename);
    if (!in.is_open()) {
        std::cerr << "Index file not found. Rebuilding index.\n";
        return;
    }

    invertedIndex.clear();

    std::string word;
    int docID, freq;

    while (in >> word >> docID >> freq) {
        invertedIndex[word][docID] = freq;
    }

    in.close();
}

const std::string INDEX_FILE = "index.txt";


int main() {
    fs::path dataDir = "data";

    if (!fs::exists(dataDir) || !fs::is_directory(dataDir)) {
        std::cerr << "Data directory not found\n";
        return 1;
    }

    int docID = 0;
    std::vector<Document> documents;
    // Document length map
    // docID -> number of valid tokens in the document
    std::unordered_map<int, int> docLength;

    
    // Inverted Index
    // Maps each word to a map of document IDs and term frequencies
    // Example: "learning" -> { 0:3, 2:1 }

    std::unordered_map< std::string, std::unordered_map<int, int>> invertedIndex;

    // Try loading existing index from disk
    loadIndex(INDEX_FILE, invertedIndex);

    bool indexLoaded = !invertedIndex.empty();


    // Maps document ID to file path (for clean output)
    std::unordered_map<int, std::string> docIdToName;

    for (const auto& entry : fs::directory_iterator(dataDir)) {
        if (!entry.is_regular_file()) continue;

        std::string filePath = entry.path().string();

        std::ifstream file(entry.path());
        if (!file.is_open()) {
            std::cerr << "Failed to open " << filePath << "\n";
            continue;
        }

        std::string content(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );

        if (content.empty()) {
           std::cout << "DocID " << docID << " -> " << filePath << " (empty file)\n";
           documents.push_back({docID, filePath, content});
           docIdToName[docID] = filePath;
           docID++;
           continue;
         }

        // Validation: docID assignment
        std::cout << "DocID " << docID << " -> " << filePath << "\n";

        documents.push_back({docID, filePath, content});
        docIdToName[docID] = filePath;


        // STEP 3 + STEP 4 validation
        auto tokens = tokenize(content);
        docLength[docID] = 0;

        
        for (const auto& token : tokens) {
            // Skip stop words
            if (stopWords.find(token) != stopWords.end()) {
                 continue;
            }

            // Increment term frequency
            invertedIndex[token][docID]++;

             // Increment document length (valid token)
             // Count number of valid (non-stopword) tokens in this document
             // Used later for TF-IDF normalization

             docLength[docID]++;
       }

        docID++;

    }

    if (!indexLoaded) {
        saveIndex(INDEX_FILE, invertedIndex);
    }

  

 // STEP 6: Multi-word Query Support + TF-IDF Ranking + Top-K
// --------------------------------------------------------
// - Tokenize user query
// - Remove stop words
// - Remove duplicate query terms
// - Compute TF-IDF scores
// - Display ranked Top-K results (configurable)

std::cout << "\nEnter query: ";
std::string query;
std::getline(std::cin, query);

// Handle empty query
if (query.empty()) {
    std::cout << "Empty query. Please enter one or more words.\n";
    return 0;
}

// Tokenize query using same tokenizer as documents
auto queryTokens = tokenize(query);

// Remove stop words and duplicate query terms
std::unordered_set<std::string> filteredQueryTokens;
for (const auto& token : queryTokens) {
    if (stopWords.find(token) == stopWords.end()) {
        filteredQueryTokens.insert(token);
    }
}

// Handle case: all query terms removed
if (filteredQueryTokens.empty()) {
    std::cout << "No valid query terms after filtering stop words.\n";
    return 0;
}

// Ask for Top-K
std::cout << "Enter K (press Enter for default 5): ";
std::string kInput;
std::getline(std::cin, kInput);

int K = 5; // default
if (!kInput.empty()) {
    try {
        K = std::stoi(kInput);
        if (K <= 0) K = 5;
    } catch (...) {
        K = 5;
    }
}

// Convert query tokens to vector (required by ranker)
std::vector<std::string> queryTokenVector(
    filteredQueryTokens.begin(),
    filteredQueryTokens.end()
);

// Compute ranked TF-IDF results (Top-K)
auto rankedResults = rankDocuments(
    queryTokenVector,
    invertedIndex,
    docLength,
    documents.size(),
    K
);

// Display ranked results
if (rankedResults.empty()) {
    std::cout << "No query terms found in the index.\n";
}
else {
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

