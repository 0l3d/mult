#include "mult.h"
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>

#define CHUNK 16384

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

  char *buffer = malloc(size);
  if (buffer == NULL) {
    perror("add file memory allocation failed");
    fclose(mult_in);
    return;
  }

  size_t read_bytes = fread(buffer, 1, size, mult_in);
  if (read_bytes != size) {
    perror("add file file read error");
    free(buffer);
    fclose(mult_in);
    return;
  }

  fclose(mult_in);

  FILE *mult_out = fopen(out, "ab+");
  if (mult_out == NULL) {
    perror("add file output file open failed");
    free(buffer);
    return;
  }

  fseek(mult_out, 0, SEEK_END);
  long offset = ftell(mult_out);

  FileBlock fb;
  fb.name = strdup(in);
  fb.offset = (int)offset;
  fb.parent = parent;
  fb.size = (int)size;
  fb.data = buffer;
  int type = TYPEFILE;
  fwrite(&type, sizeof(int), 1, mult_out);
  int name_len = strlen(fb.name);
  fwrite(&name_len, sizeof(int), 1, mult_out);
  fwrite(fb.name, sizeof(char), name_len, mult_out);

  fwrite(&fb.offset, sizeof(int), 1, mult_out);
  fwrite(&fb.parent, sizeof(int), 1, mult_out);
  fwrite(&fb.size, sizeof(int), 1, mult_out);

  fwrite(fb.data, 1, fb.size, mult_out);

  free(fb.name);
  free(fb.data);
  fclose(mult_out);
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

  DirBlock dir;
  dir.name = strdup(in);
  dir.offset = (int)offset;
  dir.parent = parent;

  int type = TYPEDIR;
  fwrite(&type, sizeof(int), 1, mult_out);
  int name_len = strlen(dir.name);
  fwrite(&name_len, sizeof(int), 1, mult_out);
  fwrite(dir.name, sizeof(char), name_len, mult_out);
  fwrite(&dir.offset, sizeof(int), 1, mult_out);
  fwrite(&dir.parent, sizeof(int), 1, mult_out);

  struct dirent *de;
  while ((de = readdir(dr)) != NULL) {
    if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
      continue;

    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", in, de->d_name);

    if (de->d_type == DT_DIR) {
      mult_add_folder_to_mult_file(path, out, dir.offset);
    } else if (de->d_type == DT_REG) {
      mult_add_file_to_mult_file(path, out, dir.offset);
    }
  }

  free(dir.name);
  fclose(mult_out);
  closedir(dr);
}

void mult_extract_mult_file(const char *in) {
  FILE *infile = fopen(in, "rb");
  if (!infile) {
    perror("extract files file open failed");
    return;
  }

  fseek(infile, MULT_MAGIC_LEN, SEEK_SET);

  DirBlock dirs[512];
  int dir_count = 0;
  int type;

  while (fread(&type, sizeof(int), 1, infile) == 1) {
    if (type == TYPEDIR) {
      int name_len;
      fread(&name_len, sizeof(int), 1, infile);

      char *name = malloc(name_len + 1);
      fread(name, sizeof(char), name_len, infile);
      name[name_len] = '\0';

      int offset, parent;
      fread(&offset, sizeof(int), 1, infile);
      fread(&parent, sizeof(int), 1, infile);

      dirs[dir_count].name = name;
      dirs[dir_count].offset = offset;
      dirs[dir_count].parent = parent;
      dir_count++;
    } else if (type == TYPEFILE) {
      int name_len;
      fread(&name_len, sizeof(int), 1, infile);
      fseek(infile, name_len + sizeof(int) * 2, SEEK_CUR);
      int size;
      fread(&size, sizeof(int), 1, infile);
      fseek(infile, size, SEEK_CUR);
    } else {
      fprintf(stderr, "Unknown type\n");
      fclose(infile);
      return;
    }
  }

  for (int i = 0; i < dir_count; i++) {
    char fullpath[512] = "";

    char *basename = strrchr(dirs[i].name, '/');
    if (basename) {
      basename++;
    } else {
      basename = dirs[i].name;
    }

    if (dirs[i].parent == 0) {
      strcpy(fullpath, basename);
    } else {
      char parent_path[512] = "";
      char temp_paths[512][512];
      int path_count = 0;
      int current_offset = dirs[i].parent;
      while (current_offset != 0) {
        for (int j = 0; j < dir_count; j++) {
          if (dirs[j].offset == current_offset) {
            char *dir_basename = strrchr(dirs[j].name, '/');
            if (dir_basename) {
              dir_basename++;
            } else {
              dir_basename = dirs[j].name;
            }
            strcpy(temp_paths[path_count], dir_basename);
            path_count++;
            current_offset = dirs[j].parent;
            break;
          }
        }
      }

      for (int k = path_count - 1; k >= 0; k--) {
        if (strlen(parent_path) > 0) {
          strcat(parent_path, "/");
        }
        strcat(parent_path, temp_paths[k]);
      }

      snprintf(fullpath, sizeof(fullpath), "%s/%s", parent_path, basename);
    }

    mkdir(fullpath, 0755);
  }

  rewind(infile);
  fseek(infile, MULT_MAGIC_LEN, SEEK_SET);

  while (fread(&type, sizeof(int), 1, infile) == 1) {
    if (type == TYPEFILE) {
      int name_len;
      fread(&name_len, sizeof(int), 1, infile);

      char *fname = malloc(name_len + 1);
      fread(fname, sizeof(char), name_len, infile);
      fname[name_len] = '\0';

      int offset, fparent, size;
      fread(&offset, sizeof(int), 1, infile);
      fread(&fparent, sizeof(int), 1, infile);
      fread(&size, sizeof(int), 1, infile);

      char *data = malloc(size);
      fread(data, 1, size, infile);

      char fullpath[512] = "";

      char *basename = strrchr(fname, '/');
      if (basename) {
        basename++;
      } else {
        basename = fname;
      }

      if (fparent == 0) {
        strcpy(fullpath, basename);
      } else {

        char parent_path[512] = "";
        char temp_paths[512][512];
        int path_count = 0;
        int current_offset = fparent;

        while (current_offset != 0) {
          for (int j = 0; j < dir_count; j++) {
            if (dirs[j].offset == current_offset) {
              char *dir_basename = strrchr(dirs[j].name, '/');
              if (dir_basename) {
                dir_basename++;
              } else {
                dir_basename = dirs[j].name;
              }
              strcpy(temp_paths[path_count], dir_basename);
              path_count++;
              current_offset = dirs[j].parent;
              break;
            }
          }
        }

        for (int k = path_count - 1; k >= 0; k--) {
          if (strlen(parent_path) > 0) {
            strcat(parent_path, "/");
          }
          strcat(parent_path, temp_paths[k]);
        }

        snprintf(fullpath, sizeof(fullpath), "%s/%s", parent_path, basename);
      }

      char tmp[512];
      strcpy(tmp, fullpath);
      for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
          *p = '\0';
          mkdir(tmp, 0755);
          *p = '/';
        }
      }

      FILE *out = fopen(fullpath, "wb");
      if (out) {
        fwrite(data, 1, size, out);
        fclose(out);
      } else {
        perror("file write failed");
      }

      free(fname);
      free(data);
    } else if (type == TYPEDIR) {
      int name_len;
      fread(&name_len, sizeof(int), 1, infile);
      fseek(infile, name_len + sizeof(int) * 2, SEEK_CUR);
    } else {
      fprintf(stderr, "Unknown type\n");
      break;
    }
  }

  for (int i = 0; i < dir_count; i++) {
    free(dirs[i].name);
  }

  fclose(infile);
}

void mult_compress_file(const char *filename, const char *out_filename,
                        int compression_level) {
  FILE *infile = fopen(filename, "rb");
  if (!infile) {
    perror("Input file open failed");
    return;
  }

  FILE *outfile = fopen(out_filename, "wb");
  if (!outfile) {
    perror("Output file open failed");
    fclose(infile);
    return;
  }
  int ret, flush;
  unsigned have;
  z_stream stream;
  unsigned char in[CHUNK];
  unsigned char out[CHUNK];

  stream.zalloc = Z_NULL;
  stream.zfree = Z_NULL;
  stream.opaque = Z_NULL;
  ret = deflateInit(&stream, compression_level);
  if (ret != Z_OK)
    return;

  do {
    stream.avail_in = fread(in, 1, CHUNK, infile);
    if (ferror(infile)) {
      (void)deflateEnd(&stream);
      return;
    }
    flush = feof(infile) ? Z_FINISH : Z_NO_FLUSH;
    stream.next_in = in;

    do {
      stream.avail_out = CHUNK;
      stream.next_out = out;
      ret = deflate(&stream, flush);
      assert(ret != Z_STREAM_ERROR);
      have = CHUNK - stream.avail_out;
      if (fwrite(out, 1, have, outfile) != have || ferror(outfile)) {
        (void)deflateEnd(&stream);
        return;
      }
    } while (stream.avail_out == 0);
    assert(stream.avail_in == 0);
  } while (flush != Z_FINISH);
  assert(ret == Z_STREAM_END);

  (void)deflateEnd(&stream);
  fclose(infile);
  fclose(outfile);
}

void mult_decompress_file(const char *filename, const char *out_filename) {
  FILE *infile = fopen(filename, "rb");
  FILE *outfile = fopen(out_filename, "wb");

  int ret;
  unsigned have;
  z_stream stream;
  unsigned char in[CHUNK];
  unsigned char out[CHUNK];

  stream.zalloc = Z_NULL;
  stream.zfree = Z_NULL;
  stream.opaque = Z_NULL;
  ret = inflateInit(&stream);
  if (ret != Z_OK)
    return;

  do {
    stream.avail_in = fread(in, 1, CHUNK, infile);
    if (ferror(infile)) {
      (void)inflateEnd(&stream);
      return;
    }
    if (stream.avail_in == 0)
      break;
    stream.next_in = in;

    do {
      stream.avail_out = CHUNK;
      stream.next_out = out;
      ret = inflate(&stream, Z_NO_FLUSH);
      assert(ret != Z_STREAM_ERROR);
      switch (ret) {
      case Z_NEED_DICT:
      case Z_DATA_ERROR:
      case Z_MEM_ERROR:
        (void)inflateEnd(&stream);
        return;
      }
      have = CHUNK - stream.avail_out;
      if (fwrite(out, 1, have, outfile) != have || ferror(outfile)) {
        (void)inflateEnd(&stream);
        return;
      }
    } while (stream.avail_out == 0);
  } while (ret != Z_STREAM_END);

  (void)inflateEnd(&stream);
  fclose(infile);
  fclose(outfile);
}
