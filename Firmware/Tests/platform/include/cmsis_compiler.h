/*
 * Host test replacement for cmsis_compiler.h
 * Only the core register accessors used to detect interrupt context are provided.
 * Use HostPlatform::setInIsr() (support/HostPlatform.h) to simulate interrupt context.
 */
#ifndef HOSTTEST_CMSIS_COMPILER_H_
#define HOSTTEST_CMSIS_COMPILER_H_
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t hosttest_ipsr; //!< Simulated IPSR register. Nonzero = interrupt context

static inline uint32_t __get_PRIMASK(void) { return 0; }
static inline uint32_t __get_IPSR(void) { return hosttest_ipsr; }

#ifndef __weak
#define __weak __attribute__((weak))
#endif

#ifdef __cplusplus
}
#endif
#endif
