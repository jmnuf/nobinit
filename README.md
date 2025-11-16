# nobinit

C project initializer that uses the [nob.h](https://github.com/tsoding/nob.h) build system.

I've been building minor scripts and testing a few things and have preferred using nob within some of these and well doing wget every time I start a new one it is slightly annoying me.
I come from the web dev space of starting things by doing a simple command like `pnpm create vite` and I wanted something similar for my minor exploration projects and scripts.

## Build

To build the project you need libcurl installed in your libraries path and a c compiler (what a surprise).

First you initialize nob and then you call it as follows:
```shell
$ cc -o nob nob.c
$ ./nob run -- -h
```

## Usage

To use the program you must provide a name for the project's directory name.
If a name is provided it must be an unexisting folder.
```shell
$ nobinit my-project
$ cd my-project
$ ./nob
```

You got 2 options you can set when initializing a project.
- `c <compiler-name>`: Provide a specific c compiler to use for initializing the build system
- `-local`: Specify that we don't need to curl for the latest nob version


## Contributions

PRs with no issue to reference will be rejected. Start an issue for a change you want or see if it already exists. Reviewing of changes may take between days to months.

