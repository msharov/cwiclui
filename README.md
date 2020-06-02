# CWICLUI

This library contains UI objects for the [cwiclo](https://github.com/msharov/cwiclo)
framework, including windows, common widgets and dialogs, and a terminal
renderer for them. Graphical renderer, based on OpenGL, will be implemented
in the [gleri](https://github.com/msharov/gleri) project, when it is converted
to use cwiclo.

Compilation requires cwiclo and C++17 support; use gcc 7 or clang 6.

```sh
./configure && make check && make install
```

Look at uitxt.cc in [test/](test/) for a usage example.
Additional documentation will be written when the project is more complete.

Report bugs on the [project bugtracker](https://github.com/msharov/cwiclui/issues).
