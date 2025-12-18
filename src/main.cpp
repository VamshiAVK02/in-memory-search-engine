#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cctype>
#include <unordered_set>

namespace fs = std::filesystem;

struct Document {
    int id;
    std::string path;
    std::string content;
};

// STEP 3: Tokenizer
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
    "a", "an", "the",

    // pronouns
    "i", "me", "my", "mine", "myself",
    "you", "your", "yours", "yourself", "yourselves",
    "he", "him", "his", "himself",
    "she", "her", "hers", "herself",
    "it", "its", "itself",
    "we", "us", "our", "ours", "ourselves",
    "they", "them", "their", "theirs", "themselves",

    // auxiliary verbs
    "am", "is", "are", "was", "were",
    "be", "been", "being",
    "have", "has", "had", "having",
    "do", "does", "did", "doing",
    "will", "would", "shall", "should",
    "can", "could", "may", "might", "must",

    // conjunctions
    "and", "or", "but", "if", "while", "because", "as",
    "until", "unless", "although", "though", "whereas",

    // prepositions
    "of", "to", "in", "on", "at", "by", "for", "with",
    "about", "against", "between", "into", "through",
    "during", "before", "after", "above", "below",
    "from", "up", "down", "out", "off", "over", "under",

    // determiners
    "this", "that", "these", "those",
    "each", "every", "either", "neither",
    "some", "any", "no", "none", "all", "both",
    "such", "same", "other", "another",

    // adverbs
    "not", "only", "very", "too", "quite",
    "so", "then", "there", "here", "when",
    "where", "why", "how",

    // common verbs (high-frequency noise)
    "say", "says", "said",
    "get", "gets", "got",
    "make", "makes", "made",
    "go", "goes", "went",
    "know", "knows", "knew",
    "think", "thinks", "thought",
    "see", "sees", "saw",
    "come", "comes", "came",
    "take", "takes", "took",
    "use", "uses", "used",
    "find", "finds", "found",
    "give", "gives", "gave",
    "tell", "tells", "told",
    "work", "works", "worked",

    // misc fillers
    "yes", "no",
    "also", "just", "even",
    "than", "rather",
    "via", "per"
};

int main() {
    fs::path dataDir = "data";

    if (!fs::exists(dataDir) || !fs::is_directory(dataDir)) {
        std::cerr << "Data directory not found\n";
        return 1;
    }

    int docID = 0;
    std::vector<Document> documents;

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

        // Validation: docID assignment
        std::cout << "DocID " << docID << " -> " << filePath << "\n";

        documents.push_back({docID, filePath, content});

        // STEP 3 + STEP 4 validation
        auto tokens = tokenize(content);

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

    return 0;
}
