// TODO: Do we really need curl? I think we can make do without it.
// Just used it for ease of getting this done sooner since I don't remember network code
#include <curl/curl.h>

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#include "templates.c"

#ifndef _WIN32
#include <pwd.h>
#endif

#define zstr_eq(a, b) (strcmp(a, b) == 0)

#define NOBINIT_DIRNAME ".nobinit"

typedef struct {
  const char *bootstrapper;
  const char *base_path;
  const char *name;
  bool run;
  bool local;
} Project_Setup;

Project_Setup setup = {0};

Cmd cmd = {0};

void usage(const char *program) {
  printf("Usage: %s [FLAGS] <name>\n", program);
  printf("    name                    ---        Name of the folder/project to initialize\n");
  printf("    FLAGS:\n");
  printf("        -c <compiler-name>  ---        Bootstrap nob with the specified compiler\n");
  printf("        -run                ---        Run nob after bootstrapping\n");
  printf("        -local              ---        Use locally cached nob.h version\n");
  printf("        -h                  ---        Print this help message\n");
}

bool sb_reserve(String_Builder *sb, size_t req_cap) {
  if (sb->capacity < req_cap) {
    size_t ncap = sb->capacity == 0 ? NOB_DA_INIT_CAP : sb->capacity;
    while (ncap < req_cap) ncap = ncap * 2;
    char *nptr = realloc(sb->items, ncap*sizeof(char));
    if (!nptr) return false;
    sb->items = nptr;
    sb->capacity = ncap;
  }
  return true;
}

#ifdef _WIN32
const char *win32_get_env(const char *name) {
  char c = 0;
  size_t n = GetEnvironmentVariable(name, &c, 0);
  if (n == 0) return NULL;
  char *buf = (char*)malloc(n);
  if (!buf) return NULL;
  GetEnvironmentVariable(name, buf, n);
  return buf;
}
#endif

const char *get_home_path() {
  const char *home_path = NULL;
  #ifdef _WIN32
  const char *wdrive = win32_get_env("HOMEDRIVE");
  const char *wpath = win32_get_env("HOMEPATH");
  if (wdrive && wpath) {
    home_path = temp_sprintf("%s%s", wdrive, wpath);
  }
  if (wdrive) free(wdrive);
  if (wpath) free(wpath);
  #else
  home_path = getenv("HOME");
  if (home_path == NULL) {
    struct passwd *pwd = getpwuid(getuid());
    if (pwd) home_path = pwd->pw_dir;
  }
  if (home_path) home_path = temp_strdup(home_path);
  #endif // WIN32

  if (!home_path) {
    nob_log(ERROR, "Failed to get home directory");
  }

  return home_path;
}

bool sv_includes(const String_View sv, const char *substr) {
  size_t substr_len = strlen(substr);
  if (substr_len > sv.count) return false;

  for (size_t i = 0; i <= sv.count - substr_len; ++i) {
    bool found = true;
    for (size_t j = 0; j < substr_len; ++j) {
      char a = sv.data[i+j];
      char b = substr[j];
      if (a != b) {
        found = false;
        break;
      }
    }
    if (found) return true;
  }

  return false;
}

static size_t _curl_mem_callback(void *contents, size_t size, size_t nmemb, void *pUser) {
  String_Builder *sb = (String_Builder*)pUser;
  size_t real_size = size * nmemb;

  if (!sb_reserve(sb, sb->count + real_size + 1)) return 0;

  memcpy(sb->items + sb->count, contents, real_size);
  sb->count += real_size;
  sb->items[sb->count] = 0;

  return real_size;
}

bool fetch_latest_nob_h() {
  const char *base_path = setup.base_path;
  bool result = true;
  size_t save_point = temp_save();
  String_Builder sb = {0};

  curl_global_init(CURL_GLOBAL_ALL);
  CURL *handle;

  const char *home_path = get_home_path();
  const char *nobinitdir = temp_sprintf("%s/"NOBINIT_DIRNAME, home_path);
  if (!mkdir_if_not_exists(nobinitdir)) return_defer(false);

  handle = curl_easy_init();
  if (!handle) return_defer(false);

  curl_easy_setopt(handle, CURLOPT_URL, "https://raw.githubusercontent.com/tsoding/nob.h/refs/heads/main/nob.h");
  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, _curl_mem_callback);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, &sb);

  nob_log(INFO, "curl <raw.github/tsoding/nob.h>");
  CURLcode ret = curl_easy_perform(handle);
  if (ret != CURLE_OK) {
    nob_log(NOB_ERROR, "Curl failed: %s\n", curl_easy_strerror(ret));
    return_defer(false);
  }

  write_entire_file(temp_sprintf("%s/nob.h", nobinitdir), sb.items, sb.count);

  const char *file_path = temp_sprintf("%s/nob.h", base_path);
  if (!write_entire_file(file_path, sb.items, sb.count)) return_defer(false);
  nob_log(NOB_INFO, "Saved nob.h");

defer:
  if (handle) curl_easy_cleanup(handle);
  curl_global_cleanup();

  if (sb.items) free(sb.items);
  temp_rewind(save_point);
  return result;
}

bool fetch_cached_nob_h() {
  const char *base_path = setup.base_path;
  bool result = true;
  size_t save_point = temp_save();
  String_Builder sb = {0};

  const char *home = get_home_path();
  if (home == NULL) return_defer(false);

  const char *nobinitdir = temp_sprintf("%s/"NOBINIT_DIRNAME, home);
  if (!mkdir_if_not_exists(nobinitdir)) return_defer(false);

  const char *cached_nob_path = temp_sprintf("%s/nob.h", nobinitdir);
  if (!file_exists(cached_nob_path)) {
    nob_log(ERROR, "No cached nob.h exists");
    return_defer(false);
  }
  if (!read_entire_file(cached_nob_path, &sb)) return_defer(false);

  const char *target_path = temp_sprintf("%s/nob.h", base_path);
  if (!write_entire_file(target_path, sb.items, sb.count)) return_defer(false);
  nob_log(NOB_INFO, "Created %s/nob.h\n", base_path);

defer:
  if (sb.items) free(sb.items);

  temp_rewind(save_point);

  return result;
}

bool create_directory(const char *path) {
  #ifdef _WIN32
  int result = _mkdir(path);
  #else
  int result = mkdir(path, 0755);
  #endif
  if (result < 0) {
    nob_log(ERROR, "Failed to create directory `%s`: %s", path, strerror(errno));
    return false;
  }
  return true;
}

bool create_nob_c() {
  const char *base_path = setup.base_path;
  const char *project_name = setup.name;
  bool result = true;
  size_t save_point = temp_save();
  const char *file_path = temp_sprintf("%s/nob.c", base_path);
  FILE *f = fopen(file_path, "w");
  if (!f) return_defer(false);

  const char *project_name_template = "{{ project_name }}";
  size_t project_name_template_len = strlen(project_name_template);
  for (size_t i = 0; i < template_nob_c_length; ++i) {
    const unsigned char c = template_nob_c[i];
    if (c != '{') {
      fputc(c, f);
      continue;
    }
    if (strncmp(template_nob_c + i, project_name_template, project_name_template_len) != 0) {
      fputc(c, f);
      continue;
    }

    if (fputs(project_name, f) < 0) return_defer(false);
    i += project_name_template_len - 1;
  }
  nob_log(INFO, "Created %s/nob.c", base_path);

defer:
  if (f) fclose(f);
  temp_rewind(save_point);
  return result;
}

bool create_main_c() {
  const char *base_path = setup.base_path;

  bool result = true;
  size_t save_point = temp_save();
  const char *file_path = temp_sprintf("%s/main.c", base_path);

  if (!write_entire_file(file_path, template_main_c, template_main_c_length)) return_defer(false);
  nob_log(INFO, "Created %s/main.c", base_path);

defer:
  temp_rewind(save_point);
  return result;
}

bool create_nob_h() {
  if (!setup.local) {
    if (!fetch_latest_nob_h()) return false;
  }
  if (!fetch_cached_nob_h()) return false;
  return true;
}

int main(int argc, char **argv) {
  const char *program_name = shift(argv, argc);
  String_View name_sv = {0};

  while (argc > 0) {
    const char *arg = shift(argv, argc);

    if (zstr_eq(arg, "-h") || zstr_eq(arg, "--help")) {
      usage(program_name);
      return 0;
    }

    if (zstr_eq(arg, "-c")) {
      if (argc == 0) {
        nob_log(ERROR, "Missing to provide a compiler name for bootstrapping after '-c' flag");
        usage(program_name);
        return 1;
      }

      const char *compiler_name = shift(argv, argc);
      if (zstr_eq(compiler_name, "msvc")) {
        setup.bootstrapper = "cl.exe";
      } else {
        setup.bootstrapper = compiler_name;
      }
      continue;
    }

    if (zstr_eq(arg, "-local")) {
      setup.local = true;
      continue;
    }

    if (zstr_eq(arg, "-run")) {
      setup.run = true;
      continue;
    }

    if (arg[0] == '-') {
      nob_log(ERROR, "Unknown flag provided: %s", arg);
      usage(program_name);
      return 1;
    }

    if (name_sv.data == NULL) {
      name_sv = sv_from_cstr(arg);
      setup.name = arg;
      continue;
    }

    nob_log(ERROR, "Unknown argument provided for project (%s): %s", setup.name, arg);
    usage(program_name);
    return 1;
  }

  if (name_sv.data == NULL) {
    nob_log(ERROR, "No project name was provided!");
    usage(program_name);
    return 1;
  }

  while (sv_end_with(name_sv, "/") || sv_end_with(name_sv, "\\")) {
    name_sv.count--;
  }

  if (name_sv.count == 0) {
    nob_log(ERROR, "Invalid project name provided");
    usage(program_name);
    return 1;
  }

  if (sv_eq(name_sv, sv_from_cstr("."))) {
    const char *cwd = get_current_dir_temp();
    if (cwd == NULL) return 1;
    setup.base_path = ".";
    setup.name = nob_path_name(cwd);
  } else {
    const char *path = temp_sv_to_cstr(name_sv);
    if (!create_directory(path)) return 1;
    nob_log(INFO, "Created directory: %s", path);
    setup.base_path = path;
    if (sv_includes(name_sv, "/") || sv_includes(name_sv, "\\")) {
      setup.name = nob_path_name(path);
    } else {
      setup.name = path;
    }
  }

  if (setup.bootstrapper) {
    cmd_append(&cmd, setup.bootstrapper);
  } else {
    nob_cc(&cmd);
  }
  setup.bootstrapper = cmd.items[0];

  printf("==================================================\n");
  printf("ProjectSetup:\n");
  printf("    name: %s\n", setup.name);
  printf("    base_path: %s\n", setup.base_path);
  printf("    compiler: %s\n", setup.bootstrapper);
  printf("    local_nob: %s\n", setup.local ? "true" : "false");
  printf("--------------------------------------------------\n");

  if (!create_nob_c()) return 1;

  if (!create_main_c()) return 1;

  if (!create_nob_h()) return 1;

  return 0;
}

