//
// PREFER { } OVER =
//
// int Pi = 3.14; // implicit truncate
// int Pi { 3.14 }; // FAILS
// float Pi { 3.14 }; // WORKS
//
// Prefer { } uniform init syntax for
// most/all elements
//
// float Pi { 3.14 }; // !! double not float
// WORKS even though 3.14 != 3.14f
// (double != float) because 3.14 is
// a constant expression (constexpr) that
// fits float!

//
// STRUCTURED BINDING OF ELEMENTS
//
// Applies to vecs, presumably string and
// arrays, and probably other container types
//
// Vec3 someVec { 0.0, 0.0, 0.0 };
// auto [x, y, z] = someVec;
// MUST use ALL container values
//
// Remember to RESERVE KNOWN size containers
// std::vector<int> list = { 0, 1 };
// list.reserve(100); // reserve full length

int main() { return 0; }
