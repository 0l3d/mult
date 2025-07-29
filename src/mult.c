#include "mult.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void mult_generate_mult_file(const char *filename) {
  FILE *multfile = fopen(filename, "wb");
  if (multfile == NULL) {
    perror("Generate mult file failed");
    return;
  }

  fwrite(MULT_MAGIC, 1, MULT_MAGIC_LEN, multfile);

  fclose(multfile);
}
void mult_add_file_to_mult_file(const char *in, const char *out, int parent) {
  FILE *mult_in = fopen(in, "rb");
  if (mult_in == NULL) {
    perror("add file file add failed");
    return;
  }

  fseek(mult_in, 0, SEEK_END);
  long size = ftell(mult_in);
  rewind(mult_in);

  FILE *mult_out = fopen(out, "ab");
  if (mult_out == NULL) {
    perror("add file output file open failed");
    fclose(mult_in);
    return;
  }

  int type = TYPEFILE;
  fwrite(&type, sizeof(int), 1, mult_out);

  int name_len = strlen(in);
  fwrite(&name_len, sizeof(int), 1, mult_out);
  fwrite(in, sizeof(char), name_len, mult_out);

  long current_pos = ftell(mult_out);
  long data_offset = current_pos + sizeof(int) * 3;

  fwrite(&data_offset, sizeof(long), 1, mult_out);
  fwrite(&parent, sizeof(int), 1, mult_out);
  fwrite(&size, sizeof(long), 1, mult_out);

  char buffer[8192];
  size_t bytes_read;
  while ((bytes_read = fread(buffer, 1, sizeof(buffer), mult_in)) > 0) {
    fwrite(buffer, 1, bytes_read, mult_out);
  }

  fclose(mult_in);
  fclose(mult_out);
}

void mult_add_folder_to_mult_file(const char *in, const char *out, int parent) {
  DIR *dr = opendir(in);
  if (!dr) {
    perror("add folder open dir failed");
    return;
  }

  FILE *mult_out = fopen(out, "ab");
  if (!mult_out) {
    perror("add folder file open failed");
    closedir(dr);
    return;
  }

  long offset = ftell(mult_out);

  int type = TYPEDIR;
  fwrite(&type, sizeof(int), 1, mult_out);

  int name_len = strlen(in);
  fwrite(&name_len, sizeof(int), 1, mult_out);
  fwrite(in, sizeof(char), name_len, mult_out);
  fwrite(&offset, sizeof(long), 1, mult_out);
  fwrite(&parent, sizeof(int), 1, mult_out);

  fclose(mult_out);

  struct dirent *de;
  while ((de = readdir(dr)) != NULL) {
    if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
      continue;

    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", in, de->d_name);

    if (de->d_type == DT_DIR) {
      mult_add_folder_to_mult_file(path, out, offset);
    } else if (de->d_type == DT_REG) {
      mult_add_file_to_mult_file(path, out, offset);
    }
  }

  closedir(dr);
}
void mult_remove_file_from_mult_file(const char *name, const char *in,
                                     int parent) {
  FILE *mult_in = fopen(in, "rb");
  if (mult_in == NULL) {
    perror("remove file open failed");
    return;
  }
  fseek(mult_in, MULT_MAGIC_LEN, SEEK_SET);

  FILE *temp_out = fopen("temp_mult_file.bin", "wb");
  if (temp_out == NULL) {
    perror("remove temp file creation failed");
    fclose(mult_in);
    return;
  }

  fwrite(MULT_MAGIC, 1, MULT_MAGIC_LEN, temp_out);

  int type;
  while (fread(&type, sizeof(int), 1, mult_in) == 1) {
    if (type == TYPEFILE) {
      int name_len;
      fread(&name_len, sizeof(int), 1, mult_in);

      char *fname = malloc(name_len + 1);
      fread(fname, sizeof(char), name_len, mult_in);
      fname[name_len] = '\0';

      int offset, fparent, size;
      fread(&offset, sizeof(int), 1, mult_in);
      fread(&fparent, sizeof(int), 1, mult_in);
      fread(&size, sizeof(int), 1, mult_in);

      char *data = malloc(size);
      fread(data, 1, size, mult_in);

      if (strcmp(fname, name) == 0 && fparent == parent) {
        free(fname);
        free(data);
        continue;
      }
      fwrite(&type, sizeof(int), 1, temp_out);
      fwrite(&name_len, sizeof(int), 1, temp_out);
      fwrite(fname, sizeof(char), name_len, temp_out);
      fwrite(&offset, sizeof(int), 1, temp_out);
      fwrite(&fparent, sizeof(int), 1, temp_out);
      fwrite(&size, sizeof(int), 1, temp_out);
      fwrite(data, 1, size, temp_out);

      free(fname);
      free(data);

    } else if (type == TYPEDIR) {
      int name_len;
      fread(&name_len, sizeof(int), 1, mult_in);

      char *dname = malloc(name_len + 1);
      fread(dname, sizeof(char), name_len, mult_in);
      dname[name_len] = '\0';

      int offset, dparent;
      fread(&offset, sizeof(int), 1, mult_in);
      fread(&dparent, sizeof(int), 1, mult_in);

      fwrite(&type, sizeof(int), 1, temp_out);
      fwrite(&name_len, sizeof(int), 1, temp_out);
      fwrite(dname, sizeof(char), name_len, temp_out);
      fwrite(&offset, sizeof(int), 1, temp_out);
      fwrite(&dparent, sizeof(int), 1, temp_out);

      free(dname);

    } else {
      fprintf(stderr, "Unknown type\n");
      break;
    }
  }

  fclose(mult_in);
  fclose(temp_out);

  remove(in);
  rename("temp_mult_file.bin", in);
}

void mult_extract_mult_file(const char *in) {
  FILE *infile = fopen(in, "rb");
  if (!infile) {
    perror("extract files file open failed");
    return;
  }

  char magic[MULT_MAGIC_LEN];
  fread(magic, 1, MULT_MAGIC_LEN, infile);
  if (memcmp(magic, MULT_MAGIC, MULT_MAGIC_LEN) != 0) {
    fprintf(stderr, "Invalid mult file\n");
    fclose(infile);
    return;
  }

  typedef struct {
    char name[512];
    long offset;
    int parent;
  } DirInfo;

  DirInfo dirs[1024];
  int dir_count = 0;
  int type;

  while (fread(&type, sizeof(int), 1, infile) == 1) {
    if (type == TYPEDIR) {
      int name_len;
      fread(&name_len, sizeof(int), 1, infile);

      if (name_len >= sizeof(dirs[0].name)) {
        fprintf(stderr, "Directory name too long\n");
        fseek(infile, name_len + sizeof(long) + sizeof(int), SEEK_CUR);
        continue;
      }

      fread(dirs[dir_count].name, sizeof(char), name_len, infile);
      dirs[dir_count].name[name_len] = '\0';

      fread(&dirs[dir_count].offset, sizeof(long), 1, infile);
      fread(&dirs[dir_count].parent, sizeof(int), 1, infile);

      dir_count++;

    } else if (type == TYPEFILE) {
      int name_len;
      fread(&name_len, sizeof(int), 1, infile);
      fseek(infile, name_len + sizeof(long) + sizeof(int), SEEK_CUR);

      long size;
      fread(&size, sizeof(long), 1, infile);
      fseek(infile, size, SEEK_CUR);
    } else {
      fprintf(stderr, "Unknown type: %d\n", type);
      break;
    }
  }

  for (int i = 0; i < dir_count; i++) {
    char *basename = strrchr(dirs[i].name, '/');
    basename = basename ? basename + 1 : dirs[i].name;
    mkdir(basename, 0755);
  }

  rewind(infile);
  fseek(infile, MULT_MAGIC_LEN, SEEK_SET);

  while (fread(&type, sizeof(int), 1, infile) == 1) {
    if (type == TYPEFILE) {
      int name_len;
      fread(&name_len, sizeof(int), 1, infile);

      char fname[512];
      if (name_len >= sizeof(fname)) {
        fprintf(stderr, "Filename too long\n");
        fseek(infile, name_len + sizeof(long) + sizeof(int) + sizeof(long),
              SEEK_CUR);
        continue;
      }

      fread(fname, sizeof(char), name_len, infile);
      fname[name_len] = '\0';

      long data_offset, size;
      int fparent;
      fread(&data_offset, sizeof(long), 1, infile);
      fread(&fparent, sizeof(int), 1, infile);
      fread(&size, sizeof(long), 1, infile);

      char *basename = strrchr(fname, '/');
      basename = basename ? basename + 1 : fname;

      printf("Extracting: %s (size: %ld)\n", basename, size);

      FILE *out = fopen(basename, "wb");
      if (!out) {
        perror("Failed to create output file");
        fseek(infile, size, SEEK_CUR);
        continue;
      }

      char buffer[8192];
      long remaining = size;
      while (remaining > 0) {
        size_t to_read =
            remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
        size_t bytes_read = fread(buffer, 1, to_read, infile);
        if (bytes_read == 0)
          break;

        fwrite(buffer, 1, bytes_read, out);
        remaining -= bytes_read;
      }

      fclose(out);

    } else if (type == TYPEDIR) {
      int name_len;
      fread(&name_len, sizeof(int), 1, infile);
      fseek(infile, name_len + sizeof(long) + sizeof(int), SEEK_CUR);
    }
  }

  fclose(infile);
}
