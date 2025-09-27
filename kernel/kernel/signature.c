/* signature.c — add both a plain embedded string
   and a dedicated .signature section. */

#include <stddef.h>

/* Simple embedded string that must not be optimized away.
   __attribute__((used)) prevents optimization removal.
*/
__attribute__((used)) static const char aprisma_embed_sig[] =
    "AprismaKernelSignature_v1";

/* Place a signature in its own ELF section named .signature.
   __attribute__((used)) ensures it's kept; the section name makes it easy
   to find with objdump/readelf.
*/
__attribute__((used,
               section(".signature"))) static const char aprisma_section_sig[] =
    "AprismaSectionV1";
