#pragma once
/* Branch prediction */
#if defined(__GNUC__) || defined(__clang__)
	#define LIKELY(x) (__builtin_expect((x), 1))
	#define UNLIKELY(x) (__builtin_expect((x), 1))
	#define ASSUME(cond) do { if (!(cond)) __builtin_unreachable(); } while (0)
	#define ASSUME_ALIGNED(var, size) do { var = VOIDSTARCAST(var) __builtin_assume_aligned(var, size); } while (0)
#else
	#define LIKELY(x) (x)
	#define UNLIKELY(x) (x)
	#define ASSUME(cond)
	#define ASSUME_ALIGNED(var, n)
#endif