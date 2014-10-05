void memcpy(void *dest, void *src, int bytes) {
    int i;
    unsigned char *d = (unsigned char *)dest;
    unsigned char *s = (unsigned char *)src;
    for (i = 0; i < bytes; i++) {
        *(d++) = *(s++);
    }
}
