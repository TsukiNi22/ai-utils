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
