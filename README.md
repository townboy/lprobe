# lprobe
lprobe利用systemtap将程序执行时的路径输出，并使用graphviz进行可视化输出，可以辅助缺陷定位，代码阅读等。

### 依赖
* [graphviz](https://www.graphviz.org/about/)

### 编译
```
$ mkdir build && cd build
$ cmake ..
$ make
```

### 使用示范
追踪leveldb的一个简单demo代码执行流程

1. 编写demo
```
townboy@Arch ~/test/leveldb % cat main.cc
#include <cassert>
#include <leveldb/db.h>
#include <string>
#include <iostream>

int main() {
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;
    auto s = leveldb::DB::Open(options, "testdb", &db);
    assert(s.ok());

    std::string value("world");
    s = db->Put(leveldb::WriteOptions(), "hello", "world");
    if (s.ok()) {
        std::string getstring;
        s = db->Get(leveldb::ReadOptions(), "hello", &getstring);
        assert(s.ok());
        std::cout << "key = hello " << getstring << "\n";
    }
    return 0;
};
```

2. 设定要追踪的代码部分
```
$ cat conf
process("/usr/lib/libleveldb.so").function("*@*.cc")
```

3. 输出stp文件 
```
$ lprobe-stpmaker < conf > leveldb.stp
```

4. 使用systemtap 捕获输出
```
$ sudo stap -v 0.stp --ldd | c++filt > leveldb.data
```

5. 使用lprobe-dotmaker -d模式 处理导出dot文件, 并有graphviz生成图片文件
```
$ lprobe-dotmaker -da < leveldb.data > leveldb.dot
$ dot -T png < leveldb.dot > leveldb.png
```
![leveldb callgraph](/examples/leveldb/leveldb.png)

6. 使用lprobe-dotmaker -t模式 输出文本模式
```
$ lprobe-dotmaker -ta < leveldb.data > leveldb.call
```
