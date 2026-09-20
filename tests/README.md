# tests

This directory contains the Flint test runner. It is written entirely in Flint, and this test runner executes a lot of different end-to-end tests for all examples, all wiki examples, a lot of regression tests and more.

To compile and run the test suite, just execute

```sh
flintc tests.ft --test --run
```

All tests should be green and should pass. This test suite is executed before any commit or change to the compiler, to make sure changes or additions did not break anything in the process. The regression test suite is updated for every bugfix, regression fix and feature addition.
