#ifndef PMM_H
#define PMM_H

#include <stdbool.h>
#include <stdint.h>
#include <tartarus.h>

extern tartarus_memap_entry_t *g_memap;
extern uint16_t g_memap_length;

void pmm_initialize();
void *pmm_request_page();
void *pmm_request_linear_pages(uint64_t number_of_pages);
void *pmm_request_linear_pages_type(uint64_t number_of_pages, tartarus_memap_entry_type_t type);

bool pmm_memap_claim(uint64_t base, uint64_t length, tartarus_memap_entry_type_t type);

void pmm_cpy(void *source, void *dest, int nbytes);
void pmm_set(uint8_t value, void *dest, int len);

#endif