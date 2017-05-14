#include <iostream>
#include <chrono>
#include <array>
#include <fstream>
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>
#include <algorithm>

#include "graph.h"

#ifndef _ENCORE_GRAPHIO_H_
#define _ENCORE_GRAPHIO_H_

namespace pbbs {

template <class Item>
struct read_from_pasl_file_struct {
};

template <class intT>
struct read_from_pasl_file_struct<graph::graph<intT>> {
  template <class VertexType>
  void initialize(std::ifstream& in, int n, int m, intT* e, graph::vertex<intT>* v) {
    std::cerr << n << " " << m << std::endl;
    VertexType* offsets = new VertexType[n + 1];
    in.read(reinterpret_cast<char*>(offsets), sizeof(VertexType) * (n + 1));

    VertexType* eor = new VertexType[m];
    in.read(reinterpret_cast<char*>(eor), sizeof(VertexType) * m);

    for (int i = 0; i < m; i++) {
      e[i] = (intT)eor[i];
    }
    for (int i = 0; i < n; i++) {
      v[i] = graph::vertex<intT>(e + offsets[i], offsets[i + 1] - offsets[i]);
    }
    delete [] eor;
    delete [] offsets;
  }

  graph::graph<intT> operator()(std::ifstream& in) {
    uint64_t header[5];
    in.read(reinterpret_cast<char*>(header), sizeof(header));
    intT n = (intT)header[2];
    intT m = (intT)header[3];

    graph::vertex<intT>* v = (graph::vertex<intT>*)malloc(sizeof(graph::vertex<intT>) * n);
    intT* e = (intT*)malloc(sizeof(intT) * m);

    in.seekg(0, in.end);
    long long contents_size = (long long)in.tellg() - sizeof(header);
    in.seekg(sizeof(header), in.beg);
    long long vertex_type = contents_size / (n + 1 + m);

    if (vertex_type == 8) {
      initialize<long>(in, n, m, e, v);
    } else {
      initialize<int>(in, n, m, e, v);
    }
    return graph::graph<intT>(v, n, m, e);
  }
};


template <class Item>
Item read_from_pasl_file(std::ifstream& in) {
  return read_from_pasl_file_struct<Item>()(in);
}

template <class Item>
Item read_from_pasl_file(std::string file) {
  std::cerr << file << std::endl;
  std::ifstream in(file, std::ifstream::binary);
  return read_from_pasl_file<Item>(in);
}

} // namespace

#endif
