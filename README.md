# In-Memory Search Engine

## Problem Statement

Searching relevant documents from a large text collection using naive full-document scanning is inefficient and does not scale.
This project implements an in-memory search engine that preprocesses documents using an inverted index to support fast keyword search,
ranked retrieval using TF-IDF, and exact phrase queries.

The system is designed to handle tens of thousands of documents efficiently and demonstrates core information retrieval
and backend system concepts used in real-world search engines.

---

## Architecture

Documents (.txt files)
|
v
+------------------+
| Tokenizer |
| - lowercasing |
| - stop words |
| - punctuation |
+------------------+
|
v
+----------------------------+
| Positional Inverted Index |
| term -> docID -> positions |
+----------------------------+
|
v
+------------------+
| Query Processor |
| - TF-IDF ranking |
| - Phrase search |
+------------------+
|
v
Ranked Search Results

yaml
Copy code

Documents are tokenized and normalized before being indexed into a positional inverted index.
Queries are processed either using TF-IDF scoring for ranked retrieval or positional matching for phrase queries.
The entire system operates fully in memory to achieve low-latency search.

---

## Algorithms Used

### Inverted Index
An inverted index maps each term to the set of documents in which it appears.
This enables fast retrieval of candidate documents without scanning the entire document collection.

### Positional Inverted Index
To support phrase queries, the index stores the positions of each term within a document.
Phrase matching is implemented by checking whether consecutive query terms appear at adjacent positions
using a two-pointer merge algorithm.

### TF-IDF Ranking
Documents are ranked using TF-IDF scoring:
- **Term Frequency (TF)** measures how frequently a term appears in a document.
- **Inverse Document Frequency (IDF)** reduces the impact of terms that appear in many documents.

Final document scores are computed as the sum of TF-IDF scores over all query terms.

### Multithreaded Index Construction
Index construction is parallelized by dividing documents among multiple threads.
Each thread builds a local index which is later merged into the global index,
reducing lock contention and improving indexing performance.

---

## Complexity Analysis

### Index Construction
Let:
- **N** be the number of documents
- **T** be the total number of tokens

Index construction runs in **O(T)** time.
With multithreading, indexing work is distributed across threads, reducing wall-clock time.

### Query Processing
- **Keyword search**:  
  O(sum of posting list sizes for query terms)
- **Phrase queries**:  
  O(P) using a two-pointer merge approach, where P is the number of term positions

### Space Complexity
The positional inverted index requires **O(T)** space to store terms, document IDs, and term positions.

---

## Benchmarks

Benchmarks were measured using `std::chrono::high_resolution_clock` on a dataset of **10,000 documents**.
Query latency measurements exclude console I/O and reflect pure in-memory execution time.

### Dataset
- 10k documents

### Index Build Time
- **Total index build time (10k docs)**: **1035 ms**
- **Single-thread indexing (10k docs)**: **67 ms**
- **Multi-thread indexing (10k docs)**: **45 ms**

### Query Latency
Measured using automated in-memory benchmarks (10 runs per query).

- **Average phrase query latency (10k docs)**: **~1 µs**

### Multithreading Impact
- Single-thread indexing: **67 ms**
- Multi-thread indexing: **45 ms**
- Speedup: **~1.49×**

### Notes
- Query latency benchmarks exclude console input/output.
- Interactive query latency (~1400 ms) is dominated by user input and output printing rather than search computation.

---

## How to Run

### Requirements
- C++17 compatible compiler
- Linux or macOS environment

### Build
compile: g++ -std=c++17 -O2 src/*.cpp -o search_engine
Run:./search_engine data/10k

Future Work

Add cosine similarity normalization for improved ranking accuracy

Implement skip pointers to speed up posting list intersection

Support disk-based indexing for datasets larger than memory

Add incremental index updates

Expose search functionality via a REST API

---



