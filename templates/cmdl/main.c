#include "nob.h"

const char *program_name;

void print_usage() {
  printf("Usage: %s [FLAGS]\n", program_name);
  printf("FLAGS:\n", program_name);
  printf("    -h,--help        ----    Print this help message\n", program_name);
}

int main(int argc, char **argv) {
  program_name = shift(argv, argc);

  while (argc) {
    const char *arg = shift(argv, argc);

    if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
      print_usage();
      return 0;
    }

    nob_log(ERROR, "Unknown argument: %s", arg);
    print_usage();
    return 1;
  }

  return 0;
}

#define NOB_IMPLEMENTATION
#include "nob.h"

