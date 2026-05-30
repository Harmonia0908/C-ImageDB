# C-ImageDB v1.0.0 Release Notes

## Project Summary

C-ImageDB is a pure C image data management and retrieval project. It focuses on PPM P6 and uncompressed 24-bit BMP image import, metadata persistence, basic image processing, RGB histogram feature extraction, similar image search, database management commands, and visual output generation.

TCP service support is optional and is not part of the main release path.

## Core Features

- PPM P6 and uncompressed 24-bit BMP import/export
- Metadata and RGB histogram feature persistence using binary files
- Image processing: gray, binary, blur, edge, equalize, median, gaussian, adjust
- Geometric operations: resize, resize-bilinear, rotate 90/180/270
- Similar image search using l1, l2, and normalized RGB histogram intersection
- Database commands: find-name, query, stats, compact, export
- Visual outputs: hist-export, hist-image, search-export, search-contact
- Hardening coverage for invalid images, oversized headers, deleted records, transaction consistency, and invalid parameters

## Test Status

Mainline test commands:

```bash
make clean && make
bash tests/run_basic_tests.sh
bash tests/run_db_tests.sh
bash tests/run_image_ops_tests.sh
bash tests/run_visual_tests.sh
```

Optional TCP test commands:

```bash
make server
bash tests/run_net_tests.sh
```

The TCP service is optional and may require an environment that permits binding a local TCP port.

## Known Limitations

- BMP support is limited to uncompressed 24-bit BI_RGB files
- PPM support is limited to P6 with maxval=255
- PNG, JPEG, GIF, compressed BMP, and indexed-color BMP are not supported
- Store operations use full-file loading and linear scans, so the project is intended for small educational datasets
- Search features use RGB color histograms only; texture, shape, and learned embeddings are not implemented
- No concurrent writer protection is provided
- Rotation supports 90/180/270 degrees only
- TCP service is optional and intentionally separate from the main CLI workflow

## Tag Recommendation

Recommended release tag:

```bash
v1.0.0
```
