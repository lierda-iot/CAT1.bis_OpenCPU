#ifndef TIRTC_STORAGE_EXAMPLE_H
#define TIRTC_STORAGE_EXAMPLE_H
/* Call from one background business task after storage becomes ready.
 * Appends a short record to /tirtc-example.txt, syncs and reads it back.
 * Returns 0 on success or a negative storage error. Never called at boot. */
int tirtc_storage_example_append_and_verify(void);
#endif
