/* 
 * Copyright 2016 Emaad Ahmed Manzoor
 * License: Apache License, Version 2.0
 * http://www3.cs.stonybrook.edu/~emanzoor/streamspot/
 */

#include <algorithm>
#include <bitset>
#include <cassert>
#include <chrono>
#include <cmath>
#include "graph.h"
#include "hash.h"
#include <iostream>
#include "param.h"
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

namespace std {

void update_graphs(edge& e, unordered_map<string,graph>& graphs) {
  auto& src_id = get<F_S>(e);
  auto& src_type = get<F_STYPE>(e);
  auto& dst_id = get<F_D>(e);
  auto& dst_type = get<F_DTYPE>(e);
  auto& e_type = get<F_ETYPE>(e);
  auto& gid = get<F_GID>(e);

  // append edge to the edge list for the source
  graphs[gid][make_pair(src_id,
                        src_type)].push_back(make_tuple(dst_id,
                                                        dst_type,
                                                        e_type));
}

void construct_shingle_vectors(unordered_map<string,shingle_vector>& shingle_vectors,
                               unordered_map<string,uint32_t>& shingle_id,
                               const unordered_map<string,graph>& graphs,
                               uint32_t chunk_length) {

  unordered_set<string> unique_shingles;
  unordered_map<string,unordered_map<string,uint32_t>> temp_shingle_vectors;

  // construct a temporary shingle vector for each graph
  for (auto& kvg : graphs) {
    //cout << "\tConstructing shingles for graph: " << kvg.first << endl;
    for (auto& kv : graphs.at(kvg.first)) {
      // OkBFT from (src_id,type) = kv.first to construct shingle
#ifdef VERBOSE
      cout << "OkBFT from " << kv.first.first << " " << kv.first.second;
      cout << " (K = " << K << ")";
      cout << " fanout = " << kv.second.size() << endl;
#endif

      string shingle; // shingle from this source node
      queue<tuple<string,char,char>> q; // (nodeid, nodetype, edgetype)
      unordered_map<string,uint32_t> d;

      q.push(make_tuple(kv.first.first, kv.first.second, kv.first.second));
      d[kv.first.first] = 0;

      while (!q.empty()) {
        auto node = q.front();
        auto uid = get<0>(node);
        auto utype = get<1>(node);
        auto etype = get<2>(node);
        q.pop();

#ifdef VERBOSE
        cout << "\tPopped (" << uid << ", " << utype << ", " << etype << ")\n";
#endif
        // use destination and edge types to construct shingle
        shingle += etype;
        shingle += utype;

        if (d[uid] == K) { // node is K hops away from src_id
#ifdef VERBOSE
          cout << "Hop limit reached\n";
#endif
          continue;        // don't follow its edges
        }

        if (kvg.second.find(make_pair(uid, utype)) == kvg.second.end())
          continue;

        // outgoing edges are already sorted by timestamp
        for (auto& e : kvg.second.at(make_pair(uid, utype))) {
          auto vid = get<0>(e);
          assert(uid != vid); // no self loops allowed
          d[vid] = d[uid] + 1;
          q.push(e);
        }
      }

      // split shingle into chunks and increment frequency
      for (auto& chunk : get_string_chunks(shingle, chunk_length)) {
        temp_shingle_vectors[kvg.first][chunk]++;
        unique_shingles.insert(chunk);
      }
    }
#ifdef DEBUG
    cout << "Shingles in graph " << kvg.first << ":\n";
    for (auto& kv : temp_shingle_vectors[kvg.first]) {
      cout << "\t" << kv.first << " => " << kv.second << endl;
    }
#endif
  }


  // use unique shingles to assign shingle id's
  uint32_t current_id = 0;
  for (auto& shingle : unique_shingles) {
    shingle_id[shingle] = current_id;
    current_id++;
  }
#ifdef DEBUG
  cout << "Shingle ID's\n";
  for (auto& kv : shingle_id) {
    cout << "\t" << kv.first << " => " << kv.second << endl;
  }
#endif

  // construct shingle vectors using shingle id's and temporary shingle vectors
  uint32_t num_unique_shingles = unique_shingles.size();
  for (auto& kvg : graphs) {
    shingle_vectors[kvg.first].resize(num_unique_shingles);
    for (auto& kv : temp_shingle_vectors[kvg.first]) {
      shingle_vectors[kvg.first][shingle_id[kv.first]] = kv.second;
    }
  }

#ifdef DEBUG
  cout << "Shingle vectors:\n";
  for (auto& kvg : shingle_vectors) {
    cout << "\tSV for graph " << kvg.first << ": ";
    for (auto& e : shingle_vectors[kvg.first]) {
      cout << e << " ";
    }
    cout << endl;
  }
#endif
}

vector<string> get_string_chunks(string s, uint32_t len) {
  vector<string> chunks;
  for (uint32_t offset = 0; offset < s.length(); offset += len) {
    chunks.push_back(s.substr(offset, len));
  }
  return chunks;
}

double cosine_similarity(const shingle_vector& sv1, const shingle_vector& sv2) {
  double dot_product = 0.0, magnitude1 = 0.0, magnitude2 = 0.0;

  uint32_t size = sv1.size();
  assert(sv1.size() == sv2.size());
  for (uint32_t i = 0; i < size; i++) {
    magnitude1 += static_cast<double>(sv1[i]) * static_cast<double>(sv1[i]);
  }

  for (uint32_t i = 0; i < size; i++) {
    magnitude2 += static_cast<double>(sv2[i]) * static_cast<double>(sv2[i]);
  }

  for (uint32_t i = 0; i < size; i++) {
    dot_product += static_cast<double>(sv1[i]) * static_cast<double>(sv2[i]);
  }

  double cosine =  dot_product / (sqrt(magnitude1) * sqrt(magnitude2));

  assert(cosine >= 0.0 && cosine <= 1.0);
  return cosine;
}

} // namespace
