#ifndef MULT_H
#define MULT_H

#define MULT_MAGIC "MULT_FILE"
#define MULT_MAGIC_LEN 9
#define TYPEDIR 1
#define TYPEFILE 2

#include <stdio.h>

typedef struct {
  char *name;
  long offset;
  int parent;
  long size;
  char *data;
} FileBlock;

typedef struct {
  char *name;
  long offset;
  int parent;
} DirBlock;

void mult_generate_mult_file(const char *filename);
void mult_add_folder_to_mult_file(const char *in, const char *out, int parent);

void mult_add_file_to_mult_file(const char *in, const char *out, int parent);
void mult_remove_file_from_mult_file(const char *name, const char *in,
                                     int parent);
void mult_extract_mult_file(const char *in);

#endif
