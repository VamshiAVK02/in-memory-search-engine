#ifndef RANKER_H
#define RANKER_H

#include <vector>
#include <string>
#include <unordered_map>
#include <utility>

// Computes Term Frequency (TF)
double computeTF(int freq, int docLen);

// Computes Inverse Document Frequency (IDF)
double computeIDF(int totalDocs, int docsWithTerm);

// Ranks documents using TF-IDF
std::vector<std::pair<int,double>> rankDocuments(
    const std::vector<std::string>& queryTokens,
    const std::unordered_map<std::string, std::unordered_map<int,int>>& invertedIndex,
    const std::unordered_map<int,int>& docLength,
    int totalDocs,
    int K
);


#endif
