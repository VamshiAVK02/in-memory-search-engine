#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Document {
    int id;
    std::string path;
    std::string content;
};

int main() {
    // Path to data directory
    fs::path dataDir = "data";

    // Validate directory exists
    if (!fs::exists(dataDir) || !fs::is_directory(dataDir)) {
        std::cerr << "Data directory not found\n";
        return 1;
    }

    int docID = 0;
    std::vector<Document> documents;

    // Iterate over files in /data
    for (const auto& entry : fs::directory_iterator(dataDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        std::string filePath = entry.path().string();

        // Open and read file
        std::ifstream file(entry.path());
        if (!file.is_open()) {
            std::cerr << "Failed to open " << filePath << "\n";
            continue;
        }

        // Read full content
        std::string content(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );

        // Validation output
        std::cout << "DocID " << docID << " -> " << filePath << "\n";

        // Store document
        documents.push_back({docID, filePath, content});
        docID++;
    }

    return 0;
}
