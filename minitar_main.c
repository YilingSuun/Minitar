#include <stdio.h>
#include <string.h>

#include "file_list.h"
#include "minitar.h"

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage: %s -c|a|t|u|x -f ARCHIVE [FILE...]\n", argv[0]);
        return 0;
    }

    file_list_t files;
    file_list_init(&files);

    // TODO: Parse command-line arguments and invoke functions from 'minitar.h'
    // to execute archive operations
    char *archive_name = argv[3];
    if (strcmp(argv[1], "-c") == 0) {    // Archive Create
        if (argc < 5) {
            printf("Error, you should have at least one file to create archive file.\n");
            return 1;
        }

        for (int i = 4; i < argc; i++) {
            if (file_list_add(&files, argv[i]) == 1) {
                printf("Fail to add file in file list.\n");
                file_list_clear(&files);
                return 1;
            }
        }
        // Create archive file
        if (create_archive(archive_name, &files) == -1) {
            printf("Fail in create_archive\n");
            file_list_clear(&files);
            return 1;
        }
    } else if (strcmp(argv[1], "-a") == 0) {    // Archive Append
        if (argc < 5) {
            printf("Error, you should have at least one file to append.\n");
            file_list_clear(&files);
            return 1;
        }

        // Add file to file list
        for (int i = 4; i < argc; i++) {
            if (file_list_add(&files, argv[i]) == 1) {
                printf("Fail in file_list_add.\n");
                file_list_clear(&files);
                return 1;
            }
        }
        // Append files to archive
        if (append_files_to_archive(archive_name, &files) == -1) {
            printf("Fail in append_files_to_archive.\n");
            file_list_clear(&files);
            return 1;
        }
    } else if (strcmp(argv[1], "-t") == 0) {
        // Get the archive file list store into files
        if (get_archive_file_list(archive_name, &files) == -1) {
            printf("Fail in get_archive_file_list.\n");
            file_list_clear(&files);
            return 1;
        }

        node_t *current = files.head;
        while (current) {
            printf("%s\n", current->name);
            current = current->next;
        }
    } else if (strcmp(argv[1], "-u") == 0) {    // Archive Update
        if (argc < 5) {
            printf("Error, you should have at least one file to update.\n");
            file_list_clear(&files);
            return 1;
        }

        // Get the archive file list store in files_in_archive.
        file_list_t files_in_archive;
        file_list_init(&files_in_archive);
        if (get_archive_file_list(archive_name, &files_in_archive) ==
            -1) {    // Checks that specified archive file already exists
            printf("Fail in get_archive_file_list.\n");
            file_list_clear(&files_in_archive);
            file_list_clear(&files);
            return 1;
        }

        // Make sure all of the files are present in the specified archive file
        for (int i = 4; i < argc; i++) {
            if (!file_list_contains(&files_in_archive, argv[i])) {
                printf(
                    "Error: One or more of the specified files is not already present in "
                    "archive\n");
                file_list_clear(&files_in_archive);
                file_list_clear(&files);
                return 1;
            }
        }
        file_list_clear(&files_in_archive);

        // Update the file in archive file
        for (int i = 4; i < argc; i++) {
            if (file_list_add(&files, argv[i]) == 1) {
                printf("Fail in file_list_add.\n");
                file_list_clear(&files);
                return 1;
            }
        }
        if (append_files_to_archive(archive_name, &files) == -1) {
            printf("Fail in append_files_to_archive.\n");
            file_list_clear(&files);
            return 1;
        }
        file_list_clear(&files);
    } else if (strcmp(argv[1], "-x") == 0) {    // Archive Extract
        if (extract_files_from_archive(archive_name) == -1) {
            printf("Fail in extract_files_from_archive.\n");
            file_list_clear(&files);
            return 1;
        }
    } else {
        printf("Usage: %s -c|a|t|u|x -f ARCHIVE [FILE...]\n", argv[0]);
        file_list_clear(&files);
        return 1;
    }

    file_list_clear(&files);
    return 0;
}
