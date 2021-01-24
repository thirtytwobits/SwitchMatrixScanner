# Unit Tests

You can ignore this. These are tests I run to ensure I'm not giving you boken code.

```bash
cd test
mkdir build
cd build
cmake ..
```

Note that the configure step will download googletest from github so you'll need an internet connection.

```bash
cmake --build . --target run_SwitchMatrixScannerTest
```
