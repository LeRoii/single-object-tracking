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
  突跳未通过，第四版 `a0d52e8` 对细长把手的尺寸未降抖。当前候选的板端
  实测见下方；仍需人的观感反馈后决定采用。

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

## 2026-11-20T13:22:52+08:00 第二把手核对暴露细长目标尺寸缺口

- 使用上节 `a0d52e8` 同一板端程序、模型、视频和配置，对第二把手初始框
  `(1450,650,100,240)` 做开启/关闭各 1800 帧。两组追踪框/状态相同，
  均接受 1800 帧；显示中心二阶差分 p95 `5.6942 → 0.000184px`，
  尺寸二阶差分 p99 `0.847% → 0.113%`。关闭/开启 CSV SHA-256
  `2c070e756bc14080e4b1afd61344a6ad392af32ca16b977f9a0d691fdf2eda12` /
  `f9cebe78cf7316d001e13ffa395d8c71540d38d6c68362d2ae19125aece2d63f`。
  但首、中、末画面核查显示该初始化框夹带柜门接缝，后期追踪框向左移并
  同时覆盖两个把手。因此这组不能用于证明单个右把手仍被锁住；其数值仅
  证明这个混合纹理 ROI 的显示降抖。其带框视频保留在本机
  `/tmp/display-v4/right-handle-minute-compare.mp4`，SHA-256
  `8fcc69e5f8327295e9611cb0bebe4da615329929a280e8bd421a4c2b276b1173`。
- 改用只包围右把手的 `(1485,675,55,195)`，同版板端开关各 301 帧，
  两组追踪框/状态仍逐帧相同且全程接受。位置二阶差分 p95
  `1.6431 → 0.7799px`，但尺寸二阶差分 p99 两组同为 `1.729%`。
  关闭/开启 CSV SHA-256
  `d0e5031771d1bc8968c6c9dfa8f04a056190c438493ceefef327c29cd5a2d8fc` /
  `7ffe67048ff0bfb7a3c8b2d1e742429440d898b055a62a4a81f5f51acc1f4470`。
  原因是短边 55px 被旧 `<64px` 小目标条件分流，尺寸保持原路径。
- 当前源码把小目标分流改为宽高面积 `<80×80px`，细长把手走完整位置/尺寸
  滤波；小车约 37--58×39--66px 仍走低延迟路径。新增细长目标交替抖动
  单测，主机 `cmake --build /tmp/rk3588-display-stability-build -j4` 与
  `ctest --test-dir /tmp/rk3588-display-stability-build --output-on-failure -j4`
  实测 23/23 PASS。同一板端原始追踪 CSV 经主机显示模块离线重放后，
  精确右把手位置 p95 约 0.0292px、尺寸 p99 约 0.383%；小车结果基本
  相同。这只是离线筛查，需将本提交在板端原生重编译并实际复测，
  再判断是否可替代 `a0d52e8` 候选。

## 2026-11-20T13:30:53+08:00 细长把手完整板端回放

- 消费本工作线源码提交 `1bf0f29625e0ecdccbaeb037e592c3b78ea0ddff`；
  Git archive SHA-256 `484729420b3bc4ca09db0c2431d4ec9b22a858f315b5429e9bdabd29098e34f3`，
  板端隔离目录 `/home/neardi/Project/SOT/nt-display-stability-1bf0f29`。
  RK3588 NEARDI LKD3588、kernel `6.1.84`、RKNN API `2.1.0`、driver `0.9.8`。
  原生 `cmake`、`cmake --build`、`ctest` 已运行；23/23 PASS，程序 SHA-256
  `3ba15349aad28ceebc535792e373916d460ec6b58c8b1564b4c7079132586aeb`，
  CTest 日志 SHA-256 `b27e0a76b79c2a1574f5aa44ff8f0fddd36dafa33611a0da8f71ef6fc8c9ef19`。
  当前板端重新核对 template/search/head/manifest 哈希，与上一节及契约一致；
  静止/车视频、开启/关闭配置哈希亦与上一节相同，未配置 detector。
- 板端对仅包围右把手的 `(1485,675,55,195)` 开关各回放 1800 帧；
  命令使用上节相同 `--video`、`--model-dir`、`--sensor visible`、
  `--controller-config`、`--output-size 480x270`、`--output-video`、
  `--output-boxes`、`--session-metadata`，仅初始框、配置和输出路径对应变更。
  两组均有 1800 个已接受 TRACKING 帧，追踪框和状态逐帧相同。
  显示中心二阶差分 p95 `1.68769 → 0.00315px`；尺寸二阶差分 p99
  `1.6776% → 0.1314%`，最大显示中心单步 `1.912 → 0.526px`。
  显示/追踪中心最大相距 3.970px，发生在第 556 帧。
  关闭/开启 CSV SHA-256
  `f2fa27e09c1595ffd98e7dca3d563c6e0e435805ed2ab15c88f3522a94e3c30f` /
  `ea0eb413de743b8e87a1dc0db7e2a764b9fcf1d5ac6066685f17a15be61e8191`；
  会话 metadata SHA-256
  `633e387b4dcb1dbd6de2a5e25714bbd15b9e4c59c0d9f860ee8f7c12f298c8b2` /
  `6fbb7ee220b6b1d9e5c48abeea1c7ba197b534ed61c42064c87ad9e9c7bb3bba`；
  开启日志 SHA-256 `2c7023688e65a6b224634adef9d86b27f35a4a2223bc9a86d19d3f82a5a6c168`。
- 用真实 `display_bbox` 生成并排视频，左关闭右开启；本机和板端同名路径
  `runs/isolated-right-handle-minute-compare.mp4`，SHA-256
  `a94430d02a5d8b24de8bd70520e1fbf9c69603bc3c20fd3e4a8ba6bc932314d7`。
  抽查第 30、900、1790 帧，候选框始终围住**右侧单个把手**，没有先前
  混合 ROI 的双把手漂移；这三帧抽查不是逐帧人工真值。
- 同版实际复测边缘 301 帧和车 271 帧：追踪框/状态与前版对照逐帧一致，
  边缘中心二阶差分 p95 `13.316 → 2.210px`、最大步 `16.63 → 10.27px`；
  车有效可见已接受 241 帧的平均显示 IoU `0.76438 → 0.76107`，
  低于 0.5 的帧与关闭组相同。边缘/车候选 CSV SHA-256
  `a4a78babaa92f8648ec29f14f839295279b71a88179b1d72eaa39e098709ee11` /
  `af2db3a38d752b7853763ec108141741947548b5f6c947608b70b6970e4a1e8f`。
  第一个把手完整 1800 帧的前版验证见上文；本次面积分流条件不会改变
  其 100×240px 初始框的显示路径。无纹理柜门曾仅初始帧被接受，
  不能从它评估显示抖动。
- 这是特定静止视频和一个运动车辆序列上的开发板候选，不是实际吊舱
  实时显示链或用户观感验收。协调对话若采纳，应消费本提交与上述工件身份，
  并保留可关闭开关；生产交叉编译仍需另行批准的 board handoff。

## 2026-11-20T13:51:12+08:00 边缘整分钟漂移与图像锚定候选

- 进一步使用 `1bf0f29625e0ecdccbaeb037e592c3b78ea0ddff` 同版板端程序，
  对边缘初始框 `(990,70,150,220)` 开关各回放完整 1800 帧，输入、模型、
  配置和 runtime 身份沿用上一节。追踪框/状态逐帧相同、两组均全程接受。
  位置二阶差分 p95 `17.4166 → 1.3512px`，尺寸 p99
  `0.800% → 0.1425%`；但候选在第 1173--1179、1229--1232 帧仍有连续
  大位移，最大单步 17.007px。关闭/开启 CSV SHA-256
  `ea304e45fd25f263a093dd241d75bd73ada909f471ee92d4b415301dedcc1534` /
  `67e8265e5f7afc64a710dc668207cad25ac929546abaee7b1ee4b4b135ddd828`。
  带真正显示框的并排视频保留在
  `/home/neardi/Project/SOT/nt-display-stability-1bf0f29/runs/monitor-edge-minute-compare.mp4`，
  SHA-256 `16bdf31f6ac1f28e6fa48dd746dfc27c0719ada2b0b48754c8d68739ff078ebb`。
  抽帧证实相机和物体边缘没有对应移动，追踪框却沿竖直边缘上下滑动；
  先前的 10 秒结果不能代表整分钟，此版边缘场景尚未达到稳定目标。
- 主机用同一视频首帧原 ROI 像素作为参照，逐帧测灰度绝对差均值；
  边缘 1799 帧最大约 `4.45`，精确右把手最大约 `9.69`；
  同一方法对 canonical 运动车辆原 ROI，从第 1 帧起至少 `37.62`。
  以约 64×64 网格采样后，边缘最大 `4.35`、把手最大 `10.23`，
  小车首帧差约 `43.91`。该差距支持尝试以图像是否仍为同一静止内容
  约束**显示框**，不能证明所有场景都能以该阈值分开。
- 新增独立配置 `display_static_anchor_enabled`（默认 `false`，当前候选
  `true`；还须 `display_stability_enabled=true` 才生效），只对初始化为
  `UNKNOWN`、初始 ROI 有足够纹理且连续 15 帧图像差不超过 14 的目标启用。
  启用时显示框停在确认时的位置；图像差持续升高、进入 SEARCHING 或
  重新捕获时退出。追踪框、控制框、模板和状态不改。配置指纹和 metadata
  记录开关；逐帧 `display_box_source=STATIC_IMAGE_ANCHOR` 可区分该路径，
  日志 schema 从 16 增至 17。候选配置 SHA-256
  `3411ec9f9c19a8666e117e4ac82d8084f2b7a118e8850ea0b60c08c1277bca67`。
- 主机 CMake 重建、CTest 23/23 PASS。合成图像测试覆盖静止纹理下追踪
  漂移、持续图像变化时释放、无纹理禁用、已识别类别禁用和搜索后退役。
  用板端现有**原始追踪输出**加同一原视频在主机重放新显示模块，
  边缘和精确右把手从第 14 帧起至 1799 帧持续记录图像锚定；
  小车 271 帧从未启用，显示路径与上一版一致。关闭开关的重放与板端
  原始显示框最大坐标差约 0.005px（文本输出精度）。
  这仍只是主机重放，不是新源码板端结果；下一步以本提交在 RK3588
  原生编译并实际复测两段静止 ROI 与运动车辆，核对视频和日志。

## 2026-11-20T14:03:33+08:00 图像锚定候选的 RK3588 原生验证

- 本轮实际消费本分支已提交源码
  `76a2df65f75449155263027708ebd9d759438eff`，从本机
  `main@cd81e0d5e656d65b981e6d499f938e4b5f3e14b5` 派生；工作树为
  `/home/u20/.codex/worktrees/display-box-stability/single-object-tracking`。
  板端使用该提交的 Git archive
  `/home/neardi/Project/SOT/rk3588-display-stability-76a2df6.tar`，
  SHA-256 `c78ec1ba1b07d2b4cb8a995d807366b1eb90ecc86fa776d24939e832db1c05bd`；
  解包、编译和运行位于隔离目录
  `/home/neardi/Project/SOT/nt-display-stability-76a2df6`。
- 开发板 `neardi@192.168.1.106`，RK3588 NEARDI LKD3588，Linux
  `6.1.84`，RKNN API `2.1.0 (967d001cc8@2024-08-07T19:28:19)`、
  driver `0.9.8`。原生构建命令：
  `cmake -S rk3566/nanotrack_v3 -B build-native -DCMAKE_BUILD_TYPE=Release -DENABLE_RKNN_RUNTIME=ON -DBUILD_TESTING=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nt-ten-target-33f34ba/assets -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、
  `cmake --build build-native -j4`、
  `ctest --test-dir build-native --output-on-failure -j4`。
  观察到 23/23 PASS；`runs/ctest.log` SHA-256
  `b770b1916f5bf2cc42543e90f88b4a932a13e9a13d128d66b36ea7e928312747`；
  程序 `build-native/nanotrack_v3` SHA-256
  `141b060e2f59d649d0c2b800a9c1ed2763edbe19dca001d8d821b5473e38cc18`。
- 板端输入视频的 SHA-256 已重核：静止 4K 视频
  `assets/pod_static_visible_20261120.mp4`
  `8dea64988e3766a410d601a59ca14f49550d0d9c57a3beacbe9aeb78bdf4bd80`，
  车辆视频 `assets/occ_video_1.mp4`
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`。
  RK3588 template/search/head RKNN 和 tracker manifest 哈希分别为
  `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`、
  `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`、
  `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`、
  `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`；
  与 `integration-contract.yaml` 中的 RK3588 模型身份一致。本轮没有配置
  detector，未知类别目标没有消费 detector 模型或标签。
- 候选配置 SHA-256
  `3411ec9f9c19a8666e117e4ac82d8084f2b7a118e8850ea0b60c08c1277bca67`；
  关闭 `display_stability_enabled` 和 `display_static_anchor_enabled` 的
  对照配置 SHA-256
  `adb7ace425fbbbb1e6f97463f53a1fb5d006238a1d0c12275381d5eed0c578b4`。
  每个 ROI 在同一板端程序运行，例如：

  ```bash
  ./build-native/nanotrack_v3 \
    --video assets/pod_static_visible_20261120.mp4 \
    --bbox 990,70,150,220 \
    --model-dir /home/neardi/Project/SOT/nt-ten-target-33f34ba/assets/models \
    --sensor visible \
    --controller-config rk3566/nanotrack_v3/config/rk3588-display-stability-candidate.conf \
    --output-size 480x270 --max-frames 1800 \
    --output-video runs/monitor-edge-minute-candidate.avi \
    --output-boxes runs/monitor-edge-minute-candidate.csv \
    --session-metadata runs/monitor-edge-minute-candidate.metadata.json
  ```

  关闭组更换配置和同名 `off` 输出。右把手使用 `(1485,675,55,195)`，
  车辆使用 `(992,582,37,39)` 及 canonical 视频，车辆为 271 帧。
- 边缘 1800 帧关闭/开启组的追踪框、状态、接受标记逐帧一致，均为
  已接受 TRACKING。第 15--1799 帧显示来源为 `STATIC_IMAGE_ANCHOR`。
  从第 30 帧起显示中心二阶差分 p95 `17.41659 → 0px`，最大单帧中心
  位移 `19.45064 → 0px`，尺寸二阶差分 p99 `0.800% → 0%`。
  第 1176 帧追踪中心距锚定显示中心约 270.48px，但并排画面确认
  第 14、1176、1790 帧新显示框仍围住原静止边缘；关闭组已沿边缘漂移。
  关闭/开启 CSV SHA-256 分别为
  `c673816152eba1c3f1152028e28958795cdcd1055c7c07bb6da552545306a611` /
  `7d84946ab243060f2d0ce85c548f52f2ef4e77c854b8a4bd14b27bae69eec006`。
- 精确右把手 1800 帧均已接受 TRACKING，第 15--1799 帧锚定。
  与上一版关闭组逐帧追踪框/状态完全一致，显示中心二阶差分 p95
  `1.68769 → 0px`，尺寸二阶差分 p99 `1.6776% → 0%`；第 30、900、1790 帧
  新框仍围住同一右把手。当前候选 CSV SHA-256
  `729506cac6769079759a5494f1132a4763ce31328d3416357caa9f1b11aeac6e`；
  复用的关闭组来自
  `/home/neardi/Project/SOT/nt-display-stability-1bf0f29/runs/isolated-right-handle-minute-off.csv`，
  板端/本机 SHA-256 一致，为
  `f2fa27e09c1595ffd98e7dca3d563c6e0e435805ed2ab15c88f3522a94e3c30f`。
- 车辆 271 帧开关两组追踪框/状态/接受标记逐帧一致；均为已接受
  TRACKING 241 帧、SUSPECTED 16 帧、SEARCHING 14 帧，新锚定没有触发。
  对可见且已接受的 241 帧，按车目标标注计算的显示 IoU 平均
  `0.764377 → 0.761071`，低于 0.5 的帧两组相同，均为
  `[188,191,192,193,194,195,196]`；排除 169--181 的估计真值。
  显示中心最大差 3.349px；第 54 帧抽看新框仍跟随同一白车。
  关闭/开启 CSV SHA-256 分别为
  `d25afa06e0b19a476a752b1664e8f8b1af0ebf04b00cf5185c2e3986ba621603` /
  `c84be79af5bb60dbd06bc170dc771816815291971ee4f31fda739c558e817daf`。
- 并排视频用 `tools/render_display_comparison.cpp` 将 CSV 中的真正
  `display_bbox` 绘在原视频上，再由 `ffmpeg -c:v libx264 -preset veryfast
  -crf 23 -pix_fmt yuv420p` 转 MP4；左为关闭、右为开启。板端 `runs/`
  下 `edge-compare.mp4`、`handle-compare.mp4`、`car-compare.mp4`
  SHA-256 分别为
  `b29b5520c1ce08e2e2c2b54f2db58b62fb061e568b5043181478800dbc3bcf92`、
  `244da887edbd04c953c12aa210a8be76a6d4c557dad35504f6861c3e8f6f0e28`、
  `ce333a0b71b437e1187dad26654ad14fcc618999c5aa0aa66f734d3c8630736d`；
  均已在板端重算校验。原程序 `--output-video` 仍绘制 `control_bbox`，
  不能拿它评价显示框。逐帧运行日志和 session metadata 保留在同一
  `runs/` 目录；它们绑定此次 RKNN runtime、配置和每次运行输入。
- 结论边界：此候选在上述静止目标的**显示路径**上消除了整分钟抖动，
  并在这段运动车辆视频没有触发静止锚定。图像锚定是在视频像素不变时
  固定显示框，不修正 NanoTrack 追踪位置或伺服控制；边缘追踪框本身
  严重漂移说明不能把此候选称为追踪质量修复。还没有真实吊舱实时测试、
  人工观感反馈或生产 A8 部署；移动目标覆盖范围只有这段车辆视频。
  `display_static_anchor_enabled` 在代码默认关闭，须经协调集成和后续
  人工验证才能考虑作为实际吊舱配置。

### Handoff 更新

- 协调对话请消费 `76a2df65f75449155263027708ebd9d759438eff` 及本节
  源码归档、程序、配置、模型、视频和三组 CSV/并排视频哈希，决定是否
  将显示路径候选集成到最新基线。此前各版失败记录保留。
- 开发板验证对话可用本节隔离目录复核原生运行结果；生产交叉编译对话
  只能在协调者批准最新 board handoff 后消费该提交。`STATUS.md` 与
  `integration-contract.yaml` 仍由协调对话维护，本任务不修改。

## 2026-11-20T14:14:44+08:00 另一把手与相机移动边界

- 再次核对工作线 `codex/rk3588-display-box-stability@2e3c8f314d944240cee9d7622bb2984788a00f94`，
  工作树无预存修改；使用的 RK3588 模型和视频身份沿用上节重新核过的哈希。
  板端同版程序对左把手 `(1390,675,55,195)` 开关各运行完整 1800 帧。
  两组追踪状态、接受标记及追踪框逐帧一致，全部已接受 TRACKING；
  候选第 15--1799 帧保持 `STATIC_IMAGE_ANCHOR`。显示中心二阶差分
  p95 `3.64673 → 0px`，尺寸二阶差分 p99 `1.6777% → 0%`。
  板端关闭/开启 CSV SHA-256 分别为
  `17ae56549a584008a83b44fe194b1545aae7cb15373ae22294a71a4a1abbcd94` /
  `011a2c6ab60ca7324ccbfed731df6a0b1214b299c0bd13de222516c99b2f290f`，
  路径均在 `/home/neardi/Project/SOT/nt-display-stability-76a2df6/runs/`。
  命令与上节相同，仅换 `--bbox`、配置开关和输出名；运行日志、
  session metadata 同目录保留。本项证明第二只把手的逐帧指标，
  仍需抽看实际框与把手是否贴合。
- 对原 60 秒静止视频的局部像素网格作附加筛查：边缘 ROI、左把手、
  右把手的 4×4 分区最大平均灰度差分别为 `14.61`、`23.78`、
  `26.44`。每个 ROI 取初始化帧为参考，按算法原采样网格读取全部
  1800 帧；这些数字反映此视频的编码变化，不能推广为其他视频上界。
- 发现旧候选的实际风险：从该 4K 视频裁出 800×600 的边缘片段，
  前 30 帧相机静止，此后模拟画面每帧竖直平移 3px，至第 119 帧
  累计约 267px。合成视频位于本机
  `/tmp/display-v6/edge-camera-shift.mp4`，120 帧，SHA-256
  `e9dad2e5a73dff071e0eb4d6d270b71af61e9854ea41f6137e5687eca05534ef`。
  旧全 ROI 采样平均差即使到第 119 帧仍仅 `11.11`，低于硬编码的
  `14`；局部 4×4 分区差从第 35 帧的 `21.12` 上升到最高 `49.53`。
  因此旧图像锚定可能在真实相机沿边缘移动时留住旧显示位置，
  上节静止回放通过不能覆盖这个边界。
- 当前源码在同一采样网格上同时检查全 ROI 平均差和 4×4 分区最大
  平均差；分区超过 `32` 的持续变化将释放锚定，超过 `40` 时加快
  确认，单帧变化仍不释放。选择 `32` 只依据上述三处静止 ROI 与
  模拟平移的间隔，属于待板端验证候选。新增单测证明仅占 ROI 约
  5% 的局部变化可触发释放，间歇单帧变化不会累计；主机重建后
  CTest 23/23 PASS。用本机图像重放程序及既有板端追踪框筛查，
  相机移动视频第 14--41 帧锚定，第 42 帧退出，之后不再锚定。
  该重放的追踪框来自静止原视频，仅证明释放逻辑；不能称为真实
  相机移动跟踪或板端证据。
- 下一步将这次源码提交到本工作线，再以**该提交**在开发板原生编译，
  用三处静止 ROI、运动车辆和相机平移视频复测；特别核对平移后
  的显示框是否跟上图像，而非只核对锚定来源。当前最新修改尚未
  有 RK3588 原生证据，也未形成新的协调 Handoff。

## 2026-11-20T14:28:34+08:00 局部运动释放版的 RK3588 复测

- 实际消费本分支源码
  `9253f4b6cf0edec3e4a9fe9674bb976ed59d7295`；该提交只在
  `AdaptiveDisplayBox` 的图像锚定释放检测、单测及本进度文件改动，
  不修改追踪/控制/模板路径。Git archive 位于板端
  `/home/neardi/Project/SOT/nt-display-stability-9253f4b/assets/rk3588-display-stability-9253f4b.tar`，
  SHA-256 `0ffe7f1141179f5b6a311a6c6d5b7605fd50e89266b1a614697ff4b040c3181b`。
  隔离工作目录 `/home/neardi/Project/SOT/nt-display-stability-9253f4b`。
- 板端仍为 `neardi@192.168.1.106`、RK3588 LKD3588、Linux `6.1.84`、
  RKNN API `2.1.0` / driver `0.9.8`；模型取自上节相同目录，
  template/search/head/manifest 四项 SHA-256 再次核对，分别为
  `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`、
  `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`、
  `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`、
  `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`。
  原生 CMake / build / CTest 命令同上节；观察 23/23 PASS，
  `runs/ctest.log` SHA-256
  `dd4a1afe28f0131892c0e958c26387532377252f5e1172f6cb9090618997a10b`；
  `build-native/nanotrack_v3` SHA-256
  `a7d2789b61f6f7153338a9ff07ddb225bc8185b14f0c32d2e413ca46a9dcb183`。
  配置仍为候选 `3411ec9f9c19a8666e117e4ac82d8084f2b7a118e8850ea0b60c08c1277bca67`，
  关闭组 `adb7ace425fbbbb1e6f97463f53a1fb5d006238a1d0c12275381d5eed0c578b4`；
  初次合成回放因忘记在新目录生成关闭组配置而立即报错，补齐并核验
  与上一轮同 SHA 后重新运行；当前同名 `off.log` 是成功回放日志，
  首次报错只保留在本任务终端输出中。
- 使用静止 4K 视频 `8dea64988e3766a410d601a59ca14f49550d0d9c57a3beacbe9aeb78bdf4bd80`
  和同样三处 ROI，分别完整回放 1800 帧。边缘、左把手、右把手各
  1800 帧均保持已接受 TRACKING，`STATIC_IMAGE_ANCHOR` 均为第
  15--1799 帧；与上一版同 ROI 的追踪框、显示框、状态和来源
  逐帧完全相同，局部分区门没有在静止视频误释放。
  三处显示中心二阶差分 p95 分别为 `0px`，尺寸二阶差分 p99
  分别为 `0%`（从第 30 帧起）。本版 `runs/` 下边缘、左把手、
  右把手 CSV SHA-256 依次为
  `702ec093df3c5ac662d93fa9de4a85bb26825abab1a168bc0a0ef0f31954afd6`、
  `3e5a73bb78ba9aa03c30e54fd3675cbab4b8a601f42ff21fb5bc18140b444b8a`、
  `29eca3b6e8f80c95a0dc401c1e7967f4ed28c7351e02d4242858a639908d4cba`。
  左把手第 30、900、1790 帧新显示框始终围住同一把手；
  `runs/left-handle-9253f4b-compare.mp4` SHA-256
  `e8829711baa07f659de75a14ddc322a306796ceabb272fd58da258eabd8a0bc5`，
  1600×720、1800 帧，板端解码帧数和哈希已复核。边缘/右把手
  并排视频沿用上节同像素输出的已核验文件，本目录 `runs/` 下有
  指向它们的链接。
- 运动车辆仍用 canonical 视频
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`
  和 `(992,582,37,39)`，271 帧无静止锚定；与上一版程序的追踪框、
  显示框、状态、显示来源逐帧一致，因此前节的可见已接受 241 帧
  平均显示 IoU `0.761071` 仍适用。`runs/car-candidate.csv` SHA-256
  `3c62c07c55e786800af8f36e5628ca0778e4ad46b4f36134f0d1d17800fad08d`。
- 使用本机同一 4K 视频生成的**合成相机平移**边缘 800×600 片段，
  SHA-256 `e9dad2e5a73dff071e0eb4d6d270b71af61e9854ea41f6137e5687eca05534ef`，
  初始框 `(190,70,150,220)`；同一板端新程序的关闭/开启组
  追踪和控制框逐帧完全相同，均 120 帧 TRACKING。另用旧版板端
  程序 SHA-256
  `141b060e2f59d649d0c2b800a9c1ed2763edbe19dca001d8d821b5473e38cc18`
  在相同视频和开启配置回放：旧版第 15--119 帧一直锚定，
  新版第 15--41 帧锚定、第 42 帧释放，之后持续跟随追踪位置。
  三组 CSV SHA-256：关闭
  `771821b1cc922b8dd46a4c865f0df8cf7d6f13bac7444abf4f09c02bb355c9e5`、
  旧版开启 `ad0b922e74a91e95cc97b4dd02607740ec2e0de78192f4860b46ccbe31632868`、
  新版开启 `6232763584f1ccd78459903079f81d6397d40cd619167c6373afd61d93ec460a`。
  真正 `display_bbox` 的旧/新版并排视频为
  `runs/edge-shift-old-new-compare.mp4`，SHA-256
  `da2623d6ee7d817b2d35fdf53b5a3c05774cff405f4f41c6827c3929aa3d0dc1`，
  3200×1200、120 帧。第 60 帧画面中新版仍围住移动后的边缘拐点，
  旧版留在先前高度。该输入为人工裁切和平移，用来证伪旧门槛，
  不等同真实吊舱相机运动。
- 另一段合成右把手平移视频 SHA-256
  `19e62a27a55443bac8ccb7740e5f1726587c989906a6e0b9831835c321d15f91`，
  初始框 `(185,175,55,195)`。新版板端第 15--33 帧锚定，第 34 帧
  释放；120 帧中 TRACKING 115 帧、SUSPECTED 5 帧。
  `runs/handle-camera-shift-candidate.csv` SHA-256
  `1977258ad9ff5f4977dfcab64a07f236baf8bad34872894df0a9878263312432`。
  平移输入只验证锚定释放，没有该合成片段的独立目标真值，
  不宣称其后每帧跟踪正确。
- `runs/` 还保留每次运行的 `.log` 与 `.metadata.json`，供核对
  配置指纹、RKNN runtime、输入身份和逐帧状态；文件 SHA-256 可在
  板端 `sha256sum runs/*candidate.{log,metadata.json}` 复核。
  本轮仍只完成隔离开发板候选，未验证真实吊舱上位机观感或 A8 显示链，
  未改变协调契约或正式生产 handoff。

### Handoff 更新

- 最新待协调审查的源码改为
  `9253f4b6cf0edec3e4a9fe9674bb976ed59d7295`，替代本文件较早
  Handoff 所列 `76a2df6` 作为代码候选。请连同本节精确程序、配置、
  模型、输入、CSV 和并排视频哈希审查；静止视频优点与相机运动释放
  均需保留。其他任务要消费此结果须先核对提交和工件哈希。
- 是否集成到 `main`/`Debug` 或作为生产 A8 输入，由协调对话依最新
  `STATUS.md`、`integration-contract.yaml` 和人工观感反馈另行决定。
  真实吊舱运行与效果验收仍属人工边界，本任务不作已批准交付声明。

## 2026-11-20T14:31:34+08:00 A8 显示输出接线只读核对

- 只读核对交叉编译工作树
  `codex/rk3588-a8-cross-compile@3fffd207a62dfc87a25f3864311a9f2173663d77`；
  该工作树干净。实际读取文件
  `docs/deployments/rk3588-a8-recovery-consistency-20261120/native-lib_impl.cpp`，
  SHA-256 `0d0078aea3984f5f4abf67fbdd435745cff0dbd391e0cc55b653958fb41d19ab`。
  `set_frame_outputs()` 将 `result.output.display_bbox` 转为 `display_box`，
  写入 `track_box_result.rect[1]` 和 `rect_show`；`rect[0]` 独立使用
  `tracking_bbox` 或控制限幅框。这表明该 A8 适配源码具备本候选
  显示框接线，但没有据此声称当前吊舱二进制已包含本候选，也没有
  证明宿主库绘制时未覆盖 `rect_show`。
- Handoff：交叉编译对话若后续消费本候选，应在协调者批准的最新
  board handoff 上集成 `9253f4b` 的显示源码，核对 `rect_show` /
  `rect[1]` 在实际 A8 程序中的返回后行为，并保留 `rect[0]`
  控制语义。该适配文件仅被只读核对，未从另一工作树复制或修改。

## 2026-11-20T14:34:49+08:00 低纹理对照与当前边界

- 使用当前已提交的显示源码 `9253f4b6cf0edec3e4a9fe9674bb976ed59d7295`
  和同一 4K 静止视频
  `8dea64988e3766a410d601a59ca14f49550d0d9c57a3beacbe9aeb78bdf4bd80`。
  主机以 `ffmpeg` 解码首帧为 BGR24，按源码的约 64×64 采样网格
  计算灰度标准差：低纹理柜门 `(1710,370,220,200)` 为 `2.04`，
  边缘 `(990,70,150,220)` 为 `29.88`，左/右把手为 `53.50` / `50.35`。
  图像锚定初始化要求方差至少 `100`（标准差至少 `10`），
  所以该柜门 ROI 不满足初始纹理门，而三处有纹理 ROI 满足。
- 同一 RK3588 板端程序
  `a7d2789b61f6f7153338a9ff07ddb225bc8185b14f0c32d2e413ca46a9dcb183`
  对柜门 ROI 运行 `--max-frames 30`，实际 CSV 为 frame 0--30 共 31 行；
  仅初始化帧为已接受 TRACKING，其余 30 帧为 SUSPECTED，显示来源
  均无 `STATIC_IMAGE_ANCHOR`。`runs/cabinet-low-texture-candidate.csv`
  SHA-256 `de41a2eb2083e5a7810f99bbbcb9a10305753550eaf85e31f64b81e8010c70db`；
  `runs/cabinet-low-texture-candidate.metadata.json` SHA-256
  `f0d2b1e222ec8ebf6f3a462c88b3803b35578b37a4ae9aa98265cdba92ad0fc4`。
  本短回放证明低纹理选点没有触发锚定，但因为没有持续可靠追踪，
  不能用它评价低纹理目标的持续显示稳定性。
- Handoff 不变：最新代码候选仍为 `9253f4b`，本节仅补充边界证据。
  仍等待人工观看静止目标和相机移动并排视频的观感反馈；不将
  板端候选等同生产 A8 或真实吊舱验收。
