/*
 * nrfx_nvmc.h shim for nRF54L
 *
 * nRF54L has RRAM, not NVMC. This header shadows the real nrfx NVMC driver
 * header (which won't compile for nRF54L) and provides the function
 * prototypes that the vendored SDK code calls. Implementations are in
 * flash_nrf5x.c using RRAMC.
 */

#ifndef NRFX_NVMC_SHIM_H
#define NRFX_NVMC_SHIM_H

#include <stdint.h>

void nrfx_nvmc_word_write(uint32_t addr, uint32_t value);
void nrfx_nvmc_page_erase(uint32_t addr);
void nrfx_nvmc_words_write(uint32_t addr, uint32_t const *src, uint32_t num_words);

#endif /* NRFX_NVMC_SHIM_H */
