# Gargoyle

## Simple Directory Watching

I came across the problem of wanting a C build system that I could use to hot-reload a WASM compiled binary in my browser. At the very same moment, I was also looking for a solution that would let me hot-reload *rendered* markdown in my terminal.

To solve this problem I decided to create Gargoyle, my simple directory watching and CLI automation solution.

## Compilation

On Linux and MacOS, run the following command in the terminal to compile Gargoyle.

Gargoyle is not implemented for Windows, but I'll look into it.

```
Make
```

## Usage

Gargoyle is defined as:

```
Usage: ggyl [-d directory] cmd [regex_patterns...]
```

### Arguments

- d: Specify a directory to monitor. If not provided, select the current working directory. Currently, all nested directories are watched, but I might change this to a flag like -r or something.

- cmd: String representation of the command you would like to execute on detected changes. 
    - Ex. `ggyl "clear & glow README.md"` will clear the CLI and then output README using Glow

- regex_patterns: Separate strings using glob regex format.
    - Ex. `ggyl "clear & glow README.md" "*.md" "*.c"` will execute the command when a markdown file or a C file are changed.
