# RK3588 Debug 部署设计

## 目的

把已合并的最新 `main@1bf657ff730891ae0bea48340a046cbcbeaa87d4`
部署到 `192.168.1.103` 的 NEARDI LKD3588。该部署用于交互式 Debug、
结构化遥测、CSV/metadata/视频留存和板端问题定位，不作为生产交付或吊舱验收。

## 范围与约束

- 在 RK3588 用户目录下创建独立、带源码提交身份的部署目录；不覆盖板上现有文件。
- 从当前 `main` 生成可核验源码包，在 RK3588 上原生编译，启用
  `BUILD_TESTING=ON` 和 `ENABLE_RKNN_RUNTIME=ON`。
- 使用 RK3588 自带 OpenCV 4.5.4 和 `librknnrt.so`；从已验证 RKNN SDK
  取得编译头文件，但不盲目覆盖系统运行库。
- 先测试现有 RK3566-target RKNN 模型能否由 RK3588 runtime 加载。若不兼容，
  用相同 ONNX、相同预处理和 Toolkit 2.3.2 重新生成 RK3588-target 模型，分别记录哈希；
  不把重新转换后的模型描述成与 RK3566 二进制同一工件。
- detector 使用现有 VisDrone9 候选，只用于 Debug 候选生成；它不得直接授权控制，
  也不改变当前 promotion held 状态。
- 新的算法运行证据只使用 `data/occ_video_1`，传输后逐项核对契约哈希。

## 采用方案

选择“最新 main 的 RK3588 原生重建”：先保留算法与模型输入身份，适配板端编译和
RKNN 运行环境，再分层验证。没有采用复制 RK3566 二进制及其完整动态库的方式，
因为 Debian 10/OpenCV 4.10/RKNN 2.3.2 与 RK3588 的 Ubuntu 22.04/OpenCV 4.5.4/
系统 runtime 不同；也没有继续使用旧 `d589215`，因为用户明确要求部署刚合并后的最新代码。

## Debug 运行形态

- 保留交互窗口、鼠标选框、暂停/继续和 Debug overlay。
- 提供一个 RK3588 专用 Debug 配置，默认指向统一测试视频和实际模型路径。
- 默认把输出写入独立运行目录，至少包含原始日志、boxes CSV 和 session metadata；
  输出视频按调试需要启用，避免无意覆盖。
- 首轮自动验证使用明确 ROI 的非交互命令，交互 GUI 作为随后单独验证层。

## 验证门槛

1. 源码、模型、manifest、labels、配置和统一数据集传输前后 SHA-256 一致。
2. RK3588 原生配置和编译成功，完整 CTest 通过；记录编译器、OpenCV、runtime、
   driver、源码提交和二进制哈希。
3. NanoTrack 三模型逐一加载成功；detector 单图 smoke 成功。
4. `occ_video_1` 非交互短程运行成功并留存日志、CSV、metadata；检查请求计数、
   控制安全字段和异常退出。
5. 交互 Debug 窗口能够启动、选框、播放和正常退出。

## 非目标与证据边界

- 不修改协调者拥有的 `STATUS.md` 或 `integration-contract.yaml`。
- 不批准 detector promotion，不生成生产 handoff，不执行交叉编译或真实吊舱测试。
- 编译成功、模型加载成功或短程视频跑通都不等于长稳、重捕质量或产品验收通过。
- 如需源码兼容修改，只在本任务工作树提交最小改动，并在板端复测后记录到本任务拥有的
  `docs/progress/board-validation.md`；无代码改动则只记录部署与验证证据。
