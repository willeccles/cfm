/*
 * Hash table for files created by the copy function.
 * The source for this was heavily inspired by libbb since I am too
 * lazy to make my own entirely from scratch.
 */

#ifndef CFM_HASHTABLE_H_
#define CFM_HASHTABLE_H_

#include <sys/stat.h>
#include <sys/types.h>
#include <stdbool.h>

/*
 * Structure representing a hashed file in the hash table.
 * Use cfm_hashtable to defined a hash table rather than using this directly.
 */
struct cfm_hashed_file {
  ino_t ino;                      // inode
  dev_t dev;                      // device
  struct cfm_hashed_file* next;   // next item
  bool isdir;                     // is this a directory?
};

// Type representing a file hash table.
typedef struct cfm_hashed_file** cfm_hashtable;

/*
 * Determines whether or not a file is hashed in the given hash table.
 */
extern bool cfm_is_hashed(cfm_hashtable* table, const struct stat* st);

/*
 * Hashes a file and adds it to the hash table.
 */
extern void cfm_hash_file(cfm_hashtable* table, const struct stat* st);

/*
 * Resets a hash table.
 */
extern void cfm_reset_hashtable(cfm_hashtable* table);

#endif /* CFM_HASHTABLE_H_ */
