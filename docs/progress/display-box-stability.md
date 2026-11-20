# RK3588 显示框稳定性候选

## 2026-11-20T12:08:53+08:00 主机阶段

- 分支：`codex/rk3588-display-box-stability`；工作树：
  `/home/u20/.codex/worktrees/display-box-stability/single-object-tracking`。
  从本机干净的 `main@cd81e0d5e656d65b981e6d499f938e4b5f3e14b5` 建立；
  当时 `origin/main@80fa51e02a9692136e1f69c17d804eba681dc9e7`。
  `STATUS.md` 与 `integration-contract.yaml` 尚未同步到此本机 main；本任务只消费
  main 已集成源码，没有消费其他候选分支的提交或板端结论。
- 输入视频：
  `/home/u20/.codex/worktrees/rk3588-a8-cross-compile/single-object-tracking/data/occ_video_1/pod_static_visible_20261120.mp4`，
  SHA-256 `8dea64988e3766a410d601a59ca14f49550d0d9c57a3beacbe9aeb78bdf4bd80`。
  3840×2160，30 FPS，容器时长 60.001 s；`ffprobe -count_frames` 解码到
  1800 帧，流头 `nb_frames=1806`。静止画面含柜门把手、柜门、物体边缘；
  它没有人工跟踪框，不能据此声称算法实际框抖动已经改善。
- 修改：在 `AdaptiveDisplayBox` 的已接受 TRACKING 显示路径增加独立开关。
  三帧中值拒绝孤立跳点；位置使用尺寸比例死区和方向一致性决定响应速度；
  尺寸使用比例死区及低速平滑。初始化、重新捕获、SUSPECTED、SEARCHING
  保留原路径；跟踪框、控制框、模板和伺服数据未改变。
- 候选配置：
  `rk3566/nanotrack_v3/config/rk3588-display-stability-candidate.conf`，
  SHA-256 `02748132dc4b354b18e1c67d3459cb7b3cb8a2576ee0129f7ad63bc25da491ee`。
  相对于 `rk3588-unknown-recovery-consistency.conf` 仅新增显示稳定性开关和四个参数。
  新开关在代码默认值中为 `false`，现有配置仍保留原行为。会话 metadata
  记录开关与有效参数，配置指纹包含它们；逐帧 CSV 字段未变。
- 主机命令：
  `cmake -S rk3566/nanotrack_v3 -B /tmp/rk3588-display-stability-build -DCMAKE_BUILD_TYPE=Release -DENABLE_RKNN_RUNTIME=OFF -DBUILD_TESTING=ON`，
  `cmake --build /tmp/rk3588-display-stability-build -j4`，
  `ctest --test-dir /tmp/rk3588-display-stability-build --output-on-failure -j4`。
  观察结果：23/23 PASS。显示模块用合成框序列验证了单帧跳点、
  交替位置/尺寸波动、连续运动和缩放、SEARCHING 旧路径、关闭开关旧路径。
  这些不是记录视频的 RKNN 推理结果。
- 当前边界：RK3588 开发板当时未开机；尚无真实 NanoTrack 回放的
  框抖动量化、带框视频、人眼观感或吊舱验收。主机 CTest 只证明代码路径和
  合成场景行为。不能以本候选替代协调契约中的生产 handoff。

## 后续板端验证

1. 板端开启后，将同一源码、配置、模型和静止视频连同 SHA-256 复制到板上；
   原生编译与运行，记录 runtime/driver、程序及模型哈希。
2. 在画面中选柜门把手、物体边缘等有纹理 ROI，及无纹理柜门 ROI 作参照。
   每个 ROI 在相同初始化框上分别运行候选开/关，导出逐帧跟踪框与显示框、
   带框视频；仅对保持正确目标的 TRACKING 帧评价显示中心、宽高的高频变化。
3. 同时检查真实运动和重新捕获时显示框拖尾、框与目标错位；若只降低抖动却
   失去对目标的贴合，不接受此参数。

## Handoff

- 协调对话需要接收此分支最终提交、候选配置与测试工件哈希，并决定是否集成。
  目前主机候选未获得板端验证，不可列为正式交付。
- 板端验证对话需要接收本任务最终提交与输入视频哈希；实际消费前核对
  源码、配置、模型与运行工件身份，勿把本文件当成实测结论。
