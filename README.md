### LC_Sandbox

LuisaCompute学习&实践的沙箱，包含多个彼此独立的小项目，使用xmake作为构建系统。

项目统一位于 `projects/` 目录下。默认只在 `projects/xmake.lua` 中包含 `Yutrel`；需要构建其它项目时，先在该文件中启用对应 `includes(...)`，再执行 `xmake build <target>`。

## Yutrel

基于LuisaCompute的离线渲染器，目前实现了MIS以及光谱渲染

Cornell Box 场景资源与测试结果图示例位于 [projects/Yutrel/scene/cornell-box](projects/Yutrel/scene/cornell-box) 目录下。

## MNIST

基于LuisaCompute实现的MNIST手写数字识别，包含训练和测试代码，正确率约98%。

MNIST 数据集应放在 `projects/MNIST/data/` 下，例如 `projects/MNIST/data/train-images.idx3-ubyte`。该目录属于本地数据目录，不提交到仓库。

## ShaderToy

基于LuisaCompute图形后端的实时着色实验项目。

## pathtracing

基于LuisaCompute的独立路径追踪实验项目，运行所需的光谱查找表位于 `projects/pathtracing/assets/`。
