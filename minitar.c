#include "minitar.h"

#include <fcntl.h>
#include <grp.h>
#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>

#define NUM_TRAILING_BLOCKS 2
#define MAX_MSG_LEN 128
#define BLOCK_SIZE 512

// Constants for tar compatibility information
#define MAGIC "ustar"

// Constants to represent different file types
// We'll only use regular files in this project
#define REGTYPE '0'
#define DIRTYPE '5'

/*
 * Helper function to compute the checksum of a tar header block
 * Performs a simple sum over all bytes in the header in accordance with POSIX
 * standard for tar file structure.
 */
void compute_checksum(tar_header *header) {
    // Have to initially set header's checksum to "all blanks"
    memset(header->chksum, ' ', 8);
    unsigned sum = 0;
    char *bytes = (char *) header;
    for (int i = 0; i < sizeof(tar_header); i++) {
        sum += bytes[i];
    }
    snprintf(header->chksum, 8, "%07o", sum);
}

/*
 * Populates a tar header block pointed to by 'header' with metadata about
 * the file identified by 'file_name'.
 * Returns 0 on success or -1 if an error occurs
 */
int fill_tar_header(tar_header *header, const char *file_name) {
    memset(header, 0, sizeof(tar_header));
    char err_msg[MAX_MSG_LEN];
    struct stat stat_buf;
    // stat is a system call to inspect file metadata
    if (stat(file_name, &stat_buf) != 0) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to stat file %s", file_name);
        perror(err_msg);
        return -1;
    }

    strncpy(header->name, file_name, 100);    // Name of the file, null-terminated string
    snprintf(header->mode, 8, "%07o",
             stat_buf.st_mode & 07777);    // Permissions for file, 0-padded octal

    snprintf(header->uid, 8, "%07o", stat_buf.st_uid);    // Owner ID of the file, 0-padded octal
    struct passwd *pwd = getpwuid(stat_buf.st_uid);       // Look up name corresponding to owner ID
    if (pwd == NULL) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to look up owner name of file %s", file_name);
        perror(err_msg);
        return -1;
    }
    strncpy(header->uname, pwd->pw_name, 32);    // Owner name of the file, null-terminated string

    snprintf(header->gid, 8, "%07o", stat_buf.st_gid);    // Group ID of the file, 0-padded octal
    struct group *grp = getgrgid(stat_buf.st_gid);        // Look up name corresponding to group ID
    if (grp == NULL) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to look up group name of file %s", file_name);
        perror(err_msg);
        return -1;
    }
    strncpy(header->gname, grp->gr_name, 32);    // Group name of the file, null-terminated string

    snprintf(header->size, 12, "%011o",
             (unsigned) stat_buf.st_size);    // File size, 0-padded octal
    snprintf(header->mtime, 12, "%011o",
             (unsigned) stat_buf.st_mtime);    // Modification time, 0-padded octal
    header->typeflag = REGTYPE;                // File type, always regular file in this project
    strncpy(header->magic, MAGIC, 6);          // Special, standardized sequence of bytes
    memcpy(header->version, "00", 2);          // A bit weird, sidesteps null termination
    snprintf(header->devmajor, 8, "%07o",
             major(stat_buf.st_dev));    // Major device number, 0-padded octal
    snprintf(header->devminor, 8, "%07o",
             minor(stat_buf.st_dev));    // Minor device number, 0-padded octal

    compute_checksum(header);
    return 0;
}

/*
 * Removes 'nbytes' bytes from the file identified by 'file_name'
 * Returns 0 upon success, -1 upon error
 * Note: This function uses lower-level I/O syscalls (not stdio), which we'll learn about later
 */
int remove_trailing_bytes(const char *file_name, size_t nbytes) {
    char err_msg[MAX_MSG_LEN];

    struct stat stat_buf;
    if (stat(file_name, &stat_buf) != 0) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to stat file %s", file_name);
        perror(err_msg);
        return -1;
    }

    off_t file_size = stat_buf.st_size;
    if (nbytes > file_size) {
        file_size = 0;
    } else {
        file_size -= nbytes;
    }

    if (truncate(file_name, file_size) != 0) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to truncate file %s", file_name);
        perror(err_msg);
        return -1;
    }
    return 0;
}

/*
 * Obtain the size of current file.
 * Must be use after fopen()
 */
int get_size(FILE *fp) {
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size < 0) {
        return -1;
    }
    return size;
}

/*
 * Check if all the member in a char array is 0
 */
int allZeros(const char arr[], int size) {
    for (int i = 0; i < size; i++) {
        if (arr[i] != 0) {
            return 0;
        }
    }
    return 1;
}

int create_archive(const char *archive_name, const file_list_t *files) {
    // Create archive file
    FILE *afp = fopen(archive_name, "w");
    if (!afp) {
        perror("Archive file fopen error: ");
        return -1;
    }

    node_t *current = files->head;
    // Iterate through all specified files
    while (current) {
        // Generate a header
        tar_header header;
        if (fill_tar_header(&header, current->name) == 0) {
            if (fwrite(&header, BLOCK_SIZE, 1, afp) != 1) {
                perror("Header fwrite error: ");
                if (fclose(afp)) {
                    perror("Error in closing archive file.");
                }
                return -1;
            }
        } else {
            perror("Fill tar header error");
            if (fclose(afp)) {
                perror("Error in closing archive file.");
            }
            return -1;
        }

        // Open the current file prepare for read
        FILE *cfp = fopen(current->name, "r");
        if (!cfp) {
            perror("Current file fopen error: ");
            if (fclose(afp)) {
                perror("Error in closing archive file.");
            }
            return -1;
        }

        // Obtain the size of current file.
        int size = get_size(cfp);
        if (size < 0) {
            printf("Error happend in checking the size of current file.\n");
            if (fclose(cfp)) {
                perror("Error in closing current file.");
            }
            if (fclose(afp)) {
                perror("Error in closing archive file.");
            }
            return -1;
        }

        // Create a buffer that every byte is initially zero, ensure buffer size is multiple of 512
        int adjust_size = (size + BLOCK_SIZE - 1) / BLOCK_SIZE * BLOCK_SIZE;
        char *buffer = (char *) malloc(adjust_size);
        if (!buffer) {
            perror("Failed to allocate");
            if (fclose(cfp)) {
                perror("Error in closing current file.");
            }
            if (fclose(afp)) {
                perror("Error in closing archive file.");
            }
            return -1;
        }
        memset(buffer, 0, adjust_size);

        // Read and write the current file
        fread(buffer, 1, adjust_size, cfp);
        // If the fread isn't successful
        if (ferror(cfp)) {
            perror("Current file fread Error:");
            free(buffer);
            if (fclose(cfp)) {
                perror("Error in closing current file.");
            }
            if (fclose(afp)) {
                perror("Error in closing archive file.");
            }
            return -1;
        }
        if (fwrite(buffer, 1, adjust_size, afp) != adjust_size) {
            perror("Current file fwrite Error:");
            free(buffer);
            if (fclose(cfp)) {
                perror("Error in closing current file.");
            }
            if (fclose(afp)) {
                perror("Error in closing archive file.");
            }
            return -1;
        }

        free(buffer);
        if (fclose(cfp)) {
            perror("Error in closing current file.");
            return -1;
        }
        current = current->next;
    }
    // Adds two-block footer to archive
    char footer[1024] = {0};
    if (fwrite(footer, BLOCK_SIZE, 2, afp) < 2) {
        perror("Footer fwrite error");
        fclose(afp);
        return -1;
    }
    if (fclose(afp)) {
        perror("Error in closing archive file.");
        return -1;
    }
    return 0;
}

int append_files_to_archive(const char *archive_name, const file_list_t *files) {
    // Make sure the archive file actually exists first
    FILE *fp = fopen(archive_name, "r");
    if (!fp) {
        printf("archive not exist!\n");
        return -1;
    }
    if (fclose(fp)) {
        perror("Error in closing archive file.");
        return -1;
    }

    // Remove old footer in archive file
    if (remove_trailing_bytes(archive_name, 1024) == -1) {
        printf("Error happened in remove trailing bytes\n");
        return -1;
    }

    // Start to write in archive file
    FILE *afp = fopen(archive_name, "a");
    if (!afp) {
        perror("Archive file fopen error: ");
        return -1;
    }
    node_t *current = files->head;
    while (current) {
        // Generate a header
        tar_header header;
        if (fill_tar_header(&header, current->name) == 0) {
            if (fwrite(&header, BLOCK_SIZE, 1, afp) != 1) {
                perror("Header fwrite error: ");
                if (fclose(afp)) {
                    perror("Error in closing archive file.");
                }
                return -1;
            }
        } else {
            perror("Fill tar header error");
            if (fclose(afp)) {
                perror("Error in closing archive file.");
            }
            return -1;
        }
        FILE *cfp = fopen(current->name, "r");
        if (!cfp) {
            perror("Current file fopen error: ");
            if (fclose(afp)) {
                perror("Error in closing archive file.");
            }
            return -1;
        }

        // Obtain the file size
        int size = get_size(cfp);
        if (size < 0) {
            printf("Error happend in checking the size of current file.\n");
            if (fclose(cfp)) {
                perror("Error in closing current file.");
            }
            if (fclose(afp)) {
                perror("Error in closing archive file.");
            }
            return -1;
        }

        // Create a buffer that every byte is initially zero, ensure buffer size is multiple of 512
        int adjust_size = (size + BLOCK_SIZE - 1) / BLOCK_SIZE * BLOCK_SIZE;
        char *buffer = (char *) malloc(adjust_size);
        if (!buffer) {
            perror("Failed to malloc buffer");
            if (fclose(cfp)) {
                perror("Error in closing current file.");
            }
            if (fclose(afp)) {
                perror("Error in closing archive file.");
            }
            return -1;
        }
        memset(buffer, 0, adjust_size);

        // Read and write the current file
        fread(buffer, 1, size, cfp);
        if (ferror(cfp)) {
            perror("Current file fread Error:");
            free(buffer);
            if (fclose(cfp)) {
                perror("Error in closing current file.");
            }
            if (fclose(afp)) {
                perror("Error in closing archive file.");
            }
            return -1;
        }
        if (fwrite(buffer, 1, adjust_size, afp) != adjust_size) {
            perror("Current file fwrite Error:");
            free(buffer);
            if (fclose(cfp)) {
                perror("Error in closing current file.");
            }
            if (fclose(afp)) {
                perror("Error in closing archive file.");
            }
            return -1;
        }
        free(buffer);
        if (fclose(cfp)) {
            perror("Error in closing current file.");
        }
        current = current->next;
    }
    // Create the 2 blocks footer
    char footer[1024] = {0};
    if (fwrite(footer, BLOCK_SIZE, 2, afp) < 2) {
        perror("Footer fwrite error");
        if (fclose(afp)) {
            perror("Error in closing archive file.");
        }
        return -1;
    }
    if (fclose(afp)) {
        perror("Error in closing archive file.");
        return -1;
    }
    return 0;
}

int get_archive_file_list(const char *archive_name, file_list_t *files) {
    // If archive file not exist
    FILE *afp = fopen(archive_name, "r");
    if (!afp) {
        perror("Archive file fopen error: ");
        return -1;
    }

    //  Read the first file name
    char file_name[100] = {0};
    fread(file_name, 1, 100, afp);
    if (ferror(afp)) {
        perror("Achieve file fread name Error:");
        if (fclose(afp)) {
            perror("Error in closing archive file.");
        }
        return -1;
    }
    // Add each file name to the file list
    int offset = 0;
    while (!allZeros(file_name, 100)) {
        if (file_list_add(files, file_name) == 1) {
            printf("Fail to add file in file list\n");
            file_list_clear(files);
            if (fclose(afp)) {
                perror("Error in closing archive file.");
            }
            return -1;
        }

        // Get the size of current file
        char size[12];
        fseek(afp, 24, SEEK_CUR);
        fread(size, 1, 12, afp);
        if (ferror(afp)) {
            perror("Achieve file fread size Error:");
            if (fclose(afp)) {
                perror("Error in closing archive file.");
            }
            return -1;
        }
        int int_size;
        int result = sscanf(size, "%o", &int_size);
        if (result != 1) {
            printf("Failed to convert int size\n");
            file_list_clear(files);
            if (fclose(afp)) {
                perror("Error in closing archive file.");
            }
            return -1;
        }

        // Relocate the archive file pointer at the next header's name
        int block_num = (int_size + BLOCK_SIZE - 1) / BLOCK_SIZE + 1;
        offset += block_num * BLOCK_SIZE;
        fseek(afp, offset, SEEK_SET);

        fread(file_name, 1, 100, afp);
        if (ferror(afp)) {
            perror("Achieve file fread name Error:");
            if (fclose(afp)) {
                perror("Error in closing archive file.");
            }
            return -1;
        }
    }
    if (fclose(afp)) {
        perror("Error in closing archive file.");
        return -1;
    }
    return 0;
}

int extract_files_from_archive(const char *archive_name) {
    // Get archive file list
    file_list_t files;
    file_list_init(&files);
    if (get_archive_file_list(archive_name, &files) == -1) {
        return -1;
    }
    // Open the archive file
    FILE *afp = fopen(archive_name, "r");
    if (!afp) {
        perror("Archive file fopen error: ");
        return -1;
    }

    node_t *current = files.head;
    FILE *cfp;
    while (current) {
        cfp = fopen(current->name, "w");
        if (!cfp) {
            perror("Current file fopen error: ");
            file_list_clear(&files);
            if (fclose(afp)) {
                perror("Error in closing archive file.");
            }
            return -1;
        }

        // Get the size of current file
        char size[12];
        fseek(afp, 124, SEEK_CUR);
        fread(size, 1, 12, afp);
        if (ferror(afp)) {
            perror("Achieve file fread size Error:");
            if (fclose(afp)) {
                perror("Error in closing archive file.");
            }
            return -1;
        }
        int int_size;
        int result = sscanf(size, "%o", &int_size);
        if (result != 1) {
            printf("Failed to convert int size\n");
            file_list_clear(&files);
            if (fclose(afp)) {
                perror("Error in closing archive file.");
            }
            return -1;
        }
        // Assign the pointer after the header block
        fseek(afp, 512 - 136, SEEK_CUR);

        // Read and write the file into current working directory
        char *buffer = malloc(int_size);
        if (!buffer) {
            perror("Failed to malloc buffer");
            file_list_clear(&files);
            if (fclose(cfp)) {
                perror("Error in closing current file.");
            }
            if (fclose(afp)) {
                perror("Error in closing archive file.");
            }
            return -1;
        }
        int num_read = fread(buffer, 1, int_size, afp);
        if (fwrite(buffer, 1, num_read, cfp) != num_read) {
            perror("Current file fwrite Error:");
            file_list_clear(&files);
            free(buffer);
            if (fclose(cfp)) {
                perror("Error in closing current file.");
            }
            if (fclose(afp)) {
                perror("Error in closing archive file.");
            }
            return -1;
        }
        free(buffer);
        if (fclose(cfp)) {
            perror("Error in closing current file.");
            return -1;
        }

        // Point to the multiple of 512 if file's size isn't a multiple of 512
        int offset = (BLOCK_SIZE - int_size % BLOCK_SIZE) % BLOCK_SIZE;
        fseek(afp, offset, SEEK_CUR);
        current = current->next;
    }
    file_list_clear(&files);
    if (fclose(afp)) {
        perror("Error in closing archive file.");
        return -1;
    }
    return 0;
}
