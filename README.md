# OpenGrok & refactor #

So, let's say you have a very large codebase, and it can be searched with an OpenGrok instance. But you can't use OpenGrok's API to do queries. `grokscrap.py` to the rescue! This handy script has functionalty to make it easy to scrape OpenGrok, reading the files that have some substring (and all the lines in each file with instances of the substring).

Moreover, let's say you have the task to remove dead simple configuration values that select between branches in conditional statements in C++, and help decide the control flow in other various ways. The configuration values are read from an XML file and checked in C++ with one or more ad-hoc boolean functions. `simpleRefactor.cpp` to the rescue! This is a small clang-based refactoring tool that will remove branches from if statements (and conditional operators) whose conditionals are calls to pre-defined functions whose first argument is a string literal: the name of the configuration value you want to remove! It can also perform simple refactorings of boolean expressions with config values. Perhaps, the most lacking feature in the current state of the tool is the removal of boolean variables and functions whose assignments / return expressions are cheap to compute and side-effect free.

`refactor.py` is a wrapper for the binary built from `simpleRefactor.cpp` to apply the refactoring to lists of C++ files, as well as removing it from XML config files. Still trivial but more involved refactorings can be implemented on top of this example. Config values appearing in header files require a somewhat more involved handling (basically detecting a cpp file that includes them).

## Using and Compiling ##

Python should be at least 2.7. The refactoring tool has been succesfully compiled with a clang 6.0 binary distribution (the one packaged in debian unstable) as of August 2018, will probably work for previous ones having the AST Matcher library. This repo is not intended as a finished, ready-to-use refactoring tool, but as a base to be adapted to each specific use case.

The build system includes commands to run/accept some "regression" tests, see CMakeLists.txt for details.

