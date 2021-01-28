#ifndef ASSERT_H_INCLUDED__
#define ASSERT_H_INCLUDED__

#ifdef WIN32 // windows
// TODO:
#else // linux family
# define VTO  "\033[0m"
# define VTRR "\033[1;31m"
# define VTGG "\033[1;32m"

# define BUG(expr) \
	if (! (expr)) { \
		fprintf (stderr, VTRR "ASSERT" VTO "!! " #expr "\n"); \
		exit (1); \
	}
#endif

#endif //def ASSERT_H_INCLUDED__
