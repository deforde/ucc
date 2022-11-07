# μ c compiler
My toy c compiler.

Based on the [book](https://www.sigbus.info/compilerbook) by Rui Ueyama.

The `ucc` compiler targets the x86\_64 architecture only.

```
              ┌───────────────┐   ┌──────────────┐   ┌─────────┐   ┌──────────┐   ┌───────────┐   ┌─────────┐
source code → │ preprocessing │ → │ tokenisation │ → │ AST gen │ → │ code gen │ → │ assembler │ → │ linking │ → binary artefact
              └───────────────┘   └──────────────┘   └─────────┘   └──────────┘   └───────────┘   └─────────┘
                                └───────────────────────┬───────────────────────┘
                                                        │
                                    ucc implements these stages of compilation
```

`ucc` uses `cpp` to pre-process the source code, and `as` in order to assemble the generated code.
The linking stage is not currently incorporated into `ucc`, linking must still be invoked externally (TODO).

The assembly produced by `ucc` is written in the Intel syntax.

`ucc` is self-hosting (i.e. it is capable of compiling itself) - with some slight cheating implemented by the `stage2.sh` script, which pre-pre-processes the `ucc` source code before it is passed to the stage 1 compiler for compilation (TODO).
The stage 2 build of `ucc` is capable of passing all the compiler tests contained in this repo.
