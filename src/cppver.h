#ifndef SRC_CPPVER_H_
#define SRC_CPPVER_H_

// C++98/03	199711L
// C++11	201103L
// C++14	201402L
// C++17	201703L
// C++20	202002L
// C++23	202302L

#ifdef _MSVC_LANG
#  define LWS_CPLUSPLUS _MSVC_LANG
#else
#  define LWS_CPLUSPLUS __cplusplus
#endif

// Detect __has_*.
#ifdef __has_feature
#  define LWS_HAS_FEATURE(x) __has_feature(x)
#else
#  define LWS_HAS_FEATURE(x) 0
#endif
#ifdef __has_include
#  define LWS_HAS_INCLUDE(x) __has_include(x)
#else
#  define LWS_HAS_INCLUDE(x) 0
#endif
#ifdef __has_cpp_attribute
#  define LWS_HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#else
#  define LWS_HAS_CPP_ATTRIBUTE(x) 0
#endif

#define LWS_HAS_CPP14_ATTRIBUTE(attribute) \
  (LWS_CPLUSPLUS >= 201402L && LWS_HAS_CPP_ATTRIBUTE(attribute))

#define LWS_HAS_CPP17_ATTRIBUTE(attribute) \
  (LWS_CPLUSPLUS >= 201703L && LWS_HAS_CPP_ATTRIBUTE(attribute))

// GE means greater or equal
#if LWS_CPLUSPLUS >= 201402L
#  define LWS_CPP14_GE 
#endif

#if LWS_CPLUSPLUS >= 201703L
#  define LWS_CPP17_GE 
#endif

#ifdef LWS_CPP14_GE
#  define LWS_CONSTEXPR constexpr
#else
#  define LWS_CONSTEXPR inline
#endif

#ifdef LWS_DEPRECATED
// Use the provided definition.
#elif LWS_HAS_CPP14_ATTRIBUTE(deprecated)
#  define LWS_DEPRECATED [[deprecated]]
#else
#  ifdef __GNUC__
#    define LWS_DEPRECATED __attribute__((deprecated))
#  elif defined(_MSC_VER)
#    define LWS_DEPRECATED __declspec(deprecated)
#  endif
#endif

#ifdef LWS_DEPRECATED_MSG
// Use the provided definition.
#elif LWS_HAS_CPP14_ATTRIBUTE(deprecated)
#  define LWS_DEPRECATED_MSG(s) [[deprecated(s)]]
#else
#  ifdef __GNUC__
#    define LWS_DEPRECATED_MSG(s) __attribute__((deprecated(s)))
#  elif defined(_MSC_VER)
#    define LWS_DEPRECATED_MSG(s) __declspec(deprecated(s))
#  endif
#endif

#endif // SRC_CPPVER_H_
