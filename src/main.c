#include "mult.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct Opts {
  char *files;
  char *folders;
  char *path;
  int addfile;
  int addfolder;
  int extract;
  int generate;
  int compress;
  int zlib;
};

const char *help_text =
    "Usage: mult [-h] [-e] [-g] [-c] [-z] [-f files] [-d folders] [-p path]\n"
    "  -h          Show this help\n"
    "  -e          Extract\n"
    "  -g          Generate\n"
    "  -c          Compress\n"
    "  -z          Use zlib\n"
    "  -f files    Add files (seperator: ',')\n"
    "  -d folders  Add folders (seperator: ',')\n"
    "  -p path     Path to mult file\n";

int main(int argc, char **argv) {
  struct Opts opts = {0};
  int opt;
  while ((opt = getopt(argc, argv, "hczged:f:p:")) != -1) {
    switch (opt) {
    case 'h':
      printf("%s", help_text);
      return 0;
    case 'e':
      opts.extract = 1;
      break;
    case 'p':
      opts.path = strdup(optarg);
      break;
    case 'g':
      opts.generate = 1;
      break;
    case 'c':
      opts.compress = 1;
      break;
    case 'z':
      opts.zlib = 1;
      break;
    case 'f':
      opts.addfile = 1;
      opts.files = strdup(optarg);
      break;
    case 'd':
      opts.addfolder = 1;
      opts.folders = strdup(optarg);
      break;

    default:
      printf("Undefined option: %c\n", opt);
      return -1;
    }
  }
  if (opts.generate) {
    if (opts.path) {
      char full_path[256];
      snprintf(full_path, sizeof(full_path), "%s.mult", opts.path);
      mult_generate_mult_file(full_path);

      free(opts.path);
      opts.path = strdup(full_path);
    } else {
      printf("Path undefined generate. Please use -p <path>\n");
      return -1;
    }
  }

  if (opts.addfile && !opts.extract) {
    if (opts.path == NULL) {
      printf("Path undefined. Please use -p <path>\n");
      return -1;
    }
    char *tokens = strtok(opts.files, ",");
    while (tokens != NULL) {
      mult_add_file_to_mult_file(tokens, opts.path, 0);
      tokens = strtok(NULL, ",");
    }
  }

  if (opts.addfolder && !opts.extract) {
    if (opts.path == NULL) {
      printf("Path undefined. Please use -p <path>\n");
      return -1;
    }
    char *tokens = strtok(opts.folders, ",");
    while (tokens != NULL) {
      mult_add_folder_to_mult_file(tokens, opts.path, 0);

      tokens = strtok(NULL, ",");
    }
  }

  if (opts.extract && !opts.addfile && !opts.addfolder) {
    if (opts.path) {
      mult_extract_mult_file(opts.path);
    } else {
      printf("Path undefined. Please use -p <path>\n");
      return -1;
    }
  }

  free(opts.files);
  free(opts.folders);
  free(opts.path);

  return 0;
}
