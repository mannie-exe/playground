//
// PREFER { } OVER =
//
// int Pi = 3.14; implicit truncate
// int Pi { 3.14 }; FAILS
// float Pi { 3.14 }; WORKS
//
// Prefer { } uniform init syntax for
// most/all elements
//
// float Pi { 3.14 }; !! double not float
// WORKS even though 3.14 != 3.14f
// (double not float) because 3.14 is
// a constant expression and fits float!

int main() { return 0; }
