
## 特别致谢
https://github.com/udochina/KPM-Build-Anywhere/
项目提供了用ndk构建的思路
- 本项目在此项目上使用cmake，更便捷的包管理工具
- 自动拉取kernelpatch源码，自动生成相关模板
- 进一步支持ide的智能提示，智能索引，让开发者动一下手指就能完成内核模块编译
- 一般用户可以直接完成构建，如果出现构建失败请看排查指南，提出issues，作者会不断完善
- 此项目是为了https://github.com/jiqiu2022/Zygisk-MyInjector的内核隐藏做的一个便捷脚手架

> 注意：从别的项目反馈来看 貌似不同的kpm版本加载有符号问题，请注意好手机的Apatch是否和当前项目自动拉取源码的版本是否相同，本项目默认拉取最新的源码来构建


## 简单的使用方法
### 1. 构建所有模块

```bash
# macOS / Linux
./build.sh

# Windows
build.bat

# 输出文件位于: build/<模块名>/<模块名>.kpm
```

### 2. 构建特定模块

```bash
# macOS / Linux
./build.sh
cmake --build build --target hello

# Windows
build.bat
cmake --build build --target hello
```

### 3. 创建新模块

```bash
# macOS / Linux
./new-module.sh my-module "Your Name" "My awesome module"

# Windows
new-module.bat my-module "Your Name" "My awesome module"
```

## 📁 项目结构

```
KPM-Build-Anywhere/
├── modules/                 # 模块目录
│   ├── hello/              # 示例模块
│   │   ├── module.c        # 模块源码
│   │   └── module.lds      # 链接脚本(可选)
│   ├── template/           # 模块模板
│   │   ├── module.c
│   │   └── module.lds
│   └── your-module/        # 你的模块
│       └── module.c
├── build/                   # 构建输出（自动生成）
│   ├── hello/
│   │   └── hello.kpm       # 编译后的模块
│   └── your-module/
│       └── your-module.kpm
├── third_party/            # 第三方依赖（自动下载）
│   └── KernelPatch/        # KernelPatch 源码
├── build.sh                # 构建脚本 (macOS/Linux)
├── build.bat               # 构建脚本 (Windows)
├── new-module.sh           # 创建模块脚本 (macOS/Linux)
├── new-module.bat          # 创建模块脚本 (Windows)
├── env.example             # 环境配置示例
└── CMakeLists.txt          # CMake 配置
```

## 🔨 模块开发

### 方法一：使用脚本创建（推荐）

```bash
# 创建新模块
./new-module.sh my-awesome-module "Your Name" "Description"

# 编辑模块代码
vim modules/my-awesome-module/module.c

# 构建
./build.sh
```

### 方法二：手动创建

1. **复制模板**
   ```bash
   cp -r modules/template modules/my-module
   ```

2. **编辑源码**
   - 修改 `modules/my-module/module.c`
   - 更改模块名、作者、描述等信息
   - 实现你的功能

3. **构建**
   ```bash
   ./build.sh
   ```

4. **输出**
   - 编译后的文件：`build/my-module/my-module.kpm`

### 模块源码要点

```c
// 1. 模块名（必须唯一）
KPM_NAME("kpm-your-module");

// 2. 版本信息
KPM_VERSION("1.0.0");

// 3. 许可证
KPM_LICENSE("GPL v2");

// 4. 作者
KPM_AUTHOR("Your Name");

// 5. 描述
KPM_DESCRIPTION("Your module description");

// 6. 回调函数
static long your_init(const char *args, const char *event, void *__user reserved) {
    pr_info("Module loaded\n");
    return 0;
}

static long your_exit(void *__user reserved) {
    pr_info("Module unloaded\n");
    return 0;
}

// 7. 注册回调
KPM_INIT(your_init);
KPM_EXIT(your_exit);
```

## 🔧 自定义配置

### 使用 .env 文件

```bash
# 复制配置模板
cp env.example .env

# 编辑配置
vim .env
```

配置示例：
```bash
# NDK 路径（可选，不设置则自动检测）
NDK_PATH=/Users/yourname/Library/Android/sdk/ndk/23.1.7779620

# KernelPatch 源码（可选，不设置则自动下载）
KP_DIR=/path/to/KernelPatch

# 并行编译任务数
BUILD_JOBS=4
```

## 📋 构建命令参考

### 基础命令

```bash
# 构建所有模块
./build.sh

# 清理构建
./build.sh clean

# 详细输出
./build.sh --verbose

# 指定并行任务数
./build.sh -j8
```

### CMake 命令

```bash
# 配置项目
cmake -B build

# 构建所有模块
cmake --build build

# 构建特定模块
cmake --build build --target hello

# 清理
rm -rf build

# 详细构建日志
cmake --build build --verbose

# 并行构建
cmake --build build -j8
```

## 🐛 故障排除

### 问题 1: "No module found"

**原因：** `modules/` 目录下没有有效模块

**解决：**
```bash
# 检查模块目录
ls -la modules/

# 创建新模块
./new-module.sh my-module
```

### 问题 2: NDK 未找到

**解决：**
```bash
# 方法 1: 设置环境变量
export NDK_PATH=/path/to/your/ndk
./build.sh

# 方法 2: 使用 .env 文件
echo "NDK_PATH=/path/to/your/ndk" > .env
./build.sh
```

### 问题 3: 模块构建失败

**检查清单：**
1. 确保 `module.c` 存在
2. 检查语法错误
3. 查看构建日志：`./build.sh --verbose`
4. 确认 KPM API 使用正确

### 问题 4: CLion 中找不到构建目标

**解决：**
1. Tools → CMake → Reload CMake Project
2. 检查 CMake 输出，确认模块被识别
3. 重启 CLion

## 🌟 示例：创建一个简单的模块

```bash
# 1. 创建模块
./new-module.sh hello-world "John Doe" "A simple hello world module"

# 2. 编辑代码
# modules/hello-world/module.c 已经基于模板创建好了

# 3. 构建
./build.sh

# 4. 或者只构建这个模块
cmake --build build --target hello-world

# 5. 输出文件
ls -lh build/hello-world/hello-world.kpm
```

## 📚 API 参考

### 必需的宏

- `KPM_NAME(name)` - 模块名称（必须唯一）
- `KPM_VERSION(version)` - 版本号
- `KPM_LICENSE(license)` - 许可证
- `KPM_AUTHOR(author)` - 作者
- `KPM_DESCRIPTION(desc)` - 描述

### 回调函数

- `KPM_INIT(func)` - 模块加载时调用
- `KPM_EXIT(func)` - 模块卸载时调用
- `KPM_CTL0(func)` - 控制函数 0
- `KPM_CTL1(func)` - 控制函数 1

### 常用函数

- `pr_info()` - 打印信息
- `pr_err()` - 打印错误
- `compat_copy_to_user()` - 复制数据到用户空间
- `compat_copy_from_user()` - 从用户空间复制数据
-  更多的请参考KernelPatch docs
## 📄 许可证

GPL v2 or later

## 🙏 致谢
- [KPM-Build-Anywhere](https://github.com/udochina/KPM-Build-Anywhere/) - 灵感的来源
- [KernelPatch](https://github.com/bmax121/KernelPatch) - 强大的内核补丁框架
