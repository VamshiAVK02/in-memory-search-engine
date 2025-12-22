#include <vector>
#include <string>
#include <utility>
using std::vector;
using std::string;
using std::pair;

// Computes Term Frequency (TF)
// freq    : number of times a term appears in a document
// docLen  : total number of valid tokens in the document
double computeTF(int freq, int docLen) {
    return 0.0; // TODO: implement TF calculation
}

// Computes Inverse Document Frequency (IDF)
// totalDocs     : total number of documents in the corpus
// docsWithTerm  : number of documents containing the term
double computeIDF(int totalDocs, int docsWithTerm) {
    return 0.0; // TODO: implement IDF calculation
}

// Ranks documents for a given query using TF-IDF
// queryTokens : normalized, stopword-free query tokens
// Returns a list of (docID, score) pairs
vector<pair<int, double>> rankDocuments(vector<string>& queryTokens) {
    return {}; // TODO: implement ranking logic
}
