# ![libab](https://cloud.githubusercontent.com/assets/379404/14627354/8b207cb0-05c1-11e6-869b-4d8d33e369ee.png)

[![Circle CI](https://circleci.com/gh/Preetam/libab.svg?style=svg&circle-token=2aa19d53d438447eae03021c0e99571e8ceb5207)](https://circleci.com/gh/Preetam/libab) [![License](https://img.shields.io/badge/License-BSD%203--Clause-blue.svg)](https://github.com/Preetam/libab/blob/master/LICENSE)

The [Background](https://github.com/Preetam/libab/blob/master/background.md) has details about this project.

## Dependencies and Building

Linux, OS X, and FreeBSD are supported at the moment.
Builds may succeed with other BSDs, but they have not been verified.

**All platforms**

- Submodules
  - `git submodule update --init --recursive`
- CMake 3.0 or higher
- A compiler that supports C++14

**Linux**

Continue to [Building](#building).

**OS X**

Continue to [Building](#building).

**FreeBSD**

- libexecinfo
  - `pkg install libexecinfo`

### Building

```sh
mkdir build && cd build
cmake ..
make
sudo make install # Optional
```

## License

BSD (see LICENSE)
