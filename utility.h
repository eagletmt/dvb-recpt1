#ifndef UTILITY_H
#define UTILITY_H

namespace recpt1 {
template <class T>
struct identity { typedef T type; };
};
#define DECLTYPE(e) recpt1::identity<decltype(e)>::type
#endif /* end of include guard */
