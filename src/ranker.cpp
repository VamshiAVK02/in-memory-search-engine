#include "ranker.h"
#include <cmath>
#include <queue>
#include <utility>

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

// Computes TF-IDF scores for query documents
std::vector<std::pair<int,double>> rankDocuments(
    const std::vector<std::string>& queryTokens,
    const std::unordered_map<std::string, std::unordered_map<int,int>>& invertedIndex,
    const std::unordered_map<int,int>& docLength,
    int totalDocs,
    int K
)
 { 

    unordered_map<int, double> docScores;

    for (const auto& token : queryTokens) {
        auto it = invertedIndex.find(token);
        if (it == invertedIndex.end()) continue;

        const auto& posting = it->second;
        int docsWithTerm = posting.size();
        double idf = computeIDF(totalDocs, docsWithTerm);

        for (const auto& docPair : posting) {
            int docID = docPair.first;
            int freq  = docPair.second;

            auto lenIt = docLength.find(docID);
            if (lenIt == docLength.end()) continue;

            double tf = computeTF(freq, lenIt->second);
            docScores[docID] += tf * idf;
        }
    }
    
    std::priority_queue<std::pair<double, int>> pq;

    vector<pair<int,double>> results;
    for (const auto& entry : docScores) {
        int docID = entry.first;
        double score = entry.second;

        pq.push({score, docID});
    }

    vector<pair<int,double>> rankedResults;
     int count = 0;

    while (!pq.empty() && count < K) {
       auto top = pq.top();
       pq.pop();

       double score = top.first;
       int docID = top.second;

       rankedResults.emplace_back(docID, score);
       count++;
    }

    return rankedResults;

}
