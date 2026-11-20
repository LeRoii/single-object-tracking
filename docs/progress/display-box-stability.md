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
- 首个候选源码提交：`62f4bed10e940c552761709164aafd8f240b1a88`；
  与本文件在同一提交。随后增加了只读对照工具，最终工作线提交见 Git 分支顶端。

### 静止视频 ROI 和对照工具

- 按视频第 30 秒缩略图估计的 3840×2160 初始框，板端运行前需核对原分辨率：
  左柜门把手 `(1390,565,70,200)`、右侧相邻把手 `(1480,565,70,200)`、
  显示器右边缘 `(990,70,150,220)`、低纹理柜门 `(1710,370,220,200)`。
  这些只是复现选点，不是人工真值；静止视频的 RTSP 压缩亦可能影响纹理。
- `rk3566/nanotrack_v3/tools/compare_display_stability.py` 读取相同视频、
  相同初始框、开关关/开两份逐帧 CSV。它先检查追踪状态和追踪框是否一致，
  再比较连续 TRACKING 帧的显示中心、宽高逐帧跳变和二阶差分。
  代码与 `--help`、合成 CSV 运行已验证；这只是度量工具，不替代带框视频的
  人工目标贴合核对。示例：

  ```bash
  python3 rk3566/nanotrack_v3/tools/compare_display_stability.py \
    baseline.csv candidate.csv --warmup-frames 30
  ```

## 后续板端验证

1. 板端开启后，将同一源码、配置、模型和静止视频连同 SHA-256 复制到板上；
   原生编译与运行，记录 runtime/driver、程序及模型哈希。
2. 在画面中选柜门把手、物体边缘等有纹理 ROI，及无纹理柜门 ROI 作参照。
   每个 ROI 在相同初始化框上分别运行候选开/关，导出逐帧跟踪框与显示框、
   带框视频；仅对保持正确目标的 TRACKING 帧评价显示中心、宽高的高频变化。
3. 同时检查真实运动和重新捕获时显示框拖尾、框与目标错位；若只降低抖动却
   失去对目标的贴合，不接受此参数。

## 2026-11-20T12:42:19+08:00 首版板端实测与修正

- 板端：`neardi@192.168.1.106`，`Rockchip RK3588 NEARDI LKD3588`，
  Linux `6.1.84`，RKNN API `2.1.0 (967d001cc8@2024-08-07T19:28:19)`，
  driver `0.9.8`。独立目录：
  `/home/neardi/Project/SOT/nt-display-stability-748c27a`，没有覆盖其他运行目录。
- 首版源码为 `748c27ae0dc622f45d7874cf437a51ba6ef42130` 的 Git archive，
  `assets/rk3588-display-stability-748c27a.tar` SHA-256
  `262f24fcea41e2c368bc8f16a3dbc84c00068cd4fcbe35549458a508e8a77465`。
  板端原生构建：
  `cmake -S rk3566/nanotrack_v3 -B build-native -DCMAKE_BUILD_TYPE=Release -DENABLE_RKNN_RUNTIME=ON -DBUILD_TESTING=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nt-ten-target-33f34ba/assets -DRKNN_LIBRARY=/usr/lib/librknnrt.so`，
  `cmake --build build-native -j4`，`ctest --test-dir build-native --output-on-failure -j4`。
  观察：23/23 PASS；`runs/ctest.log` SHA-256
  `8f8e9ef70a8edcd14962039140dc9b744f2851aaef77e35cee24e95d2cd1b451`；
  `build-native/nanotrack_v3` SHA-256
  `d84c2e088533d1fd4d8987705e9612adb138950cb645c21326530ffa24db1738`。
- 板端模型组来自
  `/home/neardi/Project/SOT/nt-ten-target-33f34ba/assets/models`，
  SHA-256：template `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`，
  search `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`，
  head `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`，
  tracker manifest `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`。
  四项与 `integration-contract.yaml` 的 RK3588 模型身份一致。运行未配置 detector，
  因此没有消费 detector label；把手初始化记录为 `UNKNOWN`。
- 板端静止视频与主机 SHA-256 一致。板端关闭配置
  `rk3566/nanotrack_v3/config/rk3588-display-stability-off.conf` 是仅将
  `display_stability_enabled` 改为 `false` 的对照，SHA-256
  `2ba13535000c555c4ca45d774476a5dcf5a9be3e408c857aeae81aff9250a494`；
  开启配置为上述候选配置的 `02748132...`。
- 实际回放命令以每个 ROI、开关分别运行；例如完整把手：

  ```bash
  ./build-native/nanotrack_v3 \
    --video assets/pod_static_visible_20261120.mp4 \
    --bbox 1375,650,100,240 \
    --model-dir /home/neardi/Project/SOT/nt-ten-target-33f34ba/assets/models \
    --sensor visible \
    --controller-config rk3566/nanotrack_v3/config/rk3588-display-stability-candidate.conf \
    --output-size 480x270 --max-frames 1800 \
    --output-video runs/handle-minute-candidate.avi \
    --output-boxes runs/handle-minute-candidate.csv \
    --session-metadata runs/handle-minute-candidate.metadata.json
  ```

  关闭组只换配置和输出文件名。第二把手 `(1450,650,100,240)`、显示器边缘
  `(990,70,150,220)`、无纹理柜门 `(1710,370,220,200)` 各运行 300 帧。
  完整把手 1800 帧均为已接受 TRACKING，两组 tracking box、状态逐帧完全一致；
  显示中心二阶差分 p95 从 `5.4645` 降为 `0.0000675` 像素，尺寸二阶差分
  p99 从 `1.099%` 降为 `0.0896%`。候选的显示/跟踪中心最大相距 `5.034` 像素；
  视频首、中、末抽帧仍围住同一把手。第二把手前 300 帧中心二阶差分 p95
  `6.856 → 0.000145` 像素。显示器边缘 `13.316 → 2.210` 像素，
  但候选仍有单帧 `10.27` 像素位移、显示/跟踪中心最大相距 `26.687` 像素。
  无纹理柜门只有初始化帧被接受，不具备持续跟踪抖动评价条件。
- 原 `--output-video` 画的是 `control_bbox`，所以同组开关 AVI 字节相同，
  不能用于显示框验收。新增 `tools/render_display_comparison.cpp` 将 CSV 的
  `display_bbox` 叠回同一原视频，输出左侧关闭、右侧开启的并排视频。
  板端 `runs/handle-minute-display-compare.mp4` SHA-256
  `7322a7221f32cc801370129037a131c3f085a5cbf77fa02c09a7517106a3ef08`；
  `runs/right-handle-display-compare.mp4` 为
  `ac15ed78ff5e89e7c59c65fbd60fb79cb8a3cf9f45d4415220185a5ccb0a3957`；
  `runs/monitor-edge-display-compare.mp4` 为
  `cadcdf98d00dae5153f5127afa0d654bf49839357836816087be65070633cc20`。
  本机编译与运行命令：
  `c++ -O2 -std=c++17 rk3566/nanotrack_v3/tools/render_display_comparison.cpp -o /tmp/render-display-comparison $(pkg-config --cflags --libs opencv4)`；
  `/tmp/render-display-comparison INPUT.mp4 OFF.csv ON.csv 1240,560,480,420 1800 OUTPUT.avi`。
- **首版失败，暂不交付**：使用已核验的契约视频 `occ_video_1/1.mp4`
  SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`
  与 car001 初始化框 `(992,582,37,39)` 做 271 帧运动测试，开关两组 tracking box
  和状态相同，但候选显示框滞后最大 `17.94` 像素；第 54 帧显示框与可见目标标签
  IoU 从基线 `0.7999` 降到 `0.3098`。有效可见且被接受的 241 帧平均 IoU
  `0.7644 → 0.6328`；这组标签的 36--168、182--270 帧为经抽样复核的候选真值，
  169--181 为估计框，以上统计已排除后者。不能用静止把手降抖掩盖运动退化。
- 因此同一工作线新增持续运动旁路：三帧中值和五帧窗口判断移动，
  确认后在短保持期内沿用原显示目标；静止时才使用强去抖。
  新增小目标连续移动单测，原静止交替抖动与孤立跳点单测继续通过。
  此修正尚未在板端测试，旧首版数据保留，不会重命名为修正版结果。

## Handoff

- 协调对话需要接收本分支最终提交、候选配置与下方板端测试工件哈希，再决定
  是否集成显示框候选。当前结果仅为隔离的开发板候选，不是生产 handoff 或
  真实吊舱验收；`STATUS.md` 和 `integration-contract.yaml` 未由本任务更新。
- 板端验证对话若消费本候选，应先核对本分支最终提交、配置、模型和视频哈希，
  复核其自身工作树基线及测试边界。不得从本工作树拷贝未提交文件。
- 首版静止场景虽显著降抖，但运动目标真值已证实明显滞后，故协调对话不得
  集成 `748c27a` 或 `e6e413c` 为默认显示策略；第三版 `0758f64` 的边缘
  突跳亦未通过。最终候选的板端实测见下方；仍需人的观感反馈后决定采用。

## 2026-11-20T12:57:28+08:00 运动回归修正与再次板测准备

- 上述持续运动旁路版源码 `e6e413c794be693f9086ebf7139a936435a09e59`
  在板端独立目录 `/home/neardi/Project/SOT/nt-display-stability-e6e413c`
  原生编译，源码归档 SHA-256
  `cff8431d8a28e07db26de874cf5945a4ac674eccc7a87fac84699afafa649ccf`，
  程序 SHA-256
  `b84ecc593a93a91718cbc612c728974e2ca6749224bf1bdd88b68c3b6b1c137f`，
  CTest 23/23 PASS。使用同一模型、配置、car001 视频及初始化框做 271 帧回放；
  追踪路径与关闭开关组相同，但显示框在第 53 帧仍落后约 17.33px，
  第 54 帧跳动 27.8px。`runs/moving-car-candidate.csv` SHA-256
  `58d8889295de973cab59d4fa4540890865df4d44124af7dcba58dd677d4fea71`。
  该版运动目标退化，不能集成或交付；它也没有运行完整静止视频。
- 当前修正版沿用原配置与模型身份，仅改显示路径：大框用五帧原始观测的
  稳健斜率判断持续运动；孤立大跳需下一帧确认；从 SUSPECTED/重新捕获回到
  TRACKING 时三帧暂走原路径。较小框（短边 <64px）只对中心做两帧 0.65/0.35
  融合，尺寸走原路径，避免滤波延迟吃掉小目标的面积。这是显示输出的像素门，
  不改变追踪、控制框或模板。64px 是本段小车约 37--58px 宽与把手 100px 宽
  之间的保守分界，尚无跨尺度调参结论。
- 主机重建并运行 `ctest --test-dir /tmp/rk3588-display-stability-build
  --output-on-failure -j4`，23/23 PASS。以首版板端同一 CSV 的追踪输出进行
  `AdaptiveDisplayBox` 离线重放，核对关闭路径与实际关闭 CSV 逐帧误差
  约 0.0006px。修正版 car001 显示框对有效可见帧的平均 IoU 约 0.7603，
  关闭组约 0.7635（两组排除 169--181 估计框）；最大中心差 3.35px。
  静止把手离线重放的显示中心二阶差分 p95 约 0.0042px，关闭组约 5.4777px。
  这只是用板端**既有追踪输出**筛查显示算法，不是新源码的 RK3588 运行。
- 下一步把本提交源码归档到新的板端目录，原生编译、CTest，并重新实际
  回放完整把手、边缘和运动车辆；检查显示框与目标贴合后再提供协调 Handoff。

## 2026-11-20T13:05:29+08:00 第三版板测与边缘突跳修正

- 消费本工作线已提交的 `0758f64bd21f5fca23e671dcb65d12d7b17fde68`；
  Git archive SHA-256 `85377504cf3d82a6014a1de936e870aaf9d233f32bb08eec807a9ded94298790`。
  开发板新目录 `/home/neardi/Project/SOT/nt-display-stability-0758f64`，
  原生 `cmake`、`cmake --build` 和 CTest 23/23 PASS；程序 SHA-256
  `62b428bee690cc8c83f837f657b284a5f4d96495775ca7925f291fdca973884d`，
  CTest 日志 SHA-256 `dfe7756d86f029df100713a92c5a53e108b935e54be65e757b46fda214f914bc`。
  输入静止视频与 canonical 车视频、候选/关闭配置哈希均与前述相同。
- 在该程序上实际回放 1800 帧静止把手，关闭组使用首版同源原始 CSV；两组
  追踪框及状态逐帧一致。显示中心二阶差分 p95 `5.4645 → 0.0000675px`；
  候选 CSV `runs/handle-minute-candidate.csv` SHA-256
  `82c6fd7da5c2fd5b64b710961de836d7552c706f3711f36ec154b06d3709236e`。
  同程序的车视频开关两组 271 帧追踪框/状态一致；有效可见且已接受的
  241 帧显示 IoU 平均 `0.76438 → 0.76107`，低于 0.5 的帧均为
  `[188,191,192,193,194,195,196]`，最大显示中心差 3.349px，
  显示中心二阶差分 p95 `4.810 → 2.881px`。关闭/候选 CSV SHA-256 分别为
  `459113c77a5ea0345d8349bb8cb6bf36127df47ee676e326a13cb47bbd32e1e5` /
  `0e5d66c6ca3421fd3122e25c7568f2c2b6d3f50cb6efc5054f04cd8f2fa9da3d`。
  统计排除 169--181 的估计框；真值只说明此单目标序列。
- **第三版仍有问题**：边缘 ROI `(990,70,150,220)` 前 301 帧显示中心
  二阶差分 p95 `13.316 → 2.145px`，但第 68 帧候选显示框单步跳
  33.30px（关闭组最大步 16.63px）。第 60--67 帧慢漂移积累偏差，
  五帧斜率的 2.5% 尺寸门误判为持续运动，随即释放旧偏差。
  候选 CSV SHA-256 `52ab1a924dd99ba59c0002022cfd7f49f7b696778095b12ba47af0a00e90c29d`。
- 当前修正仅把大框的快速运动旁路门提高到单帧 `max(2px,10% 框轴尺寸)`；
  小框的低延迟路径、所有控制和追踪逻辑不变。主机再次 23/23 PASS。
  用实际边缘追踪输出离线重放，最大单步从 33.30 降至 10.27px，
  二阶差分 p95 约 2.23px；把手仍约 0.01px，小车仍约 3.317px
  （该离线脚本按全部帧算，与上述仅已接受帧的统计口径不同）。
  此为筛查，须在板上重放新源码并复核画面后才能 Handoff。

## 2026-11-20T13:13:29+08:00 最终隔离候选的开发板实测

- 消费已提交的 `a0d52e8ee0a77a924d4b7f2f82631348b4a818e3`；源码 Git
  archive SHA-256 `98824d615f7c617b68714f8049ebc65d1c08450b054e405dfe06e30f02ef25ce`，
  板端新目录 `/home/neardi/Project/SOT/nt-display-stability-a0d52e8`。
  RK3588 NEARDI LKD3588，kernel `6.1.84`，RKNN API `2.1.0`、driver `0.9.8`。
  板端原生 `cmake`、`cmake --build`、`ctest` 实际运行；CTest 23/23 PASS。
  程序 SHA-256 `f6a85fb0c5ac6511010fc5d2587c2d2a646751b55649ae3baca6afa56df9d833`，
  CTest 日志 SHA-256 `83cbf5fe6bf429d03c663784609049ae9a1ad63da96ff2e01a2ad242399f4d0a`。
- 本轮重新核对板端模型：template
  `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`，
  search `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`，
  head `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`，
  tracker manifest `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`；
  均与协调契约一致。未配置 detector；UNKNOWN 由日志确认。
  静止/运动视频 SHA-256 `8dea64988e3766a410d601a59ca14f49550d0d9c57a3beacbe9aeb78bdf4bd80` /
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`。
  开启/关闭配置 SHA-256 `02748132dc4b354b18e1c67d3459cb7b3cb8a2576ee0129f7ad63bc25da491ee` /
  `2ba13535000c555c4ca45d774476a5dcf5a9be3e408c857aeae81aff9250a494`。
- 运行参数与上一节一致：把手 `(1375,650,100,240)`、`--max-frames 1800`；
  边缘 `(990,70,150,220)`、`--max-frames 300`；车 `(992,582,37,39)`、
  `--max-frames 271`，车在同版程序做开关对照。四路均指定视频、模型、
  `--sensor visible`、对应配置、`--output-size 480x270`、AVI、逐帧 CSV 和
  会话 metadata。板端的 AVI 画控制框，另用 CSV 生成真正的显示框并排视频。
- 把手 1800 帧均为已接受 TRACKING；与关闭基线的追踪框/状态逐帧相同。
  显示中心二阶差分 p95 `5.46449 → 0.0000675px`，尺寸二阶差分 p99
  `1.099% → 0.0896%`。候选 CSV SHA-256
  `7e445a5b3682cdc5bafc44cbc29cc541e40e7f61f96adc41c08668c765810761`；
  关闭基线为首版同输入同模型 CSV，身份见上节。第 30 帧之后显示中心相对
  关闭组最大 5.034px。抽看视频第 30、900、1790 帧，均围住同一把手。
- 边缘 301 帧均为已接受 TRACKING；与首版关闭基线的追踪框/状态逐帧相同。
  显示中心二阶差分 p95 `13.31645 → 2.21039px`；最大单帧中心移动
  `16.63 → 10.27px`，先前第 68 帧突跳已消除。候选 CSV SHA-256
  `927b2a63778ed04d3263ef8c0c796a4bdd392ebdda9f4ccb2725e8029e64d640`。
  第 67 帧显示中心与追踪中心仍相距 26.687px；第 68、150、250 帧画面
  抽查仍围住同一边缘，但这个距离显示静止场景中追踪本身也有漂移，
  不能把框静止误说成追踪位置准确。
- 车 271 帧同版开关两组追踪框、状态相同。已接受且可见的 241 帧，
  显示框平均 IoU `0.76438 → 0.76107`，低于 0.5 的帧两组相同
  `[188,191,192,193,194,195,196]`；排除 169--181 估计框。
  最大显示中心差 3.349px，中心二阶差分 p95 `4.810 → 2.881px`。
  关闭/开启 CSV SHA-256 `016dd14591669c377168644a784c53619414ad5dd20e157e289dec660dadb85a` /
  `01bebb135613c7d86cd69ee52a6ba0b6bc1fd03798712a069de28a37e1f5196a`。
  第 54/120 帧抽查看到两框仍在同一白车附近；第 188 帧目标受遮挡，
  两组都无法作为贴合性证明。
- 对照视频已存板端同目录 `runs/`：
  `handle-minute-compare.mp4` SHA-256
  `de7969f36513933bea01448a712a49d5816827c68dd4c6dde843cad103607a67`；
  `monitor-edge-compare.mp4` SHA-256
  `72f8db30fbf5e161031978a552bb6cd35cf7514db109c4db6485bf44a1148106`；
  `moving-car-compare.mp4` SHA-256
  `baa4507c458ed3b291a8ccfe7f9041b0e523562f8b88c8a446a315f10dae5b8c`。
  左侧关闭、右侧开启。生成命令为本分支 `tools/render_display_comparison.cpp`
  编译出的 `/tmp/render-display-comparison INPUT OFF.csv ON.csv CROP MAX_FRAMES OUTPUT.avi`，
  再用 `ffmpeg -c:v libx264 -preset fast -crf 23 -pix_fmt yuv420p` 转 MP4。
- 此为特定输入、ROI 的板端候选证据；没有测试真实吊舱的实时显示链，
  没有部署生产 A8 程序。已请用户观看把手并排视频反馈观感。
  若协调对话采用，只集成当前最终提交和配置并保留开关，切勿把早期失败
  候选或静止视频结果宣称为吊舱接受。
