
#include <iostream>
#include <chrono>
#include <array>
#include <fstream>
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>

#include "encore.hpp"

namespace sched = encore::sched;
namespace cmdline = pasl::util::cmdline;
namespace dsl = encore::edsl;

template <class T>
T* malloc_array(size_t n) {
  return (T*)malloc(n * sizeof(T));
}

template <class Vertex_id>
class adjlist {
public:
  
  using vertex_id_type = Vertex_id;
  
  using size_type = vertex_id_type;
  
  using neighbor_list = const vertex_id_type*;
  
private:
  
  class Deleter {
  public:
    void operator()(char* ptr) {
      free(ptr);
    }
  };
  
  std::unique_ptr<char[], Deleter> ptr;
  size_type nb_offsets = 0l;  // == nb_vertices+1
  size_type nb_edges = 0l;
  vertex_id_type* start_offsets;
  vertex_id_type* start_edgelists;
  
  void check(vertex_id_type v) const {
    assert(v >= (vertex_id_type)0);
    assert(v < nb_offsets-1);
  }
  
  long alloc() {
    long nb_cells_allocated = nb_offsets + nb_edges;;
    char* p = (char*)malloc_array<vertex_id_type>(nb_cells_allocated);
    assert(nb_cells_allocated==0l || p != nullptr);
    ptr.reset(p);
    start_offsets = (vertex_id_type*)p;
    start_edgelists = &start_offsets[nb_offsets];
    return nb_cells_allocated;
  }
  
  const uint64_t GRAPH_TYPE_ADJLIST = 0xdeadbeef;
  const uint64_t GRAPH_TYPE_EDGELIST = 0xba5eba11;
  
  const int bits_per_byte = 8;
  const int graph_file_header_sz = 5;
  
public:
  
  adjlist(size_type nb_vertices = 0, size_type nb_edges = 0)
  : nb_offsets(nb_vertices+1), nb_edges(nb_edges) {
    if (nb_vertices > 0) {
      alloc();
    }
  }
  
  adjlist(const adjlist& other);
  
  adjlist& operator=(const adjlist& other);
  
  adjlist& operator=(adjlist&& other) {
    ptr = std::move(other.ptr);
    nb_offsets = std::move(other.nb_offsets);
    nb_edges = std::move(other.nb_edges);
    start_offsets = std::move(other.start_offsets);
    start_edgelists = std::move(other.start_edgelists);
    return *this;
  }
  
  size_type get_nb_vertices() const {
    return nb_offsets - 1;
  }
  
  size_type get_nb_edges() const {
    return nb_edges;
  }
  
  size_type get_out_degree_of(vertex_id_type v) const {
    check(v);
    return (size_type)(start_offsets[v+1] - start_offsets[v]);
  }
  
  neighbor_list get_out_edges_of(vertex_id_type v) const {
    check(v);
    return &start_edgelists[start_offsets[v]];
  }
  
  void load_from_file(std::string fname) {
    std::ifstream in(fname, std::ifstream::binary);
    uint64_t graph_type;
    int nbbits;
    size_type nb_vertices;
    bool is_symmetric;
    uint64_t header[graph_file_header_sz];
    in.read((char*)header, sizeof(header));
    graph_type = header[0];
    nbbits = int(header[1]);
    nb_vertices = size_type(header[2]);
    nb_offsets = nb_vertices + 1;
    nb_edges = size_type(header[3]);
    long nb_cells_alloced = alloc();
    long nb_bytes_allocated = nb_cells_alloced * sizeof(vertex_id_type);
    is_symmetric = bool(header[4]);
    if (graph_type != GRAPH_TYPE_ADJLIST) {
      std::cerr << "Bogus graph type." << std::endl;
    }
    if (sizeof(vertex_id_type) * 8 != nbbits) {
      std::cerr << "Incompatible graph file: " << sizeof(vertex_id_type) << std::endl;
      exit(0);
    }
    in.seekg (0, in.end);
    long contents_szb = long(in.tellg()) - sizeof(header);
    assert(contents_szb == nb_bytes_allocated);
    in.seekg (sizeof(header), in.beg);
    in.read (ptr.get(), contents_szb);
    in.close();
  }
  
};

template <class Vertex_id>
int* dfs(const adjlist<Vertex_id>& graph, Vertex_id source) {
  using size_type = Vertex_id;
  size_type nb_vertices = graph.get_nb_vertices();
  int* visited = malloc_array<int>(nb_vertices);
  Vertex_id* frontier = malloc_array<Vertex_id>(nb_vertices);
  for (size_type i = 0; i < nb_vertices; i++) {
    visited[i] = 0;
  }
  size_type frontier_size = 0;
  frontier[frontier_size++] = source;
  visited[source] = 1;
  while (frontier_size > 0) {
    Vertex_id v = frontier[--frontier_size];
    size_type d = graph.get_out_degree_of(v);
    const Vertex_id* ns = graph.get_out_edges_of(v);
    for (size_type e = 0; e < d; e++) {
      Vertex_id s = ns[e];
      if (visited[s]) {
        continue;
      }
      visited[s] = 1;
      frontier[frontier_size++] = s;
    }
  }
  free(frontier);
  return visited;
}

template <class Vertex_id>
class pdfs_rec : public dsl::pcfg::shared_activation_record {
public:
  
  using size_type = Vertex_id;
  
  std::atomic<int>* visited;
  const adjlist<Vertex_id>* graph;
  Vertex_id v;
  sched::incounter** join;
  
  pdfs_rec(std::atomic<int>* visited,
           const adjlist<Vertex_id>* graph,
           Vertex_id v,
           sched::incounter** join)
  : visited(visited), graph(graph), v(v), join(join) { }
  
  encore_dc_declare(encore::edsl, pdfs_rec, sar, par, dc, get_dc)
  
  typename adjlist<Vertex_id>::neighbor_list neighbors;
  size_type lo;
  size_type hi;
  Vertex_id s;
  
  static
  dc get_dc() {
    return dc::stmts( {
      dc::stmt([] (sar& s, par&) {
        s.lo = 0;
        s.hi = s.graph->get_out_degree_of(s.v);
        s.neighbors = s.graph->get_out_edges_of(s.v);
      }),
      dc::sequential_loop([] (sar& s, par&) {
        return s.lo != s.hi;
      }, dc::stmts({
        dc::stmt([] (sar& s, par&) {
          s.s = s.neighbors[s.lo];
        }),
        dc::mk_if([] (sar& s, par&) {
          return s.visited[s.s].load() == 0;
        }, dc::mk_if([] (sar& s, par&) {
              int old = 0;
              return s.visited[s.s].compare_exchange_strong(old, 1);
            }, dc::spawn_minus([] (sar& s, par&, stt st) {
                 return ecall<pdfs_rec>(st, s.visited, s.graph, s.s, s.join); },
                 [] (sar& s, par&) { return s.join; }))),
        dc::stmt([] (sar& s, par&) {
          s.lo++;
        })
      }))
    });
  }
  
};

template <class Vertex_id>
typename pdfs_rec<Vertex_id>::cfg_type pdfs_rec<Vertex_id>::cfg = pdfs_rec<Vertex_id>::get_cfg();

template <class Vertex_id>
class pdfs : public dsl::pcfg::shared_activation_record {
public:
  
  using size_type = Vertex_id;
  
  std::atomic<int>* visited;
  const adjlist<Vertex_id>* graph;
  Vertex_id v;
  
  pdfs(std::atomic<int>* visited,
       const adjlist<Vertex_id>* graph,
       Vertex_id v)
  : visited(visited), graph(graph), v(v) { }
  
  encore_dc_declare(encore::edsl, pdfs, sar, par, dc, get_dc)
  
  sched::incounter* join = nullptr;
  
  static
  dc get_dc() {
    return
      dc::join_plus([] (sar& s, par&, stt st) {
        return ecall<pdfs_rec<Vertex_id>>(st, s.visited, s.graph, s.v, &s.join); },
        [] (sar& s, par&) { return &s.join; });
  }
  
};

template <class Vertex_id>
typename pdfs<Vertex_id>::cfg_type pdfs<Vertex_id>::cfg = pdfs<Vertex_id>::get_cfg();

template <class Vertex_id>
void launch() {
  std::string fname = cmdline::parse<std::string>("infile");
  std::string algo = cmdline::parse<std::string>("algo");
  Vertex_id source = cmdline::parse<Vertex_id>("source");
  int* visited_serial = nullptr;
  std::atomic<int>* visited_parallel = nullptr;
  adjlist<Vertex_id> graph;
  graph.load_from_file(fname);
  encore::run_and_report_elapsed_time([&] {
    if (algo == "serial") {
      visited_serial = dfs(graph, source);
    } else if (algo == "pdfs") {
      Vertex_id nb_vertices = graph.get_nb_vertices();
      visited_parallel = malloc_array<std::atomic<int>>(nb_vertices);
      for (Vertex_id i = 0; i < nb_vertices; i++) {
        visited_parallel[i].store(0);
      }
      encore::launch_interpreter<pdfs<Vertex_id>>(visited_parallel, &graph, source);
    } else {
      std::cerr << "bogus value passed for -algo:" << algo << std::endl;
    }
  });
  Vertex_id nb_visited = 0;
  if (visited_serial != nullptr) {
    for (Vertex_id i = 0; i < graph.get_nb_vertices(); i++) {
      if (visited_serial[i] == 1) {
        nb_visited++;
      }
    }
  } else if (visited_parallel != nullptr) {
    for (Vertex_id i = 0; i < graph.get_nb_vertices(); i++) {
      if (visited_parallel[i].load() == 1) {
        nb_visited++;
      }
    }
  }
  std::cout << "nb_vertices " << graph.get_nb_vertices() << std::endl;
  std::cout << "nb_visited " << nb_visited << std::endl;
}

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  int n = cmdline::parse<int>("bits");
  if (n == 32) {
    launch<int>();
  } else if (n == 64) {
    launch<long>();
  } else {
    std::cerr << "bogus value passed for -bits:" << n << std::endl;
  }
  return 0;
}
