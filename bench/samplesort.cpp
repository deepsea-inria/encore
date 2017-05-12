#include <iostream>
#include <chrono>
#include <array>
#include <fstream>
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>
#include <algorithm>

#include "encore.hpp"
#include "sequence.hpp"
#include "samplesort.hpp"

#include "test.hpp"
#include "prandgen.hpp"
#include "sequencedata.hpp"

namespace sched = encore::sched;
namespace cmdline = deepsea::cmdline;
namespace dsl = encore::edsl;

namespace pasl {
namespace pctl {

/*---------------------------------------------------------------------*/
/* Quickcheck IO */

template <class Container>
std::ostream& operator<<(std::ostream& out, const container_wrapper<Container>& c) {
  out << c.c;
  return out;
}

/*---------------------------------------------------------------------*/
/* Quickcheck generators */

using value_type = unsigned int;
  
int m = 1;

void generate(size_t _nb, parray<value_type>& dst) {
  long n = _nb * m;
  std::cerr << "Size: " << n << "\n";
  int r = quickcheck::generateInRange(0, 2); // currently something is wrong with exp_dist
  //  std::cerr << r << "\n";
  if (r == 0) {
    dst = sequencedata::rand_int_range((value_type)0, (value_type)n, (value_type)INT_MAX);
  } else if (r == 1) {
    int m = quickcheck::generateInRange(0, 1 << 10);
    dst = sequencedata::almost_sorted<value_type>(0L, n, m);
  } else if (r == 2) {
    value_type x = (value_type)quickcheck::generateInRange(0, INT_MAX);
    dst = sequencedata::all_same(n, x);
  } else {
    dst = sequencedata::exp_dist<value_type>(0L, (value_type)n);
  }
}

void generate(size_t nb, container_wrapper<parray<value_type>>& c) {
  generate(nb, c.c);
}


/*---------------------------------------------------------------------*/
/* Quickcheck properties */

using parray_wrapper = container_wrapper<parray<value_type>>;

class sorted_property : public quickcheck::Property<parray_wrapper> {
public:
  
  bool holdsFor(const parray_wrapper& _in) {
    parray<value_type> a = _in.c;
    parray<value_type> b = _in.c;
    auto p = std::less<value_type>();
    encore::launch_interpreter<encorebench::sampleSort<value_type, typeof(p), intT>>(a.begin(), a.size(), p);
    std::sort(b.begin(), b.end(), p);
    return same_sequence(a.cbegin(), a.cend(), b.cbegin(), b.cend());
  }
  
};

} // end namespace
} // end namespace

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  encorebench::SSORT_THR = cmdline::parse_or_default_int("threshold", encorebench::SSORT_THR);
  pasl::pctl::m = cmdline::parse_or_default("m", pasl::pctl::m);
  int nb_tests = cmdline::parse_or_default_int("nb_tests", 1000);
  checkit<pasl::pctl::sorted_property>(nb_tests, "samplesort is correct");
  return 0;
}
