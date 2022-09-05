#ifndef CPUID_H
#define CPUID_H

#ifdef _WIN32
#include <limits.h>
#include <intrin.h>
typedef unsigned __int32  uint32_t;
#else
#include <stdint.h>
#endif

class CPUID {
	uint32_t regs[4];

public:
	explicit CPUID(unsigned i);
	const uint32_t &EAX() const;
	const uint32_t &EBX() const;
	const uint32_t &ECX() const;
	const uint32_t &EDX() const;

	static bool hasSSE4_1();

private:
	static int	_sse4_1;
};

#define SSE4_1_FLAG     0x080000
#define SSE4_2_FLAG     0x100000

#endif