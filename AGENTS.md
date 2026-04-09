# LC_Sandbox 协作说明

## 文档约定

- 仓库内新增说明文档、注释说明和协作记录默认使用中文。
- 如需保留英文术语，优先采用“中文说明 + 英文术语”形式，避免只写英文缩写而不解释。

## 项目概览

- 本仓库是基于 LuisaCompute 的学习与实验沙箱，构建系统为 `xmake`。
- 默认主程序是 `Yutrel`，定位为离线渲染器，当前仓库内可见 Cornell Box 场景相关代码与资源。
- `src/test` 下包含独立实验目标，正常开发过程中忽略，不需要构建或运行
- `ext/`目录下为依赖仓库，任何情况下都不要修改

## 目录结构

- `src/Yutrel`：主渲染器代码。
- `src/Yutrel/base`：相机、场景、积分器、材质/光源抽象等核心基础设施。
- `src/Yutrel/cameras`、`lights`、`shapes`、`surfaces`、`textures`：具体实现。
- `src/Yutrel/utils`：图像读写、采样、坐标系、随机数、光谱工具等。
- `src/test`：独立的LC特性/功能测试，可忽略。
- `scene/cornell-box`：Cornell Box 场景资源与参考结果图。
- `ext/LuisaCompute`：Git 子模块，主依赖源码位于此处。

## 构建说明

- 首次拉取后需要初始化子模块：`git submodule update --init --recursive`
- 常用配置命令：
  - `xmake f -m release`
- 常用构建命令：
  - `xmake`

## 运行说明

- `Yutrel` 需要显式传入后端参数，例如：
  - `xmake run Yutrel <backend>`
    - cuda / dx
  - `xmake run Yutrel <backend> -i`
    - i代表可与键鼠交互

## 依赖与环境

- 顶层 `xmake.lua` 依赖 `tinyexr` 与 `assimp`。
- 项目包含 `ext/LuisaCompute`，构建时会一并纳入。
- `src/xmake.lua` 中额外补充了 CUDA `cudadevrt` 嵌入逻辑：
  - 仅在 Windows / Linux 下尝试启用。
  - 依赖环境变量 `CUDA_PATH`。
  - 若未设置 `CUDA_PATH` 或对应库不存在，构建不会中断，但会保留警告，CUDA indirect dispatch 不可用。

## 开发注意事项

- `build/`、`.xmake/`、`vsxmake2026/` 等属于构建产物或缓存目录，通常不需要手动编辑。
- `data/` 已被 `.gitignore` 忽略，属于用户本地数据目录，不要假设其中内容可提交。

## 建议的工作方式

- 修改渲染主流程时，优先从 `src/Yutrel/main.cpp`、`src/Yutrel/base/application.*`、`src/Yutrel/base/renderer.*` 开始追踪。
- 修改场景/材质/光谱相关逻辑时，优先检查 `base` 目录抽象层，再看各子目录具体实现。

## AGENT规范

### 任务定义
- task name: 任务名
- 一个新任务的相关文件，位于`.agent/[task name]`文件夹中

### 任务进行规范
- 若不存在，则创建agent/[task name]分支，并checkout
  - 需要保证task开发在这个分支中
- `.agent/[task name]`文件夹
  - 当前任务中创建的临时文件都保存其中
  - 不要自动commit这个文件夹

### 相关文档
文档不一定必须生成。当且仅当明确指定需要某文档时，才生成。

### Research.md
- 记录对一个任务进行的前期调研结果
- 包含与任务相关的文件路径、程序架构、注意事项等

### Plan.md
- 记录任务的计划执行步骤

### Progress.md
- 记录一次任务执行的过程以及问题，作为经验优化下一次任务