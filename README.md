# Mini Unix‑Like Shell (C)

This project implements a small, **ANSI‑C** command‑line shell for Linux/Unix systems as part of the CSE 3033 *Operating Systems* course (Term Project 2, 2023). The shell is delivered in a single source file—`150120035_150120004_150121025.c`—with no external dependencies beyond the POSIX standard library.

---

## ✨ Feature Overview

| Category | Capabilities |
|----------|--------------|
| **Process control** | • Launch any binary found in `$PATH` or the current working directory  
• Background execution with trailing `&`  
• Proper orphan/​zombie reaping via `SIGCHLD` handler |
| **I/O redirection** | `<` **stdin**, `>` **truncate stdout**, `>>` **append stdout**, `2>` **stderr** |
| **Search utility** | Built‑in `search` command   `search <string> [path]`  
• Recursively scans a file **or** directory for occurrences of the given string |
| **Bookmarks** | Persistent in‑memory list of favourite commands  
• `bookmark -a <cmd…>`  add  
• `bookmark -l`  list  
• `bookmark -d <index>` delete  
• `bookmark <index>` execute |
| **Signals** | `Ctrl + Z` (SIGTSTP) support to suspend foreground job  |
| **Error logging** | Redirects shell’s STDERR to `stdError.txt` |

> *Pipes and command chaining (`|`, `;`, `&&`) are **not** implemented in this version.*

---

## 🏗 File Structure

```text
ImplementingOurShell/
├── 150120035_150120004_150121025.c   # full shell implementation
├── mainSetup.c                       # starter/utility code (parsing demo)
├── CSE3033_Project2_2023.pdf         # assignment specs
├── *.zip                             # original submission archives
└── README.md                         # you are here
```

---

## ⚙️  Compilation

A **GNU/Linux** environment with GCC is assumed.

```bash
# build
gcc -std=c11 -Wall -Wextra -o myshell 150120035_150120004_150121025.c

# run
./myshell
```

No external libraries are required; only `<stdio.h>`, `<unistd.h>`, `<signal.h>`, etc. are used.

---

## 🚀 Usage Cheat‑Sheet

| Example | What it does |
|---------|-------------|
| `ls -l` | run external command |
| `cat < in.txt` | redirect `stdin` from file |
| `ls > out.txt` | save output (overwrite) |
| `echo hello >> log.txt` | append to file |
| `gcc foo.c &` | compile in background |
| `search "todo" .` | search recursively for *todo* |
| `bookmark -a "grep main *.c"` | save favourite |
| `bookmark -l` | list saved commands |
| `bookmark 2` | execute bookmark #2 |

---

## 🧩 Design Notes

* **Command Parsing** — tokenises input with `strtok`, respecting redirection symbols; max line length is **80 bytes** (modifiable via `MAX_LINE`).
* **Process Table** — maintains a dynamic array of child PIDs; cleaned up in `sigchldHandler()`.
* **Redirection Handling** — duplicates file descriptors **before** `execvp` to isolate I/O to the child process.
* **Bookmarks** — stored as a dynamically resized `char **` array; indices begin at **1** for user friendliness.
* **Search Implementation** — directory traversal uses `opendir/readdir`; file scanning reads line‑by‑line and prints matches with line numbers.

---

## 🔬 Possible Extensions

* **Pipe support** (`|`) via `pipe()` + multiple forks
* **Command history** with arrow‑key navigation (readline)
* Saving bookmarks to disk (serialize on exit, load on start)
* Job control commands (`fg`, `bg`, `jobs`)

---

> *Happy hacking — may your shell never `fork()` bombs!*  
