# MULT

mult is simple archive format. (like tar.)

## Features

- Extract Files and Folders.
- Add Files and Folders.
- ZLIB Compression

## Usage

```bash
git clone https://github.com/0l3d/mult.git
cd mult/
make

./mult -h
```

## API

- `mult_generate_mult_file` - Creates a mult file.
- `mult_add_folder_to_mult_file` - Adds a folder to a mult file.
- `mult_add_file_to_mult_file` - Adds a file to a mult file.
- `mult_remove_file_from_mult_file` - Removes a file from a mult file.
- `mult_extract_mult_file` - Extracts mult file.

## License

This project is licensed under the **GPL-3.0 License**.

## Author

Created By **0l3d**.
