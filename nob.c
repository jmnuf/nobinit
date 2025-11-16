#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#define BUILD_FOLDER "./build/"
#define TEMPLATES_FOLDER BUILD_FOLDER"templates/"

Cmd cmd = {0};


void usage(const char *program_name) {
  printf("Usage: %s [FLAGS] [run [-- [BUILD_EXE_ARGS]]]\n", program_name);
  printf("    run        ---        Run built executable\n");
  printf("Flags:\n");
  printf("    -B         ---        Force re-building\n");
  printf("    -g         ---        Include debug information\n");
}


bool build_templates(bool force_rebuild) {
  bool result = true;
  String_Builder sb = {0};
  String_Builder input = {0};

  if (!mkdir_if_not_exists(TEMPLATES_FOLDER)) return_defer(false);

  if (force_rebuild || needs_rebuild1(TEMPLATES_FOLDER"nob_c.c", "./nob.c.template")) {
    sb.count = 0;
    input.count = 0;

    if (!read_entire_file("./nob.c.template", &input)) return_defer(false);

    sb_append_cstr(&sb, "const char template_nob_c[] = {");
    for (size_t i = 0; i < input.count; ++i) {
      if (i % 8 == 0) sb_append_cstr(&sb, "\n    ");
      else da_append(&sb, ' ');
      sb_appendf(&sb, "0x%02x,", input.items[i]);
    }
    sb_append_cstr(&sb, "\n};\n");
    sb_appendf(&sb, "size_t template_nob_c_length = %zu;\n", input.count);

    if (!write_entire_file(TEMPLATES_FOLDER"nob_c.c", sb.items, sb.count)) return_defer(false);
  }
  nob_log(INFO, "Template nob_c is up to date");

  if (force_rebuild || needs_rebuild1(TEMPLATES_FOLDER"main_c.c", "./main.c.template")) {
    sb.count = 0;
    input.count = 0;

    if (!read_entire_file("./main.c.template", &input)) return_defer(false);

    sb_append_cstr(&sb, "const char template_main_c[] = {");
    for (size_t i = 0; i < input.count; ++i) {
      if (i % 8 == 0) sb_append_cstr(&sb, "\n    ");
      else da_append(&sb, ' ');
      sb_appendf(&sb, "0x%02x,", input.items[i]);
    }
    sb_append_cstr(&sb, "\n};\n");
    sb_appendf(&sb, "size_t template_main_c_length = %zu;\n", input.count);

    if (!write_entire_file(TEMPLATES_FOLDER"main_c.c", sb.items, sb.count)) return_defer(false);
  }
  nob_log(INFO, "Template main_c is up to date");

  const char *input_paths[] = {
    TEMPLATES_FOLDER"nob_c.c",
    TEMPLATES_FOLDER"main_c.c",
  };
  size_t input_paths_count = ARRAY_LEN(input_paths);
  if (force_rebuild || needs_rebuild(BUILD_FOLDER"templates.c", input_paths, input_paths_count)) {
    sb.count = 0;

    for (size_t i = 0; i < input_paths_count; ++i) {
      const char *template_name = input_paths[i] + strlen(TEMPLATES_FOLDER);
      sb_appendf(&sb, "#include \"templates/%s\"\n", template_name);
    }

    if (!write_entire_file(BUILD_FOLDER"templates.c", sb.items, sb.count)) return_defer(false);
  }

  nob_log(INFO, "Unit %s is up to date", BUILD_FOLDER"templates.c");

defer:
  if (input.items) free(input.items);
  if (sb.items) free(sb.items);
  return result;
}


int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  const char *program_name = shift(argv, argc);
  bool should_run = false;
  bool force_rebuild = false;
  bool with_debug_info = false;
  while (argc) {
    const char *arg = shift(argv, argc);

    if (should_run && strcmp(arg, "--") == 0) {
      break;
    }

    if (strcmp(arg, "-B") == 0) {
      force_rebuild = true;
      continue;
    }

    if (strcmp(arg, "-g") == 0) {
      with_debug_info = true;
      continue;
    }

    if (strcmp(arg, "run") == 0) {
      should_run = true;
      continue;
    }
  }

  if (!mkdir_if_not_exists(BUILD_FOLDER)) return 1;

  const char *output_path;

  if (!build_templates(force_rebuild)) return 1;

  output_path = BUILD_FOLDER"nobinit";
  if (force_rebuild || needs_rebuild1(output_path, "main.c")) {
    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    nob_cc_output(&cmd, output_path);
    nob_cc_inputs(&cmd, "main.c");

    nob_cc_inputs(&cmd, "-I./build");

    if (with_debug_info) {
      cmd_append(&cmd, "-ggdb");
      cmd_append(&cmd, "-fsanitize=address,undefined");
    }

    cmd_append(&cmd, "-lcurl");
    
    if (!cmd_run(&cmd)) return 1;
  }
  nob_log(INFO, "Unit %s is up to date", output_path);

  if (should_run) {
    cmd_append(&cmd, BUILD_FOLDER"nobinit");
    for (size_t i = 0; i < argc; ++i) {
      cmd_append(&cmd, argv[i]);
    }

    if (!cmd_run(&cmd)) return 1;
  }

  return 0;
}
