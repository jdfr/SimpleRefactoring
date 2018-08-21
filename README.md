# OpenGrok & refactor #

So, let's say you have a very large codebase, and it can be searched with an OpenGrok instance. But you can't use OpenGrok's API to do queries. `grokscrap.py` to the rescue! This handy script has functionalty to maje it easy to scrape OpenGrok, reading the files that have some substring (and all the lines in each file with instances of the substring).

Moreover, let's say you have the task to remove dead simple configuration values that select between branches in conditional statements in C++. The configuration values are read from an XML file and checked in C++ with one or more ad-hoc boolean functions. `simpleRefactor.cpp` to the rescue! This is a small clang-based refactoring tool that will remove branches from if statements whose conditionals are calls to pre-defined functions whose first argument is a string literal: the name of the configuration value you want to remove! Moreover, `refactor.py` is a wrapper for `simpleRefactor.cpp` to apply the refactoring to lists of C++ files, as well as removing it from XML config files.

## Compiling ##

Python should be at least 2.7. The refactoring tool has been succesfully compiled with a clang 6.0 binary distribution (the one packaged in debian unstable) as of August 2018.

