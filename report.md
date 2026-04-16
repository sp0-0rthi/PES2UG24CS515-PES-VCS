# PES-VCS Assignment Report

Name: Spoorthi A N  
Course: Operating Systems  
Assignment: PES Version Control System  

---

## Introduction

In this assignment we implemented a small version control system called PES-VCS. The goal was to understand how systems like Git internally store objects, track files, and manage commits. The project was divided into multiple phases where each phase added new functionality to the system.

---

## Phase 1 – Object Storage

In the first phase, the task was to implement object storage. Files are stored as blob objects using content-addressed storage.

When a file is stored, a SHA-256 hash is computed from the contents of the object. The hash is then used as the filename inside the `.pes/objects` directory. This approach ensures that identical content is stored only once.

The following functionality was verified using the provided tests:

- Blob storage works correctly  
- Duplicate content is not stored multiple times (deduplication)  
- Stored objects maintain integrity when read back  

Running the command:

make test_objects

confirmed that all Phase 1 tests passed successfully.

---

## Phase 2 – Tree Objects

In Phase 2 we implemented tree objects which represent the structure of files stored in the repository. Tree objects contain entries that point to other objects using their hashes.

The key functions implemented in this phase include:

- Serializing tree objects
- Parsing serialized tree objects
- Generating tree objects from index entries

The serialization process ensures that entries are stored in a deterministic order so that identical trees produce identical hashes.

The implementation was verified using:

make test_tree

Both tests passed:

- tree serialize/parse roundtrip
- deterministic tree serialization

---

## Phase 3 – Index Management

Phase 3 introduced the concept of a staging area (index), similar to the one used in Git.

The index keeps track of files that have been added to the repository but not yet committed.

The following functions were implemented:

- `index_load` – reads entries from the index file
- `index_save` – writes updated entries back to disk
- `index_add` – stages files by storing their metadata and object hash

To test this functionality the following commands were executed:

./pes init  
echo "hello" > file1.txt  
echo "world" > file2.txt  
./pes add file1.txt file2.txt  
./pes status  

The output showed that both files were successfully staged.

The `.pes/index` file stores entries in the format:

100644 <hash> <mtime> <size> <path>

which contains the file mode, object hash, modification time, file size, and file path.

---

## Conclusion

Through this assignment we gained a better understanding of how version control systems internally manage data.

The implementation successfully completed the following phases:

- Phase 1 – Object storage
- Phase 2 – Tree object management
- Phase 3 – Index staging system

All provided tests passed and the repository functions as expected.
