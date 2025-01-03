static inline void __cpuid__ (int leaf, int *a, int *b, int *c, int *d) {
   __asm__ __volatile__ (
      "cpuid"
      : "=a" (*a), "=b" (*b), "=c" (*c), "=d" (*d)
      : "a" (leaf)
      : "memory");
}
