#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cctype>

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

        // STEP 3 validation: tokenize and print tokens
        auto tokens = tokenize(content);
        std::cout << "Tokens:\n";
        for (const auto& token : tokens) {
            std::cout << token << " ";
        }
        std::cout << "\n\n";

        docID++;
    }

    return 0;
}
