#ifndef LCC_SET_H
#define LCC_SET_H

#include "lcc_map.h"

typedef struct _lcc_set_t
{
    lcc_map_t map;
} lcc_set_t;

static inline void lcc_set_free(lcc_set_t *self) { lcc_map_free(&(self->map)); }
static inline void lcc_set_init(lcc_set_t *self) { lcc_map_init(&(self->map), 0, NULL, 0); }

static inline char lcc_set_add     (lcc_set_t *self, lcc_string_t *key) { return lcc_map_set(&(self->map), key, NULL, NULL); }
static inline char lcc_set_remove  (lcc_set_t *self, lcc_string_t *key) { return lcc_map_pop(&(self->map), key, NULL); }
static inline char lcc_set_contains(lcc_set_t *self, lcc_string_t *key) { return lcc_map_get(&(self->map), key, NULL); }

static inline char lcc_set_add_string     (lcc_set_t *self, const char *key) { return lcc_map_set_string(&(self->map), key, NULL, NULL); }
static inline char lcc_set_remove_string  (lcc_set_t *self, const char *key) { return lcc_map_pop_string(&(self->map), key, NULL); }
static inline char lcc_set_contains_string(lcc_set_t *self, const char *key) { return lcc_map_get_string(&(self->map), key, NULL); }

#endif /* LCC_SET_H */
