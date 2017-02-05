# CommandLineToArgvA
Collection of functions implement CommandLineToArgvA, the ANSI version of CommandLineToArgvW in windows API

Currently there's 4 versions:

- **CommandLineToArgvA_wine** [Recommaneded]

This one works perfectly, modified from [WINE Project](https://winehq.com), from the `CommandLineToArgvW` source code of `shell32.dll` file.

It's the best and  recommaneded if you don't care the code size.

- **CommandLineToArgvA_ola**

This one is from https://github.com/ola-ct/actilog/blob/master/actiwin/CommandLineToArgvA.cpp, and don't know where is the original source really...

It's not work when quoted args contain `\"` (quote escaping), it will join the next args into one.

- **CommandLineToArgvA_wheaty**

It's from Matt Peitrek's [LIBTINYC](http://www.wheaty.net/libctiny.zip), it have more code than **CommandLineToArgvA_ola**, and do almost the same thing, also have same problem when quoted args contain `\"` (quote escaping).

- **CommandLineToArgvA_simple**

This one is simple version, only works with no quotes in args.

## BUILD

First install [lcc](www.cs.virginia.edu/~lcc-win32/)

Then build and run the source from command line

``` batchfile
test.cmd
```


