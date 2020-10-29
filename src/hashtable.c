#include <sys/stat.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdlib.h>


#include "hashtable.h"

#define HASH_SIZE 311U // prime number
#define hashfn(ino) ((unsigned)(ino) % HASH_SIZE)

bool cfm_is_hashed(cfm_hashtable* table, const struct stat* st) {
    if (!table || !*table) {
        return false;
    }

    struct cfm_hashed_file* elem = (*table)[hashfn(st->st_ino)];
    while (elem != NULL) {
        if (elem->ino == st->st_ino
                && elem->dev == st->st_dev
                && elem->isdir == !!S_ISDIR(st->st_mode)) {
            return true;
        }
        elem = elem->next;
    }
    return false;
}

void cfm_hash_file(cfm_hashtable* table, const struct stat* st) {
    if (!table) {
        return;
    }

    struct cfm_hashed_file* elem = malloc(sizeof(struct cfm_hashed_file));
    elem->ino = st->st_ino;
    elem->dev = st->st_dev;
    elem->isdir = !!S_ISDIR(st->st_mode);

    if (!*table) {
        *table = calloc(HASH_SIZE, sizeof(**table));
    }

    int i = hashfn(st->st_ino);
    elem->next = (*table)[i];
    (*table)[i] = elem;
}

void cfm_reset_hashtable(cfm_hashtable* table) {
    if (!table || !*table) {
        return;
    }

    struct cfm_hashed_file* elem;
    struct cfm_hashed_file* next;
    for (size_t i = 0; i < HASH_SIZE; i++) {
        elem = (*table)[i];
        while (elem != NULL) {
            next = elem->next;
            free(elem);
            elem = next;
        }
    }
    free(*table);
    *table = NULL;
}

