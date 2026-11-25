# <name>

> **X**xx **Y**yy **Z**zz – <one line of what it does>

## Installation

#### Cloning the repository
```bash
git clone https://github.com/<owner>/<name>
cd <name>
```

#### Build & Installation
```bash
export BUILD_DIR=build
cmake -S . -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Optimized
sudo cmake --build $BUILD_DIR --target install --parallel $(nproc)
```

#### Include
> [!WARNING]
> Everything is defined within the namespace `<ns>::`

| Include                  | Content                                   |
| ------------------------ | ----------------------------------------- |
| -l<name>                 | `Nothing for now`                         |
| <<name>/<name>.hpp>      | `<return> <function>(<parameters>)`       |

## Description
<1-3 short sentences: the idea, the main limit.>

Algorithm, parameters and complexity: [`readme/algorithm.md`](readme/algorithm.md).

## License
[MIT](https://github.com/<owner>/<name>/blob/main/LICENSE)
