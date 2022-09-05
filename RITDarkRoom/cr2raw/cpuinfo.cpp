#include "cpuinfo.h"

int CPUID::_sse4_1 = -1;

bool CPUID::hasSSE4_1()
{
	if (_sse4_1 < 0)
	{
		CPUID cpuid(1);
		_sse4_1 = (cpuid.ECX() & SSE4_1_FLAG) ? 1 : 0;
	}

	return (_sse4_1 > 0);
}

CPUID::CPUID(unsigned i)
{
#ifdef _WIN32
	__cpuid((int *)regs, (int)i);
#else
	asm volatile
		("cpuid" : "=a" (regs[0]), "=b" (regs[1]), "=c" (regs[2]), "=d" (regs[3])
		: "a" (i), "c" (0));
	// ECX is set to zero for CPUID function 4
#endif
}

const uint32_t &CPUID::EAX() const {return regs[0];}
const uint32_t &CPUID::EBX() const {return regs[1];}
const uint32_t &CPUID::ECX() const {return regs[2];}
const uint32_t &CPUID::EDX() const {return regs[3];}
