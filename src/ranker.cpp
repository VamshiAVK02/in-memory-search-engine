#include <vector>
#include <string>
#include <utility>
#include <cmath>

using std::vector;
using std::string;
using std::pair;

// Computes Term Frequency (TF)
// freq   : number of times a term appears in a document
// docLen : total number of valid tokens in the document
double computeTF(int freq, int docLen) {
    if (docLen == 0) return 0.0;  // safety check
    return static_cast<double>(freq) / docLen;
}


// Computes Inverse Document Frequency (IDF)
// totalDocs    : total number of documents in the corpus
// docsWithTerm : number of documents containing the term
double computeIDF(int totalDocs, int docsWithTerm) {
    if (docsWithTerm == 0) return 0.0;  // safety check
    return std::log(static_cast<double>(totalDocs) / docsWithTerm);
}


// Ranks documents for a given query using TF-IDF
// queryTokens : normalized, stopword-free query tokens
// Returns a list of (docID, score) pairs
vector<pair<int, double>> rankDocuments(vector<string>& queryTokens) {
    return {}; // TODO: implement ranking logic
}

/*
#include <iostream>

void testTFIDF() {
    std::cout << "TF(3, 100) = " << computeTF(3, 100) << "\n";
    std::cout << "IDF(10, 2) = " << computeIDF(10, 2) << "\n";
}
*/

