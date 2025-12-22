#ifndef RANKER_H
#define RANKER_H

#include <vector>
#include <string>
#include <unordered_map>
#include <utility>

// Computes Term Frequency (TF)
// freq   : number of occurrences of a term in a document
// docLen : total number of valid tokens in the document
double computeTF(int freq, int docLen);

// Computes Inverse Document Frequency (IDF)
// totalDocs    : total number of documents
// docsWithTerm : number of documents containing the term
double computeIDF(int totalDocs, int docsWithTerm);

// Ranks documents using TF-IDF with positional index
// TF is derived as: positions.size()
std::vector<std::pair<int,double>> rankDocuments(
    const std::vector<std::string>& queryTokens,
    const std::unordered_map<
        std::string,
        std::unordered_map<int, std::vector<int>>
    >& positionalIndex,
    const std::unordered_map<int,int>& docLength,
    int totalDocs,
    int K
);

#endif
