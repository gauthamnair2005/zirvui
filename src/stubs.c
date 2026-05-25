/* Stubs for symbols referenced by Rust's precompiled core library
   when used in a freestanding (-nostdlib) environment. */

/* bcmp — used by core for optimized slice comparison (memcmp alias) */
int bcmp(const void *s1, const void *s2, unsigned long n) {
    const unsigned char *a = (const unsigned char *)s1;
    const unsigned char *b = (const unsigned char *)s2;
    for (unsigned long i = 0; i < n; i++) {
        if (a[i] != b[i]) return a[i] - b[i];
    }
    return 0;
}

/* rust_eh_personality — unwinding personality routine;
   never called with panic=abort, but still referenced. */
void rust_eh_personality(void) {
}
