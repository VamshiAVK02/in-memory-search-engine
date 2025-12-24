#include "ranker.h"
#include <cmath>
#include <queue>

using std::vector;
using std::string;
using std::pair;
using std::unordered_map;

// Computes Term Frequency (TF)
double computeTF(int freq, int docLen) {
    if (docLen == 0) return 0.0;
    return static_cast<double>(freq) / docLen;
}

// Computes Inverse Document Frequency (IDF)
double computeIDF(int totalDocs, int docsWithTerm) {
    if (docsWithTerm == 0) return 0.0;
    return std::log(static_cast<double>(totalDocs) / docsWithTerm);
}

// Computes TF-IDF scores for query documents using positional index
std::vector<std::pair<int,double>> rankDocuments(
    const std::vector<std::string>& queryTokens,
    const std::unordered_map<
        std::string,
        std::unordered_map<int, std::vector<int>>
    >& positionalIndex,
    const std::unordered_map<int,int>& docLength,
    int totalDocs,
    int K
) {
    unordered_map<int, double> docScores;

    // Accumulate TF-IDF scores across all query terms
    for (const auto& token : queryTokens) {
        auto it = positionalIndex.find(token);
        if (it == positionalIndex.end()) continue;

        const auto& posting = it->second;
        int docsWithTerm = posting.size();
        double idf = computeIDF(totalDocs, docsWithTerm);

        for (const auto& docPair : posting) {
            int docID = docPair.first;
            int freq  = docPair.second.size(); // TF from positions

            auto lenIt = docLength.find(docID);
            if (lenIt == docLength.end()) continue;

            double tf = computeTF(freq, lenIt->second);
            docScores[docID] += tf * idf;
        }
    }

    // Rank documents using max-heap (priority queue)
    std::priority_queue<std::pair<double, int>> pq;
    for (const auto& entry : docScores) {
        pq.push({entry.second, entry.first}); // {score, docID}
    }

    // Extract Top-K results
    vector<pair<int,double>> rankedResults;
    int count = 0;

    while (!pq.empty() && count < K) {
        auto top = pq.top();
        pq.pop();

        rankedResults.emplace_back(top.second, top.first);
        count++;
    }

    return rankedResults;
}
