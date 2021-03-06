#include <glog/logging.h>
#include <thread>
#include <array>
#include <cassert>
#include <xact/AtomicU64.h>
#include <xact/FixedAtomicU64Group.h>

using xact::AtomicU64;
using AtomGroup = xact::FixedAtomicU64Group<4>;
using namespace std;

int main() {
  google::InstallFailureSignalHandler();
  static const size_t kAtoms = 4;
  static const size_t kIncrements = 100000;
  std::array<xact::AtomicU64, kAtoms> atoms {0, 1, 2, 3};
  std::thread reader([&atoms]() {
    AtomGroup group{{&atoms[0], &atoms[1], &atoms[2], &atoms[3]}};
    array<uint64_t, kAtoms> values;
    for (;;) {
      if (group.load(values)) {
        for (size_t i = 1; i < kAtoms; i++) {
          // When a group `load()` succeeds, that means we have
          // a consistent snapshot.  Because the writer is incrementing
          // all elements by one, the original relationship between
          // the values will always be preserved.
          assert(values[i-1] == values[i] - 1);
        }
        if (values[0] >= kIncrements) {
          // exit once the writer has finished incrementing
          break;
        }
      }
    }
  });
  std::thread writer([&atoms]() {
    AtomGroup group{{&atoms[0], &atoms[1], &atoms[2], &atoms[3]}};
    array<uint64_t, kAtoms> currentValues;
    array<uint64_t, kAtoms> targetValues;
    size_t numSuccesses {0};
    while (numSuccesses < kIncrements) {
      for (;;) {
        if (group.load(currentValues)) {
          for (size_t i = 0; i < kAtoms; i++) {
            targetValues[i] = currentValues[i] + 1;
          }
          // CAS is overkill for a fetch-add operation --
          // normally you would use `fetchAdd` here.
          if (group.compareExchange(currentValues, targetValues)) {
            numSuccesses++;
            break;
          }
        }        
      }
    }
  });
  reader.join();
  writer.join();
  array<uint64_t, kAtoms> finalValues;
  AtomGroup group{{&atoms[0], &atoms[1], &atoms[2], &atoms[3]}};
  for (;;) {
    if (group.load(finalValues)) {
      break;
    }
  }
  for (size_t i = 1; i < kAtoms; i++) {
    assert(finalValues[i-1] == finalValues[i] - 1);
  }
  assert(finalValues[0] == kIncrements);
  LOG(INFO) << "done";
}
