#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#define BUILD_FOLDER "./build/"
#define TEMPLATES_FOLDER BUILD_FOLDER"templates/"

#define __AS_STR(...) #__VA_ARGS__
#define AS_STR(...) __AS_STR(__VA_ARGS__)

Cmd cmd = {0};


void usage(const char *program_name) {
  printf("Usage: %s [FLAGS] [run [-- [BUILD_EXE_ARGS]]]\n", program_name);
  printf("    run        ---        Run built executable\n");
  printf("Flags:\n");
  printf("    -B         ---        Force re-building\n");
  printf("    -g         ---        Include debug information\n");
}

char *zstr_replace_in_place(char *zstr, char needle, char replacement) {
  for (char *it = zstr; *it != 0; ++it) {
    if (*it == needle) *it = replacement;
  }
  return zstr;
}


bool build_template(const char *template_name, bool force_rebuild) {
  static String_Builder input = {0};
  static File_Paths children = {0};

  bool result = true;
  size_t save_point = temp_save();
  const char **input_paths = NULL;
  FILE *output = NULL;

  const char *template_input_folder = temp_sprintf("./templates/%s", template_name);
  const char *output_path = temp_sprintf(TEMPLATES_FOLDER"%s.c", template_name);

  if (!mkdir_if_not_exists(TEMPLATES_FOLDER)) return_defer(false);

  children.count = 0;
  if (!read_entire_dir(template_input_folder, &children)) return_defer(false);
  for (int i = children.count - 1; i >= 0; --i) {
    const char *base_name = children.items[i];
    if ((strcmp(base_name, ".") == 0) || (strcmp(base_name, "..") == 0)) {
      da_remove_unordered(&children, i);
    }
  }

  input_paths = malloc(sizeof(*input_paths)*children.count);
  if (!input_paths) {
    fprintf(stderr, "[ERROR] Failed to allocate memory while attempting to load the input paths for template %s", template_name);
    return_defer(false);
  }
  for (size_t i = 0; i < children.count; ++i) {
    const char *base_name = children.items[i];
    input_paths[i] = temp_sprintf("%s/%s", template_input_folder, base_name);
  }

  if (!force_rebuild && !needs_rebuild(output_path, input_paths, children.count)) {
    nob_log(INFO, "Template '%s' is up to date", template_name);
    return_defer(true);
  }

  output = fopen(output_path, "wb");
  if (!output) {
    nob_log(ERROR, "Failed to open output template file(%s): %s", output_path, strerror(errno));
    return_defer(false);
  }

  size_t loop_save_point = temp_save();
  da_foreach(const char*, it, &children) {
    temp_rewind(loop_save_point);

    String_View file_name_sv = sv_from_cstr(*it);
    const char *input_path = temp_sprintf("%s/%s", template_input_folder, *it);

    const char *file_name;
    const char *data_name;
    bool is_template;
    if (!sv_end_with(file_name_sv, ".template")) {
      is_template = false;
      file_name = *it;
      data_name = zstr_replace_in_place(temp_strdup(*it), '.', '_');
    } else {
      is_template = true;
      // strlen(".template") = 9
      int zstr_len = (int)file_name_sv.count - 9;
      if (zstr_len <= 0)  {
        nob_log(ERROR, "Template file must have a name excluding the '.template' extension: %s", *it);
        return_defer(false);
      }
      file_name = temp_strndup(*it, (size_t)zstr_len);
      data_name = zstr_replace_in_place((char*)temp_strdup(file_name), '.', '_');
    }

    input.count = 0;
    if (!read_entire_file(input_path, &input)) return_defer(false);

    fprintf(output, "const unsigned char templates_%s_%s_data[] = {", template_name, data_name);
    for (size_t i = 0; i < input.count; ++i) {
      if (i % 8 == 0) fputs("\n    ", output);
      else fputc(' ', output);
      fprintf(output, "0x%02x,", (unsigned char)input.items[i]);
    }
    fputs("\n};\n", output);
    
    fprintf(output, "const Template_File_Data templates_fdata_%s_%s = {\n", template_name, data_name);
    fprintf(output, "    .file_name = \"%s\",\n", file_name);
    fprintf(output, "    .is_template = %s,\n", is_template ? "true" : "false");
    fprintf(output, "    .data_length = %zu,\n", input.count);
    fprintf(output, "    .data = templates_%s_%s_data,\n", template_name, data_name);
    fputs("};\n\n", output);

    fflush(output);
  }

  fprintf(output, "const Template_Data template_data_%s = {\n", template_name);
  fprintf(output, "    .name = \"%s\",\n", template_name);
  fprintf(output, "    .files = {\n");
  da_foreach(const char*, it, &children) {
    temp_rewind(loop_save_point);

    const char *input_path = temp_sprintf("%s/%s", template_input_folder, *it);
    String_View file_name_sv = sv_from_cstr(*it);
    
    const char *data_name;
    bool is_template;
    if (!sv_end_with(file_name_sv, ".template")) {
      is_template = false;
      data_name = zstr_replace_in_place(temp_strdup(*it), '.', '_');
    } else {
      is_template = true;
      // strlen(".template") = 9
      int zstr_len = (int)file_name_sv.count - 9;
      data_name = zstr_replace_in_place((char*)temp_strndup(*it, zstr_len), '.', '_');
    }

    fprintf(output, "        &templates_fdata_%s_%s,\n", template_name, data_name);
  }
  fputs("        NULL,\n", output);
  fprintf(output, "    },\n");
  fputs("};\n", output);

  nob_log(INFO, "Updated template %s: '%s'", template_name, output_path);

defer:
  if (output) fclose(output);
  if (input_paths) free(input_paths);
  temp_rewind(save_point);
  return result;
}

bool build_templates(bool force_rebuild) {
  const char *templates[] = { "simple", "cmdl", NULL };
  const char *templates_file_path = BUILD_FOLDER"templates.c";
  size_t templates_count = ARRAY_LEN(templates)-1;

  for (const char **it = templates; *it != NULL; ++it) {
    if (!build_template(*it, force_rebuild)) return false;
  }

  FILE *f = fopen(templates_file_path, "w");
  if (!f) {
    nob_log(ERROR, "Failed to open %s: %s", templates_file_path, strerror(errno));
    return false;
  }

  fputc('\n', f);
  fputs("typedef struct {\n", f);
  fputs("  const char *file_name;\n", f);
  fputs("  const bool is_template;\n", f);
  fputs("  const size_t data_length;\n", f);
  fputs("  const unsigned char *data;\n", f);
  fputs("} Template_File_Data;\n\n", f);

  fputs("typedef struct {\n", f);
  fputs("  const char *name;\n", f);
  fputs("  const Template_File_Data *files[];\n", f);
  fputs("} Template_Data;\n\n", f);

  for (const char **it = templates; *it != NULL; ++it) {
    fprintf(f, "#include \"templates/%s.c\"\n", *it);
  }

  fprintf(f, "\nsize_t templates_count = %zu;\n", templates_count);
  fputs("const Template_Data *templates[] = {\n", f);
  for (const char **it = templates; *it != NULL; ++it) {
    fprintf(f, "    &template_data_%s,\n", *it);
  }
  fputs("};\n", f);

  fclose(f);

  return true;
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
