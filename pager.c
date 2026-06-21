#include "pager.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

Pager *pager_open(const char *filename) {

  int fd = open(filename, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
  if (fd == -1) {
    printf("Unable to open file: %s\n", filename);
    exit(EXIT_FAILURE);
  }

  off_t file_length = lseek(fd, 0, SEEK_END);

  Pager *pager = malloc(sizeof(Pager));
  pager->file_descriptor = fd;
  pager->file_length = file_length;
  pager->num_pages = (file_length / PAGE_SIZE);

  if (file_length % PAGE_SIZE != 0) {
    printf("Db file is not a whole number of pages. Corrupt file.\n");

    exit(EXIT_FAILURE);
  }

  return pager;
}

void pager_read(Pager *pager, uint32_t page_num, void *dest) {
  if (page_num >= pager->num_pages) {

    memset(dest, 0, PAGE_SIZE);
    return;
  }

  off_t offset = lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
  if (offset == -1) {
    printf("Error seeking: %d\n", errno);
    exit(EXIT_FAILURE);
  }

  ssize_t bytes_read = read(pager->file_descriptor, dest, PAGE_SIZE);
  if (bytes_read == -1) {
    printf("Error reading file: %d\n", errno);
    exit(EXIT_FAILURE);
  }

  if (bytes_read < PAGE_SIZE) {
    memset((char *)dest + bytes_read, 0, PAGE_SIZE - bytes_read);
  }
}

void pager_write(Pager *pager, uint32_t page_num, const void *src) {
  off_t offset = lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
  if (offset == -1) {
    printf("Error seeking: %d\n", errno);
    exit(EXIT_FAILURE);
  }

  size_t bytes_remaining = PAGE_SIZE;
  const char *buf = src;

  while (bytes_remaining > 0) {
    ssize_t bytes_written = write(pager->file_descriptor, buf, bytes_remaining);

    if (bytes_written == -1) {
      printf("Error writing: %d\n", errno);
      exit(EXIT_FAILURE);
    }

    bytes_remaining -= bytes_written;
    buf += bytes_written;
  }

  if (page_num >= pager->num_pages) {
    pager->num_pages = page_num + 1;
    pager->file_length = pager->num_pages * PAGE_SIZE;
  }
}

void pager_close(Pager *pager) {
  if (close(pager->file_descriptor) == -1) {
    printf("Error closing db file.\n");
    exit(EXIT_FAILURE);
  }

  free(pager);
}

int pager_get_num_pages(Pager *pager) { return pager->num_pages; }
