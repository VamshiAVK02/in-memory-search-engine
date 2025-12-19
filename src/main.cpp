
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cctype>
#include <unordered_set>
#include <unordered_map>


namespace fs = std::filesystem;

struct Document {
    int id;
    std::string path;
    std::string content;
};

// STEP 3: Tokenizer
// - Converts all characters to lowercase
// - Keeps only alphanumeric characters
// - Splits on any non-alphanumeric delimiter
// - Ignores words shorter than length 2
// This ensures consistent normalization for indexing and querying
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
    // word -> { docID -> frequency }
    std::unordered_map< std::string, std::unordered_map<int, int>> invertedIndex;

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
           docID++;
           continue;
         }

        // Validation: docID assignment
        std::cout << "DocID " << docID << " -> " << filePath << "\n";

        documents.push_back({docID, filePath, content});

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
             docLength[docID]++;
       }

        std::cout << "Filtered tokens:\n";
        for (const auto& token : tokens) {
            if (stopWords.find(token) != stopWords.end()) {
                continue; // skip stop words
            }
            std::cout << token << " ";
        }
        std::cout << "\n\n";

        docID++;
    }
    
     std::cout << "\nInverted Index (with frequencies):\n";

     for (const auto& entry : invertedIndex) {
          std::cout << entry.first << " -> { ";

         for (const auto& docPair : entry.second) {
              std::cout << docPair.first << ":" << docPair.second << " ";
         }

         std::cout << "}\n";
     }
     
     std::cout << "\nDocument Lengths:\n";
     for (const auto& entry : docLength) {
          std::cout << "Doc " << entry.first << " length: "<< entry.second << "\n";
     }

    

    // STEP 6: Multi-word Query Support
    std::cout << "\nEnter query: ";
    std::string query;
    std::getline(std::cin, query);

    // Tokenize query
    auto queryTokens = tokenize(query);

    // Remove stop words
    std::vector<std::string> filteredQueryTokens;
    for (const auto& token : queryTokens) {
        if (stopWords.find(token) == stopWords.end()) {
            filteredQueryTokens.push_back(token);
        }
    }

    // Union of document IDs
    std::unordered_set<int> resultDocIDs;

    for (const auto& token : filteredQueryTokens) {
         auto it = invertedIndex.find(token);
         if (it == invertedIndex.end()) continue;

        for (const auto& docPair : it->second) {
            resultDocIDs.insert(docPair.first);
        }
    }

    // Display results
    if (resultDocIDs.empty()) {
       std::cout << "No documents found.\n";
    } 
    else {
        std::cout << "Found in documents:\n";
        for (int docId : resultDocIDs) {
            std::cout << "- " << documents[docId].path << "\n";
        }
    }


    return 0;
}

