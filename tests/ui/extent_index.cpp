#include <limits>
#include <numeric>
#include <vector>

#include <support/Test.hpp>
#include <ui/collections/ExtentIndex.hpp>

using namespace playground;

int main() {
  return test::run([] {
    std::vector<double> values(67);
    for (std::size_t i = 0; i < values.size(); ++i)
      values[i] = static_cast<double>(i % 9);
    ui::detail::ExtentIndex index{values};
    for (int iteration = 0; iteration < 100; ++iteration) {
      const auto changed = static_cast<std::size_t>(iteration) % values.size();
      values[changed] = iteration % 13;
      index.set(changed, values[changed]);
      for (std::size_t count = 0; count <= values.size(); ++count)
        test::require(
            index.prefix(count) ==
                std::accumulate(values.begin(), values.begin() + count, 0.0),
            "Fenwick prefix agrees with linear oracle after updates");
      for (double position = -1; position <= index.total() + 1;
           position += 0.5) {
        std::size_t expected{};
        double end{};
        if (position >= 0)
          while (expected < values.size() && end + values[expected] <= position)
            end += values[expected++];
        test::require(
            index.itemAt(position) == expected,
            "Fenwick lower bound agrees with half-open linear oracle");
      }
    }
    const auto total = index.total();
    test::rejects([&] { index.set(0, -1); }, "negative update rejected");
    test::rejects(
        [&] { index.set(0, std::numeric_limits<double>::quiet_NaN()); },
        "NaN update rejected");
    test::rejects<std::out_of_range>([&] { index.set(values.size(), 1); },
                                     "out-of-range update rejected");
    test::rejects([&] { index.reset(std::vector<double>{1, -1, 3}); },
                  "invalid replacement rejected");
    test::require(index.total() == total && index.size() == values.size(),
                  "rejected operations preserve old index");
    index.reset({});
    test::require(index.total() == 0 && index.itemAt(0) == 0,
                  "empty index is usable");
  });
}
