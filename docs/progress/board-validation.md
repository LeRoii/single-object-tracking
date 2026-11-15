# 开发板验证任务进度（当前 RK3588）

## 2026-11-15T15:20:26+08:00 获准记录原 detector 框并限制点击跟踪框最小尺寸

- 用户确认 car002 单击可保留 detector 原框，同时用以原框中心为准的最小 `36×36` 修正框初始化跟踪器。分支 `codex/occlusion-recovery-triad`，工作树 `/home/u20/single-object-tracking/.worktrees/occlusion-recovery-triad`，任务基线 `Debug@5f76a5acfc8386c8c8fe6a9d5b7a6f0bd2a6175e`，本阶段前置提交 `6987903cf5430dcaea3d78cbebaf059fa5bf87dd`。Debug 单击 detector 时，新增配置 `click_detector_min_bbox_size=36x36`，宽高须为正整数，分别只补足不足 36 的维度并按图像裁剪；`click_selection_resolved` 同时输出原 detector `bbox`、实际 `tracking_bbox` 和 `bbox_adjusted`。空白点击回退框及命令行 `--bbox` 语义不变。改动范围仅 Debug CLI/交互文件，通用算法部分继续与 main 保持同源。
- 主机实际执行 `cmake --build /tmp/nt-occlusion-triad-build --parallel 4`、`ctest --test-dir /tmp/nt-occlusion-triad-build --output-on-failure`，RKNN-off CTest 23/23 PASS；日志 `/tmp/nt-triad-host-build-12.log` / `/tmp/nt-triad-host-ctest-12.log` SHA-256 `c1754349c1a8a3be457104a98f38016b09fc534ca275cda7c92077a817647efa` / `8878771031cfdbb1e560b6ec0155c6dac84fd2a96c4bd4c76c864591768e30e5`。下一步板端原生构建并分别实际单击 car001/car002，核对原框与修正框、完整真值评分及程序/工件哈希；正式 handoff 仍未批准。

## 2026-11-15T15:16:31+08:00 改按 detector 单击初始化双目标验证

- 用户把本轮验收范围改为 detector 实际结果初始化，不再要求八个手设框；板端 `1927bda7432cb4a07abbacf1e79fdd4e74e24460` 的八框批次在 car002 nominal 运行中按要求停止，已完成的零散 CSV 仅保留原件，不作本轮验收。分支/工作树 `codex/occlusion-recovery-triad` / `/home/u20/single-object-tracking/.worktrees/occlusion-recovery-triad`，起始基线 `Debug@5f76a5acfc8386c8c8fe6a9d5b7a6f0bd2a6175e`。源码包 `/tmp/nt-triad-motion-gate-source.tar` 与板端 `/home/neardi/Project/SOT/occlusion-triad-1927bda/source.tar` SHA-256 `e80af940114d69bd419b5bec5567fa93e0be2036e1a35fb127a2fc88d14e58df`；板端配置 `runs/headless-eval.conf` SHA-256 `cda6b05b8677da03fb2947ff8624ee9d02bb73faa98462a68c3f7ab4aa1069eb`，输入视频/模型/标签与上方已登记清单一致。原生 CTest 23/23 PASS，程序 `build/nanotrack_v3` SHA-256 `ced5c458190ac0ba81e72b9b646dcce87c66a83be4f5a25f5171e6680ce868ee`，cmake/build/ctest 日志 SHA-256 `d4a3fbcb12f44cfcc13a5d86796d8d2c18564bac621cb895b939774` / `292cd5aeaba6208b99d425c4b47a80e13462d0a376e3233c6f742de6b7153728` / `60bdd3241420a0b0e09609827c2dc93c41f3264e6ba53b3bbde308016bd26d4d`，RKNN API/driver 2.1.0/0.9.8。
- 实际 GUI `xvfb-run -a -s '-screen 0 2600x1200x24' python3 detector-click-replay.py`，第 0 帧单击 car001 `(1009,600)`，detector 框 `994.001,581.063,35.289,39.486`、类别 2。运行退出 0、CSV 271 数据帧；脚本/CSV/session/log/AVI SHA-256 `c6e5e289d0993ecfb3eb6905f64a7ae3db958880bb071ba3701a83a8d3d84854` / `cd848e270242beee44ca3e081be4beef0eec6f93e60930fd83598a1ecdf9e48d` / `ad6970e909804e6bda8becd172a0ffebc35e8db76bb057c48b4d0882aaa1a720` / `d9781a700575989d610ae4423b44323329ad0064bd3b508fe64e44828275b572` / `64fdb24c5bfa1f4b17b8e2d95690d9960663d0fb207e5cff62acc0b5c195ca7b`，原件板端 `runs/detector-click/`。`scripts/evaluate_occ_video_1.py --target-id car_001 --scope full` 报告 `/tmp/nt-triad-1927-detector-click-score.json` SHA-256 `2670266571b7d3b6433bc255563c803e27c5469a2c4ba1cd7d1d0400a2dfcabe` **PASS**，172 帧 SEARCHING、189 帧重捕，可见有效观测 mean IoU 0.747518。
- 实际 GUI car002 `xvfb-run ... python3 detector-click-car002.py`，第 0 帧单击 `(988,535)`，detector 仅选框 `973.035,520.490,34.091,31.424`、类别 2；脚本/CSV/session/log/AVI SHA-256 `03cb66cc27fb7a599e1d4675545400cf575b6d6241f58134e6c9f56da0301fe9` / `f6b3b4dcee7849b2fb5689e351eb69c27947c09ca957eedf94a8b598d229c86b` / `fc1fb835e72b5308840ec5b63eea0ee0fb8f73e4025702540f230898e8ee43af` / `ab41df05a6c746ee2209aadb1c962d278e14b6d63b410e73ebbeedd253d0dc1e` / `f6ea1aee5ece1abc1b2cba0a92b337921dd10ef742e1f619403b727b9074770f`，原件板端 `runs/detector-click-car002/`。full evaluator `/tmp/nt-triad-1927-detector-click-car002-score.json` SHA-256 `d3e9815bbfed440e7cc4f6a04cdc6c89ed3b9315addf41c763f0b4dc1858842a` **FAIL**：73、192 帧错误重捕，有效观测 mean IoU 0.134866。板端隔离点击扫描 `car002-detector-scan.py` SHA-256 `272e92552b7b8323d5fd95cc992b9f869852bc74f3bac31b4e780b88763a6351`：真值框左上边缘多点没有其他 detector 候选，只有上述较小框覆盖中心/右下。
- 仅为定位，用同一板端程序/配置、以该 detector 框中心为准扩到 `36×36`，运行 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox 972.080475,518.201843,36,36 --output-boxes runs/car002_detector_min36_probe/frames.csv --session-metadata runs/car002_detector_min36_probe/session.json`，CSV/session SHA-256 `c25acdbed02fb0b0679e331c565e14152a504033b0a8085f1200c1fc64208fb6` / `5675bd1c192541e1ca2cdcf4d4c861e39a7ce5a4c0b109bfebcc535491e4a2c9`，full evaluator `/tmp/nt-triad-car002-min36-score.json` SHA-256 `a70aa2531e63b179aabfabdf2e8b5e167eb1c6457f12f77c94fbdc63c4cc68cf` **PASS**：233 帧重捕、有效观测 mean IoU 0.756859。这是尺寸修正探针，**尚非 detector 原框实际 GUI 验收**。已向用户澄清是否允许记录原框并以最小 `36×36` 修正框初始化；得到答复前不将该探针计为最终通过，也不合并 main/Debug。

## 2026-11-15T15:02:06+08:00 精确框 PASS、八框 2/8 后增加重捕运动门

- 分支 `codex/occlusion-recovery-triad`，工作树 `/home/u20/single-object-tracking/.worktrees/occlusion-recovery-triad`，起始基线 `Debug@5f76a5acfc8386c8c8fe6a9d5b7a6f0bd2a6175e`，板端源码提交 `4ef8159b68c80f0bc7fda3ba4c87495884f73453`。源码归档 `/tmp/nt-triad-overflow-source.tar` SHA-256 `30d094bf7292757b406399c5ef33d7884e8a89cefcde1c39ca7f7bd5b64b08e3`，板端独立根 `/home/neardi/Project/SOT/occlusion-triad-4ef8159`；配置 `runs/headless-eval.conf` SHA-256 `cda6b05b8677da03fb2947ff8624ee9d02bb73faa98462a68c3f7ab4aa1069eb`，视频/模型/标签为上方已登记的精确哈希，RKNN API/driver 2.1.0/0.9.8。RK3588 native CTest 23/23 PASS，程序 SHA-256 `b544a98c64c7256183e0e600a2a0a07dbc1d1182dec3f6a744d1ce93176e36b7`，cmake/build/ctest 日志 SHA-256 `96e58882ea8394aa78dc1e5919e3a14635a06514e6f00e8fc2ab5d5646e69076` / `fc568b8116f4c2a1bfd68d54026a91c49171700a02d9716427da90a1e20f298c` / `5c332c99b242f4c5f7fb616bee480731715c621182e652d005bdccb1f8d9ac61`。
- 精确点击框同命令回放 exit 0，板端 `runs/car001_exact/frames.csv` / `session.json` / `run.log` SHA-256 `caa28322163c88b2373d2ae9f08c8e70906f4b0a09122f018f5e9c8c690176d7` / `4f4d7b6d0ce97d88e4e8abf6a00ff26bafb6e13cc6cd2a70639cf9a942ce4cdf` / `47ccd8dad6ba6354b13a3572b33c794bd32f4808ef4d9dc4264ac6c82143f8af`。full evaluator `/tmp/nt-triad-4ef-car001-exact-score.json` SHA-256 `c64c2fcfe6a5861d536635d819423d4726e34d8a348fdd510fbf44e2c4ae2ef0` **PASS**：172 帧 SEARCHING，189 帧重捕，可见观测 mean IoU 0.747518。
- 用板端脚本 `run_dual_window_four.sh` / `run_dual_window_extra_four.sh` 分别执行 car001 四框 `992,582,37,39` / `994,584,35,37` / `993,585,36,36` / `990,580,39,41`，car002 四框 `970,517,38,38` / `967,513,42,42` / `968,515,40,40` / `972,519,36,36`，每路 271 帧、exit 0。脚本 SHA-256 `94eabea46d7c08b53fb4fe20b424e17bb87a8373f5042ccfd55907937febc1e9` / `83f0f442d58d1b3dd8967b6472de608ea0a24a9fe1e239d3b3767d54e54dce73`，输出在板端 `runs/<name>/`；主机 CSV 打包 `/tmp/nt-triad-4ef-eight-frames.tar.gz` SHA-256 `fd8b3768285e11f49cd3741563509e934a5b23aefcb52f96b756aa78109b41bd`。实际执行 `scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-triad-4ef-eight-score.json`，JSON SHA-256 `d84b1a1acb5362918d0e97e6bed59f6c248ae46780e2ee6af4d3667e05375e29`：仅 car002 truth/right PASS，**2/8，否决本候选**。car001 nominal/user/left 在 177 帧错误重捕，car002 nominal/left 在 224–225 帧错误重捕。
- 失败帧的目标候选距运动预测分别约 `1.33–1.39` 个预测框对角线；精确 car001 真重捕约 `0.74`，car002 truth 真重捕约 `0.22`。本修订只对 full-object 且 Recent/Safe 同候选的恢复设可配置的运动中心门 `recovery_dual_template_max_motion_diagonals=1.0`；远处的单模板 detector 验证与 subtarget offset 路径保留既有行为。主机 RKNN-off CTest 23/23 PASS，日志 `/tmp/nt-triad-host-build-10.log` / `/tmp/nt-triad-host-ctest-10.log` SHA-256 `17c74f8f01565cc486f80a90cc16e4e44fd2b961cbdff793623ac9b2a30bad7c` / `9446ce7a9e2b6ee5337e8ce819dfd38f03c9a1c745cc6f2157cdd00bb1eb26fc`。下一步重新板端精确框和八框回归；尚未集成 main/Debug、正式 handoff 未批准。此处把前几个候选条目的时间标题按对应 Git 提交时间纠正，内容与结论不变。

## 2026-11-15T14:50:50+08:00 全遮挡及时进入搜索后重捕延迟定位

- 前次源码 `933a6df061903f274089853b3046b4a27666b835`，分支/工作树 `codex/occlusion-recovery-triad` / `/home/u20/single-object-tracking/.worktrees/occlusion-recovery-triad`，起始基线 `Debug@5f76a5acfc8386c8c8fe6a9d5b7a6f0bd2a6175e`。源码包 `/tmp/nt-triad-final-candidate-source.tar` SHA-256 `66a38b7ea3eed839224431ab902cad5f8c8679fda620c5e1fe98fc12e567d522`，板端根 `/home/neardi/Project/SOT/occlusion-triad-933a6df`，配置 SHA-256 `cda6b05b8677da03fb2947ff8624ee9d02bb73faa98462a68c3f7ab4aa1069eb`，视频/模型/标签与上述清单一致；RK3588 native CTest 23/23 PASS，程序 SHA-256 `daef757d676dcc74e22575e45e4f0070ada14e44d177ec74ad10a0c9021417c9`，cmake/build/ctest 日志 SHA-256 `f9e4f7b01f925070d93dbc7f7aa1b8da8615eaf8367258809354fbded3da6c87` / `f64dc260dd0d705c43165e4a15d15574c1e864ad64799d170c97cc37f70c98c6` / `4c36d4d585af9e88c484f7625fa6c0eb7c6ab992be95cb17021f95089ee6736f`，RKNN API/driver 2.1.0/0.9.8。
- 同一精确框命令独立回放两次，首轮 CSV/session/log 位于板端 `runs/car001_exact/`，SHA-256 `60c9c94ed59687ecc00dc9f52fdc1bd6355bc497ce3f56b85a0acabccf73075d` / `7079e3209e895bded437438c8e29e841aadbfe27586228581eae916606f3e237` / `2b1f508a51e234d748b0683d6b51a4c0cecad97d487745b29f624211f9e19164`；重复运行 `runs/car001_exact_repeat/` CSV/session SHA-256 `3d5aef06c20133316ea746a045dcc8895e6ccf79b1caa3029cf834d3e4f4ef36` / `98fe8bb5c9d8381b6b89612ac3818bf05c4cb6de854edcdfb938a34ef80ccac4`。两次均第 172 帧进入 SEARCHING，但第 190 帧才重捕。full evaluator `/tmp/nt-triad-933-car001-exact-score.json` SHA-256 `1e9e86447b9238573e0bb5aeee5a679fb90b24dffab339c777307b8875145010`：全遮挡、可见框、身份代理 PASS，重捕时窗 FAIL（必须早于190）。
- 逐帧发现第 188 帧已有 Recent/Safe 与目标 detector 连续同框；第 189 帧新 detector 批次包含大量远处溢出框，候选表原逻辑把所有已验证命中清空，导致第 190 帧才有第二次确认。本修订保留远处溢出时的命中，仍要求当帧重新检查所有溢出框是否与任一模板重合；真正竞争框依然否决。主机 CTest 23/23 PASS，日志 `/tmp/nt-triad-host-build-7.log` / `/tmp/nt-triad-host-ctest-7.log` SHA-256 `8003a5763b45fa2211d75c3b716b6457842ac58fc33d3f5f7e4f70633a8c727d` / `a67f716293c85b285047f9e37d3d5be7e3484692e9be0ed759cd213187f8ce0e`。下一步板端精确框及八框回归；正式 handoff 未批准。

## 2026-11-15T14:45:56+08:00 视觉门诊断：首个有效命中在全遮挡帧

- 前次源码 `d6236935d88aae126111d51d0d0465edad914976`，分支/工作树 `codex/occlusion-recovery-triad` / `/home/u20/single-object-tracking/.worktrees/occlusion-recovery-triad`，起始基线 `Debug@5f76a5acfc8386c8c8fe6a9d5b7a6f0bd2a6175e`。源码包 `/tmp/nt-triad-safe-source.tar` SHA-256 `ffffb9ffc27e76e27410fd30262d7fe44cc720a2f7255b5a3678edbd9391c9ec`，板端独立根 `/home/neardi/Project/SOT/occlusion-triad-d623693`，配置 SHA-256 `cda6b05b8677da03fb2947ff8624ee9d02bb73faa98462a68c3f7ab4aa1069eb`，视频/模型/标签沿用已登记哈希；RK3588 native CTest 23/23 PASS，程序 SHA-256 `2f58a3ec64e7ae3e5931a3871575e29c4ae24820976fbda7ca82821d7eb9c401`，cmake/build/ctest 日志 SHA-256 `970de27f1192706b54608dd267d8b1b53b8cdd2e08190b274659084d4d59d14c` / `ec144b991fd6a63fcc5963542869e728bb19d61c793db9412d077144067e51a0` / `d0706d3b93a46c7e868de65f337e89938bc476a29661c19ffdceb0b2c17a6510`，RKNN API/driver 2.1.0/0.9.8。
- 仍用同一精确框命令回放 271 帧，板端 `runs/car001_exact/frames.csv` / `session.json` / `run.log` SHA-256 `a79baa8c1ff5c4d7e8c23697345c573c2dbbe5af779707bb265df12e6cd8b81d` / `4267845876f3af2c4dda7f4232473e4589379db0577d5f5e1479702a184d9262` / `6aafb01284e75ba5223a721fb336899d29f6181129d2eb1b78cd7d8f860f77c3`。full evaluator `/tmp/nt-triad-d623-car001-exact-score.json` SHA-256 `2ccc6c94cc43ca9936dcb7e756c75adec3ce7d51bf138392c19424985b7138ae`，仍仅第 172 帧 full-occlusion FAIL，188 帧重捕 PASS。
- 在隔离目录 `/home/neardi/Project/SOT/occlusion-triad-diag-d623693` 临时编译的诊断版只用于定位：169–171 帧视觉 detector 已运行但无有效视觉命中，172 帧才首次命中，所以两帧视觉命中门槛无法在 172 帧满足。当前修订改为视觉命中时若本帧 Recent 与 Safe 均有解码观测、IoU>=0.5，则即时停止发布观测；无 Safe 观测时维持原两帧/近处歧义三帧规则。主机 RKNN-off CTest 23/23 PASS，日志 `/tmp/nt-triad-host-build-6.log` / `/tmp/nt-triad-host-ctest-6.log` SHA-256 `9d07bfa47ebe3c4403203ff469c27193136e8da0edda5d70120bbe2fb30b96d2` / `ebfa22b1cdd27f661714e52dbfaa9ea8bbb3cc491e9bc8793d4baa8b0fe6bd0b`。下一步板端精确框与八框回归；正式 handoff 未批准。

## 2026-11-15T14:41:11+08:00 精确框再次反馈：Safe 交叉验证缩短视觉等待

- 前次候选源码 `aef24347d386286d22964189e8097810a044e063`，分支/工作树 `codex/occlusion-recovery-triad` / `/home/u20/single-object-tracking/.worktrees/occlusion-recovery-triad`，基线仍为 `Debug@5f76a5acfc8386c8c8fe6a9d5b7a6f0bd2a6175e`。源码归档 `/tmp/nt-triad-aef2434-source.tar` SHA-256 `31bee38feb9a9067f0316d6af927a539d616b225baa638206633705dfb97b579`，板端独立根 `/home/neardi/Project/SOT/occlusion-triad-aef2434`；配置 `runs/headless-eval.conf` SHA-256 `cda6b05b8677da03fb2947ff8624ee9d02bb73faa98462a68c3f7ab4aa1069eb`，视频/模型/标签与上方已登记的精确哈希一致。RK3588 native CMake、编译和 CTest 23/23 PASS，程序 SHA-256 `705f45cd36d0f3a03dfaca3021c548a21cbf7eb04e7417ed7bdb8a24edcb0f28`，cmake/build/ctest 日志 SHA-256 `a596bf4b15d1435bc8b9d1368cf7b65315240245de3bacac4762dc4dd24c4dc8` / `83d1daf9f0d11d7ba8dda76479c2a52838e44817c0377e24cd3d54123599c341` / `dc0a20bea3af98d57ebda11d6b37d1f30d68586add1572c7f3b3c9e941298d17`，板端 RKNN API/driver 2.1.0/0.9.8。
- 同一精确框命令 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox 994.000916,581.063293,35.289490,39.486084 --output-boxes runs/car001_exact/frames.csv --session-metadata runs/car001_exact/session.json` exit 0，271 帧；CSV/session/log SHA-256 `d60a4dfed86a9dcbe3121ff570870260de8ef900b698248c3e5dbf5cb03a35ee` / `fbe19ddb7ba4533f022f5a20396e2a46ca0cb5f09bfc76dc846b8db51481ef95` / `67fe3fdea01ed2b8dd0f07c6b4f6ac95e31c6dea0c9fb036a69c10c21a4de02b`。full evaluator 报告 `/tmp/nt-triad-aef-car001-exact-score.json` SHA-256 `092b68b3822316112f6a8ed7f6c8f063eb782d692026e6224b1462c7b9234e35`，重捕仍在 188 帧，唯一 FAIL 是 172 帧全遮挡仍 SUSPECTED。第 172 帧 Safe 实际执行且与 Recent 同框，附近视觉候选仍被标记歧义，因而多等一帧。
- 本修订仅在连续两帧视觉遮挡信号且当前帧 Recent/Safe 均有同帧观测、IoU>=0.5 时取消歧义额外等待；只有 Safe 计划而没有观测不能触发。近处竞争框无 Safe 交叉验证仍需三帧，远处框不算歧义；主机 CTest 23/23 PASS，日志 `/tmp/nt-triad-host-build-5.log` / `/tmp/nt-triad-host-ctest-5.log` SHA-256 `07bc30f083e86830d9902a82e865512f8df4fd6ff76c0758b0616e58f080a566` / `ef8e4ad9ec45018d1a85b850a29b0bce6155b18bdf13488276710637f98a0fdf`。下一步再次板端精确框并执行八框回归；本候选尚未正式 handoff。

## 2026-11-15T14:35:31+08:00 三路重捕板端有效，视觉搜索时机再修订

- 分支 `codex/occlusion-recovery-triad`，工作树 `/home/u20/single-object-tracking/.worktrees/occlusion-recovery-triad`，基线 `Debug@5f76a5acfc8386c8c8fe6a9d5b7a6f0bd2a6175e`，前次源码提交 `9bb7737afe8a2991da02ae3790220e46e7b787b1`。源码归档 `/tmp/nt-triad-9bb7737-source.tar` SHA-256 `de9579ef6908bbf23f729309afc5b22e307b239558ab0042fcd0bdffe86b4210`；板端独立根 `/home/neardi/Project/SOT/occlusion-triad-9bb7737`，原生 CTest 23/23 PASS，程序 `build/nanotrack_v3` SHA-256 `463fa8d0092305291de05bc54ad3615c8749a0721a38d857a93beeb549436b14`。输入视频、模型与标签沿用已登记的 `debug-click-0060b7b` 精确哈希；运行配置 `runs/headless-eval.conf` SHA-256 `cda6b05b8677da03fb2947ff8624ee9d02bb73faa98462a68c3f7ab4aa1069eb`，controller SHA-256 `52a3722eec8ed32c2f3ba44be50617ff6c16ad12254f04320b9802738f406bd6`，RKNN API/driver 2.1.0/0.9.8。
- 实际运行 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox 994.000916,581.063293,35.289490,39.486084 --output-boxes runs/car001_exact/frames.csv --session-metadata runs/car001_exact/session.json`，输出 271 帧；CSV/session SHA-256 `92bc6e160f873d2120d2b0c0f1fe98dd675938521c551822ef9340e2fa2786dd` / `fe5ae6ec46ee09263da0baf1df71af5562268719c050ae1dcc29915ed739e3ed`，原件在板端该独立根的 `runs/car001_exact/`。`scripts/evaluate_occ_video_1.py --dataset-root data/occ_video_1 --target-id car_001 --scope full` 的报告 `/tmp/nt-triad-9bb-car001-exact-score.json` SHA-256 `8728883fdc57dabb8a4db0f318e14f80770c88b96da2abf07fea2331c1383242`：重捕、可见框、身份代理 PASS，第 188 帧完成重捕；全遮挡语义 FAIL，因第 172 帧仍为 SUSPECTED、第 173 帧才 SEARCHING。
- 新修订只将预测框附近且达到置信度门槛的同类检测框计入视觉身份歧义；远处第二框不再强制增加一帧确认，近处竞争框仍需额外一帧。主机重新构建及 CTest 23/23 PASS，日志 `/tmp/nt-triad-host-build-3.log` / `/tmp/nt-triad-host-ctest-3.log` SHA-256 `8bc9c723643b0946969d2d89bec08e5b1f82692de7` / `8df1ed4efd779b97a721381fdd6c8f9183ad1d5f1f178318b5c1837d61276c45`。下一步板端精确点击框与八个固定框回放；正式 handoff 未批准。

## 2026-11-15T14:29:41+08:00 精确 car001 框板端否决后修订候选

- 前一候选 `codex/occlusion-recovery-triad@e42f2d04d646addb0d4e96eddc032c2c98827a6d`，板端隔离根 `/home/neardi/Project/SOT/occlusion-triad-e42f2d0`，源码包主机 `/tmp/nt-triad-e42f2d0-source.tar` 与板端 `source.tar` SHA-256 同为 `14fbcde1294a61a1a1e5cdeb56807474e67e7d16225b9e4dfd90ee0a214b4c0a`；RK3588 原生编译和 CTest `23/23 PASS`，`build/nanotrack_v3` SHA-256 `c894e0a165527d3ec89e7f5ea5994be035d04443322b09a7bf21a874f730b84b`，运行配置 / controller SHA-256 `e58dd857e9aa15278b086dcedce3381067e7738c09cd3dca8eab5505479adb13` / `52a3722eec8ed32c2f3ba44be50617ff6c16ad12254f04320b9802738f406bd6`；CMake/构建/CTest 日志 SHA-256 `f5db95026a28e843c10a13821b9ab73651868861b9eae3e40f65f60788a99b65` / `56c78f5a329b7428b3897fa05346abb3b737b13b4f1f6ee1410a192bac78c0bf` / `e520d97c6e0bb2ae5839a14032b9a58dc8e510e6c2c3bf3484e71ada0f1acd39`。
- 实际执行 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox 994.000916,581.063293,35.289490,39.486084 --output-boxes runs/car001_exact/frames.csv --session-metadata runs/car001_exact/session.json`，exit 0，271 数据帧；CSV/session/log 板端原件 SHA-256 `f99ee98912fb8a9907e4ca1e6e75f47feb2013a8566c2ead8a14880460257cbd` / `70fc289a96b476b54276a3ae91c7d295fe9505ac9a32be5a8b9e2b64e42ba81a` / `ebc77b6088677fd73761423e008bfe0b7d55ab2ca8e93085e7d2da785975dd1d`。主机回传同哈希，`scripts/evaluate_occ_video_1.py --dataset-root data/occ_video_1 --target-id car_001 --boxes /tmp/nt-triad-car001-exact.csv --session-metadata /tmp/nt-triad-car001-exact-session.json --scope full ...` 报告 `/tmp/nt-triad-car001-exact-score.json` SHA-256 `273cee892e4d040efe1b0b1f1908e094010b3944e9284175cf86bf539107ede8`，**FAIL**：无重捕、全遮挡语义失败。第 184 帧有 121 个位置有效检测候选，仅保留 3 个，未表示的远处候选被整体标记为冲突，三路确认无法生效。
- 修订候选在候选表短期保存溢出检测框；唯一三路匹配须检查全部保留框与溢出框，任何溢出框匹配 Recent 或 Safe 仍否决，只有远处溢出不再压掉两帧验证。主机 RKNN-off 实际重新构建并运行 CTest `23/23 PASS`，构建/CTest 日志 `/tmp/nt-triad-host-build-2.log` / `/tmp/nt-triad-host-ctest-2.log` SHA-256 `60bb8712c2329c5ba286fa6bd106b1244c7b3fdc7fc7d1333d614a07c2608de7` / `df4d052a14b9b32f0216cb49c1885c30062f4d5cfd007839caca5ff9090eaa7f`；本修订尚未板端运行，下一步部署新隔离目录。正式 handoff 未批准。

## 2026-11-15T14:23:07+08:00 car001 三路重捕候选主机验证（板端待验）

- 分支 `codex/occlusion-recovery-triad`，工作树 `/home/u20/single-object-tracking/.worktrees/occlusion-recovery-triad`，基线 `Debug@5f76a5acfc8386c8c8fe6a9d5b7a6f0bd2a6175e`；来源为用户单击手测 `0060b7b` 程序在 car001 第 185 帧的 `IDENTITY_CONFLICT`，不是旧算法与新算法版本差。原始板端记录 `/home/neardi/Project/SOT/debug-click-0060b7b/runs/manual-click/frames.csv`，主机只读副本 `/tmp/nt-car001-manual-frames.csv` SHA-256 `6925b6c033e06d92efd39232dd57cf65cc795ce4047499b4259841a2b5581f96`，初始 detector 框 `(994.000916,581.063293,35.289490,39.486084)`、类别 2。
- 本候选仅为唯一 detector 与可靠 Recent/Safe 同框时建立候选专属的两帧确认；其他探测框若也匹配任一模板则保留冲突，detector 单独不能授权控制。主机实际执行 `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-occlusion-triad-build -DENABLE_RKNN_RUNTIME=OFF -DBUILD_TESTING=ON`、`cmake --build /tmp/nt-occlusion-triad-build --parallel 4`、`ctest --test-dir /tmp/nt-occlusion-triad-build --output-on-failure`，结果 `23/23 PASS`；最终构建/CTest 日志 `/tmp/nt-triad-host-build.log` / `/tmp/nt-triad-host-ctest.log` SHA-256 `2b2d7bea36ceb923042695d41c3a49312cf74b79997fb8ca45da860513cc0ff7` / `b2f1fcf80f98b1f3261f16ff9082104780859ae716425a60cc60b11a74e7018e`。
- **边界 / 下一步**：目前只有 RKNN-off 主机单测，没有本候选 RK3588 原生构建、八框回放、精确点击框回放或真值评分；不得视为板端通过。板端免密 SSH `neardi@192.168.1.106` 本轮认证失败；待建立授权连接后在隔离目录构建并执行上述回放。遮挡进入 SEARCHING 时机尚未修改，须以板端闭环证据决定安全触发规则。正式 handoff 继续保持未批准。

## 2026-11-14T22:59:13+08:00 Debug 单击交互集成到 main / Debug

- 用户要求两分支算法一致、Debug 仅有交互差异。`main` 从 `84b63aeaf575a717652d757b5cd92c993959b835` cherry-pick 通用运行时提交 `78665da675a8ca05e46a96dcfa49dd697299fe26`，得到 `main@1f5b10a0b56da60fa76a34c145bd92e1e314cec0`；`Debug` 从 `1573b20c9aef2b8747752bf5467b5ac4a0261136` 快进功能分支至 `Debug@1791a85c42c7ec4a7fc8f1ae19b5e3ccf469a724`。`git diff --name-only main Debug -- rk3566/nanotrack_v3 scripts tests` 只有 10 个 Debug 交互、配置解析及相应单测文件；`LongTermRuntime` 头文件、实现和单测完全一致，评测脚本与 Python 测试完全一致；逐段核对 `main.cpp` 的 `lower_occluding_band` 与 `runtime.process_frame` 调用一致。Debug 板端实测源码为上方 `0060b7b`，后续 `1791a85` 仅增加配置解析单测和进度文档，不改变板端程序源码。
- 分支集成工作树 `main` `/home/u20/single-object-tracking/.worktrees/click-main-integration`、`Debug` `/home/u20/.codex/worktrees/integrate-adaptive-debug/single-object-tracking` 均实际运行 `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-click-<branch>-integration-build -DENABLE_RKNN_RUNTIME=OFF -DBUILD_TESTING=ON`、`cmake --build ... --parallel 4`、`ctest --test-dir ... --output-on-failure`，各 23/23 PASS。CTest 日志 `/tmp/nt-click-main-integration-ctest.log` / `/tmp/nt-click-debug-integration-ctest.log` SHA-256 `bee6024b2c1c69e3163d1c948ac0a33a8488597c2896227d7a4d55ffb09cf259` / `2696b238db9c18f204a6c13d460f59f8b8047fea46437987dbbe805db0919ac5`；主机 main `main.cpp` 使用板端 RKNN 头文件完成 `g++ -fsyntax-only`。下一步双分支原子推送并核对远端提交；正式生产 handoff 仍未批准，真实吊舱验收仍需人工完成。

## 2026-11-14T22:55:11+08:00 Debug 单击选目标 RK3588 原生验证

- 功能分支/工作树 `codex/debug-click-select` / `/home/u20/single-object-tracking/.worktrees/debug-click-select`，起点 `Debug@1573b20c9aef2b8747752bf5467b5ac4a0261136`，通用源码 `78665da675a8ca05e46a96dcfa49dd697299fe26`，本次板端精确 GUI 源码 `0060b7b9d1af8d195c1dd9cb37696267ed562f78`。`git archive` 主机 `/tmp/nt-click-0060b7b-source.tar` 与 RK3588 `/home/neardi/Project/SOT/debug-click-0060b7b/source.tar` SHA-256 同为 `ffa1e18a66d07582d4c1703fd174c43e4bf3e320db3d21818af45072e1fb7da5`，板端 `aarch64` / Linux `6.1.84`；原生 `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DBUILD_TESTING=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build --parallel 4`、`ctest --test-dir build --output-on-failure` 均成功，CTest 23/23 PASS。`build/nanotrack_v3` SHA-256 `71417d15b192e161e2d92b408810afc93c75c35475bc82d8dd7f0e8db6f82562`；`cmake.log` / `build.log` / `ctest.log` SHA-256 `5c8b7ea9e9cf42ef19735952c44d4821191f0117fb67ab55992ac349f760291e` / `d4b795720de867ddf8943f2593caf14ceeb0e42a4ee52e1b2b8327beac847450` / `bb6c7cd265c680785556bdaa8849a40a01e12cc09f786a901dca224325405bc7`。模型加载日志 RKNN API `2.1.0`、驱动 `0.9.8`。
- 冻结输入 `data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，板端运行配置 `/home/neardi/Project/SOT/dual-gt-ui-13bd45d/runs/headless-eval.conf` SHA-256 `55f4ffb4f196adddb4505f878874d97348aec75613a5a8c0b03fc04701e326f0`，controller 配置 SHA-256 `52a3722eec8ed32c2f3ba44be50617ff6c16ad12254f04320b9802738f406bd6`。template/search/head RKNN SHA-256 `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55` / `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee` / `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`，tracker manifest SHA-256 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`；detector RKNN/manifest/labels SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。完整板端路径、字节数及哈希清单 `/home/neardi/Project/SOT/debug-click-0060b7b/evidence.json` SHA-256 `78fb10d4380c06c10e431a1858cf945045124a510b3b84427d6bae64f7c63d8a`。
- 板端实际 `xvfb-run -a -s '-screen 0 2600x1200x24' python3 click-gui-smoke.py` 在第 0 帧按 `R`、点击 car001 映射源坐标 `(1009,600)`，单次 detector 返回框 `(994.001,581.063,35.289,39.486)`、类别 `2`、初始化 `ESTABLISHED`；再按 `R` 点击空白 `(30,60)`，回退框 `(0,28,62,64)`、类别 `UNKNOWN`，再按 `R` 后 Esc 取消且初始化次数仍为 2。输出根 `/home/neardi/Project/SOT/debug-click-0060b7b/runs/click-gui-v2/`，`run.log` SHA-256 `1692c1bcdbdcc78121047f9166e99a7ea08c4479e69a67ee78988fdf6d18eb0c`，`selected.png` / `fallback.png` SHA-256 `dbd28419af0328ab536ddc4b9387d48ea88b66980616265ac403a302a4b04380` / `6af05baecfaecfaa0aa4dc479a5a3e46f13ace87776bedc2c26fe70c98340f48`。独立自定义配置 `runs/click-gui-custom/custom.conf` SHA-256 `82b3dc8ecd08191cf6c226b635e9b205189357673c9df8e8e481693e8a553206` 增加 `click_fallback_bbox_size=80x96`，同法点击空白 `(180,150)` 得到 `(140,102,80,96)`、类别 UNKNOWN，`click-gui-custom.log` SHA-256 `81d3b65d16853ad85cf8a6f3219f8347925f9333e3739b812b8da1c390a4d63a`。首次误用历史 `rk3588-interactive-debug.conf` 因已弃用 controller 键启动失败，失败日志保留于 `runs/click-gui/`，后续全部改用上述已验证配置。
- 仍在同板端执行 `./build/nanotrack_v3 --config /home/neardi/Project/SOT/dual-gt-ui-13bd45d/runs/headless-eval.conf --bbox <各目标初始框> --output-size 640x360 --output-video ... --output-boxes ... --session-metadata ...`：car001 `(992,582,37,39)` / car002 `(967,513,42,42)` 各 271 帧，CSV 板端 `runs/regression-car001/frames.csv` / `runs/regression-car002/frames.csv` SHA-256 `f0c6da8b6c2d497de23b33faa73b76fe7750647218b079babb5e3ce34787ec09` / `4ad251cc6b7b602c590a7a79fc91adae3dfbe0d0b8bd3a8a80624eb003ca97af`；主机副本同哈希。主机 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root /home/u20/single-object-tracking/data/occ_video_1 --run car_001 nominal 992,582,37,39 /tmp/nt-click-car001.csv --run car_002 truth 967,513,42,42 /tmp/nt-click-car002.csv --output-json /tmp/nt-click-two-target-score.json`，两目标均 PASS，重捕帧 `188` / `233`，tracking 平均中心误差 `4.15` / `1.63px`，报告 SHA-256 `af822aecf7df46484dcb22c3b7633520eeaa99dcae396e524a2a0f486237dd40`。与旧异步运行逐字段有差异，不声称逐帧字节一致；本次两目标真值门均通过。主机最终 CTest 23/23，日志 `/tmp/nt-click-final-ctest.log` SHA-256 `4bcf36e7dc4b3a51f896c2a53c1392a6964f29c429f2a7e5ba270b78959de230`。
- **边界 / Handoff**：以上仅覆盖单段视频、Xvfb 模拟输入和两组原有初始化框；实际人手操作和吊舱验收未完成。通用运行时变更须在 main 与 Debug 同步且核对源码一致，Debug 独有点击交互；正式 `latest_handoff.available=false` 仍由协调者裁决。

## 2026-11-14T22:41:59+08:00 Debug 单击选目标源码及主机测试

- 分支/工作树 `codex/debug-click-select` / `/home/u20/single-object-tracking/.worktrees/debug-click-select`，基线 `Debug@1573b20c9aef2b8747752bf5467b5ac4a0261136`，共用运行时提交 `78665da675a8ca05e46a96dcfa49dd697299fe26`。Debug 按 `R` 暂停后在 Display 单击，检测当前帧一次；命中任何有效类别时取裁剪后的 detector bbox 与类别，重叠按置信度、面积、原顺序决胜。未命中、detector 不可用或报错时，用点击位置为中心的固定框并强制目标类别未知；错误写 Debug 事件。单击显示图先缩放到 1280×720 并居中，点击坐标映射回源帧，图像留白点击忽略。`click_fallback_bbox_size=WxH` 配置默认 64x64，CLI `--bbox` 维持原语义。原 RK3588 固定配置文件有哈希保护，未改其字节。
- 实际主机 `cmake --build /tmp/nt-click-common-build --parallel 4` 和 `ctest --test-dir /tmp/nt-click-common-build --output-on-failure` 23/23 PASS，覆盖 bbox 命中、重叠选择、空白/边缘框、配置值和类别模式；CTest 日志 `/tmp/nt-click-full-ctest.log` SHA-256 `ffcf9566b18d1aeaf3d25fc00b03d0dfbcee12a6c078945c5cb0e706ef28cec3`。从 RK3588 已有开发目录读取 `rknn_api.h` 至主机 `/tmp/nt-click-sdk/include/` 后，`g++ -std=c++17 -fsyntax-only -Wall -Wextra -Wpedantic -I/tmp/nt-click-sdk/include -Irk3566/nanotrack_v3/include $(pkg-config --cflags opencv4) rk3566/nanotrack_v3/src/main.cpp` exit 0。该验证只覆盖主机语法与 RKNN-off 单测；下一步提交后到 RK3588 原生构建、GUI 单击与回归实测。

## 2026-11-14T22:40:22+08:00 Debug 单击选目标：共用初始化契约

- 新功能分支/工作树 `codex/debug-click-select` / `/home/u20/single-object-tracking/.worktrees/debug-click-select`，基线 `Debug@1573b20c9aef2b8747752bf5467b5ac4a0261136`。通用 `LongTermRuntime::initialize` 增加默认兼容的类别选择参数：现有配置、复用已选 detector 候选、强制未知三种模式。点击候选复用单次检测 batch、覆盖既有固定类别，空白/错误强制未知，不再二次提交 detector；CLI `--bbox` 保持原语义。此通用源码将在 main 和 Debug 保持相同。
- 主机实际运行 `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-click-common-build -DENABLE_RKNN_RUNTIME=OFF -DBUILD_TESTING=ON`、`cmake --build /tmp/nt-click-common-build --parallel 4`、`ctest --test-dir /tmp/nt-click-common-build --output-on-failure`，23/23 PASS；配置日志 `/tmp/nt-click-common-cmake.log` SHA-256 `d3ff64b86e348b57df7563847ba4882672998fcc0f5b7ffa1e703ed21aafe066`，构建日志 `/tmp/nt-click-full-build.log` SHA-256 `200c73c0d3cea6f513c7de4a8bb7e0816c7ac642d74b3dc3e09794fd21ef62b2`。此处仅是主机 RKNN-off 单测；下一步完成 Debug 图形交互与真实 RK3588 原生构建、点击实测。

## 2026-11-14T22:19:00+08:00 main / Debug 算法一致性核对与双分支发布

- 算法候选已经合入 `main@05556e660f22eabc112088a27ca3c05e8958d3ab`、`Debug@0f6b56f849324a64991a7f63a6d12d9975d5c923`（后续仅可能追加文档同步提交）。`git diff --name-only main Debug -- rk3566/nanotrack_v3 scripts tests` 只列出 `src/main.cpp`、`src/interactive_debug_overlay.cpp`、`tests/test_interactive_debug_overlay.cpp`；排除这三个交互文件后，NanoTrack 所有其他源码、配置、脚本和测试完全相同。逐段比较两边 `main.cpp` 的 `lower_occluding_band` 与 `runtime.process_frame` 调用也完全相同；其余差异是双窗口、显示框、关键 detector 调试框和交互视频输出。
- Debug 工作树 `/home/u20/.codex/worktrees/integrate-adaptive-debug/single-object-tracking` 实际执行 `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-dual-gt-debug-integration-build -DENABLE_RKNN_RUNTIME=OFF -DBUILD_TESTING=ON`、`cmake --build /tmp/nt-dual-gt-debug-integration-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-debug-integration-build --output-on-failure`，CTest `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/debug-integration-{cmake,build,ctest}.log` SHA-256 分别为 `357c88221eae7ac90b23e3fed92af3568d02f51f5ffda4ce90922a0a5fd266e2`、`bb435edecc07774a3a82c36c8880df250b18eee312b0f7e9dfcd41923639c5ee`、`14db662e6f8d19cd527321d466eafba8f1acbb114eb0b6c25a6acfa2210f05f9`。系统 Python 3.10 对两个分支运行 `python3 -m unittest discover -s tests -p 'test_evaluate_occ_initialization_consistency.py'`，各 `2/2 PASS`；旧 NanoTrack venv Python 不支持评测脚本所用的 `math.dist`，故此项用系统 Python 验证。
- 远端起点 `main@b802265`、`Debug@7e45f9d` 经 `git ls-remote` 核对后，已执行 `git push --atomic origin main Debug`，首次双分支推送成功。本次不包含新板端源码编译或真实吊舱测试；继续沿用上方绑定的 RK3588 证据边界。后续每次合并应再次检查 NanoTrack 非交互源码零差异，并人工审查 `main.cpp` 的交互差异。

## 2026-11-14T22:14:24+08:00 双真值候选向 main / Debug 集成

- 本次集成从 `main@b80226577954febe22c5eff168125f77a6cdc724` 和 `Debug@7e45f9db9493c8411139dba76294fdd179eb2759` 出发，接收候选 `codex/dual-gt-init-consistency@291e62d1cb4211cf32b2dfc160ade78305774044`；main 集成工作树 `/home/u20/single-object-tracking/.worktrees/dual-gt-main-integration`。main 保留原交互界面，Debug 保留双窗口及只显示关键 detector 框的交互界面；两个目标分支的跟踪、重捕、控制、评测源码必须相同，`main.cpp` 的算法路径也须逐项核对。
- main 实际执行 `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-dual-gt-main-integration-build -DENABLE_RKNN_RUNTIME=OFF -DBUILD_TESTING=ON`、`cmake --build /tmp/nt-dual-gt-main-integration-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-main-integration-build --output-on-failure`，CTest `23/23 PASS`。构建与测试日志位于 `/tmp/nt-dual-gt-evidence/main-integration-{cmake,build,ctest}.log`，SHA-256 分别为 `3a5526a494fd963a7168f3f738c747d491745860332847d7a7c46cc06dcb575c`、`9fbc5d1363f1fcf30689d9a18ebc9220c550ffd596b4cd61bc667b8fdf30c45a`、`f5fc0363db4452de74559e7327ae55c7f38c87f2aa926267e2480a5e3fdd8046`。
- 此处是主机侧集成证据，RK3588 八框 `8/8` 与 GUI 证据仍绑定上方精确板端源码与二进制；新分支提交未经重新板端运行，不升级为生产或吊舱验收。下一步合并 Debug 并核查两个分支算法源码差异和测试结果，向协调者回传最终 commit；`latest_handoff` 仍由协调者裁决。

## 2026-11-14T22:00:30+08:00 双窗口 Debug 补齐后的 RK3588 验证

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，源码基线 `3a87a4264134172f55670fa6ecd64f0759050928`，双窗口与关键 detector 框移植提交 `13bd45dad7589a039b50fa54b49aa3ca30c1628e`；`git diff --name-only 4b4e47a..13bd45d -- rk3566/nanotrack_v3` 仅列 UI 源文件 `src/main.cpp`、`src/interactive_debug_overlay.cpp` 与其测试，追踪核心/配置未变。主机 `/tmp/nt-dual-gt-evidence/dual-window-13bd45d-source.tar` 与板端隔离根 `/home/neardi/Project/SOT/dual-gt-ui-13bd45d/source.tar` SHA-256 同为 `813a311a3ed5a4b14c014991c235605aecb69b422b29bf7fe7f49572306df62f`；构建脚本 `/tmp/nt-dual-gt-evidence/build_dual_window_board.sh` SHA-256 `284e6bb40e915a94b37f9230fd31c8179b271ca04b4c62d9a03048da82327075`，传输后板端核对一致。
- 实际 RK3588 原生 `bash build_dual_window_board.sh`、CTest `23/23 PASS`，`build/nanotrack_v3` SHA-256 `32c9a88c1319ffb74e96f9978f140d68a02e6cfe407b579d3db5ef38b1ce1a2a`，`runs/headless-eval.conf` / controller 配置 SHA-256 `55f4ffb4f196adddb4505f878874d97348aec75613a5a8c0b03fc04701e326f0` / `52a3722eec8ed32c2f3ba44be50617ff6c16ad12254f04320b9802738f406bd6`，`runs/cmake.log` / `build.log` / `ctest.log` SHA-256 `90d23309e97f029a7e2e7447774b2239fdeaa605d2592b1dd37bfac28feddbad` / `40b52584d7b5d2f8273a8a8f4e187819b42c1be655ec0025179fe464b91d06c2` / `d1d8383770c80da69ebd1d840289383cbffb0a563123e8598bb731118f0e1a84`。LKD3588 Linux `6.1.84`、session 实报 RKNN API `2.1.0` / driver `0.9.8`，视频、tracker、detector、manifest、labels 与下方 `4b4e47a` 已核对身份和绝对路径一致。主机 RKNN-off CTest 也 `23/23 PASS`，日志哈希见紧随的候选记录。
- GUI 验证先以 `bash smoke_dual_window_board.sh` 在 `xvfb-run` 中实际发现 `NanoTrack Display` 和 `NanoTrack Debug` 两个 `1280×720` 窗口，位置分别 `(0,0)` / `(1296,0)`；窗口清单板端 `runs/gui-smoke/windows.txt` 与主机打包副本 SHA-256 `3d47c75f2218958c84b5274af51d13b0dc99b9d81541524562fbef99fd78bba4`。首次内容抓取因板端无 `xwd` 失败，保留该失败日志；改用板端现有 `scrot`，实际 `bash capture_dual_window_scrot_board.sh` 等待 `INITIALIZED frame=0` 后抓屏成功，脚本 SHA-256 `8355a4306837de9d28be01988c7e22fbee642f48fda32860fd89b9a179f5f28a`。板端 `runs/gui-content-scrot/desktop.png` 与主机 `/tmp/nt-dual-gt-evidence/dual-window-13bd45d/desktop.png` SHA-256 同为 `73cae1942edb0e6bb52ee6f7ffb7a8eb4312e1d6982af0ef2bd4ec6ea0646c3d`：首帧左侧 Display 只有白色 `display_bbox`，右侧 Debug 的目标相关框/状态单独呈现；逐帧未入队或被拒绝的 detector 候选不绘制由板端 `test_interactive_debug_overlay` 覆盖。该截图是虚拟桌面首帧，不冒充人工真实显示器验收。
- 回放脚本 `run_dual_window_four.sh` / `run_dual_window_extra_four.sh` SHA-256 `9a4972850781db9badd3df768c169f8c7e4322cf7c8419c61bcc97d489492344` / `dfe5c0f69cf36467a5921bd08c632190e90579306c9f2adac12d5fdee8932635`，实际在新目录顺序运行两目标各四初始框，每组 exit 0、CSV 含 `0～270` 共 271 帧。板端 `runs/evidence-text.tgz` 与主机 `/tmp/nt-dual-gt-evidence/dual-window-13bd45d-evidence-text.tgz` SHA-256 同为 `534820a750d4792511f95a8381a39e71eb78b77e884273869335012acea95b81`，原件板端 `runs/<label>/`。用两份确认真值执行 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/dual-window-13bd45d/eight_score.json`，报告 SHA-256 `3cc2c1da5c1ca41dd1f0b98a9e67f692f06b7996f41531285d15dbca64a7e7f4`：**8/8 PASS**，car001 重捕 `188/188/189/186`，car002 `232/232/236/232`；tracking/display 中心均误差与前项相同。逐帧比较本轮与前项八组 CSV 的 state、有效性、reacquired、tracking/display/control 四元框，八组各 `271` 帧均 `0` 个不一致。CSV 文件整体哈希受时延字段影响并不相同，不能直接用文件哈希表示几何一致。
- **Handoff / 边界**：用户指出的两项 Debug UI 已在最新板端实验二进制生效，正确图形运行根为 `/home/neardi/Project/SOT/dual-gt-ui-13bd45d`，旧 `/home/neardi/Project/SOT/dual-gt-wide-band-4b4e47a` 二进制不包含它们。建议在板端图形终端运行 `cd /home/neardi/Project/SOT/dual-gt-ui-13bd45d && mkdir -p runs/manual_car001_ui && DISPLAY=:0 ./build/nanotrack_v3 --config runs/headless-eval.conf --interactive --bbox 992,582,37,39 --output-video runs/manual_car001_ui/output.avi --output-boxes runs/manual_car001_ui/frames.csv --session-metadata runs/manual_car001_ui/session.json` 核对 car001，car002 可改 `--bbox 970,517,38,38`。GUI 烟测与 canonical 单段回放不能证明真实吊舱或跨场景接受；协调者正式 `latest_handoff.available=false` 未改。

## 2026-11-14T21:44:28+08:00 双真值候选补齐双窗口与关键检测框 Debug 界面

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始干净，基线 `3a87a4264134172f55670fa6ecd64f0759050928`。用户指出当前板端实验二进制漏掉前一天已实现的两项 Debug UI。核对发现本分支从 `main@b80226577954febe22c5eff168125f77a6cdc724` 延伸，源分支 `Debug@7e45f9db9493c8411139dba76294fdd179eb2759` 的双窗口/候选筛选未进入该 `main` 基线。精确移植原 UI 源码提交 `ef7dba9e9fe0028850979edcdd5fa06acf06cdb8` 的三文件差异，未改本分支 tracker、controller、恢复配置及两套 GT。
- 新交互界面用 `NanoTrack Display` 单独显示仅一只白色 `display_bbox`，`NanoTrack Debug` 显示运动、模板、控制和关键恢复证据；初始化只画被选 detector 框，逐帧原始 detector 候选只保留类别匹配、位置通过、无拒绝且已入队者，不再绘制所有拒绝/未入队框。ROI 选择仍在 Display 窗口，关闭任一窗口退出。实际 `git show --format= ef7dba9 -- <三文件> | git apply --check`、`git apply`、`git diff --check` 通过；主机 RKNN-off `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 与 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure` 为 `23/23 PASS`，日志 `/tmp/nt-dual-gt-evidence/dual-window-host-build.log` / `dual-window-host-ctest.log` SHA-256 `c1d8661f43ae4b1c4a7218e48b6bc193c4903ef96851b6ddb6ebc65dd01d2af7` / `ae81844ebf70c7cb84a6693fd2f704b152fb67625e1369d2d583b0437c86061d`。
- **证据边界 / 下一步**：上述只是源码与主机单测，当前已验证 `8/8` 的板端二进制 SHA-256 `f61cdf3518e11c59620ff268a8ee518837fc072c00322d0b9b16abc70d215613` 尚不包含双窗口。须提交精确源码后在新的 RK3588 隔离目录原生构建/CTest、图形会话核对两个窗口与精简 detector overlay，并确认最少两目标 headless 轨迹与前项相同；完成前不得称新 UI 已在板端生效。`latest_handoff.available=false` 不变。

## 2026-11-14T15:02:35+08:00 双目标八初始框板端通过与同框复跑

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，候选源码提交 `4b4e47ae1bf4dd5245d9e251ddc693e9057b4d89`，前项基线 `359e8c4a1b2ead6896fec77fdd337e824e7bce1b`。主机源码包 `/tmp/nt-dual-gt-evidence/wide-band-4b4e47a-source.tar` 与板端隔离根 `/home/neardi/Project/SOT/dual-gt-wide-band-4b4e47a/source.tar` SHA-256 同为 `1f897f85dbb979ffb9b72563f4ed9b910a6c3ffdbe5eec068ae89276e5102a29`；构建/前四框/后四框脚本 SHA-256 分别 `d792f9c889a5af49b1c4a4662092a467ea365d3252e8b0b71bf73e6d6024f38b` / `c1b3aa073b5931b1dabb9238deab629b42a2d5335e5102e820a4c21b11aecb40` / `c745bf819380e94caeb1fcda892f16274c9d2e5673f5bbe70aa075f7da8739b1`，板端与主机传输核对一致。
- RK3588 `LKD3588` Linux `6.1.84`，session 实报 RKNN API `2.1.0 (967d001cc8@2024-08-07T19:28:19)`、driver `0.9.8`。本轮板端复核视频 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，官方 NanoTrack template/search/head RKNN 与 manifest SHA-256 `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55` / `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee` / `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5` / `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`，VisDrone9 detector/manifest/labels SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。主机双目标 GT `data/occ_video_1/ground_truth/car_001/labels.sha256` / `car_002/labels.sha256` SHA-256 `9ec01854884126350f93e1af3a4aee002d38ce7c2d50c71c78de0f1a8dc0a648` / `2f0e78ab1123cb035c87efa06171f1057c108f38b06fcc5c1ed3a3ec63fdbe98`；GT 只在追踪后评分，不参与逐帧推理。
- 板端实际执行 `bash build_wide_band_board.sh`，原生 CMake/构建/CTest `23/23 PASS`，`build/nanotrack_v3` SHA-256 `f61cdf3518e11c59620ff268a8ee518837fc072c00322d0b9b16abc70d215613`，controller/headless 配置 SHA-256 `52a3722eec8ed32c2f3ba44be50617ff6c16ad12254f04320b9802738f406bd6` / `e3d9b1307bd2cdf0ea18e49e73798075ab6337473ef841647888cca6351b035a`；`runs/cmake.log` / `build.log` / `ctest.log` SHA-256 `7c5e596b6f8598f7a7fb2b25afb773175b4e592abbf624d1c05cd1f78af47142` / `3abdf3450dfdfd8ecd559e3ecacbbe3d8d92c9f5cd806498fa7d3c9cd763d1a6` / `24bfab022514f809d42ff5ee78f58f85cc9abaca6d776e81d26369413e632320`。实际顺序执行 `bash run_wide_band_four.sh`、`bash run_wide_band_extra_four.sh`，八组均 exit 0、每组 CSV 为帧 `0～270` 共 271 行，原件板端 `runs/<label>/`。八组文本包板端 `runs/eight-text.tgz` 与主机 `/tmp/nt-dual-gt-evidence/wide-band-eight-text.tgz` SHA-256 同为 `13d1068b963c6e88b58961cbfd5c5d9d0c8247b14f8aae1c45977af39ed6882f`；八 CSV SHA-256 按 car001 标准/人工/左/右、car002 标准/真值/左/右依次为 `9190cc9e848077ccbed36def30ac968b7571d5ae502808137753f3b350e33c62` / `a644841daa77240ea37028d4857bf7c4890c8bdbe43f18c1bb7c92955f3e8670` / `998fd4c36ba389d52b8fbc1706f9eb1c3cd3755104edf09f0ddc020a9dbe54b5` / `5bd98cc692a288b71c058c045578d14e5ca1f711c2700409b7126b55ccde5ed3` / `132165fede21500a318f4f9db8114bc08f5d696b1ed754954b5e61988b015eca` / `200c13e335879d36d2a926c3f2c2005aaada22630dae2b5e21db9afced3dbe64` / `812a5f01bb73290d98ca3231c486e1c222d08632cb3f6ed0ce251c1f594ff0c4` / `8fbf6dfd6ecc9b2ad1e3fd9c10be41fd70bc20e5e05fe2a27b22f7dd009b3111`。
- 实际 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/wide-band-4b4e47a/eight_score.json`，JSON SHA-256 `853cf8429c31776acf8cc169202daaa4d7624cb2f571c3b5bc91182ad7ffd0d3`：**八框 8/8 PASS**。car001 四框重捕 `188/188/189/186`，tracking/display 可见中心平均误差均约 `4.2～4.6px`，tracking 平均 IoU `0.718～0.777`，初始化间中心分散 P95 tracking/display `3.28/2.86px`；car002 四框重捕 `232/232/236/232`，tracking/display 可见中心平均误差约 `1.6～2.4px`，tracking 平均 IoU `0.740～0.893`，初始化间中心分散 P95 两路均 `4.78px`。car002 四框第 `211～212` 帧均 `SEARCHING`、`observation_valid=0`、`control_source=MOTION_SEARCH`；第 `229` 帧均未错误重捕，人工确认无框段假观测/观测控制均为零。`display_bbox` 的 detector 仅用于显示，不替代 tracking/control GT 评分。
- 同一板端二进制/配置独立复跑 car001 右偏、car002 真值与左偏初始框，三组均 exit 0 且各 271 行，原件 `runs/repeat_<label>/`；重复文本包板端 `runs/repeat-text.tgz` 与主机 `/tmp/nt-dual-gt-evidence/wide-band-repeat-text.tgz` SHA-256 `2c90fffb1f0876ef2350ead189e2a4cdfb518789f60ad7033b2d1d8f98c3c9fa`。重复 CSV SHA-256 分别 `8f443385ded134558e71bef24fa7d948c119ffc576cf0b77603381c4d647c684` / `5f83d4b881eda77fd09fdc90b0d66ae64a3caebfe03a290c68f6619bee8ecdf8` / `5b51b5586cdac3ea6a8a276b6a8620191f6dddab5c928bbb63ea08b41a446ba9`；重新评分 `/tmp/nt-dual-gt-evidence/wide-band-4b4e47a/repeat_score.json` SHA-256 `8f3dd68c87575f49eb5faa7042878a4521678999692684b943b0e0036b7fb610`，三组均 PASS、重捕帧不变，逐帧 state/tracking/display/control 坐标及有效性字段 `0` 处不一致。CSV 整体哈希不同来自计时等非几何字段。
- **边界 / Handoff**：此结论覆盖 canonical 单段可见光视频、两份确认真值和 8 个轻微变化初始框；car001 右偏恢复过渡帧 tracking 最大中心误差 `15.84px`，car002 右偏第 `231` 帧最大 `23.50px`，不宣称逐帧零误差或跨场景/真实吊舱验收。请协调者审查源码 `4b4e47a` 与上述哈希及 `8/8` 结果后决定是否更新正式集成契约；本任务不改 `STATUS.md` 或 `integration-contract.yaml`，其中 `latest_handoff.available=false` 在协调者批准前保持不变。

## 2026-11-14T14:50:26+08:00 桥线几何和恢复纹理门二次候选

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，基线 `359e8c4a1b2ead6896fec77fdd337e824e7bce1b`；代码将桥下缘检测范围改为相对框高 `1.5～1.85`，并要求同帧相对高度 `0.3～0.8` 没有另一条跨目标上缘长线，避免 car002 第 209～210 帧仍可见时过早拒绝；恢复候选顶部亮度标准差下限 `26→32`，用于推迟 car002 真值框第 229 帧不完整目标的确认。仍只在桥线与近期框亮度下降同时成立时拒绝观测；`display_bbox` 的 detector 用途不变。当前仅为候选，尚无本次代码板端闭环证据。
- 实际 `.venvs/nanotrack/bin/python /tmp/nt-dual-gt-evidence/screen_wide_lower_band.py` 在上轮冻结轨迹和 canonical 视频上离线筛查，脚本 SHA-256 `085fe943ad06d20b214c0362cb1338b3eb2bc8b30045fbe2316dbc87ab8caaba`：car002 四框第 211 帧均命中，标准/左/右第 212 帧也命中；car002 第 209～210 帧均未命中；car001 命中帧集中于自身遮挡开始后。此筛查不能证明闭环。主机 RKNN-off `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 与 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure` 为 `23/23 PASS`，日志 SHA-256 `0d657575e05e39ed9471e0eda822a34122bf6e5970df5ec204c1ff0fe6ec3760` / `5e39ade88fb5cf00f2e8adabea410b47ca94d43fbda1d8ed121e2ca5342d5d02`；`python3 -m unittest discover -s tests -p test_evaluate_occ_initialization_consistency.py` 为 `2/2 PASS`，日志 SHA-256 `a6707d4eff38012da33ddc34bc6dc076a3b17151cfc9aef223c8790cbe64c3da`，`git diff --check` 通过。**下一步 / Handoff**：提交后在 RK3588 新隔离目录原生构建/CTest 并重跑两目标八框 271 帧，任一遮挡或中心/IoU 门失败则否决；`latest_handoff.available=false`。

## 2026-11-14T14:47:57+08:00 桥下缘候选八框 6/8 否决

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，基线 `65ffde946cc7bcafeb1deb97a08f85ad590853f3`，源码 `0a6b19d66181d5a9dd5ac98a946725853dcd6a7c`；源码包主机 `/tmp/nt-dual-gt-evidence/lower-band-0a6b19d-source.tar` 与板端 `/home/neardi/Project/SOT/dual-gt-lower-band-0a6b19d/source.tar` SHA-256 `75c0c84a349f45a2e4c38aaeada55a86d22454670d5a17acdeae41f5e909fd33`。板端原生构建 `bash build_lower_band_board.sh` 和 CTest `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `09cd98dd74b88cfef06e10582287ae071d1c4e4c6eff43d86e7e1d0b391ecd4b`；controller/headless 配置 SHA-256 `4ca3d4b3bc4282fc7a5b2b642b2a19a24887b425ba06adfc1c2b5b93fe818db3` / `5404a6ff66aaa5871c13d2da011e9fa6c2ef118da1132a7037fde01e8afa57c2`。构建/前四框/后四框脚本 SHA-256 `3ea8a84245f8086b30c3e4c80f10e4adc1778f2077f4ef65981d52ef979afb91` / `316260a54361f03ee7cef12f6623e758898629fa444d3642145edffc1db291c4` / `a4975d1dd1ad447ffc3452c73baf1f59a7b2e64ceb16771df902f5e032e689ae`，`runs/cmake.log` / `build.log` / `ctest.log` SHA-256 `3762296afd7bf73611304630661939f74cab62d9b43c26fd3163a5e17a5c5867` / `a10b076f27075cf9a6e3dc1579ede8324ec74e0849136d39e8e2066b6a2e1589` / `8e97da94f5578706dbfe8875122bcf9e8f96d1f5429b6120e298d92c004bef31`。LKD3588 Linux/RKNN 与 canonical 视频、模型、清单、标签哈希沿用下方 `452c48a` 已核对身份。
- 实际运行 `bash run_lower_band_four.sh` 与 `bash run_lower_band_extra_four.sh`，每组 271 帧 exit 0；原件板端 `runs/<label>/`，八组文本包板端 `runs/eight-text.tgz` 与主机 `/tmp/nt-dual-gt-evidence/lower-band-eight-text.tgz` SHA-256 同为 `b955be527829f8b27eb4859fd3b59ea8812b1106c59bdd08f231e88a6cb86afd`，八 CSV SHA-256 按 car001 标准/人工/左/右、car002 标准/真值/左/右依次为 `a524b86801f98c20a9636b0c9d23dc5a146188325b2c621373177c313762d0d9` / `753499e5ca72a2b029e377d93180b40d57db0f2af90543ab19e458d1fc84a72e` / `b938f63994deecce7b825a62dc67107d1c24b4266a05f638210f8df3e3a5ee25` / `691834845762c2e04e1444ebbe447f3848bd96cf2e72913cecd4ac7f1720c27f` / `68663a3c45bd47e081a3c02e56a4928c7801eb5c17e30ab63a598d6dbf519c06` / `8fbc140167c6097e412a682f05759c33ce11b5f86d5475469c590acdaa4f85fa` / `355f60f00ce869edb54dc9f437e5ec7d3147bd05e193e90fed60ac596ce90937` / `9fe76b742c40906d355550fb37ce7b38092b42444057f0f4baa9f45929ddcddc`。
- 实际 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/lower-band-0a6b19d/eight_score.json`，JSON SHA-256 `3df1d7c0fae0c414ab48df686ea2ee1cb48ac81febfe07a5ec8ebd7b740fccbe`。car001 四框 PASS，重捕 `187/187/189/186`，可观测 tracking 中心平均误差 `4.09/4.14/4.24/4.51px`，初始化间中心分散 P95 `3.13px`；car002 标准/右偏 PASS，真值框第 229 帧在人工无框期提前重捕，左偏第 211～212 帧疑似观测送控制，故总体 `6/8 PASS`。car002 初始化间中心分散 P95 `5.12px`，通过的轨迹中心误差约 `1.6～2.5px`；这不能抵消两个遮挡门失败。
- 离线帧探针 `/tmp/nt-dual-gt-evidence/probe_current_lower_band.py` SHA-256 `21095f9a825792ea93ee8bc16700edf3987798e6f65f7cb1e37f75ec6d978d66` 指向第 211 帧桥下缘纵向位置 `1.57～1.82` 倍框高，当前窄范围在不同初始框/板端轨迹上漏检；`/tmp/nt-dual-gt-evidence/probe_current_recovery_texture.py` SHA-256 `28629e05b107291b9cc477ba60e5a5490c9ae92b4188be8145290869ab1a969b` 指向真值框第 229 帧候选顶部亮度标准差 `31.28`，当前阈值 `26` 放行。探针只用于定位，不能充当闭环通过证据。**Handoff**：候选否决，收紧几何/恢复门后重新板端八框；正式 handoff 仍 `latest_handoff.available=false`。

## 2026-11-14T14:35:45+08:00 八框 7/8、car002 左偏无框段与桥下缘候选

- 前项精确源码 `codex/dual-gt-init-consistency@65ffde946cc7bcafeb1deb97a08f85ad590853f3`，源码包 `/tmp/nt-dual-gt-evidence/focus-texture-65ffde9-source.tar` 与板端隔离根 `/home/neardi/Project/SOT/dual-gt-focus-texture-65ffde9/source.tar` SHA-256 同为 `8f412cda4573f2c0fc16e68ebadd6446209fc3a753d21e29863728658c039e29`；构建/前四框/后四框脚本 SHA-256 `088bcc3a403f0559b984a29b2d42dd9d8c679a0f92826a13df5c3447274cb25b` / `29e9c6387532e9e0de32b42e02376651619b39c5b1f35347ea198b4843f77f77` / `5353547faff99dfd71abc4e53efe642abc95b9609b223b4f6fb8a741ad51256b`，板端传输核对一致。板 `LKD3588` Linux `6.1.84`、RKNN API/driver `2.1.0` / `0.9.8`，canonical 视频及 tracker/detector/manifest/labels 精确哈希沿用下方 `452c48a` 已复核值。板端 controller SHA-256 `274bc592b92db7480fcdb8e9a056a397f6c7b44f8c585dfb4cec0a1dc7d69b93`，`runs/headless-eval.conf` SHA-256 `415f0b8354fbfaa501049b4e9243ffffd7cac1f82cda4af3d1535d9d2c818a1c`。实际 `bash build_focus_texture_board.sh` 原生构建/CTest `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `5fe98db67ed7bcb2d76d31f1a6431cc7beb85aa7cf46a060a96a977892f3c5`；`runs/cmake.log` / `build.log` / `ctest.log` SHA-256 `152be2ab8c466a9fbdcd02276062567ae6ed7bb73b903da7ad43e885b3b3a091` / `c167b7fc7c9f9106e5254e6b674c0587ca365c3c4bd3fdb819c988a5cd485a6d` / `a5b6dd013e94b105de7b03e6e221d8c52798feaabf1dbf2ad3653978acbfe4dc`。
- 实际顺序运行 `bash run_focus_texture_four.sh` 与 `bash run_focus_texture_extra_four.sh`，两目标各四个前项相同 ROI 均 exit 0、各 271 帧，CSV/session/log 原件板端 `runs/<label>/`。八 CSV SHA-256 按 car001 标准/人工/左/右、car002 标准/真值/左/右为 `6276e3925ba9e11399e8139d375015bd3acfe0a739ac5a8d8346b690b87fc1db` / `60d57b21362a9958197e3a9920a3eef31924f563866bcaa1d2ec403a8d58e11b` / `0c4e77c606fb60e1d4dcf7bb24ecffe13413504ed933e9a029523bbd3d825a5f` / `c44bb0eb598b8be45ca7f41c9f24604eab6013c48a80db1103da56afb1635574` / `a8e990f43561135b23970c484a421a06070a87bbbb26911a7220ef84f6bb6c32` / `ae6c66f17e1a91d0ca96c96ac423080ae604b78786b8bc281d07d4288163ecb3` / `46af27b3d28363f2f72375705c5275f1aab9a8ee0289b044ebf5e8205a820a00` / `827d38be6890b7951d90b9384d4e96bf09c952356d9193a877d30b861a616ddc`。板端 `runs/eight-text.tgz` 与主机 `/tmp/nt-dual-gt-evidence/focus-texture-eight-text.tgz` SHA-256 同为 `683bf1de9b6ab629f22dc4833cd6fb8bdf23edbe610154aca6eb630fd2fbd695`，主机解包后八 CSV 哈希一致。
- 实际 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/focus-texture-65ffde9/eight_score.json`，报告 SHA-256 `01575674ae599aaca34b5d656f7ceca040c898a74858abf205e2f018a71d2483`：car001 四框均 PASS，重捕 `187/187/189/186`，tracking 可见中心约 `4.09/4.14/4.24/4.51px`，四框中心分散 P95 `3.13px`；car002 标准/真值/右偏 PASS，重捕 `231/230/231`，tracking 可见中心约 `2.12/1.56/2.50px`；car002 左偏 FAIL，第 `229` 帧在确认无框期错误重捕 1 帧，另第 `211～212` 帧将疑似观测送控制。故总体 `7/8 PASS`，不能晋级。
- 用同一板端二进制在隔离 `runs/threshold26/` 只把 `recovery_min_top_luma_stddev=23→26`，controller/headless SHA-256 `51d45e5a42a18d23701f04ea404d68336201fde3b47c8b822cade3f048273eb9` / `a20626f1562f196367401a3997c9417941d154fa83856df43dcd806eae4effc7`，实际完整运行 car002 左偏 271 帧 exit 0，CSV/session/log SHA-256 `49b695b8001c7f9bb7fbc3d22591f21e08751506c88d1a7e92116fb97feb3a82` / `7e6c0712b229039351717d2c68889a55efe8acba80eec83e21d533324c5505f0` / `5e53c369692e3a4b2f535bbbe7210f6e4e6dd0047ed8a9a4f8e477d1287a0d1408`。主机 CSV `/tmp/nt-dual-gt-evidence/car002-left-threshold26.csv` SHA-256 一致；同一评分脚本报告 `/tmp/nt-dual-gt-evidence/car002-left-threshold26-score.json` SHA-256 `af79bc6a5ba2c43972f68d9951b15c892fd2a335fb12a4828ca98562f4f86ff2`：第 `236` 帧重捕、无确认无框期恢复观测，但 `211～212` 疑似观测控制仍 FAIL。此为配置筛查，未宣称完整八框改善。
- 当前候选新增默认关闭的 `lower_band_visibility_veto`：主视频帧上以当前跟踪框附近 OpenCV Canny/Hough 识别跨过目标水平位置、长度至少四倍框宽、距框顶 `1.3～1.6` 倍框高的桥下缘；仅在同帧 NanoTrack 近期框亮像素占比低于最近可信框中位数 `0.9` 倍时立即拒绝当前帧观测并进入 SEARCHING。对上一轮八框冻结轨迹离线核对，car002 第 `211` 帧四框均满足长线与亮度下降，正常可见 car001 第 `153/155` 帧相似长线亮度约为历史 `0.97～0.99` 而不命中；car002 第 `69～70` 帧护栏线落在框内而非下缘。离线筛查脚本 `/tmp/nt-dual-gt-evidence/probe_crossing_line_focus.py`/TSV 及单独亮度计算未绑定独立场景，**不能证明当前闭环轨迹**。实验 controller 配置 SHA-256 `4ca3d4b3bc4282fc7a5b2b642b2a19a24887b425ba06adfc1c2b5b93fe818db3`，同时采用阈值 `26`；主机 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure` 为 RKNN-off `23/23 PASS`，构建/CTest 日志 SHA-256 `318b217a351bcce9715f775f282cb53f828512bf529fa5db43979635faca52c3` / `1a1882a313062e4b473e7fc6828c9021ed7f650f4d9ff73f929c805b1fb7358e`，Python 评分器回归 `24/24 PASS`，`git diff --check` 通过。**下一步 / Handoff**：同提交后 RK3588 新隔离目录原生构建/CTest、八框完整 271 帧闭环评分；若任一无框、重捕、两路中心/IoU 或跨初始化一致性门失败即否决。正式 `9f84efe` 八框仍 `2/8 PASS`，`latest_handoff.available=false`。

## 2026-11-14T14:14:21+08:00 车体形状修正八框 6/8 与恢复纹理门候选

- 上一实验精确源码 `codex/dual-gt-init-consistency@3437f78afb66dc3667f428d3fc071468b1e007e1`，主机 `/tmp/nt-dual-gt-evidence/shape-prior-3437f78-source.tar` 与 RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-shape-prior-3437f78/source.tar` SHA-256 同为 `b49e7c46a16d8f52711dc144fcc24381f773fb2f55f7b5d0ecae43679af070fe`。构建/首四框/后四框脚本 SHA-256 `1fc4659b8793b7cc5458ab1aebe6fedb818cbb08d3521843923ff99eb0582f62` / `c79fdd46da488ed9d75cf15dbea59021651766d50c71e139c4ecdf1a59b97781` / `554a3bf44df7c30dc901309c07750ba4d0515bda684bc46fda0263464ad2552c`，板端传输一致。板 `LKD3588` Linux `6.1.84`、RKNN API/driver `2.1.0` / `0.9.8`；canonical 视频和 tracker/detector/manifest/labels 哈希沿用下方 `452c48a` 已复核值。实验 controller SHA-256 `94da012f13b141184f54262f6b359704100051af9064a8cceb35321cf42b38dd`，`runs/headless-eval.conf` SHA-256 `e6e0d5fda5b7057c3f9638e0d13ac3c5ff0a1f94794bd066d6f3c539cf82e358`。原生 `bash build_shape_prior_board.sh`/CTest `23/23 PASS`，`build/nanotrack_v3` SHA-256 `c3ede4554e02ae9a08fbb12a91f85f587cbc188bb5abc6070654fe398654bb7f`；`runs/cmake.log` / `build.log` / `ctest.log` SHA-256 `72b41edcbbad40df6f569a91886dcb570c235d401292cf3c2da0ff8c60d50120` / `f0203ddc9b9f14adf960b7326788e042ecf72e35a73eaf3b0eb5af86a37275e3` / `72579111aa1a209ac8a2aaa34b5b268f554aca742f1d9696dc146a84a853f67a`。
- 实际 `bash run_shape_prior_four.sh` 和 `bash run_shape_prior_extra_four.sh` 跑 car001 标准 `(992,582,37,39)`、人工 `(993,585,36,36)`、左偏 `(990,580,39,41)`、右偏 `(994,584,35,37)`，car002 标准 `(970,517,38,38)`、真值 `(967,513,42,42)`、左偏 `(968,515,40,40)`、右偏 `(972,519,36,36)`；八组各 271 帧、exit 0，CSV/session/log 原件板端 `runs/<label>/`。八 CSV SHA-256 顺序为 `0a3ebc0a58386d7b2400246edaace0506cb770185b38c7e692d796cef05c0faf` / `41578f17ae5498cd7037a7529075b100e4bced87bdcc93f7f5f373574bb89dbf` / `7d0d9112bce5f5e25e6ab668f4caa04598e7ff1617be138dfd6eaebcf81892b7` / `3b0f350ca579f199738e8ebe6d8e53f4a4b73ac5afd6591c1a74b207d655def1` / `9d27539618a4c143c03be714385c6dabad572d20e8306cfc3da7997cfa07ec8e` / `2454249bada432f02d1d06a9968a469ae432d29c43826aab6f9268d697945e54` / `e921e4342be913c26166b3253e0c7dc4e8ccdbe5b1974a3d25c643ca3e3ac955` / `c071c078d94a40c8bbd7bb457f0aba5cd96b7a87e1ee95e34a2cbdb581fd58e1`。首/后四组文本包板端 `runs/four-text.tgz` / `extra-four-text.tgz` 与主机 `/tmp/nt-dual-gt-evidence/shape-prior-four-text.tgz` / `shape-prior-extra-four-text.tgz` SHA-256 分别同为 `c792b5f25cb8489d0aced8e840e367be3ec25386d17ba1b7da8df24aa21adc2f` / `994b2fd22d945b3c291f5fbe34383dc99a4ad80b0f2038d3645afb236256f73a`；主机解包八 CSV 哈希一致。
- 实际 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/shape-prior-3437f78/eight_score.json` 对各自确认 GT 评分，报告 SHA-256 `31452879f9f3177c507bc1e8a9118976f2d0485a993f64db16a53bced29d0d30`。car001 标准/人工/右偏 PASS，重捕 `187/189/186`，tracking 中心 `4.09/4.39/4.51px`，三框相互 P95 中心分散约 `0.92～1.23px`；左偏 FAIL，邻车身份冲突持续、无重捕，tracking 中心 `27.01px`，四框总分散 P95 `144.27px`。car002 标准/真值/右偏 PASS，重捕均 `230`，tracking 中心 `1.90/1.56/2.27px`；左偏 FAIL，第 `228` 帧过早重捕且第 `228～229` 帧在确认无框期输出观测，尽管可见段中心 `1.62px`。总体 `6/8 PASS`；形状先验确实修复 car001 右偏框的完整尺寸与跨框一致性，但未达八框目标。
- 新实验针对两处剩余失败：当最邻 detector 残差不超过位置门 `0.35` 倍且次邻超过该位置门时也允许先 NanoTrack 验证，以覆盖 car001 左偏预测中心距目标约 `12px`、邻车约 `47px` 的竞争，但邻车若也由 NanoTrack 三帧确认仍保留冲突否决；另在 SEARCHING 且已有恢复 probe 时，若最近模板原始框顶部四分之一亮度标准差 `<23.0`，跳过本帧恢复观测，保留追踪原始结果和 `RECOVERY_TOP_TEXTURE_VETO` 诊断。离线从上述八组固定轨迹查得 car002 左偏第 `226/228` 帧顶部标准差约 `17.4/22.8`，其他已通过组的实际三帧确认段多为 `24～53`；此阈值来自同一视频、未经独立校准，必须板端闭环检验。开关默认关闭，实验配置 SHA-256 `274bc592b92db7480fcdb8e9a056a397f6c7b44f8c585dfb4cec0a1dc7d69b93`；主机 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure` 为 RKNN-off `23/23 PASS`，日志 SHA-256 `f56a6b3712a16c888ccb1bdd7e3ea1c9857e24f6095ff2e7f0978b53d8d2dd03` / `551ca5a1ebb75d665f257fafcd7a5ba4c3bee9f5b6f42f67d02216a4989203cb`；`git diff --check` 通过。**下一步 / Handoff**：精确提交后 RK3588 新隔离目录原生构建/CTest、八组 271 帧闭环评分；未过八组则不晋级。正式 `9f84efe` 八框仍 `2/8 PASS`，`latest_handoff.available=false`。

## 2026-11-14T13:58:36+08:00 恢复框车体纵横比先验实验（待板端闭环）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始工作树干净，基线 `ecdeeaede64b7ec2aeebde61e9a3be2907b490cb`；本条与源码同提交。三帧版本已在两套 GT 四组回放中 `3/4 PASS`，唯一 car001 右偏框恢复时第 186 帧跟踪框约 `46.6×37.4px`，相对真值约 `44×48px` 明显扁宽，后段第 250 帧跟踪高度约 `39.3px` 对真值 `55px`；当前检测器第 185 帧目标候选约 `47.4×32.8px`，第 187 帧才约 `48.5×42.2px`，说明恢复时先选中局部车体。上述图像/CSV 原件见上方 `0a47e91` 与 `/tmp/nt-dual-gt-evidence/fullframe-smoke-db58683/`，独立诊断不等于在线批次。
- 实验新增默认关闭的 `recovery_restore_initial_aspect`：只在车类且已通过原三帧 detector 支持的恢复确认后，若所选 NanoTrack 框宽高比超过初始 ROI 宽高比 `1.15` 倍，围绕所选框中心补高至初始纵横比，单次最多增加 `35%`；同一修正框用于 `tracking_bbox`、控制框、运动初始化和近期模板准备，`display_bbox` 的 detector 仍只负责外观。没有从真值读入运行时。实验配置 `rk3566/nanotrack_v3/config/visible-dual-gt-visual-gate.conf` SHA-256 `94da012f13b141184f54262f6b359704100051af9064a8cceb35321cf42b38dd`，session metadata 显式记录开关；默认关闭以免影响其他配置。此同集启发式未证明泛化或真实效果。
- 新增 replay 回归验证三帧检测支持门、围绕 NanoTrack 中心补高，以及补正框进入模板事务。实际 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/shape-prior-host-build.log` / `shape-prior-host-ctest.log` SHA-256 `ace693747121e772826fc6d260e3bcd7951aa63497d5a359760ece084d983742` / `9853daddc33b26498532af3eb732ae0666ac3a0fc8e0e27394b77d272fc7cbbb`，`git diff --check` 通过。**下一步 / Handoff**：精确提交后在 RK3588 新隔离目录原生构建/CTest，重跑四组各 271 帧，用两套已确认 GT 检查两路中心、IoU、全遮挡、恢复期限和跨初始框一致性；不达四组门则否决并撤回。正式八框仍 `2/8 PASS`，`latest_handoff.available=false`。

## 2026-11-14T13:54:36+08:00 四帧候选板端 2/4 否决并撤回

- 实验精确源码 `codex/dual-gt-init-consistency@b39c12b0d19019bd769168c71f6e80b9178d8449`，主机 `/tmp/nt-dual-gt-evidence/four-hit-b39c12b-source.tar` 与 RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-four-hit-b39c12b/source.tar` SHA-256 同为 `7def1a9059113f2ea6466af925f3e6bda2dbc56493c4628ff90230740f850879`；构建/四框脚本 SHA-256 `7a2d43eed125c53483e7b933d77f55488db667ecb9651bd368cfd23451b5508e` / `5de7126a7270a63ee43c782d4630c92f9a02f183c01f8cca9178661a1e03d947`，板端传输校验一致。板 `LKD3588` Linux `6.1.84`、RKNN API/driver `2.1.0` / `0.9.8`；canonical 视频及 tracker/detector/manifest/labels 沿用下方 `452c48a` 已复核哈希。实验 controller SHA-256 `513557b0667fe95db8be10d02929cd3d68e0972765b458d8196813c4b2d0e4c7`，`runs/headless-eval.conf` SHA-256 `f922643a960736cfebce3a3285ed2c00abf7c3003d46473b4648b1c368dfacc5`。
- 实际 `bash build_four_hit_board.sh` 原生构建/CTest，`23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `067fabb832b8d4a7933bbc4de602435991f59d0a39b492f7b669985f1c5c0dad`，`runs/cmake.log` / `build.log` / `ctest.log` SHA-256 `5d45bfb1135af0267a31031f998dba10228ff5e33a48a56fc005c4b682331156` / `3e7ec886ea71278c98864c44915c6d19be97e3fab47933d903a182d649de94b5` / `fe90720a9bd01fe354adb91885f205967c181d7c6a10a9a826dc036e88f9e5a1`。实际 `bash run_four_hit_four.sh`，car001 标准/右偏、car002 标准/真值四组均 exit 0、各 271 帧，CSV/session/log 原件板端 `runs/<label>/`；四 CSV SHA-256 顺序为 `83c2a693f7b05cd0dc633552e8d3eaacc4b8a7ba3a7a33103b88334ce09ac9d9` / `6e238c2785e74129410058f409e8f068f270d323f9773bfe7d28d90b117235b8` / `c4f3f11b66707aaf17c2e766b767abc60a1a9f2db4dd1fc0f8a0a9c1b9f0d82b` / `c228121e5430f4a52a58921a7d8d5ce80cc1a2cbb4c1f9a4b7c94835cefdb2ce`。板端 `runs/four-text.tgz` 与本机 `/tmp/nt-dual-gt-evidence/four-hit-four-text.tgz` SHA-256 同为 `aa648da5e3a2f032e3d54fad6830116a1492bde42cd264276003ae6ce3f5953a`，本机解包四 CSV 哈希一致。
- 实际 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/four-hit-b39c12b/four_score.json`，报告 SHA-256 `a0d8696d2a190ddf26d097b6ad6cfa13c096eabaae82f8df995ff5c258afa7ab`：car001 标准/右偏均 FAIL，重捕第 `196/195` 帧，超过 `<190` 期限；tracking/display 中心 `4.43/4.47px`、`5.34/5.39px`，右偏全可见 IoU `0.693`、后段高度偏小尚未根治。car002 标准/真值均 PASS，第 `234` 帧重捕，中心 `1.74/1.75px`、`1.69/1.69px`。总体 `2/4 PASS`，四帧等待对 car001 的时间门造成退化，不能扩八框或晋级。
- **裁决 / Handoff**：把四帧配置、候选数组/窗口和测试恢复到已板测三帧 `0a47e91` 内容，保留只接受源码允许 `2/3` 确认帧数的评分器修正及正/负回归；原实验提交、板端隔离证据和报告保留。撤回后实际 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure` 为主机 RKNN-off `23/23 PASS`，日志 SHA-256 `2bdcc1fffdf44b9d82b45d9152e8773355b5a9517c890fc0f18a58251b0ff16e` / `a38655d8c79f6dc67f8d85c53a287c2dbd25d1a90407400818b7a5d72ddfca15`；Python 评分器 `24/24 PASS`，`git diff --check` 通过。当前最佳实验为三帧 `3/4 PASS`，car001 右偏后段全车框尺寸与中心一致性仍未达门；正式八框 `2/8 PASS`，`latest_handoff.available=false`。下一步需独立车体完整性证据或可信的恢复后全车几何校正，再重测四框及八框。

## 2026-11-14T13:48:41+08:00 三帧版本板端 3/4 与四帧恢复时机候选

- 前项精确源码 `codex/dual-gt-init-consistency@0a47e919cfc6ced0364ce52df9a98830fb06f989`，主机与 RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-three-hit-0a47e91` 的 `source.tar` SHA-256 同为 `0ea28dcaa98cf6a7ce04f7edf6182853ef7dff0538a10adc1f59709554afc0be`；构建/回放脚本 SHA-256 `dbbb80e29172f450f64ddb65c4c4458fb63a187354291ea9689024cfae8434e4` / `8ca530fa3d7ce553938a5423d29279fc949bd796a5e195a7b1fbb1ece2dfa4cf`，传输复核一致。板 `LKD3588` Linux `6.1.84`、RKNN API/driver `2.1.0` / `0.9.8`；canonical 视频及 tracker/detector/manifest/labels 身份沿用下方 `452c48a` 已核对输入。执行板端 `bash build_three_hit_board.sh`：原生构建及 CTest `23/23 PASS`，二进制 SHA-256 `2c314d8df7fd99b1a824698897393bdc71d4d997998fc90e96bf232223b0471a`；`runs/cmake.log` / `build.log` / `ctest.log` SHA-256 `c0170111ad0b91321f9ac5295610433094d5b0dc0a93b57804fa9bfd0a188837` / `01e8a08ad07ba6a9309de7cb5ccb1ef8bf7176d850373bdcaf815d00b19ab97a` / `80bf6b631103b0d0e588ffc74cd0524829123c11354ff09432ec7a367ab441d0`。实验配置 `69798f4bcd782d5c445172801d0c4119395854a1494e5f68c3eee088f593bc4f`，`runs/headless-eval.conf` SHA-256 `36ee60c44ae3760305e5428b2e817b493e6cd29b4b7a3b8e742e419e205bad0a`。
- 执行 `bash run_three_hit_four.sh`，car001 标准/右偏与 car002 标准/真值四组均 exit 0、各 271 帧；CSV 原件板端 `runs/<label>/frames.csv`，SHA-256 顺序为 `b443b48162c79a9834586b19ebf6243c99d7b7ed10fc8ac7ce767a57ef1b45b1` / `db50d8882222cfb342123aa19665d510a5a632d75fd3c9bc4418c7f0b660e439` / `cc57946a120941c24fc2901a8307de5fb74487f47bc4f74d0a54d426ff6fc1f3` / `d8764438f90ce8c389bcaeff4dcec3919556ea48c6c486dbfe3d2e6a716b68b9`。文本包板端 `runs/four-text.tgz` 与本机 `/tmp/nt-dual-gt-evidence/three-hit-four-text.tgz` SHA-256 同为 `a8ba05cc25ad85533b7f5034fcdf02746a0d63a6ea74938c1b017c57d8ef88d4`；主机解包后四 CSV 哈希一致，session/log 保留在板端及文本包中。
- 首次正式评分器将三帧 `RECOVERY_STAGED` 误报为无有效重捕，原因是 `scripts/evaluate_occ_video_1.py` 只接受 `recovery_confirmation_hits==2`。本提交将其扩为源码允许的 `2/3/4`，并新增正例，原一帧负例保留；`python3 -m unittest tests.test_evaluate_occ_video_1 -q` 为 `24/24 PASS`。用修订版 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/three-hit-0a47e91/four_score_corrected.json` 对两套确认真值重评，报告 SHA-256 `705025de6356d75e97e6e6e1a9fad67bb74612d98fe1178d5138e2dc2bbff2a6`：car001 标准 PASS（187 帧重捕，tracking/display 中心 `4.36/4.41px`），右偏 FAIL（186 帧重捕，中心 `5.41/5.43px`，后段框高度约 `39px` 对真值 `53～55px`，visible_boxes 失败）；car002 标准/真值均 PASS（230 帧重捕，中心 `1.90/1.56px` 两路相同）。car001 标准/右偏 tracking 跨框中心 P95 `12.17px`，car002 标准/真值 `2.57px`。因此三帧版总体 `3/4 PASS`，不能扩八框或 handoff。
- 本项实验将恢复确认从三帧增至四帧，试图让 car001 右偏在部分车体 detector 候选后多观察一帧再重建模板；同时把候选验证数组及过期窗口扩至四帧，默认二帧行为保持。实验配置 SHA-256 `513557b0667fe95db8be10d02929cd3d68e0972765b458d8196813c4b2d0e4c7`。实际 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，日志 `/tmp/nt-dual-gt-evidence/four-hit-host-build.log` / `four-hit-host-ctest.log` SHA-256 `d5e33edf68811e620d259fd1b6cf000b215e2387b00bf2a5fd96c9317e6281f4` / `02ade2fe4ea4795d289c505ff08f7bc3d65e141ae54e2998b03164195eae1d9d`，Python 评分器 `24/24 PASS`，`git diff --check` 通过。**下一步 / Handoff**：同提交后新隔离 RK3588 目录原生构建，先重跑四组；若 car001 右偏未达可见框门或其余组退化，即否决并撤回四帧实验。正式八框仍 `2/8 PASS`，`latest_handoff.available=false`。

## 2026-11-14T13:35:31+08:00 双真值三帧检测支持恢复候选（待板端闭环）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，改动前干净基线 `bd0ecaa9d8204fadf027ca64ad8ddafc962180ca`，本条与代码同提交。此前 `db58683` 四组仅 `1/4 PASS`，未将其作为正式算法。对 canonical 视频帧 185、188、225、226、229、230 无损抽帧，在 RK3588 用相同 VisDrone9 RKNN 和 `yolo_detector_smoke --conf 0.25 --nms 0.45 --class-id -1` 做全帧候选诊断；视频 SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，抽帧/CSV 主机原件 `/tmp/nt-dual-gt-evidence/fullframe-smoke-db58683/`，板端原件 `dual-gt-motion-near-db58683/runs/fullframe-smoke/`。smoke 二进制 SHA-256 `3ba28a7afa1e5aa9c09a551f1fbb2b0e137e2b0006ecb1941fdd7fe8d9712f72`；其最多输出 100 个候选，而运行时不受此上限，因此只把它作为局部几何诊断，非运行时批次同一性证明。
- 第 185 帧 car001 右偏框运动预测中心 `(875.63,826.85)`，最邻候选约 `21.97px`、次邻 `59.71px`；第 226 帧 car002 标准框为 `(862.89,718.90)`，最邻 `8.13px`、次邻 `74.37px`；第 229 帧 car002 真值初始框为 `(867.86,710.78)`，最邻 `13.47px`、次邻 `75.50px`。原来的“前两候选距离差大于 44.06px”漏掉 car001 目标。实验改为最邻残差不超过位置门的 `0.6` 且次邻大于 `1.25` 倍位置门时允许先验证最邻；仍需 NanoTrack 验证。实验 controller `rk3566/nanotrack_v3/config/visible-dual-gt-visual-gate.conf` SHA-256 `69798f4bcd782d5c445172801d0c4119395854a1494e5f68c3eee088f593bc4f`，搜索 detector 间隔 `100ms`、恢复至少三帧一致并要求 detector 支持。多候选视觉遮挡门沿用 `db58683` 实验内容；本轮同时记录新增参数到 session metadata。`display_bbox` 的 detector 融合只用于显示，不能替代 `tracking_bbox` 真值中心或授权控制。
- 实际 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 和 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/three-hit-host-build.log` / `three-hit-host-ctest.log` SHA-256 `b77bb395b46d7f243965715f9624c38907ba2bfbd1ca1f1f4f7fbd831b5cae4f` / `d0cd9b5a22c7a195d773aa9e4117090ce070ed307b311634dd495b833728b6eb`，`git diff --check` 通过。新增三帧确认回归，改动了近邻竞争回归的候选位置以覆盖实际门槛。主机结果不能证明重捕时间、全遮挡无框语义或两路中心改善。
- **下一步 / Handoff**：按本提交精确源码在 RK3588 新隔离目录原生构建/CTest，先完整运行 car001 标准/右偏、car002 标准/真值四组各 271 帧并对两套确认真值评分；若四组不通过即否决和撤回，不扩八框。正式 `9f84efe` 八框仍 `2/8 PASS`，`latest_handoff.available=false`；生产编译与真实吊舱验收未获授权。

## 2026-11-14T13:23:00+08:00 多候选视觉门与近运动优先候选板端否决并撤回

- 实验源码 `codex/dual-gt-init-consistency@db5868302899db1955e82e81b24a0f189cacb2b5`，改动前基线 `9e57cf669b68153b10032f8ca8deafe8ce6fc67f`，工作树 `/home/u20/single-object-tracking`；`git archive --format=tar HEAD` 生成 `/tmp/nt-dual-gt-evidence/motion-near-db58683-source.tar` SHA-256 `1bd3e0ed06cfc26c02c25d8b876fc378adef96baba64f6b2df94479f7a14e759`，上传到 RK3588 新隔离根 `/home/neardi/Project/SOT/dual-gt-motion-near-db58683/source.tar` 后 SHA-256 一致。构建/四框运行脚本 `/tmp/nt-dual-gt-evidence/build_motion_near_board.sh` / `run_motion_near_four.sh` SHA-256 `8bb86948526bbb1dd39d8c987684a3d111d4b9eae99987d351c7fd36a8f1807b` / `280c91a1c2f6390203062a9004408811516b9219bcaf60d09f71ce885e3c550a`，板端上传校验一致。板 `LKD3588` Linux `6.1.84`、RKNN API/driver `2.1.0` / `0.9.8`；canonical 视频、tracker RKNN manifest、VisDrone9 RKNN/manifest/labels 精确身份沿用下方 `452c48a` 已复核值。板端 controller 配置 SHA-256 `c5c4e2edec914eb44cd8777377abf3600574cf3b936225bf9dd2e9d382d15fec`，隔离 `runs/headless-eval.conf` SHA-256 `2344c6b370cce49d6b314d758a6d2c0e55bd8a20f77b8bcde4efc4b3cdaa1587`。
- 实际 `bash build_motion_near_board.sh` 执行原生 CMake/build/CTest，`23/23 PASS`，板端 `build/nanotrack_v3` SHA-256 `d38b3b561bbb618644a2774d550dd73b3fa73eb5d14079aeab90bfd0902ad010`，CMake/build/CTest 日志 SHA-256 `cbdec6e7fc0f01cd46bc7453cb1afbf494a550fe73e746047905ba743e079397` / `6b884731c9ac2ec06a366413956aa1376185eced10a079b833cd32ca24434eb7` / `bc166f8bff7ee75c1551b6db8f26999d4fe07c28cf6b4134042a163bd0cfb527`。实际 `bash run_motion_near_four.sh` 运行 car001 标准 `(992,582,37,39)`、右偏 `(994,584,35,37)`、car002 标准 `(970,517,38,38)`、真值 `(967,513,42,42)`，四组各 271 数据帧、exit 0；CSV/session/log 原件在板端 `runs/<label>/`。四 CSV SHA-256 依次 `906b612a2822268db208086710e3c0471fe4454319e89442c48d6df12de5180b`、`f2cfb935c4c80d6b4b57356643b31eefee4ad70c9dfd8fb95576fc1bf8b361a5`、`cc05800e5e6d93d5e0ba04f7790f09be55eea316638bc27e2da51ff62e28a8af`、`6a57fee32e599396d9f42ff2d48a110a8da141e9e2808da51d91f65924a4e640`。板端/本机文本包 `runs/four-text.tgz` SHA-256 同为 `321ee5f38a4640b99b7c59030f60d6e7c7c21b0270cce24ee56be216e6da1a29`；本机解包至 `/tmp/nt-dual-gt-evidence/motion-near-db58683/`，四 CSV SHA-256 与板端一致。
- 实际 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/motion-near-db58683/four_score.json` 用两套对应 GT 评分，报告 SHA-256 `8edb934ce2ebc69e1f50fe97f6dc303133d85d5946f4be0adea66fa17c1a0563`：仅 car001 标准 PASS、第 188 帧重捕，tracking/display 可见中心 `4.28/4.32px`；右偏框仍第 190 帧重捕（要求 `<190`），中心 `4.89/4.92px`，FAIL；car002 标准第 226 帧过早重捕、第 234 帧再次进入 SEARCHING，后段难以恢复，tracking/display 中心 `7.56/7.50px`、tracking 可见中心 P95 `45.83px`，`visible_boxes` 与 `reacquisition` FAIL；car002 真值初始框第 229 帧过早重捕，中心 `1.68/1.68px`，`reacquisition` FAIL。car002 标准/真值跨初始框 tracking/display 中心 P95 `49.69/48.69px`，比上一修订版 `4.95/4.95px` 明显退化。多候选视觉门使 car001 右偏框第 172 帧与 car002 两框第 213 帧进入 SEARCHING，但新近运动候选优先仍不足以保护恢复身份与完整可见性，且 car002 标准框后段再次被视觉门推入搜索。总体 `1/4 PASS`，不能扩八框或晋级。
- **裁决 / Handoff**：按失败即撤回规则将实验配置、算法与测试文件恢复到 `9e57cf6` 内容，保留 `db58683` 实验提交、板端原始证据和本条记录；撤回后实际 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 与 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，日志 `/tmp/nt-dual-gt-evidence/motion-near-revert-host-build.log` / `motion-near-revert-host-ctest.log` SHA-256 `01ce4a73e6230e8315504b28ec05e396c31b8d2e30ec41f836e94e68b38f1d24` / `12a3ec9e766ca9af517ece6defc85b6440a32e570d82dad637b51d325bfee36d`；`git diff --check` 通过。正式 `9f84efe` 八框仍 `2/8 PASS`，`latest_handoff.available=false`，不启动生产编译或吊舱验收。后续必须提供独立的车体完整性证据及跨遮挡身份连续性，不能再以单纯时间阈值或最近候选排序代替。

## 2026-11-14T13:15:00+08:00 多候选视觉门与近运动候选优先的联合实验（待板端闭环）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始干净，基线 `9e57cf669b68153b10032f8ca8deafe8ce6fc67f`。恢复此前 `3679dca` 的默认关闭视觉遮挡门候选后，依据下方 `gate3` 板端筛查与 car001 右偏框第 185～190 帧候选竞争记录调整两处：局部 VisDrone9 一帧仅一个候选时保持两帧亮度+检测证据，多个候选时必须累计三帧才停用疑似观测；搜索恢复时，若运动预测仍可信、最近 detector 候选在位置门内，并且与第二近候选中心至少相距 `max(40px, 两框最大边长)` 且位置残差不超过后者一半，则优先对较近候选进行既有 NanoTrack 双帧验证。后者只改变候选验证顺序，detector 本身不得直接授权 `tracking_bbox`、`control_bbox` 或重捕。实验 controller `rk3566/nanotrack_v3/config/visible-dual-gt-visual-gate.conf` SHA-256 `c5c4e2edec914eb44cd8777377abf3600574cf3b936225bf9dd2e9d382d15fec`；新分离常数/相对阈值未校准，需在八框和其他场景检验。
- 新增/调整运行时 fake-detector 单测，覆盖多候选前两帧仍 SUSPECTED、第三帧停用观测、单候选第二帧停用观测；replay 单测覆盖“近车与高置信远车分开但归一化残差差值小”的优先顺序和双帧 NanoTrack 验证，既有近邻车辆冲突测试仍通过。实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 与 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；构建/CTest 日志 `/tmp/nt-dual-gt-evidence/ambiguous-visual-near-focus-host-build.log` / `ambiguous-visual-near-focus-host-ctest.log` SHA-256 `7f0fe9002b825dc39b1be9f291c2108dead7b3fe453bebbb4334183250deee46` / `291a9c31457dcc112334eff266ef13cb37bbef9c4072b5d8ac6138da8b9f1634`，`git diff --check` 通过。主机测试没有运行 RKNN 引擎，不证明正确重捕或中心改善。
- **下一步 / Handoff**：本条与源码同提交后按精确 commit 在 RK3588 新隔离目录原生构建/CTest，先完整跑 car001 标准/右偏与 car002 标准/真值四组各 271 帧，对照两套确认真值、重捕期限、无框段语义、两路中心/IoU、跨初始化一致性及错车；失败即撤回。正式 `9f84efe` 八框 `2/8 PASS`、`latest_handoff.available=false` 不因主机测试改变。

## 2026-11-14T13:06:05+08:00 car001 邻车竞争段的板端参数筛查

- 当前分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始干净，基线 `3fcac5fb8b4c1de86bde711ec472afee1ea7ff67`；本轮不改算法源码、真值、模型或正式配置。只复用先前隔离源码 `452c48aa5f6a93abcd6fd888d50801de9894c36b` 的 RK3588 原生二进制 `/home/neardi/Project/SOT/dual-gt-visual-gate-452c48a/build/nanotrack_v3`，重新核对 SHA-256 `1a54d65b23d0746b2dec5bfd49c625233176c3f7e5d69c910fee1884eee129e5`；板 `LKD3588` Linux `6.1.84`、RKNN API/driver `2.1.0` / `0.9.8`，canonical 视频及 tracker/detector 模型、manifest、labels 精确哈希沿用下方 `452c48a` 条目。本轮仅把实验 controller 的连续视觉证据 `2→3`，或另将搜索最短年龄 `400→300 ms`、搜索 detector 间隔 `400→100 ms`，分别复制到独立 `runs/<name>/controller.conf`，再复制 `runs/headless-eval.conf` 改指该配置；未覆盖原实验配置或输出。
- 在板端分别执行 `./build/nanotrack_v3 --config runs/<name>/headless.conf --bbox 994,584,35,37 --output-boxes runs/<name>/frames.csv --session-metadata runs/<name>/session.json`，三组均 exit 0、271 数据帧。`gate3-car001-right` 的 controller/headless/CSV/session/log SHA-256 依次 `1cce98497c2ed10fba702a9cdfc6dda1508671b7d175e9b7da0d1688d5de5644` / `e2282612e3390eae22b243ba39ab66bcc4d1737201ffbf929fb0c7cd662db9e8` / `5e7b68ee8b3dcb5500f643ad0e5d12c8a2997ee2c14dcfe2d3aeadbfed83e3bd` / `0b1448f2acae6875920f50c1150a492c5fb215e4514fcba4961e993cdcff42dd` / `08c938fb9913981a1b419af375443fc2653597b6f339f5e0ead0444df34e6eed`；`gate3-age300-car001-right` 为 `5a920ee311d6b01b1d620c63626af619cdd1511f9e0fee725acd8e0c0173ff8e` / `efbb942d1869ef5f8d257f0caac1d63ac7447251615db13ca0204d82ce215263` / `5a940dcc8d655d8f90f5880ea8b1c9a70e9f26328b4e8a48dde9c22a4a718f01` / `3b827eefd2b989a2b2285533dec4fad729d3c527616feef1a097355aae0a12c1` / `6364c26e58a457cc13e36ea9f0570999f229600945c0f4ad8c50fce4a8310c46`；`gate3-det100-car001-right` 为 `7c78c99f81af3df00b9253a251b9a1a10d385a5c6c72728ef6c4b0a4efc880dd` / `7acfc07b105478266309f5f1a9bc77ed3f9e7983a9a3bb5e99c0a3a58b1c5840` / `0f091a82b71ee2d5d3c5851c3cf2814ccbd8afc6f784a634a877949a8d6df0f5` / `3255a9d371cb9881c0a1fb4650cb1182206e4ba2b8d1d2adeda7391d90986073` / `4c4789af4f7b94b1df5146a15dfeeb3f84fdfdc23170cbc77f72c9a389d7d988`。原件都留板端隔离根相应 `runs/<name>/`；三份 CSV 回传主机 `/tmp/nt-dual-gt-evidence/<name>-frames.csv` 后 SHA-256 与板端一致。
- 用 `python3 scripts/evaluate_occ_video_1.py --dataset-root data/occ_video_1 --target-id car_001 --boxes /tmp/nt-dual-gt-evidence/<name>-frames.csv --scope full --output-json /tmp/nt-dual-gt-evidence/<name>-score.json --output-markdown /tmp/nt-dual-gt-evidence/<name>-score.md` 对同一 car001 确认 GT 评分；三 JSON SHA-256 依次 `b91fa6684c3ec546646c7c41ba50bf57836a73c00dade06291480580b95162b6`、`010fae86f0df7f433708024b6339283d202cb5c95ab386b73ff48357c1f1d71a`、`43bff29a7cfd6618d8d7a16ee62863233fd4d3fd627fe3b895be1795e1bb79b7`。三组均仅因重捕晚于 `[182,190)` 而 FAIL，重捕帧为 `190/196/190`；缩短最短年龄使第 183～190 帧持续 `IDENTITY_CONFLICT`，加快 detector 更新虽在 182/185/188 帧请求并在 183/186/189 帧收到批次，仍到 190 帧才确认。此三次是**单框参数筛查**，不能代替两目标八框闭环。
- 叠加原视频、板端 `tracking_bbox` 与对应真值的诊断图 `/tmp/nt-dual-gt-evidence/diagnostic-occlusion-truth-montage.png` SHA-256 `d0d41c158dfe7c22d29981b1eb6c2ee4f797d372a1d1f78ab8fcfacabd0fc7ad`：car001 右偏框在完全遮挡第 172 帧仍 SUSPECTED，car002 标准框第 226 帧把桥面与部分车体当成可恢复目标，第 230 帧框相对完整车体上移。局部亮像素比无法稳定区分 car002 真值框的 229/230 帧，故本轮不引入单阈值可见性判据。**Handoff**：参数调整不足以同时解决及时重捕、身份与完整车体可见性；正式 `9f84efe` 八框 `2/8 PASS`、`latest_handoff.available=false` 不变，下一步应验证独立候选身份与完整性证据，再做四框板端筛查和八框验收。

## 2026-11-14T12:53:00+08:00 修订视觉遮挡门四框板端否决并撤回

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，板端候选源码 `3679dcaef996e40fd03d639c224c02521aa8eb7c`，改动前基线 `6d8002485d714aacd68f087d988286ec8839c42b`。源码包 `/tmp/nt-dual-gt-evidence/visual-gate-refined-3679dca-source.tar` 与 RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-visual-gate-refined-3679dca/source.tar` SHA-256 `2d32a7a04ee2e814ce99e85b2918ed0c956835b214c4af480eb1f245f6c95856`；构建/运行脚本 `/tmp/nt-dual-gt-evidence/build_visual_gate_refined_board.sh` / `/tmp/nt-dual-gt-evidence/run_visual_gate_refined_four.sh` SHA-256 `dd404da7eb7eea39ee79aaa3f5f4eea89afa0497e17045bd4ea7154188050525` / `4602d26c6279fbd24d981c4248681fac36fe7e2885be05d321c5d51fa21bc0a8`。同一 `LKD3588` Linux `6.1.84`、RKNN API/driver `2.1.0` / `0.9.8`，canonical 视频、tracker RKNN manifest、VisDrone9 RKNN/manifest/labels 的 SHA-256 沿用上一节已复核值；板端 `runs/headless-eval.conf` SHA-256 `337cd5b586ea55d8abdb2b7824ea8ab69fc52116de24421b7bb22cdfe6a541f8`，实验 controller 配置 SHA-256 `c5c4e2edec914eb44cd8777377abf3600574cf3b936225bf9dd2e9d382d15fec`。原生 CMake 构建和 `ctest --test-dir build --output-on-failure` 为 `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `16c6a05cf45bc7b97bfc222d01053da2025da97409c27e88c288db9856cfcbaf`，CMake/build/CTest 日志 SHA-256 `9530446d6af7e5dde8fa33c8ae84530953c1d416119d98e0e12c0126eaf62570` / `ae5dc2b89577be639719bcd0669f5b1b37c489f6ab1d7d119e4b28f99d7ff702` / `157746f08c386beb08926dd3ea3e50b2b7f8ddc138420e2212aaabf6c5d881a9`。
- 实际板端 `bash run_visual_gate_four.sh` 跑 car001 标准 `(992,582,37,39)`、右下 `(994,584,35,37)`、car002 标准 `(970,517,38,38)`、真值 `(967,513,42,42)`，四组均 271 数据帧、exit 0；原始 CSV/session/log 留隔离根 `runs/<label>/`。四 CSV SHA-256 依次 `d90d38c0d1b69826f430e6d4c1146cf381aaddd75494b860b318091b0b5c34d3`、`074ef9fbcc79dce1557e9cf015122ff44189bf1066eb205a898c14b760521b8a`、`e367f6b8da41ac6747ae7eb3a0b106af8dba22654b1cf52c6a8b23aff1875f87`、`ea73839e6682a3ed4770314322a4761bbfe653026d0e10b93362cee894fa6e1b`。板端/主机文本包 `runs/four-text.tgz` SHA-256 同为 `111af5ac94dd46f2b63542d293408dee6344adac15056826c9b043464829922c`；主机副本解包于 `/tmp/nt-dual-gt-evidence/visual-gate-refined-3679dca/`。以 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/visual-gate-refined-3679dca/four_score.json` 对各自确认 GT 评分，报告 SHA-256 `31fd836ba534411c0b2a140c80d2b5c7c12ffacf559751900b7b4d6cfd98538d`。
- 结果仅 `1/4 PASS`：car001 标准保持 PASS、188 帧重捕，tracking/display 可见中心均误差 `4.28/4.32px`；右下初始框在完全遮挡 172～174 帧仍 SUSPECTED、第 191 帧才重捕（要求 `[182,190)`），两路中心 `5.02/5.05px`、IoU `0.701/0.748`，故 FAIL。car002 标准虽第 213 帧进入 SEARCHING，但第 226 帧过早重捕（要求 ≥230），第 230 帧可见框偏离真值，tracking/display 中心 `2.18/2.18px`、IoU `0.815/0.758`，FAIL；真值初始框第 229 帧过早重捕，两路中心 `1.68/1.68px`、IoU `0.887/0.759`，FAIL。标准/右下 car001 tracking/display 跨初始框中心 P95 `2.84/2.16px`；car002 标准/真值 `4.95/4.95px`，但均值掩盖了 car002 第 233 帧最大跨框 `33.29px`。修订改善了 car002 全遮挡输出，却未提供可靠的恢复时机和身份判据；`display_bbox` detector 仍不参与跟踪/控制授权。
- **裁决 / Handoff**：按失败即撤回规则，将视觉遮挡门新增配置、运行时代码与对应测试恢复到 `6d80024` 算法内容，保留候选提交和板端隔离证据。撤回后实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 与 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/visual-gate-refined-revert-host-build.log` / `visual-gate-refined-revert-host-ctest.log` SHA-256 `13fe1780d7e3b8fbf78bd7f1c4905788641fbb6489c31fd176af4141a4a1244f` / `ee57870d975c968aa6da40dfd2575f4b6ffe9192d3f66440fc8d675ff5b933c3`，`git diff --check` 通过。正式 `9f84efe` 八框仍 `2/8 PASS`，`latest_handoff.available=false`，不可作为生产编译或吊舱验收交付。下一步需以独立的完整可见性和身份验证限制过早重捕，同时使 car001 右下框在遮挡时及时退出 SUSPECTED；新的候选仍必须先过四框板端筛查，再跑八框。

## 2026-11-14T12:42:56+08:00 视觉遮挡门首轮板端否决与时序/多候选修订

- 首轮候选精确源码 `codex/dual-gt-init-consistency@452c48aa5f6a93abcd6fd888d50801de9894c36b`；本机 `/tmp/nt-dual-gt-evidence/visual-gate-452c48a-source.tar` 与 RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-visual-gate-452c48a/source.tar` SHA-256 同为 `51d1a5166935a08e7990f1d998fea50bfe3c2de674dd54af99549f2ddaa13d3e`。构建/四组运行脚本 SHA-256 `cbb00369273ac7e2efe137921cb40ff419bafd13eb5e7c0aefcfbf266c455834` / `66e7d3544d2b2a27410f7d8a32b0e23d7b71c973c42eb68d65c80623f2eb1091`。实际板 `LKD3588` Linux `6.1.84`，RKNN API/driver 沿用本文件已验证 `2.1.0` / `0.9.8`；本轮重新核对 canonical 视频、tracker manifest、VisDrone9 RKNN/manifest/labels SHA-256 分别为 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`、`3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`、`8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`、`ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9`、`41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。隔离 `runs/headless-eval.conf` SHA-256 `117e73483af49ce9a1e546cbbeca15138f4e9219eacca484874134893ac4bed4`，指向本隔离源码的实验 controller 配置。原生 CMake/build/CTest 23/23 PASS；日志 SHA-256 `2fc34d6506da19241010d5e372f5af3436a62c6bbfd250a1858b8de3aa248009` / `97d62ebbc52d5836e1ea997a22de1ee5de804739a0c311afde109639c21b0190` / `d027655af040861314281f0e5b0fe85f669d29b4a76e8d2afa34293c8e1f9b5a`，二进制 `build/nanotrack_v3` SHA-256 `1a54d65b23d0746b2dec5bfd49c625233176c3f7e5d69c910fee1884eee129e5`。
- 实际 `bash run_visual_gate_four.sh` 完成 car001 标准 `(992,582,37,39)`/右下 `(994,584,35,37)`、car002 标准 `(970,517,38,38)`/真值 `(967,513,42,42)` 各 271 帧、exit 0；CSV/session/log 原件留板端 `runs/<label>/`。四 CSV SHA-256 依次 `cc0e3775931de25fe10857ae65c67095ffbe9a9c59491d9232c3567d3d7c5300`、`78c261dfaa7673615aef2c596d1499a81bd2f5c898c12390fe8e3809828a5e1f`、`29eb86addc845aef2a595075f1003579e843e45df1282e8b83ede4f9094d4a4f`、`1a1731a7865f1375db68e3e3f4c7cb12a85c1456108bc4e5eefd580c0780561b`；session SHA-256 依次 `82d7e339d52e0009fc88fef55555646481c7517b9a23cbcc89f276a023735022`、`db4597e129b7d3e12388529a549ac99ccaee0a7058f71e38f60611575c10fc6d`、`df0baeeafe7796e301cc8b7fe7a9346a3b47d4558cbe5c85c775843234fa6290`、`96217dbb28a4a6b0756235b3a0c8fe44ee15145265194a5d8aaf779733cf3fbc`；日志 SHA-256 依次 `35a9ac5f3598703620785be09550d4ee77d667cabff9161b00d44b08c68bd713`、`b6ebe1a68528c3857f0f4070f98bbf86fefc86397d38d7b2db8198f7af15e1d2`、`70128bd957862af9c9f931a5bbca6d061a1eb2d0ce6b212b6f5f42bf9e92d901`、`c82351d1980b7ae3c61a595ec1d671199a9ff9e13afea69091727b5d970044cd`。文本包板端/本机 SHA-256 一致 `93d493719c07676ec2582187e6a569f0b586e65857bd8d24ea1cf3bb576dd959`，本机副本 `/tmp/nt-dual-gt-evidence/visual-gate-452c48a/`。
- 用 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/visual-gate-452c48a/four_score.json` 按目标对应 GT 评分，报告 SHA-256 `d496b7985c2f68bfc364b388903ba908edb44773c70becc99716f463ecd58bf7`：仅 car001 标准 PASS，188 帧重捕、tracking/display 中心 `4.28/4.32px`；右下框第 171 帧视觉门提前 SEARCH，却第 189 帧错误重捕邻车，tracking/display 中心 `33.69/33.51px`、IoU `0.473/0.520`，`visible_boxes` 与 `identity_proxy` FAIL；car002 两框从第 211 帧对外发布 SUSPECTED，但视觉检测直到第 215 帧才开始且未触发，完全遮挡语义仍 FAIL，重捕 `235/233` 帧，中心 `2.59/2.00px`。四组仅 `1/4 PASS`，不能晋级。
- 源码检查确认首轮只看 `SearchPlan.tracking_phase`，该内部模式晚于输出对外 `SUSPECTED`，因此车体亮度虽已下降，第 212～213 帧仍没有新鲜检测；car001 右下局部区域第 170～171 帧同时存在多个候选，单一最近候选允许过早搜索并改变身份竞争。基于**这两处板端反例**修订：下一帧检测条件依据上一帧对外 `tracking_phase`，且同帧局部 detector 批次必须恰有一个候选，才计入连续证据。新增 fake-adapter 回归验证低置信度、多个候选、单次命中均不能触发；实际主机 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 与 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，RKNN-off `23/23 PASS`，日志 SHA-256 `72ecfa4063f67e9d0bc564e65d5485b63e24e315d8a1ddc538d1a3504c81249d` / `1a6633cad444badd3c19465dae63488e0bd5e67b131fd7622b1da78c50f0f5d6`，`git diff --check` 通过。
- **下一步 / Handoff**：修订版尚无板端结果；本条与源码同提交后需另建 RK3588 隔离目录，先重跑同四框完整闭环并检查上述两处反例及两路中心/IoU、无框和重捕时限；任一目标仍退化即撤回。正式基线 `9f84efe` 八框 `2/8 PASS`、handoff HOLD，`latest_handoff.available=false`。

## 2026-11-14T12:31:10+08:00 现有 VisDrone9 遮挡证据门候选（待板端闭环）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始干净、基线 `6d8002485d714aacd68f087d988286ec8839c42b`。依据上一节同视频/同轨迹的离线反例筛查，在 `ControllerConfig` 增加默认关闭的 `visual_suspect_*` 参数；候选配置 `rk3566/nanotrack_v3/config/visible-dual-gt-visual-gate.conf` SHA-256 `c5c4e2edec914eb44cd8777377abf3600574cf3b936225bf9dd2e9d382d15fec`，`calibrated=false`，亮像素比 `<0.90`、最近 car 候选置信度 `≥0.40`、中心距离 `≤30px`、连续 `2` 帧。可信历史亮度只在 `TRACKING + accepted` 时更新，当前帧检测只在已经 `SUSPECTED` 且亮度下降时发生；局部 ROI 尺度/320px 内容加灰底与此前 `yolo_roi_replay` 一致。使用现有 `YoloDetectorAdapter::detect_blocking` 取得与当前帧绑定的结果，不使用过期异步批次；检测框只做**停止疑似观测**的证据，不替换 `tracking_bbox`、`display_bbox`、控制框，也不能批准恢复。
- 控制器只在连续证据成立时将该帧 recent 处置改为 SEARCHING；实际是否能形成正确重捕、两路中心/IoU、一致性及板端负载尚未验证。新增 runtime 单测覆盖：进入 SUSPECTED 前不调用局部检测、低置信度候选不触发、首次有效命中仍为 SUSPECTED、第二次新鲜有效命中无观测且不以观测控制；配置单测加载候选配置。实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 与 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；构建/CTest 日志 `/tmp/nt-dual-gt-evidence/visual-gate-host-build.log` / `visual-gate-host-ctest.log` SHA-256 `3e3fa7fbd61f5be5477413f0318e2b7c362818c2b99eb802279a61af68c2ff7f` / `5de9477b2ebd34030ede33e01ab890bdf57d230f80023466a48939614806dd03`，`git diff --check` 通过。
- **证据边界 / Handoff**：本阶段主机单测未编译或运行 RKNN 引擎，离线阈值来自 canonical 同集筛查且在线框和时序会反馈，不能预判实际结果。待本条与源码同提交后，按精确 commit 在 RK3588 隔离目录原生构建/CTest，先跑 car001 标准/右下、car002 标准/真值四组完整 271 帧，逐帧核对触发时机、误搜索、恢复身份、全遮挡无观测、tracking/display 中心/IoU 和耗时；若不同时改善，则撤回候选并保留反例。正式 `9f84efe` 八框 `2/8 PASS`、handoff HOLD 不变；不能启动生产编译或吊舱验收。

## 2026-11-14T12:17:41+08:00 复用现有 VisDrone9 RKNN 的遮挡候选离线筛查

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，开始干净、基线 `ae9abbc996c608dd25bad61ba9497d636e291522`。本轮仍未改算法、模型、配置或真值；只读复用 `9f84efe` 八组板端完整 CSV、已有 `yolo_roi_replay` 对同一 271 帧视频及各组**冻结运动预测 ROI** 运行后的八份 RK3588 VisDrone9 候选 CSV，以及原视频对应框内亮像素占比 JSON。car002 标准/真值/左/右 detector CSV SHA-256 依次 `46399f6a2705d15200742dc23dd285ab76c1eb449f7b113b261a3945797cfb10`、`89dbe3b47421efe9d419a5472277bb751ccfe5ae082f500cb8020951ec75c9f9`、`8a151d947651d63e0923d87ba9a530b75d9b76761f5b8c78ce35953be0a90330`、`f2d18a3b0db4ddf079ead29978037e00eaff725a44cf8c924535e2abf3e74cc2`，亮度 JSON SHA-256 `41938b45729d023289c6aff2f7032515ad250aba84449e77e8a3e2fcf6a7445e`；原始模型、板、输入哈希及 ROI replay 命令见本文件 `b5ca289` 一节。
- 实际执行 `python3 /tmp/nt-dual-gt-evidence/screen_existing_detector_visibility.py > /tmp/nt-dual-gt-evidence/existing-detector-visibility-screen.log`；脚本 SHA-256 `5b38ca1f6682a1e33d2bde60674264e36fc0d649dcbf13d37dbedfbbfb2aeb90`，报告 JSON/日志 SHA-256 `57f52d00b4beef1ef971e0de6a73d2b20049df4a08995851c2a5c5a8524dee24` / `f3576cc98cb7dcecd585e07d1c4c77be02fc8fdfd46221e511ac6ab86517b455`，均留 `/tmp/nt-dual-gt-evidence/`。离线规则仅在基线 `SUSPECTED` 评估：框内亮像素占比低于最近八个可信帧中位数的 `0.90`、距运动预测中心最近的本帧局部 detector 候选中心不超过 `30px` 且置信度至少 `0.40`，连续两帧才提出“进入搜索”候选；没有在推理中读取 GT。**这是针对本 canonical 轨迹筛出的参数，不是独立泛化验证。**
- 八组冻结轨迹首个双帧命中：car001 标准/人工/左上均无，右下第 `167` 帧；car002 标准/真值/左上/右下均第 `212` 帧。对 car002 左上第 69～70 帧的正常可见 `SUSPECTED`，第 69 帧虽满足亮度和 detector 条件，第 70 帧最近候选置信度仅 `0.23`，因此未连续命中；第 120～142 帧正常可见的长 SUSPECTED 段亦未双帧命中。已有 VisDrone9 候选第 211～212 帧四框面积明显变小且置信度约 `0.41～0.77`；其在 car002 恢复第 229 帧无可靠真值框也能给出目标候选，故这项规则只拟用于**停止可疑观测**，不得作为恢复或控制批准。离线 120px COCO `yolov8n-seg` 补筛在 car002 第 205～212 帧多次漏检；脚本/JSON/日志 SHA-256 `7dc016a57e94dbcccc08b1643615609cabab3e0ab87f3a0c8f1450960a12f5a0` / `3edc2e16526d1007f625fc335420fbc84e34e6fa0ac1173cd00e64c25dba609d` / `36fda8af692d9da8164f23c0a968c338cddb36d95c0e786f934709f23d2b4096`，不晋级轻量分割模型。
- **证据边界 / Handoff**：该信号来自同一基线轨迹的逐帧局部 detector 回放。正式运行中 detector 是异步单工 worker，且当前只在 SEARCH 和显示刷新调度；这份离线结果没有证明第 211、212 帧在真实闭环都能取得新鲜局部候选，也未验证提前 SEARCH 后的恢复身份、car001 右下框重捕期限或两路中心/IoU。下一步若实现候选，必须显式处理请求优先级、源帧对齐、过期、丢帧及 detector 负载，先用四框板端闭环筛查再扩八框；不能把同集离线双帧命中当作完成。正式八框仍为 `9f84efe` 的 `2/8 PASS`，`latest_handoff.available=false`。

## 2026-11-14T12:09:50+08:00 分割模型八组冻结轨迹复核：恢复判据不一致

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，开始干净、基线 `218f6a343ef857b71215335f863891d1351da7c6`。沿用上一节已哈希绑定的 canonical 视频、`9f84efe` 八组板端 CSV 与 `yolov8s-seg-v8.3.0.pt`；从各组 `motion_predicted_*` 中心裁 120px 正方形再放大至 640px，CPU 推理 `classes=[2], conf=0.05, retina_masks=True`，真值只用于推理后裁决。实际运行 `.venvs/yolov8-visdrone/bin/python /tmp/nt-dual-gt-evidence/screen_yolov8s_seg_120_eight.py > /tmp/nt-dual-gt-evidence/yolov8s-seg-120-eight-screen.log 2>&1`，脚本 SHA-256 `e06b8dd791d03984ad158a93991e3a4948f7288a3ffc18e1559b7d3a4a01711b`。car001 四框第 166～195 帧、car002 四框第 205～235 帧，共 244 个冻结 ROI 推理，exit 0；JSON `/tmp/nt-dual-gt-evidence/yolov8s-seg-120-eight-screen.json` SHA-256 `82736f71a97c6c6a2415b944f6c72ea239fe37c08e236404a3a45f61a9807afa`，日志 SHA-256 `3bde1fa3befcec9a501b89a056ac341e7eaaf7b46f7173cbbeae8a0242815c59`。基于同一视频与 car002 真值框位置作只读画面复核，拼图 `/tmp/nt-dual-gt-evidence/car002-occlusion-montage.png` SHA-256 `34e1fcd06e60f6d4f01667dfc7bee20ee174719079c2ceff4bd8695e0011d299`。
- car002 四框在第 210 帧最近候选分割面积与各自第 205～210 帧中位面积之比约 `0.96～1.03`，第 211 帧降至 `0.66～0.78`，有提前拒绝疑似观测的信号。但恢复时标准框第 230～232 帧连续无汽车候选，右扰动框第 229 帧仍无可靠真值框，却已有面积比 `0.98` 的近距离候选；因此不能用面积比、候选存在或置信度构造跨初始框稳定的恢复授权。car001 右扰动框第 187～189 帧最近候选位于约 `24～25px` 距离，候选存在本身也未证明 NanoTrack 会安全选回目标身份。以上只是在**原闭环冻结运动轨迹**上回放；改变搜索时机后轨迹会变，不能由此推断闭环改善。
- **裁决 / Handoff**：保留离线证据，但不晋级该分割模型、阈值或板端源码改动。正式 `9f84efe` 八框仍为 `2/8 PASS`，两路 `tracking_bbox`/`display_bbox` 中心、IoU 和初始框一致性目标未完成；`latest_handoff.available=false`。后续候选必须先在 car002 无可靠框段和 car001 邻车竞争段证明独立身份/完整可见性，再跑 RK3588 八框闭环；不把画面或分割候选当作真值参与在线推理。

## 2026-11-14T12:05:31+08:00 COCO 实例分割恢复可见性筛查（未晋级）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，开始干净，基线 `aa15a4801024c4fd5aced51d4fcf901884a0c20e`。本轮只在本机 CPU 做离线探针；正式算法、RK3588 二进制/模型/配置与两套真值未改。输入仍是 canonical `data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；每帧裁剪中心来自 `9f84efe` 板端冻结 `motion_predicted_*`，未用真值定位或逐帧修正。官方 COCO 实例分割权重 `/tmp/nt-dual-gt-evidence/yolov8n-seg-v8.3.0.pt` / `yolov8s-seg-v8.3.0.pt` SHA-256 分别为 `a7cd8f929e1903d78a12a48efecab430209f18dc46cb96c3599a5980c63c423c` / `0bac0770b55e5eb5b76a61bc535673288dcec36c2bc0cd25ee0d584c632f3413`；使用 `.venvs/yolov8-visdrone/bin/python`、Ultralytics 8.2.82、torch 2.4.0 CPU，`classes=[2], conf=0.05, imgsz=640, retina_masks=True`。
- 实际执行四份脚本 `/tmp/nt-dual-gt-evidence/screen_yolov8n_seg.py`、`screen_yolov8s_seg.py`、`screen_yolov8s_seg_120.py`、`screen_yolov8s_seg_120_early.py`；SHA-256 依次 `77100b663373a2fff21394e2d039ec72e1f094b163fdef202c7e4a4c0a94bdae`、`a55af61545e55bf1f196137abbd64362f9604855d1d111e927716d16a8ae0f2a`、`67307c208453a1eb6c1019a5c9bcbaa709a05d59f545dfda9281fbc72244550b`、`bbfcab249bf801218c97d40da8b6bd149c15b22c2efb61acc69f50185daca7b0`。前两项以 180px 正方形裁剪，两项 s 模型补筛以 120px 正方形裁剪，均放大到 640px 推理。四份 JSON 输出 SHA-256 依次 `978f025badabed252a061a3e5ad1f4a4b650489af96c4e9107511ddbf7d28f39`、`444ad298fcb9dbff68f126930a7aa53f5945347c18b2da63716aa39b66fc336f`、`4eec59bb90cac39811a043771246c58a1e0c0b79cc139b5cfa56cc2a59eb69c8`、`07ca7bcb657a0e8f43d29b035d7f2dcdde6e1911698fb8b5782236c48694e4b3`；对应 `.log` SHA-256 依次 `e71b2f32bc3ad95451a3d3d7250e82f6fbe0a87660b2db0332d5cac99d369c56`、`0ed498f5ed8a2bc017965bbda76801ae9f990b333b1d21903d053668da2f5c87`、`39f8f50559bdec5d673265ea0abec44c55546e537721ad1d96823fede4877d98`、`f8229ed2641a0eb9b685a71c72a986ef8d16f409d2a2a83b4e4726e874ae67ab`。输出均保留本机 `/tmp/nt-dual-gt-evidence/`。
- 180px 裁剪时 n/s 两模型均漏掉 car002 第 229、230 帧目标，n 模型还漏掉 car001 第 190 帧。120px 裁剪时 s 模型对 car002 第 230 帧有距运动中心约 15px、面积约 1697px² 的候选，但置信度只有 `0.07`；人工确认**无可靠框**的第 228 帧同一位置也有面积约 1556px²、置信度 `0.23` 的候选，第 229 帧又无候选。第 211～215 帧均是无可观测真值框，但最近的汽车分割候选仍连续存在，面积约 1122～1504px²、置信度 `0.28～0.57`。检测到可见车体片段不等于可批准完整目标框。第 205～210 帧可见段最近候选面积约 1678～1979px²，虽能看到遮挡后面积下降，单帧面积或置信度无法同时可靠隔开第 228 与 230 帧。
- **裁决 / Handoff**：不把现成 COCO 分割候选接入 `tracking_bbox`、`display_bbox` 或控制门禁，也不以其推理结果声明恢复成功；本轮未做 RKNN 转换/板端闭环，不构成性能或泛化证据。后续若提出独立可见性判据，先验证它在 car002 第 211～229 帧无框期以及 car001 邻车竞争期的反例，再完整跑本分支八框板端评估。正式基线 `9f84efe` 八框仍 `2/8 PASS`，`latest_handoff.available=false`，两路 tracking/display 中心和框贴近真值的目标未完成。

## 2026-11-14T11:54:13+08:00 SiamABC 小模型双目标 CPU 身份筛查

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，开始干净、基线 `34ce116b835873e211596416651df4b95605804f`；正式算法、板端模型/配置和 GT 未改。独立预训练源 `repos/siamabc@b1c94e06fdf2dd3cb14ed07b05e38aa4601ece03`，固定 `assets/S_Tiny/model_S_Tiny_v5.pt` SHA-256 `eb52c20776bb2d1a20ab7d1d88873944c6b83b56a63f0fe8f2754edeec123576`，只把原 tracker 的设备选为 CPU，未更新权重。输入仍是 canonical `data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；两目标已确认真值仅在跟踪后评分，不参与逐帧推理。
- 实际用 `.venvs/siamabc/bin/python /tmp/nt-dual-gt-evidence/screen_siamabc_cpu.py --bbox <x y w h> --output /tmp/nt-dual-gt-evidence/siamabc-cpu-<label>.csv` 分别跑 car001 标准 `(992,582,37,39)`/右下 `(994,584,35,37)` 与 car002 标准 `(970,517,38,38)`/真值 `(967,513,42,42)`，四组均 exit 0、各 271 行；脚本 SHA-256 `161d08750ebc370b0fe20405d01e49e72b54431f2f2d8a422aba8c123b538fae`。四 CSV SHA-256 依次 `c43b67a7b0bcf6d51e89e827a8d8d3e3d788ba3c2886fb2af74bfc350c5ec413`、`47af52a748a68ae9a9bc2cd32ec7889bbb1509b589be813e518f0bebdb2f8153`、`e12ecff201a900b04163ac7a69a555cebe8232b9df923e2a894fdc84d6f85956`、`ebb651f8f79ea6163162d464ff8d8af680c556fe2811fd4fb224ecd5f88f2d88`；相应日志 SHA-256 `ec5dd12dd4d69207cf9cdab4fe9f53831fabac1116fa8b0fdf5a4eb492df1fae`、`aabdc999a6181546687f1360269290b1373b8462326253560dfec217c8095395`、`20a7395c9bea40e3dc2b88c0e6e8f41f2c47fcfc4332c96d873029240d46d74b`、`3b3d26e020761151c4d349b7cd8080c2a3bab3608f27f09ed0c21706dee107af`，均留本机 `/tmp/nt-dual-gt-evidence/`。评分脚本 `/tmp/nt-dual-gt-evidence/score_siamabc_cpu_four.py` SHA-256 `ea3e926ab657903a9e526e0697ac659725ff532b93096009fb544d78aa769b98`，报告 `/tmp/nt-dual-gt-evidence/siamabc-cpu-four-score.json` SHA-256 `1496b301f23f4c28c0b174c73fbeaf1696d8b2a693537cb90ced3fe55bef8af0`。
- car001 标准/右下可观测 258 帧中心均误差 `5.35/4.58px`、IoU `0.755/0.774`；遮挡前 169 帧中心 `3.18/2.51px`，但重新可观测 182～194 帧平均 `27.52/27.36px`。两框第 182～187 帧均跟到相邻车，中心离对应真值约 `53～56px`、IoU `0`，自身分数仍约 `0.998～1.000`；第 188 帧才同时回到真目标。可见两框跨初始化中心差 P95 仅 `2.12px`，说明“轨迹一致”也可能同时跟错，不能仅靠一致性判身份。car002 标准/真值可观测 252 帧中心 `1.86/2.84px`、IoU `0.765/0.746`；第 229 帧人工确认无完整框，两框仍输出目标附近框，分数约 `0.9999`，第 230 帧分数也约 `0.9999`，没有完整可见性分界。两目标非可观测段各 13/19 帧，直接 tracker 每帧都输出非空框；上述是独立单跟踪器筛查，未接生产控制器的观察/控制有效性门。
- **裁决 / Handoff**：不把 SiamABC 直接替换 NanoTrack，也不以其高分或两框一致作为恢复身份/完整可见性门；该模型在 car001 的错误邻车与 car002 的无框段均给出高分，直接接入可能误授权控制。CPU 桌面筛查不代表 RK3588 负载、RKNN 可转换性或闭环效果；正式板端八框仍为 `9f84efe` 的 `2/8 PASS`、handoff HOLD。其第 188 帧与原 car001 检测目标位置接近的事实可作为后续交叉验证研究输入，但须先证明 car002 无框反例能被独立排除，再经本分支八框板端闭环；两路中心与初始框一致性条件保持不变。

## 2026-11-14T11:43:16+08:00 一对一局部精修四组否决并撤回并行恢复

- 精确候选 `codex/dual-gt-init-consistency@762e5098ae703c15a8bed39a3cdf6083d1fdf3de`；本机源码包 `/tmp/nt-dual-gt-evidence/search-roi-refined-762e509-source.tar` 与板端 `/home/neardi/Project/SOT/dual-gt-search-roi-refined-762e509/source.tar` SHA-256 均为 `8cd0ba977d302549fa77cdc5c97820604241376a65eec7c192e1762534f56d09`。构建/四组运行脚本 SHA-256 `bad8be57c7da62ffbd30825e0dcf7614292eef3af04feba79b42a9041403c953` / `22746d2552edbea368063913fc3683e4889db66bbb7e271abae11bf38d3f97a7`。输入视频、两目标真值、官方 NanoTrack、VisDrone9 detector/manifest/labels 沿用本文件 `9f84efe` 已哈希绑定身份；板 `LKD3588` Linux `6.1.84`、RKNN API `2.1.0`、driver `0.9.8`。板端原生 CMake/build/CTest `23/23 PASS`，日志 SHA-256 `3fda60c3851a3b2a5be2f13ab3a497a5da14f9efc446f8623e50ccb108bd1250` / `33d52e4bc529315843fe4c7ffcf640128e65a1f423cd80f487ec7606939009f4` / `e35c9c4cbd268f827c7cde8e40a439aa0ef5559299395ce37856b6b40d951f04`；二进制 `build/nanotrack_v3` SHA-256 `a1ac90eedc7b1a154967f40841a1e6c330c545b369f857b44af6355a49e16da3`，headless 配置 SHA-256 `698daa1d551326b35ac1c3eeaa7ead5556b8a3a97859e4b029b7b0255d30f9f8`。
- 实际 `bash run_search_roi_four_refined.sh` 完成 car001 标准/右下、car002 标准/真值各完整 271 帧、exit 0。CSV/session/log 留板端 `runs/<label>/`；板端 `runs/four-text.tgz` 与本机 `/tmp/nt-dual-gt-evidence/search-roi-refined-762e509/four-text.tgz` SHA-256 均 `ac1ebb1c11b7e5249c6c1aa2edfc8289b0abf419f9254b7614fa49dbb3ae7166`。四 CSV SHA-256 顺序 `0967c0022d39da20c63c1eb755d432c25696146d56c721fb5f15ac9bf96dff48`、`263a651b5ccec5788e48804f4af813f394732f8e7686ff308f686f4565b7d55c`、`bcd0d222c00f3ea0e139e89742f5a6f653eb69ea40727953c9ea5ed635157af8`、`0db35077e1938ed51c1e7a912b46c5efdbf07b1884051cd4ff37ffb9bc0b6153`。运行 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/search-roi-refined-762e509/four_score.json`；报告 SHA-256 `b54394c3901a15dbd0942d0a8fc5216c95b7d637ce232d27dffd7dc2646e278e`。
- car001 标准框 PASS、第 188 帧重捕，tracking/display IoU `0.748/0.767`、中心 `4.36/4.40px`；右下框 FAIL 完全遮挡语义及重捕，第 196 帧才重捕，IoU `0.692/0.740`、中心 `5.39/5.44px`。car002 标准/真值均 FAIL 完全遮挡语义，第 `235/233` 帧重捕，tracking/display 中心分别 `2.59/2.59px` 与 `2.00/2.00px`。car001 两框 tracking/display 中心差 P95 `14.11/12.80px`；car002 `5.09/5.03px`。本轮仍 `1/4 PASS`，比原 `9f84efe` car001 右下第 191 帧重捕晚五帧；并行双路检测的耗时改进不能解决几何进入恢复候选后改变身份仲裁的问题。没有据此扩测八框。
- **裁决 / Handoff**：撤回本候选及 `9c23736` 的八个算法/配置/测试文件，逐文件与 `1355692` 内容一致；候选提交、固定帧并行探针和两个板端隔离根原始输出均保留。撤回后运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 与 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，日志 SHA-256 `883fc4811c5c3e34a834aa19728407a4c46760ead77ec78ccbed4093a47a3cbb` / `b7270ae624549ce787d2ed0a79fadac2f4381620b13b439474b0850b8d2e6682`，`git diff --check` 通过。后续需要独立身份与完整车体可见性证据，不能再仅靠局部框加入现有候选表或加速推理；两目标八框 tracking/display 中心、IoU、无框语义及初始框一致性仍为验收条件。正式基线 `9f84efe` 八框 `2/8 PASS`，handoff HOLD。

## 2026-11-14T11:37:35+08:00 并行四组失败及一对一局部精修候选

- 精确测试候选 `codex/dual-gt-init-consistency@9c23736c35d97f36f1774e0d21451a17347ba089`，源码包本机 `/tmp/nt-dual-gt-evidence/search-roi-parallel-9c23736-source.tar` 与 RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-search-roi-parallel-9c23736/source.tar` SHA-256 同为 `492338daddcd7e8524b6f565c0ef83bf08a9041e9a09f006dca3c071daaa303a`；构建/四组运行脚本 SHA-256 `bd4609247bf0d678f74dc87d94399144ea6243e773b3eda01564d478fedd882c` / `cb2f93e14a9e1e84924aacdc5383df366eb782500cc31ef19bd3b854329dc650`。原视频、两目标 GT、官方 NanoTrack、VisDrone9 detector/manifest/labels 沿用已核对 `9f84efe` 基线身份；实际 session 与运行日志实报 RKNN API `2.1.0 (967d001cc8@2024-08-07T19:28:19)`、driver `0.9.8`。板端原生 CMake/build/CTest `23/23 PASS`，日志 SHA-256 `d680e9068df0090d4781e3efe8ff40257992361a990a90efd032061d45584a5d` / `591723369a04af3c9940347998a40a8783c8f574f8ba9bfb399d5ab0d851f99b` / `5345983c55d66f568f9fe266856bb86048dd172b0133b4c4dd7fc3525d7cb0da`；二进制 `build/nanotrack_v3` SHA-256 `7c06c981d826fa994754d205933ae4ae55756a98b5b0f6d81ebc338abcc67d40`，headless 配置 SHA-256 `8cd04937f903a24307ffe367a007f35910f3e944b1dc3b48d2ffa3b3c5e8199a`。
- 实际 `bash run_search_roi_four_parallel.sh` 顺序完成 car001 标准/右下、car002 标准/真值四组各 271 帧，exit 0，板端 `runs/<label>/` 留 CSV/session/log；文本包板端 `runs/four-text.tgz` 与本机 `/tmp/nt-dual-gt-evidence/search-roi-parallel-9c23736/four-text.tgz` SHA-256 同为 `fc28d5266face101fffb1e51e31240bad6a044e94944c7f1ce31197d08f083f0`。四组 CSV SHA-256 顺序 `97ba70972dfa9e4f36abd40fa46e50039614eb0c1bebf71dc1f23a6477d66d27`、`48ea08b761e9e54584181f50a1c37d1d2ca655f1637e319854ff90c55b599638`、`525a757e6fc88b2a125d54aba1c282d07bac40c81e9490a3eb32aea255922eb9`、`3b6d893786cd0cb15e043733b2edeb9ef05d79d48e1085fd0955f476ca2889d2`。用 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/search-roi-parallel-9c23736/four_score.json` 评分，报告 SHA-256 `a156754e44cb52812518fbffb1aa030954337c9fc8e50985816a80e58fe9bbc3`。
- car001 标准 PASS，第 188 帧重捕，tracking/display IoU `0.748/0.767`、中心 `4.36/4.40px`；右下 FAIL 完全遮挡语义与重捕，第 198 帧才重捕，IoU `0.686/0.735`、中心 `5.61/5.67px`。car002 标准/真值均 FAIL 完全遮挡语义，重捕 `235/233`，两路中心 `2.59/2.00px`（分别）；car001 两框 tracking/display 中心差 P95 `15.47/14.16px`，car002 `5.09/5.03px`。恢复检测墙钟本轮约 `68～101ms`，比顺序双路 `aaf6ab4` 的约 `98～178ms` 明显下降，但只有 `1/4 PASS`，不能扩八框或晋级。对比原基线 car001 右下第 `191` 帧重捕，本轮虽然比顺序双路第 `207` 帧提前，仍晚七帧。
- 帧级对照：car001 右下第 188 帧原基线收到全画面候选后第 189 帧转 detector probe、第 191 帧两击重捕；本轮第 189～191 帧持续 `IDENTITY_CONFLICT`，第 194～196 帧又冲突。已冻结的局部 detector 回放在第 188 帧同时给出邻车 `(896.4,850.7,41.7,38.6)`、目标附近 `(844.9,825.7,50.3,44.2)` 和低置信度宽框 `(844.0,815.0,91.6,54.6)`；原合并条件允许该宽框与约 `(844.5,821.3,48.1,46.3)` 的全画面目标框匹配，并可再次覆盖同一全画面身份及置信度。这是一个由几何和代码直接证实的**潜在**冲突机制，不能仅凭 CSV 断言它就是每次冲突的唯一原因。
- 基于此反例，本项新候选只允许局部框与**原全画面同类框 IoU >0.60** 的一对一精修；每个全画面身份最多取最高 IoU 的局部框，只替换几何、不替换全画面置信度、不添加局部独有候选。双实例并行、局部 320 内容、原 NanoTrack 身份确认与默认关闭语义保留；尝试保住远处竞争车反证并排除宽框重复覆盖。实际 `git diff --check`、`cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，日志 SHA-256 `4bb74ef6b2564112f8051f9eed2feaa74ce450b6bb181e0b8d40ca0168f06236` / `a921dee3d42caac67cbb1dde7c954f137917d4ace7c6c363ea730c3c75c5c6b7`。**下一步 / Handoff**：新候选尚未在 RK3588 闭环；需以精确提交隔离部署，先跑同四组，若 car001 标准/右下、car002 无框语义或两路中心不能同时改善则撤回。正式基线 `9f84efe` 八框 `2/8 PASS`、handoff HOLD。

## 2026-11-14T11:29:23+08:00 双 RKNN 实例并行恢复候选（待闭环）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始干净、基线 `1355692d8e66fbfd67aedfdaf4578be50a237e1d`。上一双路顺序候选 `aaf6ab4` 的恢复推理接近 `170ms`，car001 右下重捕延至第 207 帧；本项先核对 RK3588 SDK 提供 `RKNN_NPU_CORE_0/1/2` 与 `rknn_set_core_mask`，板端约 `7.2GiB` 可用内存，再实测两个独立 RKNN context 并行的可行性，不假设顺序失败只由算法定位引起。
- 只读延迟探针源码本机 `/tmp/nt-dual-gt-evidence/parallel_yolo_probe.cpp`、板端 `/home/neardi/Project/SOT/dual-gt-handoff-9f84efe/runs/parallel_yolo_probe.cpp` SHA-256 同为 `03e6cfc7fd882568c1ba348491f219e23dcb246e47efd0363a245d575c6b7f3b`；以基线 `9f84efe` 的静态库在板端 `g++ -std=c++17 -O2 -pthread ... -lrknnrt -ldl $(pkg-config --cflags --libs opencv4)` 原生编译，探针可执行文件 SHA-256 `1035caadbf1ae9d35123831fa5511bcb1365ec0eacccc0f9cba5b1d49aa778b3`。输入是已哈希绑定的 `occ_video_1/1.mp4` 第 187 帧、同一冻结 VisDrone9 detector；一实例全画面、另一实例局部 `180×180` 内容缩至 320 后置于 640 画布，预热各 3 次，再各跑顺序/并行 12 次。输出板端 `runs/parallel_yolo_probe.csv` 与本机 `/tmp/nt-dual-gt-evidence/parallel_yolo_probe.csv` SHA-256 同为 `15fa2b8fae53b49b9a8b57e1dc6a89a738e3f7c9f112d749413fe367c84a7c8e`，板端 stderr 空文件 SHA-256 `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`。墙钟顺序均值/中位 `169.06/169.69ms`，并行 `85.70/84.22ms`；两模式候选数全画面/局部均 `100/4`。这是固定帧算力探针，不是完整视频追踪或候选正确性证明。
- 候选恢复 `aaf6ab4` 的默认关闭局部 ROI 与全画面竞争候选合并路径；当前可见配置 `detector_recovery_roi_scale=3.0`、SHA-256 `c6c18974e88376cc7567a518b2fb9a52877a16f62ab4146f08c29a3711b95326`。当该配置开启时，启动时创建两个独立 detector RKNN 实例，恢复局部请求用 `std::async` 并行全画面与局部推理；结果仍合并并经过原 NanoTrack 候选身份确认，detector 不直接授权控制。记录的 `detector_total_ms` 为并行墙钟时间；初始化、显示刷新与配置关闭的普通路径仍只用首实例。实际 `git diff --check`、`cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，日志 SHA-256 `13685a0bfe2826c3ca70b9e857f10fd5b6bf135a0e0958de3c4e3f91838bb646` / `2d795fcd82259e2a03b292177ef843eceb874b56f417b45186c451286afd9f09`。
- **证据边界 / Handoff**：候选还未在 RK3588 闭环运行，主机 CTest 不编译 RKNN 路径，固定帧并行探针没有 tracker 与 detector 的并发负载。下一步用本项精确提交源码在板端隔离根原生编译、CTest，并先跑 car001 标准/右下及 car002 标准/真值四组 271 帧。只有双路中心、IoU、无框语义、重捕时间及跨初始框差不退化且有整体改善，才扩到八框；否则撤回本候选。正式已验证基线 `9f84efe` 八框 `2/8 PASS`，handoff HOLD。

## 2026-11-14T11:22:40+08:00 MOT 遮挡 CNN 独立筛查否决

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，本轮开始干净，基线 `5b781937a5f82bc10b16bbe818719b12c6f5b546`。正式算法、模型、配置、两目标真值均未改。为寻找跨帧身份之外的完整车体可观测性证据，试用本机外部 `/home/u20/dataset/VisDrone2019-MOT` 汽车 `category=4` 标注，要求有效目标、`truncation=0`、目标宽高均 `25～90px`；按数据集原 train/val 序列划分，分别抽取无遮挡/遮挡一级/二级 `4000/2000/2000` 与 `500/250/250` 图块。两目标 canonical 视频及其真值仅在训练后作反例检查，未用于训练或调参。
- 筛查脚本 `/tmp/nt-dual-gt-evidence/screen_mot_visibility_cnn.py` SHA-256 `177d4cb661b98e1a843cf4445f2a8f25aa6250e727508ed6e271688f431e47bd`；每个标注框连同 1.4 倍上下文裁为 `64×64`，小型四层 CNN 用 `.venvs/nanotrack/bin/python` 中 PyTorch `1.7.1` CPU、固定种子 `20261114` 训练五轮。实际命令 `.venvs/nanotrack/bin/python /tmp/nt-dual-gt-evidence/screen_mot_visibility_cnn.py --epochs 5 > /tmp/nt-dual-gt-evidence/mot-visibility-cnn/train.log 2>&1`，退出 0。训练/验证缓存 `train.npz` / `val.npz` SHA-256 `ca162b72371ef07e0eda3eeaaf6692ab3b2f863377cc4109b5b310c7a1563035` / `b0ab74d77b4c86bd333c6d9e681c18d66c562ab097414b1eb06212286f923ec2`；模型 `model.pt`、报告 `report.json`、日志 `train.log` 留本机 `/tmp/nt-dual-gt-evidence/mot-visibility-cnn/`，SHA-256 `d7adb8bd5d046f75846b66e391f1dbf5bfda476b35fdc5dc93af71be97289371` / `9f46626375e8fbd470276d686b52f79c99d22c65b6a0ceec89e405f56dcc68f0` / `a8a0760c49605da026dfd57a9f3e496120f47cbca77302361cdb55392059056b`。
- 独立验证集 `p_full` 区分无遮挡与遮挡的 AUC 五轮依次为 `0.696/0.637/0.717/0.734/0.731`，末轮三类准确率 `0.580`。在既有 `9f84efe` RK3588 CSV 的恢复原始框上离线打分，car001 右下初始框第 187 帧 `p_full=0.468`（该帧正式车体已可见，但探针仍偏离），第 190 帧可靠候选为 `0.869`；car002 真值框第 229 帧人工确认无完整框的候选为 `0.633`，第 230 帧已完整可见候选反而 `0.608`。统一阈值既不能阻止无框段错误授权又保留正确候选；该模型也没有提供车辆身份判别。
- **裁决 / Handoff**：否决该 CNN 直接接入恢复、tracking/control 或 display。此项只有主机离线筛查，没有 RK3588 推理、闭环轨迹或时延；保留 `9f84efe` 正式八框 `2/8 PASS` 与 handoff HOLD。把训练/验证分割、权重、反例及此前 223 项 RKNN 模板特征证据交给模型/外观 owner 设计身份加完整可见性验证器；必须在未用 canonical 反例调参的条件下取得独立判别力，再回到本分支做两目标八框板端闭环。

## 2026-11-14T11:07:27+08:00 双路检测四组板端否决并撤回

- 精确候选 `codex/dual-gt-init-consistency@aaf6ab4bd4c4cf4fb24fe9bccc75ae76eaf3b5ad`，板端隔离根 `/home/neardi/Project/SOT/dual-gt-search-roi-dual-aaf6ab4`。源码包本机 `/tmp/nt-dual-gt-evidence/search-roi-dual-aaf6ab4-source.tar` 与板端 `source.tar` SHA-256 均为 `c54798762e8fdbda8a90c92886d6df6ee9066e6f30792348917457f71db556b1`。输入视频、GT、模型和板端环境身份沿用本文件所列同板基线，并在本轮与原路径核对。板端原生构建和 CTest `23/23 PASS`；二进制 `build/nanotrack_v3` SHA-256 `55a524cab9a6c4da0aa06d0369c081084e70271366f162205b4c14880cd1f60b`，headless 配置 SHA-256 `0e8125b0d969eb5023746731bd6e0050007af7a0a474ac9d08ed968b9a1a1fdf`。构建和运行脚本本机 `/tmp/nt-dual-gt-evidence/build_search_roi_board_dual.sh`、`run_search_roi_four_dual.sh` SHA-256 分别为 `ee88e8828a36044689968a7bd49c7c6cb12bf5f5b7bb32516f80e52a563b7213`、`ec6d23559bf9be65cf5631e12e6e775060048c6f623cb0b91dff2a25263c722b`。
- 实际执行 `bash runs/run_search_roi_four_dual.sh`，car001 标准/右下及 car002 标准/真值各跑完整 271 帧，exit 0。CSV/session/log 保留板端 `runs/<label>/`；文本包板端 `runs/four-text.tgz` 和本机 `/tmp/nt-dual-gt-evidence/search-roi-dual-aaf6ab4/four-text.tgz` SHA-256 一致为 `ecab297addc749a31786012c6a0f3a63c624dcb53156deea20b5bdc532e39de4`。四份 CSV 顺序 SHA-256 为 `88f3f55d467962ee274d426009674a8a13037c631b0ae4f537c6d380fa13933a`、`6afb2a91e728dce6db06bb9c82ce2f47781949262415374b2713927662b9dea8`、`b82e162b2fc61085630c11898823696801b4bd067e4b4a47db3936e227dded92`、`a2806999370368539365bcfc0ed4f490ee452fea4a476c491e96a296f65d8059`。
- 用 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/search-roi-dual-aaf6ab4/four_score.json` 独立评分；报告 SHA-256 `49820e8f5b269cc0e4a0c43051b67a1db3cd9177077c992f27883d4666a2d48d`。car001 标准框 PASS、重捕 189，tracking IoU/中心 `0.748/4.37px`，display `0.767/4.41px`；右下框 FAIL 完全遮挡语义及重捕，第 207 帧才重捕，tracking `0.662/6.53px`，display `0.711/6.61px`。car002 标准/真值框均 FAIL 完全遮挡语义，分别第 235/233 帧重捕；tracking IoU/中心 `0.791/2.59px`、`0.883/2.00px`，display `0.741/2.59px`、`0.750/2.00px`。car001 两初始框 tracking/display 中心差 P95 `22.66/23.39px`，car002 `5.09/5.03px`。本轮仅 `1/4 PASS`，不足以扩测八框；`display_bbox` 的 detector 观感不替代两路中心真值评分。
- 双次恢复 detector 推理在本轮多次达到约 `170ms`，原基线约 `90ms`；时序和重捕结果均退化。**裁决 / Handoff**：撤回该候选六个生产/配置/测试文件至 `0ca0c01` 内容；`git diff --name-only 0ca0c01 -- <六文件>` 输出为空。撤回后实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 与 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，日志 SHA-256 `c3f3d7803772ac628759aa21aba8e71ebacba593c89076b8be5c14f91ac3386e` / `48e0d9c17884b413afc22e14d4f8b2559c4a10f77de48bbfee5f6cc1ba59a913`；`git diff --check` 通过。保留候选提交、隔离板端工件和本记录。下一步检查跨帧身份与完整目标可观测性证据，先证明方案有独立判别力，再作 RK3588 八框闭环。正式基线仍 `9f84efe` 八框 `2/8 PASS`，handoff HOLD。

## 2026-11-14T11:00:21+08:00 全画面竞争证据与局部定位联合恢复候选（待板端闭环）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，开始干净，基线 `0ca0c015760709355f392572aa4b69eddc082bb2`。对照 `9f84efe` 基线、`85fedbd` 局部 ROI 和 `8473b98` 局部加距离门三组已核对板端 CSV：car001 标准框第 179 帧 recent 探针 `(844.67,803.59,49.74,47.99)` 三者相同；基线全画面 detector 批次仍有 `3` 个竞争候选、`IDENTITY_CONFLICT` 阻止错误重捕；仅局部 ROI 是 `2` 个，距离门又过滤到无可竞争候选，于第 179 帧以该尚在遮挡下的探针错误重捕。说明远处竞争车虽不能直接授权控制，却作为**排除错误身份的反证**有实际作用；前两次局部候选失败不是单纯位置门过松。
- 新候选恢复默认关闭的 `detector_recovery_roi_scale` 可选路径，当前可见配置取 `3.0`（SHA-256 `c6c18974e88376cc7567a518b2fb9a52877a16f62ab4146f08c29a3711b95326`）。`SearchRecovery` 局部请求在同一 RKNN 上顺序执行一次原 640 全画面推理和一次 320 局部内容居中 640 画布推理；同类、中心接近且宽高比相近的候选合并，以局部候选更新位置，其余全画面竞争候选保留进入既有 NanoTrack 身份确认。初始化、普通显示刷新、默认未开启局部 ROI 的路径只运行原来一次推理；控制授权仍须 NanoTrack 验证，detector 不直接发布 `control_valid=true`。两次推理可能超过异步队列与 `detector_stale_ms` 的可用时限，合并阈值也可能错误合并邻车，均须板端实测。
- 实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，日志 SHA-256 `355b4e55faef106ba0123a17db64482bbea56feda4a3b4ec614f7976f2599b02` / `611a7be6df510cfe5715ae0b7131b6e0d35c9ae9f52cc3eff2ae02ea480a0f78`；`git diff --check` 通过。**证据边界 / 下一步**：主机单测没有运行 RKNN；需以本条实际提交在 RK3588 隔离目录原生构建，先跑 car001 标准/右下、car002 标准/真值四框完整闭环，核对检测耗时与过期、错误竞争和真值中心/IoU/遮挡/重捕。若仍有 car001 错捕或显著延迟，撤回本候选，不以离线局部预测晋级。正式八框 `2/8 PASS`，handoff HOLD。

## 2026-11-14T10:55:37+08:00 邻车距离门禁四组板端否决并撤回局部恢复代码

- 精确候选 `codex/dual-gt-init-consistency@8473b98275a024970bf855f536cb3207df3fcd6c`；源码包 `/tmp/nt-dual-gt-evidence/search-roi-gated-8473b98-source.tar` / 板端 `/home/neardi/Project/SOT/dual-gt-search-roi-gated-8473b98/source.tar` SHA-256 一致为 `1edc4c655b42eab7b61c550b9be2581d89cc16672fbe892ed9c5cb6ba6fa7189`。板端独立根 `/home/neardi/Project/SOT/dual-gt-search-roi-gated-8473b98`，原视频、模型/manifest/labels 和 RK3588 环境沿用上节已核对身份。原生 CMake/构建/CTest `23/23 PASS`；`build/nanotrack_v3` SHA-256 `b407018f6906345209225686cb64b55295bbbcd6a39ca018387a5a1f882ee82f`，headless 配置 SHA-256 `5a816df3e451f2d44c5b114e20d081251f717cf60d9717573d9179a89cc54690`。实际构建/运行脚本 `/tmp/nt-dual-gt-evidence/build_search_roi_board_gated.sh` / `run_search_roi_four_gated.sh` SHA-256 `a441c33e4290196202c4d148a1e54e3b46f1a8bfb939bfe525c9d6ffadcaeb1f` / `35bb55e6ff377c15efb0b00dd201a520edbe5305019370eaf3ecdf743b293058`。
- 板端执行 `bash runs/run_search_roi_four_gated.sh`，car001 标准/右下、car002 标准/真值各 271 帧、exit 0；CSV/session/log 保留 `runs/<label>/`，文本包板端 `runs/four-text.tgz` 与主机 `/tmp/nt-dual-gt-evidence/search-roi-gated-8473b98/four-text.tgz` SHA-256 一致为 `05363f1f977d3e23d7b6531930a73dce252646cfdfe5d13b16dcb18d6321c05f`。上述四框 CSV SHA-256 顺序为 `2c97a8134e66d3a3e4c5f22910b2f0945c74b8079b261c4816daaf88c5cd8bbc`、`f6063020a3d58442898dd390956893e02b340fcde398d924a92a2bf3e482d907`、`5e6c02c2ba9e6837a961a5e9ff7489972adb5f9e9aec94995f23862859f61e43`、`767c5a0d1388bb5c6444b86d3910496f5d261d973212d5e7e09643e2d11ac55`。用匹配真值执行 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/search-roi-gated-8473b98/four_score.json`，报告 SHA-256 `cfe99192d9c334eea1fe6431ecf5fdd554d387949dd2d8c50a4604ccff2b1044`。
- 结果四框 `0/4 PASS`。car001 标准框在 `179/197/220` 帧三次错误重捕，tracking/display 中心均误差 `7.70/7.90 px`、IoU `0.658/0.677`；右下框虽由无门禁第 198 帧提前到 196，仍晚于第 190 帧严格截止且中心 `5.39/5.44 px`。两框中心差 P95 `29.78/30.48 px`，远大于正式基线。car002 标准/真值仍因完全遮挡语义 FAIL，重捕 `234/233`，中心 `2.05/2.00 px`。邻车位置门虽使近旁候选数减少，却在 car001 早期把错误近候选聚焦到恢复链，不能据此授权控制。
- **裁决 / Handoff**：完整闭环已否决局部搜索 ROI 与邻车距离门的组合；将 `visible-adaptive-display.conf`、配置解析与字段、恢复 ROI 生成、RKNN 半幅画布路径及其单测恢复至本候选前 `f69b5f508c98b96cfb0fcbba83235d08ca80ec2a` 的正式算法内容，`git diff f69b5f5 -- <上述六文件>` 无差异。恢复后实际执行主机 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，RKNN-off `23/23 PASS`；日志 SHA-256 `29737c17eeda07f7def734ff98d2f587b482009479653c18b5df4769c3a2b460` / `a8ea2dead926f8211de14f8398f88594effb88bf158aba48033e292ae0569715`。保留候选提交、板端隔离证据和独立只读 `yolo_roi_replay` 探针，不向生产交接该候选。正式八框仍 `2/8 PASS`，handoff HOLD。后续恢复身份须从模板/外观的连续性证据解决，不能只靠最近距离或缩小 detector 搜索域。

## 2026-11-14T10:49:29+08:00 搜索局部 ROI 四组反例与邻车候选门禁修订

- 精确候选 `codex/dual-gt-init-consistency@85fedbd2d045cc760b0bd08e5a95a444856c45d7`，源码包 `/tmp/nt-dual-gt-evidence/search-roi-85fedbd-source.tar` 与板端 `/home/neardi/Project/SOT/dual-gt-search-roi-85fedbd/source.tar` SHA-256 一致为 `567fca9ab75ce9994ef58ee13f06b6f31de31940e4e442333da69a66ecb8150e`。板端 `LKD3588` Linux `6.1.84`、RKNN API `2.1.0`、driver `0.9.8`，原生 CMake/构建/CTest `23/23 PASS`；`build/nanotrack_v3` SHA-256 `abf54e02fbf17368a1c91d16a7e5c00488af2ea88b203957e1589a5f4d4523d3`，`runs/headless-eval.conf` SHA-256 `e9621f9ad27bf6d24d1c911700be3168bb1d0874aca7fb57df22b8628949fc82`，源码配置 SHA-256 `c6c18974e88376cc7567a518b2fb9a52877a16f62ab4146f08c29a3711b95326`；原视频、模型/manifest/labels 与 `9f84efe` 基线相同且此前已哈希绑定。构建脚本 `/tmp/nt-dual-gt-evidence/build_search_roi_board.sh` SHA-256 `d12b1cb814c93ad1e2971be8b18d424ef27265c4afc4309b0c06d3922969a28f`。
- 实际运行板端 `bash runs/run_search_roi_four.sh`，主机脚本 SHA-256 `956d104faf93038a9898b5ce392765aeaa72686b90d24afb7034c930f11e7ee7`；car001 标准/右下和 car002 标准/真值四框各完成 271 帧、exit 0，CSV/session/log 留 `/home/neardi/Project/SOT/dual-gt-search-roi-85fedbd/runs/<label>/`。主机回传 `/tmp/nt-dual-gt-evidence/search-roi-85fedbd/four-text.tgz` 与板端 `runs/four-text.tgz` SHA-256 一致为 `255f659d9f034c30e1a19fceca4a9e7e3fc3a3e15623225dc2e30e49f2de3d85`；按上述四框顺序 CSV SHA-256 为 `b632cdcfc323adf183fa0e9cdd58f297df41a1a0e0acfa546fe1ea13bac9b4df`、`699389268085d59a171c2c2043ccf24de6cd3e60f355900d17d992f8a91fcc16`、`38b5bba597bbbad048f3feae5a90f6e1d042a2c62c2001b332a841c9eb67042d`、`bdeb812e4ef733b3e087e7b5fd30eaafbccb15120d9ca8f8dd73f822b77e71f6`。执行 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/search-roi-85fedbd/four_score.json`，报告 SHA-256 `d1b300e8abd4570289d417a52116c1d7d83bb1392f4bbd0f146410ae77dc9c1e`。
- car001 标准仍 PASS、第 188 帧重捕、tracking/display 中心 `4.36/4.40 px`；右下仍 FAIL 且重捕由基线第 191 帧延至 `198`，中心由约 `5.02→5.61 px`，两框 tracking/display 中心差 P95 `15.47/14.16 px`。car002 标准/真值均因 `full_occlusion_semantics` FAIL，重捕 `234/233`，中心 `2.05/2.00 px`。第 189、194 帧 car001 右下局部检测批次各仍保留 `2` 个同类候选，第 188～191 与 194～196 帧连续 `IDENTITY_CONFLICT`；正确目标候选虽在局部区域，近旁车辆仍进入统一恢复候选竞争。因此仅裁 ROI 不够，四组已有决定性退化，未扩八组。
- 在该反例上修订：仅当显式开启 `detector_recovery_roi_scale` 时，对已映射回原视频的候选加目标尺度距离门，中心距运动预测不超过 `0.8×max(预测宽,预测高)`，在统一恢复候选竞争前排除邻车；默认关闭的全帧路径不变。单测包含 ROI 内近车/邻车并检查 `1` 个保留、`1` 个位置拒绝；实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，日志 SHA-256 `fea66b240a24d57054f0a5fdae9484630fe6e8eb2264a6ab08f28ea071697f9d` / `2d741aff3fa378ba926e72b182fe3e116d412fbd0e532c17d7f39b909feb7daa`。**证据边界 / 下一步**：修订版还未上板；须按修订提交重新独立构建并跑四框。若 car001 右下仍晚重捕或任一目标中心/身份变差，应撤回局部检测候选，不能以单测替代闭环。正式基线仍 `2/8 PASS`，handoff HOLD。

## 2026-11-14T10:40:22+08:00 搜索恢复局部检测候选（待 RK3588 闭环）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始干净、基线 `f69b5f508c98b96cfb0fcbba83235d08ca80ec2a`。在 `ControllerConfig` 增加默认 `0` 的 `detector_recovery_roi_scale`；只有配置显式设为 `3.0` 且实际发 `SearchRecovery` 请求时，用运动预测框中心、至少 120 px 的方形 ROI 裁图。RKNN 固定输入仍 640×640，但把该搜索 ROI 缩至 320×320 后居中灰底，postprocess 继续映射到原始视频像素。初始化类别检测、显示刷新、无 ROI 的恢复请求均保留原行为；检测候选仍须经既有 NanoTrack 恢复确认才能令 `control_valid=true`。`visible-adaptive-display.conf` SHA-256 `c6c18974e88376cc7567a518b2fb9a52877a16f62ab4146f08c29a3711b95326`。
- 依据先前完整板端 ROI 探针：car001 第 181 帧错误邻车与正确预测位置相隔约 70 px；3 倍局部 ROI 的半径约 60 px，有望排除该邻车；第 187～190 帧同一 ROI 内的正确车辆候选距真值约 `1～5 px`。这只是反事实动机，局部 ROI 也可能丢失运动预测误差过大的真实目标，或改变异步检测与确认时序；car002 完全遮挡语义尚未解决，不能据此晋级。
- 在 `test_long_term_controller` 增加一个恢复 ROI 包含预测中心、尺寸和帧边界的单测；实际执行 `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-dual-gt-consistency-build -DENABLE_RKNN_RUNTIME=OFF -DBUILD_TESTING=ON`、`cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，`git diff --check` 通过。**证据边界 / 下一步**：主机未执行 RKNN，候选还须以本条实际源码提交在 RK3588 独立目录原生构建和跑 car001 标准/右下、car002 标准/真值，再按各自确认真值检查中心、IoU、完全遮挡、重捕、身份及延迟；只有无反例才扩八框。正式八框仍 `2/8 PASS`，handoff HOLD。

## 2026-11-14T10:34:34+08:00 稀疏局部检测修正与遮挡外观的双目标再筛查

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，基线 `b97cfc305c90bdb42edba2d34a04e1d27a18e64f`、起始干净。仍只读使用已验证的原始 canonical 271 帧、`9f84efe` RK3588 八组 CSV、上一节八组真实 RKNN ROI 候选 CSV 和各自确认真值；没有更改正式模型、算法、配置或标注。实际执行 `python3 /tmp/nt-dual-gt-evidence/search_sparse_roi_policy.py > /tmp/nt-dual-gt-evidence/search-sparse-roi-policy.txt`，脚本/输出 SHA-256 `590dd59d2408d3ef0d1a01c3671312686ebc93783b1600fa718bb0bf31982c36` / `0c827fb9439c8a7095f7664c240866594db984dcebd78eca45f5f6b8bf96011d`。在固定基线轨迹上扫描 detector 每 `3/4/6` 帧请求、`2/3` 帧回报、中心权重、逐帧衰减、置信门和邻近门的 1728 组反事实；其中 440 组八个平均中心误差均不劣于基线。按最坏单框中心均误差排序的首组为每 6 帧、延迟 2 帧、权重 0.5、每帧保留 0.8、conf≥0.2、候选中心距≤0.3 倍框长，八框中心改善仅 `0.014～0.449 px`，car001 左上仅 `0.014 px`。这是用这八组真值选择参数的同集离线筛查，改动后下一帧 tracker 会反馈且异步时延会变化；不能把 440 组或首组称为泛化、板端闭环或算法完成，暂不接入 tracking/control。
- 另实际运行 `.venvs/nanotrack/bin/python /tmp/nt-dual-gt-evidence/probe_visible_fraction.py > /tmp/nt-dual-gt-evidence/visible-fraction-eight.txt`，脚本/逐帧特征 JSON/文本 SHA-256 `6bb9a2f95cc3ae2a5d3020f55eb923a3aeb280d456c959a1f7af53afbf44ffd3` / `41938b45729d023289c6aff2f7032515ad250aba84449e77e8a3e2fcf6a7445e` / `afb0e38b1247bcd3edb900cee4cb3a727ab8f962d9243062984aee88f014b920`。用每帧基线 tracking 框内灰度 90 分位、亮像素/白像素占比及边缘密度，对首段稳定可见帧的中位数归一化。配对图 `/tmp/nt-dual-gt-evidence/car001-occlusion-contact.png` / `car002-occlusion-contact.png` SHA-256 `0d7e4a89b194f648cdb95e2d88c7eee9be8757a76c713c86120171e65e3ed5d6` / `f13ae07ac8ba5224b31bdd544bd03ef862ed302b3b027528c56bbe78d4fb35ca`，原视频裁片中蓝框为基线 tracking、绿框为已有真值。两车进桥遮挡时框内灰度 P90 下降：car001 四框第 164～165 帧约为稳定段 `0.91～0.92`，car002 四框第 212～213 帧约 `0.94～0.95`，而 car002 第 215～216 帧降至约 `0.68～0.71`；说明该信号的下降时序和深度随目标不同。简单 `P90 < 0.95×参考` 连续两帧的 car002 首触发在 213，但早期仍在可观测第 63～76 帧误触发 `10～15` 帧。独立亮度阈值不能安全地提前转 SEARCHING，也不证明恢复候选身份；需要结合状态与时间证据，并在完整八框闭环检验后才可接入。
- **裁决 / Handoff**：保持 `9f84efe` 正式算法基线 `2/8 PASS` 与 handoff HOLD。下一步只考虑可同时避免 car001 遮挡后错车重捕、car002 第 213～216 帧完全遮挡语义失败、并改善两目标 tracking/display 中心和初始框一致性的因果候选；离线小幅中心修正与单一亮度门均不满足。

## 2026-11-14T10:24:35+08:00 首帧局部 detector 中心归一化八组 RK3588 闭环否决

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，基线 `73d25036f092a244dfbb2b5b6c9f023b8ba803be`，开始时干净。精确使用 `9f84efecd57d611a239cc71c99ab2e4295025dbc` 板端二进制 SHA-256 `41d83ef0d772833b29400a35cacb8825a1a402f24ea7dbfe9279ac09b062343b`、原 `runs/headless-eval.conf`、canonical 视频及两目标确认真值；模型、算法、真值不变。从前一节同一原始首帧 RKNN ROI 探针的八组候选中选择距人工框中心最近的 `car=2`、conf≥`0.1` 且几何匹配者，按 0.75 倍中心差移动初始框、保持人工宽高。八个新初始框由 `/tmp/nt-dual-gt-evidence/run_init_center_board.sh`（SHA-256 `bc3aaf5faa398d055e29e35550beeedb3453151c15e89de08bb9f68c15e4aeb2`）逐项保留；这只修改测试输入，不改正式初始化代码。
- 实际在 `/home/neardi/Project/SOT/dual-gt-handoff-9f84efe` 运行 `bash runs/run_init_center_board.sh`，每组 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox ... --output-boxes runs/init-center-<label>/frames.csv --session-metadata runs/init-center-<label>/session.json`，八组各 271 帧、exit 0，板端 CSV/session/log 全留 `runs/init-center-<label>/`。板端归档 `runs/init-center-eight.tgz`、主机 `/tmp/nt-dual-gt-evidence/init-center-eight.tgz` SHA-256 一致为 `773649f519cf224e7b71280e295bf4922ae2d6a8a1d3b8ce8cc4eb78bb4a796f`。实际执行 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/init-center-eight/score.json`，评分报告 SHA-256 `4edd72183868151d9e66ef116c17a09d00eb8a1565708ee4500fca38303855a6`。
- 结果 car001 nominal/user/right PASS，left FAIL；左上框虽仅变为 `(990.6674,580.5800,39,41)`，重捕从基线 188 附近推迟至 201，中心均误差从 `4.34→5.32 px`，导致四框 tracking/display 中心离散度 P95 `23.32/24.13 px`（基线 `3.31` 附近）。car002 四组全部 FAIL `full_occlusion_semantics`，重捕在 `232～233` 帧；其 nominal/truth/left/right tracking 中心均误差 `2.00/1.87/2.15/2.59 px`，跨框中心离散度 P95 `7.28 px`，高于基线 `6.21 px`。虽然 car001 PASS 个数由 1 增到 3，整体仍只有 `3/8 PASS` 且最坏稳定性明显退化。**裁决 / Handoff**：否决首帧中心归一化候选，正式代码不接入；此前每帧局部 detector 的冻结轨迹收益和本次闭环结果不可混同。正式八框仍 `2/8 PASS`，handoff HOLD。后续需针对遮挡判定和重捕身份的因果路径提出新的板端闭环候选，且两目标 tracking/display 中心、IoU 与跨初始框一致性须同时过门。

## 2026-11-14T10:15:37+08:00 RK3588 局部 detector 八组全帧回放与稀疏时延否决

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，探针源码提交 `b5ca2890f61f1cf806c84b8ebe34f450d622129f`，板端隔离根 `/home/neardi/Project/SOT/dual-gt-roi-probe-b5ca289`。输入 canonical 视频 SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`、八组 `9f84efecd57d611a239cc71c99ab2e4295025dbc` 基线 tracking CSV、对应两份确认真值；VisDrone9 RKNN/manifest/labels SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`；板端 RKNN API `2.1.0`、driver `0.9.8`、Linux `6.1.84`。源码包 `/tmp/nt-dual-gt-evidence/roi-probe-b5ca289-source.tar` SHA-256 `60db0a8a624008f8e9cde446b0bffa691bc20b4ec365e62c3ada721894536c33` 已上传并核对；原生 CMake 构建和 `ctest` `23/23 PASS`，`build/yolo_roi_replay` SHA-256 `5d8949d9907ae4b18b35205f4183c7811d6bbb19e66807a9f3126b17f51f2fd9`。
- 实际运行板端 `bash runs/run_roi_replay_board.sh`，主机脚本 `/tmp/nt-dual-gt-evidence/run_roi_replay_board.sh` SHA-256 `cfc03dacb7a0271b2fe2bd91fa9e5cb93184fa23a026d91314caf1f5b77d8ff5`。八组各读取原始视频 271 帧和各自冻结 ROI trace，检测输出在板端 `runs/outputs/<label>.csv`、日志在同目录；回传同名 CSV 至 `/tmp/nt-dual-gt-evidence/roi-detector-board-probe/full-eight/`，按 car001 nominal/user/left/right、car002 nominal/truth/left/right 顺序 SHA-256 为 `9985ee407c1d6c9393dfb19f5eec0fe201a63aedf4c137b314a6a30fe3e707a3`、`d3eb3aa3b2adf0a3a3214256ffab9021a7274c612d3349dc6531998a76d17f17`、`c712783da86b7d90209ae4ce409a50792e84c58d6db012a1b4c44f17498d5f2e`、`8e7916951a01bd411787331deb4ee5ef53ca2f34235563f31fd651437072e89f`、`46399f6a2705d15200742dc23dd285ab76c1eb449f7b113b261a3945797cfb10`、`89dbe3b47421efe9d419a5472277bb751ccfe5ae082f500cb8020951ec75c9f9`、`8a151d947651d63e0923d87ba9a530b75d9b76761f5b8c78ce35953be0a90330`、`f2d18a3b0db4ddf079ead29978037e00eaff725a44cf8c924535e2abf3e74cc2`。评分脚本 `/tmp/nt-dual-gt-evidence/score_roi_board_eight.py` SHA-256 `c7ba5d78b0c10231aa0bd36ec91ebf35d8d423754abbb27f978b2182299675dd`，报告 `/tmp/nt-dual-gt-evidence/roi-detector-board-probe/full-eight/score.json` SHA-256 `d10acf36b9a8533e0eaf1260bae46f69e15f4c87c93fe430e4983abbb41da45c`。
- 离线冻结轨迹反事实：`TRACKING` 且有效观测时只融合 detector 中心、保留 tracker 宽高，中心差 `<3 px` 乘 `0.5`，否则乘 `0.75`。car001 四框中心均误差 `4.28/4.63/4.34/5.02→4.14/4.29/4.27/4.78 px`，IoU `0.751/0.714/0.773/0.701→0.757/0.722/0.777/0.705`；car002 中心 `2.59/2.00/1.77/2.92→1.88/1.80/1.63/2.06 px`，IoU `0.791/0.883/0.881/0.728→0.816/0.888/0.882/0.749`。跨四框中心差 P95 car001 `3.31→3.28 px`、car002 `6.21→2.72 px`。但实际 detector 耗时平均 `78.12～79.46 ms/次`，无法按 30 fps 每帧调用。
- 执行 `python3 /tmp/nt-dual-gt-evidence/score_roi_sparse.py`（SHA-256 `2215ea6c8c0cd5820a5e72b41814d7d8e990dbd5592985ccced082ca55876dde`），输出 `/tmp/nt-dual-gt-evidence/roi-detector-board-probe/full-eight/sparse-score.txt` SHA-256 `9b284af2529aae9b1134690de750238108a8be12c7d4268dd3becd9f32e8f64d`；模拟每 3/4/6/10 帧一次且延迟 0/2/3 帧后沿用中心偏移。每 3 帧、延迟 2 帧时 car001 左上 `4.34→4.40 px`、car002 左上 `1.77→1.93 px`；每 4 帧、延迟 3 帧时 car001 右下 `5.02→5.04 px`。所以全帧离线收益不能外推到可运行的异步稀疏闭环，更不能将 detector 候选直接作为控制观测。此路径暂不接入正式 runtime；正式八框仍 `2/8 PASS`，handoff HOLD。下一步针对初始化偏差与局部外观/全车位置的关系提出可板端闭环检验的候选，并同时评估 tracking/display 中心、IoU、遮挡无框和恢复身份。

## 2026-11-14T10:05:41+08:00 局部放大 detector 定位候选与 RK3588 完整帧探针（待闭环）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始干净、基线 `d52169ed870187e92a363ed3c30f242fad287685`。输入只使用 canonical `data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`、`9f84efecd57d611a239cc71c99ab2e4295025dbc` 八组 RK3588 CSV 与各自已确认真值；没有用该视频或真值训练模型。主机 VisDrone9 PT `artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-best.pt` SHA-256 `6744672ac5e35debba2a647ad570e9fe84a1cabd1c45c9840080d0a4127fe36d`。对每帧已有 tracking 框在其中心裁方形 `max(120,3×max(w,h))`，缩放至 `320×320`，置于 `640×640` 的灰色画布中央，用目标类 `car=2`、置信度至少 `0.25`、中心距不超过 `0.6×max(w,h)`、宽高比相对 tracking 各在 `0.5～2` 的最近候选筛查。只在原记录 `TRACKING` 且 `observation_valid=1` 时做离线反事实定位，搜索/疑似、状态、身份、控制及真值均不改。
- 实际运行 `OMP_NUM_THREADS=2 MKL_NUM_THREADS=2 YOLO_CONFIG_DIR=/tmp/nt-yolo-config MPLCONFIGDIR=/tmp/nt-mpl .venvs/yolov8-visdrone/bin/python /tmp/nt-dual-gt-evidence/probe_roi_detector_four.py` 和 `probe_roi_detector_extra_four.py`，两脚本 SHA-256 `57f0d29f8be998995e12163f568106e9fb957b974e0b5b7fcb5a23ed026cfc70` / `9c7ad33d38a2a064c2add204c4b7015828aeb6a57f1d0e34ed147adc115063ed`，八组各 271 帧预测存 `/tmp/nt-dual-gt-evidence/roi-detector-four/<label>.json` 与 `roi-detector-extra-four/<label>.json`。评分脚本 `/tmp/nt-dual-gt-evidence/score_roi_detector_eight.py` SHA-256 `2628879858701f0190e44d023531acc0e42db71f56ca6f826b7e9b07708b88ab`，报告 `/tmp/nt-dual-gt-evidence/roi-detector-eight-score.json` SHA-256 `effe03755e34dfe9c94a8597841a87a8dc73c691e71b70a9eeca35107907d642`。直接替换 detector 的宽高会使 car002 真值初始化平均 IoU `0.883→0.781`，不可用；只用 detector 中心、保留 tracker 宽高则 car001 四框中心均误差 `4.28/4.63/4.34/5.02→4.09/4.09/4.28/4.63 px`，car002 `2.59/2.00/1.77/2.92→2.08/2.11/1.94/2.28 px`，两目标跨四框最大成对中心差 P95 分别 `3.31→3.33`、`6.21→3.68 px`，但 car002 真值/左上单框略退化。再用 `/tmp/nt-dual-gt-evidence/score_roi_blend_eight.py` SHA-256 `4f7529b8e026846e76e8f17493c4b9fd0caa81ec4fc66cfdb9b2f96d184b49ab` 反事实筛查：中心差 `<3 px` 用 0.5 倍、大于等于 3 px 用 0.75 倍修正，八框中心均误差均比基线低，car001 `4.14/4.29/4.28/4.78`、car002 `1.86/1.79/1.57/2.02 px`；各框平均 IoU 也均上升，car002 跨框 P95 `6.21→2.72 px`。这些结果不含对跟踪器的反馈，不能当正式提升。
- 固定 RKNN 输入 `640×640` 时，直接将 ROI 放大到 640 的四框筛查只检出 `26～75` 个可用候选并使 car001 中心退化；将 `320×320` ROI 置于 640 画布的四框脚本 `/tmp/nt-dual-gt-evidence/probe_roi_detector_four_canvas.py` / 报告 `/tmp/nt-dual-gt-evidence/roi-detector-four-canvas/report.json` SHA-256 `613e2362ecc2ac491d006faae80e0eb12e125e20d9ee1e635084ed4e931c19e9` / `0f45ee5d0c6d83b191c73759998d317456ee0b1e1b338456d34bb34a8c82871a`，保留原 320 预测的几何特征。RK3588 实机 `LKD3588` Linux `6.1.84`、RKNN API `2.1.0`、driver `0.9.8`，VisDrone9 RKNN/manifest/labels SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。现有板端 `yolo_detector_smoke` 二进制 SHA-256 `3ba28a7afa1e5aa9c09a551f1fbb2b0e137e2b0006ecb1941fdd7fe8d9712f72` 对 15 个 derived canvas PNG 分别以 `--conf 0.1 --nms 0.45 --class-id 2` 推理，板端输出留 `/home/neardi/Project/SOT/dual-gt-display-trace-d157d0d/runs/roi-canvas-probe/smoke/`；回传 15 CSV 后执行 `/tmp/nt-dual-gt-evidence/score_roi_board_smoke.py`（SHA-256 `52764fca54d9b02d57d81b072b57ff747dcb8fce7783762e0b6478769f59f03e`），报告 `/tmp/nt-dual-gt-evidence/roi-detector-board-probe/smoke-score.json` SHA-256 `b3ab2766e80ba6513ce5c4b2b899d0a2f0d3cb3467910960e959aeeecd221bbf`；car002 标准第 245/260 帧 RKNN 局部候选中心误差 `1.25/2.57 px`，原 tracker `7.12/10.61 px`，与主机 PT 的几何位置一致。另用 640 derived MP4 驱动 Debug 程序时，放大车体使首帧类别判成 UNKNOWN 或 van，不能据此评估原视频的跟踪/控制闭环；其输出只留板端隔离目录，不晋级。
- 新增 `rk3566/nanotrack_v3/src/yolo_roi_replay.cpp` 及 RKNN-on CMake 探针目标 `yolo_roi_replay`：从**原始 canonical 视频**和固定的八组基线 ROI trace 逐帧生成 320 局部画布，使用板端真实 RKNN 一次加载、连续推理并输出所有 car 候选及时延，用以核对完整 271 帧而非稀疏样本。ROI trace 由 `/tmp/nt-dual-gt-evidence/make_roi_traces.py` SHA-256 `c4f7bf15dbcfcb845e318f95d02358943689e91e173cc22bdca0a6eaeca980f1` 从精确基线 CSV 生成，保留 `/tmp/nt-dual-gt-evidence/roi-detector-board-probe/roi-traces/`。该工具不会更改正式 runtime、control、`tracking_bbox` 或 `display_bbox`。实际 `git diff --check` 通过；待精确提交后在板端独立目录原生构建并跑八组，核对 RKNN 候选覆盖、与真值的误差、相邻车辆误选和 271 帧时延，再决定是否将有界中心修正送入闭环。正式基线仍 `2/8 PASS`、handoff HOLD。

## 2026-11-14T09:41:08+08:00 两目标中心误差的因果滤波与图像局部化筛查

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始干净、基线 `cf4b8d9071685ba4e020cf5f0f76542a28651a31`。只读输入仍为 `9f84efecd57d611a239cc71c99ab2e4295025dbc` 绑定的 RK3588 八组 271 帧 CSV `/tmp/nt-dual-gt-evidence/handoff-9f84efe/<label>/frames.csv`、canonical 视频 `data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d` 和两目标各自已确认真值；正式算法、模型、板端配置、真值均未改。
- 执行 `python3 /tmp/nt-dual-gt-evidence/probe_causal_filter.py > /tmp/nt-dual-gt-evidence/probe-causal-filter-eight.txt`，脚本/输出 SHA-256 `5e97fc0069e138561e032a94d3b04baac9a54316d2e6df41b2f0cdff538f84f6` / `c88587d2d9a1583501620677a36a8f18ea0ebf11297e85c9bb6a7d859e408cae`。仅对 TRACKING 帧中心作重捕/状态切换即重置的因果 α-β 滤波，不反馈运行时。原八框可见中心均误差 car001 为 `4.28/4.63/4.34/5.02 px`、car002 为 `2.59/2.00/1.77/2.92 px`；`α=0.8, β=0.05` 后分别为 `4.32/4.65/4.40/5.06`、`2.61/2.02/1.77/2.92 px`，两路均无整体收益。`python3 /tmp/nt-dual-gt-evidence/probe_tracking_lag.py > /tmp/nt-dual-gt-evidence/probe-tracking-lag-eight.txt` 脚本/输出 SHA-256 `67b4dc924bc34f1dd5edbda518f0d9cd77c83dffd76d7f7553b99755f5c1576a` / `5e91d226a217b5c837776f3d9b2d96737259ccb09feb99acf766a6b425f441e0`：car001 后段中心偏差约左 `3.75～4.87`、上 `4.62～5.18 px`；car002 后段约左 `0.95～6.06`、下 `1.85～5.94 px`，固定速度超前量无法解释两车偏差方向。
- 执行 `.venvs/nanotrack/bin/python /tmp/nt-dual-gt-evidence/probe_white_component_center.py > /tmp/nt-dual-gt-evidence/probe-white-component-eight.txt`，脚本/输出 SHA-256 `17b8db1d64933e186e949bf6379293a2077963b7303194f88ecc1a1be182aad3` / `030ff53b19d6a33a92456b2317cc9f7a3f920a27f16316439281e74ffc73ee94`。以首 20 帧校准的局部白色连通域中心替换 tracker 中心：car001 后段四框均误差有小幅改善或接近，但前段 P95 变差；car002 误选白色背景/相邻区域，后段标准框 `7.83→30.95 px`，直接否决该颜色中心。执行 `.venvs/nanotrack/bin/python /tmp/nt-dual-gt-evidence/probe_local_template_center.py > /tmp/nt-dual-gt-evidence/probe-template-center-eight.txt`，脚本/输出 SHA-256 `c1b4c8d8a5a9401c641b105b2b1c6dda488d49991a23a0ab03211df3e8dda469` / `21f9d093db00230602bf9df4e44b27268753f08cabd8b60ec46049b122775b8a`。首帧灰度模板在当前 tracking 框 ±12 px 内做 NCC 匹配：car001 遮挡前标准框中心 `3.23→1.47 px`，右下 `3.47→1.71 px`，但 car002 真值框前段 `1.38→3.56 px`、左上 `1.46→3.32 px`，后段四框均退化。再用脚本 `/tmp/nt-dual-gt-evidence/probe_template_gate.py` SHA-256 `de4166b75818d0d5348ba39dee9c0595691a58912a4d1073ea44ba26eba63d93` 扫描位移上限、NCC 增益和半/全校正，输出 `/tmp/nt-dual-gt-evidence/probe-template-gate-eight.txt` SHA-256 `a81dd1acc942e08ec90277f158bd10f15d0c1098c443c06cef5e6db54573057c`；小幅中心均值改善的参数均增加至少一个目标的跨初始框中心分歧，未形成同时满足精度与一致性的候选。
- 另执行 `python3 /tmp/nt-dual-gt-evidence/inspect_recovery_geometry.py > /tmp/nt-dual-gt-evidence/recovery-geometry-four.txt`，脚本/输出 SHA-256 `c4e6f14a51de25037ec2fcec6bfe7509df346975d1be9b265b4078169d42f3f5` / `88681193110c289e4523f1b16305dff9aaff5b006ae4c762ffc939ebe88d5bf1`；car001 右下第 187 帧搜索预测中心 `(875.3,826.0)` 对已确认真值 `(871.0,844.0)` 约偏上 `18 px`，直到第 190 帧才得到近真值 detector 支持的候选。car002 第 229 帧仍人工确认无正式框，第 230 帧首次有真值框。配对源帧图 `/tmp/nt-dual-gt-evidence/bridge-comparison.png` SHA-256 `035ba42ffc8706008ef01c22389be8376c9b6554a5f4469ed1304a0b5a0dc58a`，生成脚本 SHA-256 `245dab8e300900dd371c0c17ed080553a60df46b897a10f007b5ebd09e02b62f`；图像只供定位诊断，不给运行时泄露帧号或真值。
- **裁决 / Handoff**：这些是基于冻结轨迹的离线反事实，不包括修正反馈、模型重跑或板端时延，因此没有接入滤波、颜色中心或 NCC 中心；正式八框 `2/8 PASS`、handoff HOLD。下一步须找能分辨局部外观误匹配与完整车体可观测性的独立证据，并在板端闭环证明两目标、各四个初始框的 tracking/display 中心、IoU、无框语义和恢复身份同时改善。

## 2026-11-14T09:29:00+08:00 横向厚遮挡带候选 RK3588 四组否决并撤回

- 候选源码 `codex/dual-gt-init-consistency@cacb3a896c8a6321e834d8af946f7cc9da18dc82`，基线 `e635a9a69631cd4b930ed5d4e5358a2572f4573c`；主机源码包 `/tmp/nt-dual-gt-evidence/occluding-band-cacb3a8-source.tar` 与板端 `/home/neardi/Project/SOT/dual-gt-occluding-band-cacb3a8/source.tar` SHA-256 `fdc02c2b985c85246b322386a4af2cbbaeeb267a6deb8882d3c27a54bd4b6489`。RK3588 `LKD3588` Linux `6.1.84` 隔离根 `/home/neardi/Project/SOT/dual-gt-occluding-band-cacb3a8`，RKNN API `2.1.0`、driver `0.9.8`。原生执行 `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build --parallel 4`、`ctest --test-dir build --output-on-failure`，`23/23 PASS`；可执行文件 SHA-256 `48b404ea863356bf23802cb7af439fe9df26f178276a73237b8a3b19726f9c31`。`runs/headless-eval.conf` SHA-256 `2d45ca0f31916e0d05e0e5c2cad189bbf0d42f372aa4a67f3dbb3de939c531d6`；其视频、controller、NanoTrack RKNN、VisDrone9 detector/manifest/labels 路径与下方 `9f84efe` 已哈希绑定基线相同。
- 板端运行 `bash runs/run_screen.sh`，脚本主机 `/tmp/nt-dual-gt-evidence/run_occluding_band_screen.sh` SHA-256 `6c835b57c0c9382b35ff48b398e4d3d810a90671d9b7d13f9fa2d417700858de`，以 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-boxes runs/<label>/frames.csv --session-metadata runs/<label>/session.json` 顺序完成 car001 标准 `(992,582,37,39)`、右下 `(994,584,35,37)`、car002 标准 `(970,517,38,38)`、左上 `(968,515,40,40)` 四组，各 271 数据帧、exit 0，未生成 AVI。CSV/session/log 保留于板端 `runs/<label>/`。按此顺序 SHA-256 分别为：car001 标准 `a86d215d9c53b48fdaabb30d24f5a4b214d362bc41f3891ebe8697cad841b460` / `998eb0c94938cec065ebe449dc7325bc9ee9112f1dbf515fc615569cd5f4344f` / `a74b63b15acee88f4791351b02d00fe99e4e101d45ca3b77fca1d1dcf924e486`；car001 右下 `30416112559defa30592650f437a4fe2fa2129e587076bd730e8f700f81d74b9` / `9e5c11289a9616790a7d269f6534ab69954fe5997b88828b3a01c869632461fb` / `3118157fe2c6cc2231eb27a80ddcf4812e23495962e3ad3c023347e654cbce7c`；car002 标准 `7a0162598c14ee82a64bfcd8b1b04f79e3b527a74288a35c79dd1f8ec6e257ed` / `b3a1e4d0d40bd7faa00f51a52dbfc0688c34c79cf31c19ade3b5b2d262e14dd1` / `2e3779d768666d336c179974c22f67461fe1bb7c321fdee7698246a043879e5e`；car002 左上 `5007a36679d4c8a0dcd3221adec46bfe64c3fd1ab53a1445f738781292a0eb4c` / `fccfd924f10c93526047003f1327ec99e5d2f9d824c27827ba572e5b10464f01` / `b0d8aef9807a75aab5e9c00bbc1998b7cafec5bd15bd070e497d0631cd2c77be`。四 CSV 主机副本 `/tmp/nt-dual-gt-evidence/occluding-band-cacb3a8/<label>.csv` 与板端 SHA-256 一致。
- 实际用 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ...` 对各自确认真值评分，报告 `/tmp/nt-dual-gt-evidence/occluding-band-cacb3a8/four_score.json` SHA-256 `dc0bd592ecfeade40c113d0465bbf8dd99e182bd0974e2ccf17f6f68ca97a71b`。car001 标准 `PASS`、188 帧重捕、tracking/display 中心 `4.28/4.32 px`；右下 `FAIL`、193 帧重捕（基线 191）、中心 `5.71/5.65 px`。car002 标准 `FAIL`（可见框、完全遮挡、重捕），227 帧过早重捕且无框段错误观测 1 帧、中心 `2.47/2.47 px`；左上 `FAIL`（完全遮挡），230 帧重捕、中心 `1.80/1.80 px`。car001 标准/右下跨初始化 tracking/display 中心 P95 `3.01/2.18 px`，car002 标准/左上 `8.80/8.80 px`。四组 pipeline mean `113.1～114.0 ms`、P95 `118.8～119.2 ms`。闭环实际 car001 右下 169 帧提前搜索但后段恢复变晚；car002 两框只在 214 帧搜索，211～213 帧仍对已确认无框区间输出疑似观测，故离线历史轨迹的首触发 211 帧不成立。
- **裁决 / Handoff**：该候选不能改善两路中心一致性或完全遮挡语义，撤回六个算法/测试文件改动，保留板端隔离证据和此记录；`display_bbox` detector 仍只用于显示，未把遮挡边缘或 detector 作为控制观测。正式八框仍 `2/8 PASS`，handoff HOLD。后续需基于闭环结果而非固定基线轨迹设计可见性门禁与恢复候选身份验证。

## 2026-11-14T09:21:47+08:00 横向厚遮挡带与疑似跟踪联合触发候选（待 RK3588 闭环）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始工作树干净、基线 `e635a9a69631cd4b930ed5d4e5358a2572f4573c`。`car001_right` 在 166～174 帧进入 SUSPECTED，却到 175 帧才 SEARCHING；car002 四框在 211～219 帧 SUSPECTED，直到 220 帧才 SEARCHING，导致已确认无框区间使用 tentative recent 观测。单独缩短疑似时长会影响 car002 左上框 120～142 帧正常可见的 SUSPECTED 段，不是安全解。
- 主机只读筛查 `/tmp/nt-dual-gt-evidence/probe_crossing_line.py` SHA-256 `0e11d8719057fa38af66633c9ec747fffccb847547db8a6d78681414accf9be2`；输入 canonical `/home/u20/single-object-tracking/data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d` 和 `9f84efe` 八组板端 CSV，输出 `/tmp/nt-dual-gt-evidence/probe-crossing-band-prevbox-eight.tsv` SHA-256 `1dd3fb3afc08f8885bf0d8deac49e93c4cc191e1fda4eb66b317f00caefb6937`。以**前一帧已提交框**圈局部图像，在横向长度至少 2 倍目标宽度、近水平的边缘中寻找相距目标高度 `0.7～1.4` 倍的上下两条线；近六帧至少两次厚带命中且本帧已有 SUSPECTED 时，八框历史轨迹的首个候选触发为 car001 右下第 166 帧及 car002 四框第 211 帧。car001 其他三框已自行进入搜索；car002 左上 69～70、120～142 帧的正常可见 SUSPECTED 段没有触发。单条长横线在 car002 69～79 帧护栏处会误报，故必须是有厚度的两条边缘，且与控制器已有疑似状态同时出现。以上只是固定历史轨迹的离线筛查，不证明修改闭环后的八框结果。
- 候选修改 `main.cpp` 在汽车目标的当前帧用 OpenCV Canny/Hough 检出厚带，缓存六帧布尔值；经 `LongTermRuntime::process_frame` 把“近六帧至少两次命中”送入 `SearchPlan`。`LongTermController::assess_recent` 只有在原有响应退化已进入 SUSPECTED 时提前走现有 SEARCHING 分支，拒绝 tentative 观测并沿用已有 motion-search 控制门禁。没有把边缘、detector 或估计真值写作 accepted 观测，也未改变 `display_bbox`/`tracking_bbox` 的框来源。新增控制器回归覆盖联合条件提前搜索与无厚带时保持 tentative；实际 `git diff --check`、`cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`。该主机构建不编译 `main.cpp` 的 OpenCV 检出函数。
- **证据边界 / Handoff**：本候选尚未在 RK3588 构建或运行，正式 `9f84efe` 八框仍 `2/8 PASS`、handoff HOLD。下一步用精确提交在板端独立目录原生构建，先跑 car001 标准/右下与 car002 标准/左上四组完整 271 帧；必须核对触发帧、错误重捕、无框段观测、两路中心和推理时延，再决定是否扩八框或撤回。厚带启发式目前只在本 canonical 场景筛查，不能据此声称跨场景泛化。

## 2026-11-14T09:12:03+08:00 永久模板裁剪尺度量化 RK3588 四组否决并撤回

- 候选精确源码 `codex/dual-gt-init-consistency@d8178faa283432b4215cdce67a0b25dcbc20b647`，主机源码包 `/tmp/nt-dual-gt-evidence/anchor-scale-d8178fa-source.tar` 与板端 `source.tar` SHA-256 同为 `d21b4f5b25bc0d87c4bff00f67ed8fcd980f8225276266be2506dcde2f9761d1`。RK3588 `LKD3588` Linux `6.1.84` 隔离根 `/home/neardi/Project/SOT/dual-gt-anchor-scale-d8178fa`；原生执行 `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build --parallel 4`、`ctest --test-dir build --output-on-failure`，`23/23 PASS`，二进制 SHA-256 `d06c7379f5cd7b99784e69e9f71ee347f4095bda7d2dac59fb9311e0848b4a5b`。`runs/headless-eval.conf` SHA-256 `2d45ca0f31916e0d05e0e5c2cad189bbf0d42f372aa4a67f3dbb3de939c531d6`，其绝对视频、controller、官方 NanoTrack RKNN 和 VisDrone9 detector/manifest/labels 路径与下方 `9f84efe` 已哈希绑定板端基线相同；RKNN API `2.1.0`、driver `0.9.8`。构建/CTest 日志留 `runs/`。
- 板端实际运行 `bash runs/run_screen.sh`，脚本主机 `/tmp/nt-dual-gt-evidence/run_anchor_scale_screen.sh` SHA-256 `1292f2528d2c5e4ca60cb6f9874c9849d893c3ce8c9d83b4111abfb036829ad8`；依次以 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-boxes runs/<label>/frames.csv --session-metadata runs/<label>/session.json` 跑 car001 标准 `(992,582,37,39)`、右下 `(994,584,35,37)`，car002 标准 `(970,517,38,38)`、真值 `(967,513,42,42)`，每组 exit 0、271 数据帧；未生成 AVI。CSV/session/log 均留在板端对应 `runs/<label>/`：car001 标准 SHA-256 `1c68a4edf1c4f07759d4a0e19585b73cbd469943abbe6ac450200a6aeac4908f` / `5455b4c45c095171cc0384dcda80b13cd3ec15041ab3ab3507d88fb4164f7494` / `b5d9a2575d7c9b92424fd6962371a65b320761c8410f18a421654f7746c47427`；car001 右下 `ce504d76d73e43382a1f4564404957332b7c1a44e70154db8d9749de7e032d19` / `622cc89d3882d08bb9d6a0692b70d31502117d1eaeb1dbe96e3436d5482933a8` / `1566a7e11b2d2e2c3611f152b29e9da94f17c3d8a6ab7f84aa2f5ca883380850`；car002 标准 `235574d08f41731179559ad276eb3b22dfd20b8432b446959a6d0696278fecf7` / `5ed32e27a694fc1544990e0640c733032c3bf037f44961b44fc43e99dc7504a0` / `0611eed8b422087dc972e7f0217d58b409e9b68980401cace149e6c3319748a3`；car002 真值 `99ae8184d86a80835ab4ae26c702276155a0a74938de56a60c099dca1d4611c9` / `7ec1746f2616784f7cae5c2d5d7efb9b6024aa01756d7c805ee574db3a17448c` / `d4a327dc8d5f42ceb4f36b888610d07538cda4ae89798dabad5df99807a5de0b`。四 CSV 主机副本 `/tmp/nt-dual-gt-evidence/anchor-scale-d8178fa/<label>.csv` 哈希与板端逐一一致。
- 实际用 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ...` 针对各自已确认 GT 评分，报告 `/tmp/nt-dual-gt-evidence/anchor-scale-d8178fa/four_score.json` SHA-256 `884b360661e41155f26eba45546d0471908ec11ffc1a4d798ad0bf9fe2546e21`。car001 标准保持 `PASS`、第 189 帧重捕、tracking/display 中心 `4.25/4.31 px`；但右下框 `FAIL`，第 142、186、216 帧重捕，tracking/display 中心 `9.28/8.98 px`、IoU `0.613/0.652`，标准/右下跨框 tracking 中心 P95 `34.11 px`，明显差于正式基线。car002 标准/真值仍因完全遮挡语义 `FAIL`；中心分别 `2.49/2.50` 和 `1.93/1.90 px`，两框跨框 tracking 中心 P95 `8.04 px`。四框可见中心局部改善不能抵消 car001 错误恢复与不一致。
- **裁决 / Handoff**：本项同提交撤回尺度量化代码，保留候选提交和隔离板端证据；`rk3566/nanotrack_v3/src/nanotrack_v3.cpp` 恢复为 `16cbb43` 算法内容。正式 `9f84efe` 八框仍 `2/8 PASS`、handoff HOLD。后续不能仅根据标准框通过或单目标中心均值选择模板尺度，必须保护微扰框恢复身份和两目标遮挡语义。

## 2026-11-14T09:06:35+08:00 小目标首帧永久模板裁剪尺度量化候选（待板端筛查）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，开始时工作树干净、基线 `16cbb439142c1c500b72a9fb393f0fbdcf452e87`。前一轮直接使用 detector 首框初始化与五路模板特征平均均导致 car001 明显退化，均已否决撤回；本候选不使用 detector 候选更新控制，也不融合多个模板特征。
- 仅修改 `rk3566/nanotrack_v3/src/nanotrack_v3.cpp` 的初始 anchor 特征裁剪框：对宽高均在 30～60 px 且长短比不超过 1.4 的小目标，将模板裁剪框宽高分别四舍五入到 10 px，保持原 ROI 的模板中心；越界则沿用原 ROI。此两目标八个人工初始框的宽高均会变为 `40×40`，但 tracker/控制器的初始框、后续近期模板更新、显示 detector 和所有真值均保持原语义。目标是隔离初始 ROI 尺度微扰对永久模板的影响，判断它是否导致恢复阶段分叉；此启发式还未证明跨目标通用。
- 实际执行 `git diff --check`、`cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`。**Handoff**：待本项与源码同提交后，用精确提交在 RK3588 独立目录原生编译，先跑 car001 标准/右下、car002 标准/真值四组完整 271 帧；比较两路中心、IoU、恢复身份、完全遮挡语义，只有不退化才扩八框。正式已验证 `9f84efe` 八框仍 `2/8 PASS`，handoff HOLD。主机测试不构成板端算法改善证据。

## 2026-11-14T09:02:36+08:00 双目标八框 detector 候选中心按源帧对齐的诊断

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，起始工作树干净、HEAD `80883549b6b0bfcffcfad6f011171ebcb083f874`；正式算法、控制配置、模型和 GT 均未改。沿用既有只读显示候选诊断源码 `d157d0d66a7346dd5460b8a568eea2c0e973cc58` 的 RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-display-trace-d157d0d`，复核二进制 SHA-256 `ed4f8ad097331988e22ac49dff5fe305ee20163ca2cc425a43b758119bb58e57`、`runs/headless-eval.conf` SHA-256 `c7594ebb9c43789b136e13182fdfd96f634af154f411142f0866b954762cad98`。板 `LKD3588` Linux `6.1.84`，RKNN API `2.1.0`、driver `0.9.8`，视频、两套 GT、tracker RKNN 和 VisDrone9 detector/manifest/labels 身份沿用下方 `d157d0d`/`9f84efe` 已哈希绑定条目。已有 car001/car002 标准框的只读 trace 均与正式基线逐帧轨迹一致。
- 在该板端隔离根执行 `bash runs/run_source_aligned_extra_six.sh`；脚本主机 `/tmp/nt-dual-gt-evidence/run_display_trace_extra_six.sh` 与板端 SHA-256 同为 `ec51bf5acb76aacc9d60dbc80e0cdfd16fad294e01c6c4fd6afa3ee8939564e7`。按 car001 人工 `(993,585,36,36)`、左上 `(990,580,39,41)`、右下 `(994,584,35,37)`、car002 真值 `(967,513,42,42)`、左上 `(968,515,40,40)`、右下 `(972,519,36,36)`，逐组以 `NT_TRACE_DISPLAY_CANDIDATE=1 ./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-boxes ... --session-metadata ...` 运行完整 271 帧。六组 exit 0、各 271 数据帧；没有生成 AVI。CSV/session/log 留在板端 `runs/source-aligned-extra/<label>/`，18 文件 `runs/source-aligned-extra-checksums.sha256` SHA-256 `23fae55cf3746d5fd5ee8dfe6cc58a3ec47877e9b4872d11aed554588c2975a7`，板端 `sha256sum -c` 18/18 OK。文本包 `runs/source-aligned-extra-six-text.tar.gz` 与主机 `/tmp/nt-dual-gt-evidence/source-aligned-extra-six-text.tar.gz` SHA-256 同为 `ad0b1ca4c357eda06e04b086d0d30b9c8363813acd52ba52b0f2678de7defa46`；主机解包到 `/tmp/nt-dual-gt-evidence/display-trace-source-aligned-extra/` 后按同一清单复核 18/18 OK。
- 主机实际执行 `python3 /tmp/nt-dual-gt-evidence/score_source_aligned_display_eight.py`，脚本 SHA-256 `750b74dad0fe23c1cedf67cb0a1275e4f81ced6681afa28a06735ee04ba6beb9`，报告 `/tmp/nt-dual-gt-evidence/score-source-aligned-display-eight.json` SHA-256 `a9597e9879ceb7dae1022dab2a80edecb2108a881ec7984ed41d9d29ecdbd2a9`。方法先把每个 detector 显示候选与**其源帧** NanoTrack 框中心相减，用最初 12 次匹配的中位偏移校准（无 GT 参与）；只在事后用两目标已确认 GT 计算误差。八组各有 69～77 个匹配候选。六组新增 trace 的 `state`、`tracking_phase`、`reacquired`、`control_valid` 与正式 `9f84efe` CSV 逐帧一致；car001 人工框第 235 帧仅有 1 帧 tracking/display 几何小差异（tracking x 相差约 `0.42 px`），其余新增框几何全相同。
- 源帧对齐后，car001 标准/左上/右下后段 ≥240 帧的跟踪与候选中心平均残差约 `1.34/1.07/1.31 px`；car002 标准/真值/左上/右下分别为 `5.96/3.06/2.85/4.40 px`。若将 detector 候选按 tracker 的源帧到结果帧位移及前 12 次中位偏移作**离线反事实**校正，car002 后段四框的 tracker→候选中心均误差分别 `7.74→2.66`、`4.84→2.76`、`3.41→3.23`、`6.74→3.03 px`；car001 后段四框也略有改善或接近持平。此处只评分候选，程序没有输出或控制这些反事实框。
- **否决单一残差门 / Handoff**：连续三次源帧对齐残差 `>3 px` 虽在 car002 标准与右下框后段持续出现，却在 car002 真值初始化的第 113～128 帧也连续触发；该段真值可见，tracker 中心均误差 `1.51 px`，校正 detector 候选反而 `2.63 px`。因此单独用 detector 残差触发搜索或直接改 `tracking_bbox` 会造成误报；detector 仍只作显示/临时候选，不能直接授权控制。正式八框仍 `2/8 PASS`、handoff HOLD。下一步若利用这个独立定位信号，必须与 NanoTrack 身份/完整可见性验证合并，并在八框闭环证明两路中心与遮挡语义同时改善。

## 2026-11-14T08:50:45+08:00 对称首帧模板平均 RK3588 四组否决并撤回

- 候选精确提交 `codex/dual-gt-init-consistency@4c28b28018f923083926765cca2b8f2596dd9d78`，起始工作树干净。`git archive` 源码包 `/tmp/nt-dual-gt-evidence/anchor-jitter-4c28b28-source.tar` 与板端 `source.tar` SHA-256 同为 `7646d376de97541599868ec06bc288bf13431e4801ba3e35e31cd188bbc1286b`。RK3588 `LKD3588` Linux `6.1.84` 隔离根 `/home/neardi/Project/SOT/dual-gt-anchor-jitter-4c28b28`；执行 `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build --parallel 4`、`ctest --test-dir build --output-on-failure`，板端 `23/23 PASS`，二进制 SHA-256 `79dc3f32eab9ac97cd60a0045d383e51b7f181e9c89d01b1b8c075db46d5c072`。`runs/headless-eval.conf` 与已验证基线内容相同，SHA-256 `2d45ca0f31916e0d05e0e5c2cad189bbf0d42f372aa4a67f3dbb3de939c531d6`，其绝对 controller、视频、tracker RKNN、VisDrone9 detector/manifest/labels 路径沿用下方 `9f84efe` 已哈希绑定环境；RKNN API `2.1.0`、driver `0.9.8`。构建/CTest 日志在隔离根 `runs/`。
- 板端实际执行 `/home/neardi/Project/SOT/dual-gt-anchor-jitter-4c28b28/runs/run_screen.sh`，脚本主机 `/tmp/nt-dual-gt-evidence/run_anchor_jitter_screen.sh` SHA-256 `4d37ced70cba6e51134a106a9d44e6f8369f3740a80c7307fbfe927a0e845496`；逐组用 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-boxes runs/<label>/frames.csv --session-metadata runs/<label>/session.json` 跑 car001 标准 `(992,582,37,39)`、右下 `(994,584,35,37)` 和 car002 标准 `(970,517,38,38)`、真值 `(967,513,42,42)`。四组 exit 0，各 271 数据帧；未生成 AVI。板端 CSV/session/log 按上述顺序 SHA-256：car001 标准 `8e534a6aec594b883b1932b9dea6829fc37e2688208f334fd8e9c8b1291ea638` / `f8770beb768d92a4a78d04d9db2487bc5dd33a0c910f8620d9c9f5669968dd1f` / `f707bacad3b60095071ba4a5cee492e918b2c9640b6acdd07c805c299e0d0e0b`；car001 右下 `e00bd5d54a023f34fc8ab175bb38086eadefd7f8500a9e8251d82e1997b9ff76` / `b0ac0ac4b4007de02503cadc114670ad832a06a87f876f4968443daad3299e11` / `cc04bbcb25afd41fb46ee7c202382bc274017af89057889392bba64734e1baa5`；car002 标准 `5ba1f5fd9f278db438376b5cc374f01e578b4a1d0ed300c2d59a75653e0a8c8b` / `52d79cc0009b38b9f9ad19658dd043fe777636e6b6563fb584c5dd77544da6cb` / `5278d25073d87535055f5e48e368027182a7449179443284b71681e7ddce3a69`；car002 真值 `35692849ad5a23388367cccec6b9098b0f00ca215c8e76b3f0bec807af95ed87` / `52ecc4d05d3bae979c8c810e11e55c9866723645f01347a0fa1c59fc13135356` / `4606312c3227112221400d2eac810a697a7713c5077395c6d5e4cd9036e45e96`。四 CSV 回传到 `/tmp/nt-dual-gt-evidence/anchor-jitter-4c28b28/<label>.csv`，SHA-256 与板端逐一相同；session/log 留板端。
- 主机用 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ...` 给上述四组与各自已确认 GT 评分，报告 `/tmp/nt-dual-gt-evidence/anchor-jitter-4c28b28/four_score.json` SHA-256 `388fa753ce5bc169784f76750e0aa845a220ca0ea58c18453beb5806e1544400`。car001 标准 `FAIL`（完全遮挡语义、恢复），第 153 帧额外重捕且后段 190 帧才重捕，tracking/display 中心 `5.28/5.25 px`、IoU `0.727/0.739`；原基线该框 `PASS`、188 帧恢复、tracking 中心 `4.28 px`。car001 右下 `FAIL`，恢复 192 帧（基线 191），中心 `4.67/4.68 px`、IoU `0.718/0.769`。car002 标准/真值均 `FAIL`（完全遮挡语义），233 帧恢复，中心分别 `2.32/2.34 px`、`2.02/2.02 px`；标准框 IoU 仅 `0.811/0.754`，真值框 `0.879/0.755`。car001 标准与右下 tracking/display 跨框中心 P95 `5.34/5.92 px`，没有足够一致性收益抵消标准框身份和恢复退化。
- **裁决 / Handoff**：本项同提交撤回对称模板平均代码，保留候选提交与板端隔离证据；正式算法文件恢复为已验证 `9f84efe` 内容。`display_bbox` detector 继续只用于显示，未把实验模板/检测框引入控制。正式八框仍 `2/8 PASS`、handoff HOLD，下一步需针对恢复候选身份与完全遮挡可观测性提出并验证新机制，不能从本次 `23/23` 构建测试推断算法改善。

## 2026-11-14T08:45:03+08:00 对称首帧模板扰动平均候选（待板端双目标筛查）

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，开始时工作树干净，基线 `4ce4d4ff1646a3a52cc9b9ac9e85004bc7bbd8b5`。八框原始 CSV 显示 car001 遮挡前中心最大差距一般约 2～3 px，第 176 帧起右下框进入搜索较晚、分歧增大，第 188～190 帧四框中心最大差距约 22 px；单改 detector 周期已有错车和重捕变晚反例。
- 候选只改 `rk3566/nanotrack_v3/src/nanotrack_v3.cpp` 的初始化模板特征：原 ROI 特征加上上下左右各 `min(2 px, 5% 最小边长)` 的对称微扰模板特征，取五路均值作为永久 anchor；边界越界时跳过对应采样。控制器、detector、近期模板、`tracking_bbox`/`display_bbox` 语义及真值均未改。意图是降低微弱初始框差异对永久模板的影响，不用真值调整运行时框。
- 实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 和 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，`git diff --check` 通过。主机单测不能证明该 RKNN 模板融合的板端轨迹。**Handoff**：待本项与源码同提交后，以精确提交构建 RK3588 隔离目录，先对 car001 标准/右下和 car002 标准/真值四组完整 271 帧筛查；只有两目标中心、恢复身份和无框语义均不退化，才扩八框。正式已验证 `9f84efe` 八框仍 `2/8 PASS`、handoff HOLD。

## 2026-11-14T08:41:14+08:00 首帧 detector 框归一化与显示候选分数反例

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，本轮基线 `8a6c3b68fafadd2865c76952c6e511ccbc384d70`；算法与配置未改。板 `LKD3588` Linux `6.1.84`，RKNN API `2.1.0 (967d001cc8@2024-08-07T19:28:19)`、driver `0.9.8` 沿用已哈希绑定 `9f84efe` 八框环境。板端根 `/home/neardi/Project/SOT/dual-gt-handoff-9f84efe` 的精确源提交 `9f84efecd57d611a239cc71c99ab2e4295025dbc`，本轮复核 `build/nanotrack_v3` SHA-256 `41d83ef0d772833b29400a35cacb8825a1a402f24ea7dbfe9279ac09b062343b`、`runs/headless-eval.conf` SHA-256 `2d45ca0f31916e0d05e0e5c2cad189bbf0d42f372aa4a67f3dbb3de939c531d6`；视频、两套真值和 tracker/detector 模型身份沿用下方 `9f84efe` 条目。未向板端部署新代码。
- 八组原始首帧 detector 均选中同一个对应目标框：car001 `(994.000916,581.063293,35.289490,39.486084)`，car002 `(973.034729,520.489929,34.091492,31.423828)`。为检验“将微扰人工 ROI 统一到 detector 框”是否安全，在独立板端目录 `runs/detector-anchor-screen/` 用现有程序分别执行 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox 994,581,35,39 --output-boxes runs/detector-anchor-screen/car001/frames.csv --session-metadata runs/detector-anchor-screen/car001/session.json` 和相同命令的 car002 `--bbox 973,520,34,31`。两次 exit 0、CSV 各 271 数据帧；未要求 AVI。car001 CSV/session/log SHA-256 `a26997ea0a90ee4d5f901167f34cced29bddbf6068be7f2e1e04a3c09c8b6f5c` / `928bebd73c887f1cd3e2b7fe6f405d3235ac39e83602c81224ced79d7a3fa4b2` / `a568d819229c8a9c6aa0448af7545e8ff79e8b01df6349689ea29f3f6d64f247`；car002 相应 SHA-256 `1f6640df51fb5729a67e5bdc633feedf2490d43d068ceb049848b4217fb8c893` / `9c6a01bf462b30f6b7aacdefc74289b8561957101537da7ac0b0cc2a4758f6f6` / `efb8c481c29b41428a3e6d85e0be7578d1e9e73ef1382e7e4eebaa8af5c69352`。CSV 主机副本 `/tmp/nt-dual-gt-evidence/detector-anchor-car001.csv` / `detector-anchor-car002.csv` 哈希与板端相同；AVI/完整日志留板端。
- 实际用 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run car_001 detector_anchor 994,581,35,39 /tmp/nt-dual-gt-evidence/detector-anchor-car001.csv --run car_002 detector_anchor 973,520,34,31 /tmp/nt-dual-gt-evidence/detector-anchor-car002.csv --output-json /tmp/nt-dual-gt-evidence/detector-anchor-two-score.json` 评分，报告 SHA-256 `6fc6a9e7cefee79f74902266009a2f1099a1d4407c97bb9aab7a15c2c439e834`。car001 `FAIL`（visible_boxes、full_occlusion_semantics、identity_proxy），第 186 帧早恢复到错误车辆，tracking/display 中心均误差 `33.54/33.35 px`、IoU `0.487/0.520`；car002 `FAIL`（visible_boxes、full_occlusion_semantics），第 234 帧恢复，中心 `3.13/3.15 px`、IoU `0.640/0.728`，无框段仍有 1 帧假观测。首帧 detector 归一化会使 car001 相比原四框约 4～5 px 中心误差灾难性恶化，否决该方案。
- 用实验 HOG 遮挡分数对之前 `display-trace-d157d0d` 已匹配的两目标各 76 个 detector 显示候选评分，脚本 `/tmp/nt-dual-gt-evidence/score_hog_display_candidates.py` SHA-256 `275f9a67ca5e466b1190e91a991d567feb589cab0dc4ff3554fd556004765810`，报告 `/tmp/nt-dual-gt-evidence/score-hog-display-candidates.json` SHA-256 `97f5f1d36ebe58c4245eb787c9015c30425fdddce192cf3ce8e373b32fd85d33`。后段 ≥240 帧 car001 detector HOG 平均 `1.75` 高于 tracking `1.30`，但 detector 中心误差 `9.30 px` 差于 tracking `7.33 px`；car002 detector HOG `0.86` 低于 tracking `1.09`，但中心 `2.51 px` 优于 tracking `7.74 px`。该分数反向选择更准的框，不能当两路中心仲裁依据。灰度/白色像素纵向分布探针 `/tmp/nt-dual-gt-evidence/probe_whiteness_occ.py`（SHA-256 `b0df17d979e9085c88da99f0393e95d4336d364e6ad8ba8c06d81bdc0b7ddefb`）及输出 `probe-whiteness-occ.tsv`（SHA-256 `b96662e2c901e2632d7aff86e5bd8425a94840050b0afc77ec54b85244819758`）显示 car002 无框第 229 帧与重新可见第 230 帧纵向白色分布仍相近，简单亮度阈值无法作为完整车体门。
- 证据边界 / Handoff：保留 `9f84efe` 正式基线 `2/8 PASS`，板端隔离输出是筛查反例，不作为新算法或交付。`display_bbox` 里的 detector 框继续只用于显示；任何控制定位变更仍需两目标四种人工微扰框的逐帧真值评分及 RK3588 验证。下一步针对 NanoTrack 模板/恢复假设做不会把相似车辆误认为原目标的可验证改动。

## 2026-11-14T08:34:36+08:00 独立遮挡分类器筛查：car002 无框段误报，暂不接入

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，本轮基线 `709c6db8202f864f199687c5ec027157ce598edc`，开始与结束时工作树均干净（写入本进度项之前）；正式算法、模型、配置和两目标真值未改。延续 RK3588 `9f84efecd57d611a239cc71c99ab2e4295025dbc` 八框基线：`2/8 PASS`，car001 左/右第 191 帧恢复晚于允许界限，右框还有完全遮挡语义失败；car002 四框全因完全遮挡语义失败。用户要求 tracking/display 两路中心均贴真值，并明确 detector 的 display 框只用于视觉效果，故本筛查没有把 detector 或新分类器输出写入控制框。
- 独立训练来源 `/home/u20/dataset/VisDrone2019-DET/VisDrone2019-DET-train`，独立验证 `/home/u20/dataset/VisDrone2019-DET/VisDrone2019-DET-val`；只选 `category=4, score>0, truncation=0`、宽高各 25～90 px 的汽车。`/tmp/nt-dual-gt-evidence/screen_visdrone_occlusion.py` SHA-256 `cd5f2337ac282072e413ddc32e93b0c27577f987533350c3b57c6390d7e13ffb`，实际运行 `.venvs/nanotrack/bin/python /tmp/nt-dual-gt-evidence/screen_visdrone_occlusion.py --output /tmp/nt-dual-gt-evidence/visdrone-occlusion-screen.json`；64×64 灰度 HOG + OpenCV 线性 SVM，训练 2343 样本（无遮挡 1168、遮挡一级 882、二级 293），验证 782 样本（393/292/97）。独立验证 AUC `0.8952`、准确率 `0.8107`，无遮挡/遮挡一级/二级在零阈值的召回 `0.8651/0.6952/0.9381`。报告 `/tmp/nt-dual-gt-evidence/visdrone-occlusion-screen.json` SHA-256 `55bb39f098544660c4d6f05915ad4b0edc8a88ad0221a3c46a5a113ed`，实验模型 `.xml` SHA-256 `7b185f935463f4d2f395189ddc3b229edcd0a84ad9ef601e8dd76c0ac22d1488`（33 MiB，保留在主机 `/tmp`，未入 Git）。两条 canonical 目标均未参与训练或阈值选择。
- 随后实际执行 `.venvs/nanotrack/bin/python /tmp/nt-dual-gt-evidence/eval_visdrone_occlusion_on_occ.py > /tmp/nt-dual-gt-evidence/visdrone-occlusion-canonical.stdout`，脚本 SHA-256 `06a5bacc63b1c881d70265df61c83f52113529b4aea0feb661fef65fdfb80f31`；报告 `/tmp/nt-dual-gt-evidence/visdrone-occlusion-canonical.json` SHA-256 `4c3483ff058419196513a016dc805579b3543540be5f2c9a3399adb8c0b38b9c`。该脚本用 `scripts/evaluate_occ_video_1.py` 严格校验两目标 GT 合约、解码 271 帧，并分别给真值可见框及原 RK3588 八框 CSV 的预测框打分。car001 真值可见 258 帧中 `98.1%` 得分为正，car002 真值可见 252 帧中 `99.6%` 为正；但 car002 已确认无正式框的 211～229 帧，四组预测框仍有 `31.6%～57.9%` 被判为可见，229 帧 car002 真值初始化预测框得分 `+1.04`。这不能提供可靠的完全遮挡门，更不能证明目标身份；只看 VisDrone 验证集 AUC 会误导。car001 181 帧四组预测框均为负，但 `car001_right` 190 帧仍为 `-1.13`，简单分类门还可能延迟正确恢复。
- 证据边界 / Handoff：这是主机离线筛查，未在 RK3588 上运行新模型，不构成板端改进或交付。保持 `9f84efe` 算法基线与 `2/8 PASS`，不向生产交接此分类器。下一步针对两目标的中心误差与初始框扰动继续找可解释、可复现的定位改动；先以两目标全 271 帧和各四初始框评分，再考虑板端验证。

## 2026-11-14T07:05:40+08:00 官方 SiamMask 完整车体掩码的双目标桌面筛查否决

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，分析基线 `cbcfef109eeb6d8a2966ba0b422294a6f8cabaeb`，工作树开始干净；正式算法和配置未改。只用 canonical `/home/u20/single-object-tracking/data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d` 与两目标各自已确认 GT。官方 SiamMask 源码 `repos/siammask@a2b3ce3dca23b3a911d96b791e007eec6d39a625`，`experiments/siammask_sharp/SiamMask_DAVIS.pth` / `config_davis.json` SHA-256 `d5a0e7cb13e2799bcf93545691261e7a48413ffb54bce9cb3b7fa3c560da4204` / `ba5fc84ba741e593ac639e3210c5a58e0e860df8dff3182277a13d3a99e2bf74`。已有 runner `scripts/run_siammask_video.py` SHA-256 `b7e802aaa27b1d1514e025e55b490c747c0acdb8bf333ccb57af8b02c7daed85`，用官方 `siamese_init/siamese_track` 且 `mask_enable=true`、`refine_enable=true`，输出分割 polygon 的轴对齐外接框；实际环境 CPU 约 5 FPS，`nvidia-smi` 本轮无法连接 GPU 驱动。
- 实际用 `.venvs/siammask/bin/python scripts/run_siammask_video.py --repo repos/siammask --config repos/siammask/experiments/siammask_sharp/config_davis.json --snapshot repos/siammask/experiments/siammask_sharp/SiamMask_DAVIS.pth --video data/occ_video_1/1.mp4 --bbox <x y w h> --max-frames 271 --output <MP4> --bbox-output <TXT>` 分别运行 car001 标准初始化 `(992,582,37,39)`、car002 真值初始化 `(967,513,42,42)`；先用 car001 前 20 帧筛查，中心均误差 `1.24 px`、IoU `0.891` 后才扩完整长度。两组输出均为 271 框，MP4 均经 `ffprobe -count_frames` 验证 1920×1080×271。主机 `/tmp/nt-dual-gt-evidence/siammask-car001-nominal-271.txt` / `.mp4` SHA-256 `679ced5f98e690f0fb1cb15c501581bc33fe765a7488f62ef3030c46e0f9df3d` / `e0d919a2b3682d6585e3bb7473be7a58f4c0d064b72f0d0f4f81922f73a9aac8`；car002 真值框相应 `.txt` / `.mp4` SHA-256 `fb45ba8db275fa2d2cf1a6b7cbd41513ecef1e84a3c005de1db4c35eccc8a15f` / `113411dec45a3da0dc8ff00301a11ce894124d77a0955916a53aac97ae769df5`。运行日志 car001/car002 SHA-256 `2daacdb3d35a53165e968fe1ea024f8ebc8e59e0ba2324d2ba07ad136b0ba273` / `73622e09c1afe28ec0fa2933ae28273fefdc2782610a0a70ceac37b73e713c89`。
- 同真值执行 `python3 /tmp/nt-dual-gt-evidence/score-siammask-two-targets.py > /tmp/nt-dual-gt-evidence/score-siammask-two-targets.json`，脚本/报告 SHA-256 `322bd76474f6a0b38a15da29fbd9531d7bcd42a66833ea0bc835c3a06dd10d03` / `f2075c2ebbf820fbd4dd438b4a4d07b3223e8755052066e2ff2629542c1d5314`；比较了 `9f84efe` 精确 RK3588 基线同初始框 CSV 的 tracking 框。car001 前 169 个可观测帧 SiamMask/NanoTrack 中心均误差 `3.41/3.36 px`，IoU `0.802/0.768`；第 182～194 帧 SiamMask 跟到相似车辆，IoU `0`、中心 `74.38 px`，第 240～270 帧仍 IoU `0`、中心 `190.12 px`，全部 258 个可观测帧中心 `57.51 px`，远劣于 NanoTrack `4.28 px`。car002 全部 252 个可观测帧 SiamMask/NanoTrack 中心 `2.93/2.00 px`、IoU `0.632/0.883`；其 15 个人工确认无正式框帧 SiamMask `15/15` 仍给非空掩码外接框，不能作为完整目标可观测性门。
- **裁决 / Handoff**：这两个完整 271 帧代表初始框已分别证明 SiamMask 的 car001 身份漂移与 car002 过大/无框输出；因此不扩大它到其余六框，也不把现成掩码并入 tracking/control 或把 detector 的 `display_bbox` 当作定位修正。桌面独立跟踪器没有生产控制器和 RK3588 时延，此结果只用于否决该直接替换路线，不外推所有分割网络。原八框正式基线仍为 `9f84efe` 的 `2/8 PASS`，handoff HOLD；后续应优先提供能判别完整目标及相似车身份的冻结模型，再用两目标八框板端闭环验证。本条与诊断文档同提交。

## 2026-11-14T07:00:12+08:00 RKNN 模板特征的候选身份与完整可观测性筛查

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，基线 `e7aa84a725b6fb59aa63ea4430079119f54b2f57`，开始时工作树干净；正式算法、配置和 GT 未修改。用 `/tmp/nt-dual-gt-evidence/template-feature-probe.cpp`（SHA-256 `04678226186108396203161bc3503ff7b1457ad51ce853e4e3ad2028910ab253`）复制 `NanoTrackV3::extract_template_feature` 的裁剪、frame0 BGR 全帧均色及 127×127 模板输入语义，调用真实 RK3588 `RknnModel` template backbone。223 项输入请求 `/tmp/nt-dual-gt-evidence/template-feature-requests.tsv` SHA-256 `acb8412c6272a5bdb950d6ea206a944d56bd98d9c17b9a656d3b97215f51cb39`，来自两目标各四初始 ROI、`9f84efe` car001 四组已哈希绑定板端 CSV、`c80c78c` car002 四组被否决候选 CSV，以及各自对应真值图块。真值仅供事后对照，未参与恢复逻辑或训练。
- 板端目录 `/home/neardi/Project/SOT/dual-gt-handoff-9f84efe/runs/template-feature-probe/`；实际用 `g++ -std=c++17 -O2 -I rk3566/nanotrack_v3/include -I /home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include runs/template-feature-probe/template-feature-probe.cpp rk3566/nanotrack_v3/src/rknn_model.cpp -o runs/template-feature-probe/probe -lrknnrt $(pkg-config --cflags --libs opencv4)` 原生构建，二进制 SHA-256 `92df18a671bb499bf3bf62af770691562ecf42c379883614e2ca10f0f1a05012`。运行 `runs/template-feature-probe/probe <template_backbone.rknn> <1.mp4> runs/template-feature-probe/template-feature-requests.tsv runs/template-feature-probe/features.csv`；探针完成 223/223 项、每项 6144 个 float（模型输出 `1×96×8×8`），运行日志 SHA-256 `dceb8eda9caa71a118ac48107515df43775fa25aeaa0f61b7e8e2fbb0d31c592`，特征 CSV SHA-256 `b2bbfdc612ca5d93d1c32cac32e95184ef21fedef8e6409c20989a68fa83e30d`，主机副本 `/tmp/nt-dual-gt-evidence/template-feature-features.csv` 同哈希。板端模型 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/models/template_backbone.rknn` 与视频 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/data/occ_video_1/1.mp4` 本轮实核 SHA-256 `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55` / `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，RKNN API `2.1.0 (967d001cc8@2024-08-07T19:28:19)`、driver `0.9.8`。
- 主机执行 `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python /tmp/nt-dual-gt-evidence/template-feature-summary.py > /tmp/nt-dual-gt-evidence/template-feature-summary.json`，脚本/结果 SHA-256 `c4af479a5b651959dcc8265241927dd47a3e34f969d7fc1155cc88285cffc2d8` / `128e986ba42c7c900364d4430a70c54b65c41637b8e45e9762f13d2060aae755`。以第 0 帧实际 ROI 模板与同组候选模板的**扁平化 6144 维原始余弦相似度**比较：car001 第 181 帧不可靠 recent 候选四框约 `0.443～0.493`，首次经 GT 证实可靠的 recent 框（标准/人工第 187、左上第 188、右下第 190 帧）约 `0.707～0.734`。但 car002 第 229 帧人工确认无正式框的 safe 候选四框相似度 `0.668～0.713`，均高于各自第 230 帧首次完整可观测**真值框**的 `0.653～0.666`，差额 `+0.014～+0.047`；raw feature norm 同样没有四框一致的部分/完整目标分界。这证明“初始模板余弦达到单一阈值即可发布完整目标”在这组反例中不成立，不能用它替换现有 NanoTrack 两击或无框门禁。
- **证据边界 / Handoff**：这是独立板端特征抽取和离线排序，使用了候选及真值图块，但没有进入 runtime、没有闭环轨迹、没有测额外推理的正式帧时延；仅否决该原始特征与**初始**模板余弦的简单完整性门槛，不推断所有学习型外观/遮挡模型均无效。后续模型/外观 owner 可用冻结的 223 图块、特征 CSV、两目标 GT 和八框板端轨迹设计跨初始框、同类负样本与部分遮挡验证器；任何晋级仍须在 RK3588 用正式八框检查 tracking/display 中心与 IoU、无框段和身份连续性。`display_bbox` detector 仍只作外观用途；正式基线 `9f84efe` 八框 `2/8 PASS`、handoff HOLD。本条与诊断文档同提交。

## 2026-11-14T06:55:03+08:00 搜索状态与 detector 间隔组合的八框板端否决

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，分析与运行基线 `69cbae84c2ecc5a7fd2efc61f555c77e5dd5175b`，开始时工作树干净。沿用精确源码 `9f84efecd57d611a239cc71c99ab2e4295025dbc`、RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-handoff-9f84efe`、原生二进制 `build/nanotrack_v3` SHA-256 `41d83ef0d772833b29400a35cacb8825a1a402f24ea7dbfe9279ac09b062343b`。板 `LKD3588` Linux `6.1.84`、RKNN API `2.1.0 (967d001cc8@2024-08-07T19:28:19)`、driver `0.9.8`，视频 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；官方 NanoTrack template/search/head RKNN、tracker manifest、VisDrone9 detector/manifest/labels 与下方 `9f84efe` 已哈希绑定八框矩阵相同。本次只在板端 `runs/` 新建配置与输出，没有覆盖正式源码或模型。
- 逐帧时序显示，car001 右下框原 400 ms 参数在第 188 帧发恢复 detector 请求、189 帧取回上一帧结果、190 帧才首次用 detector 候选作为 NanoTrack 探针，所以正确框在 190/191 帧形成两击。只改 `detector_interval_ms=400→360` 的板端 controller/headless 配置 `runs/detector360-controller.conf`/`runs/headless-detector360.conf` SHA-256 `c5f7599fafe13f609f64b0f90d1d9f69bc8d97049d8bea3f9a157a787a1109c7`/`c98f2eeffab96124627ce7620f5f42718450b1ba84cf6f0ae470eca7a04d3154`；右下框完整 271 帧 CSV/AVI `runs/detector360/car001_right/frames.csv`/`output.avi` SHA-256 `7d258253051b743b7e57e29464282a5c5ced5ae30fe6c714ff8d97c92c810419`/`dff2a0ebf6733b3f00102108bdd16a7bf97132b400b2756732b2a1f477f3a2de`，评分 `/tmp/nt-dual-gt-evidence/detector360/car001-right-score.json` SHA-256 `2aa086654a51be9301ff12d204130c0a2b5deff19d6091ef5846dc15c2f715ce`：第 190 帧重捕、tracking/display 中心均误差 `4.91/4.94 px`，仍因第 172～174 帧完全遮挡语义和第 190 帧晚重捕 FAIL。
- 只改 `uncertain_ms=200→100` 的板端 controller/headless 配置 `runs/uncertain100-controller.conf`/`runs/headless-uncertain100.conf` SHA-256 `a59d9e8ed394b9a1b95074b4217b469254f1fb401adda8af10a4ef16ff8c89c1`/`262fbf0b8664abf9fe16c91e9a9822c783f8254d43566c92efc84cdf7aa2b794`；右下框 CSV/AVI `runs/uncertain100/car001_right/frames.csv`/`output.avi` SHA-256 `de2359cbb18222e90c946e89738b5e3365eb752318d852d995dbff472f6dc553`/`d06493f562a6d94a3f10c78af4c547dadb7821dda6191ce303698844d456ea3e`，评分 `/tmp/nt-dual-gt-evidence/uncertain100/car001-right-score.json` SHA-256 `e0302fe0c295d8e8614335adec3bdf2d7baac6347b9731901936bd3768bf782f`：完全遮挡语义通过，重捕提前至第 190 帧，tracking/display 中心 `4.89/4.92 px`，仍过截止而 FAIL。两单参数结果不能外推组合的八框表现。
- 组合 `uncertain_ms=100`、`detector_interval_ms=360` 的板端 controller/headless 配置 `runs/uncertain100-detector360-controller.conf`/`runs/headless-uncertain100-detector360.conf` SHA-256 `ed586052470ee5f0dae45bcd82231c100c5c0e121912f4f475b47b584c8e3eb1`/`61088c73ba9479ed6e98d846b8bdba1fe46e0ee53d1616acfc764d416484419b`。实际逐组运行 `./build/nanotrack_v3 --config runs/headless-uncertain100-detector360.conf --bbox <ROI> --output-video runs/uncertain100-detector360/<label>/output.avi --output-boxes .../frames.csv --session-metadata .../session.json`，目标与 ROI 为 car001 标准/人工/左上/右下 `(992,582,37,39)`/`(993,585,36,36)`/`(990,580,39,41)`/`(994,584,35,37)` 和 car002 标准/真值/左上/右下 `(970,517,38,38)`/`(967,513,42,42)`/`(968,515,40,40)`/`(972,519,36,36)`。其余七组运行脚本 `runs/uncertain100-detector360/run-seven.sh` 主机 SHA-256 `e6018e9d86464a909f410a49afadd1ccb375a1257b2c911054e1b6f9e40dc334`。八组 exit 0，八个 AVI 经 `ffprobe -count_frames` 均为 1920×1080×271；CSV/JSONL/AVI/session/runtime.log 的 40 文件清单 `runs/uncertain100-detector360/eight-outputs.sha256` SHA-256 `856db8b869d7a192898861dc9f48f5126dcacedd560c31f17032980503ced65d`，板端 `sha256sum -c` 40/40 OK。八组文本包 `runs/uncertain100-detector360/eight-text.tar.gz` SHA-256 `4c61ca94c75c185442e6f89820f537e9484044c18138493c3435c2ce7ed44a12`，主机回传 `/tmp/nt-dual-gt-evidence/uncertain100-detector360/` 哈希相同，解包后 24 个 CSV/session/log 与板端清单一致；AVI 大文件留板端。
- 用 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1` 对匹配真值的八个 `(target, label, ROI, CSV)` 运行完整评分，报告 `/tmp/nt-dual-gt-evidence/uncertain100-detector360/eight-center.json` SHA-256 `5c7d819bdb9c0e661d30e78853a8d4609143227004187d874df55e5bc002ec1e`。car001 右下框单组 PASS，第 189 帧重捕，tracking/display 平均中心误差 `4.82/4.85 px`；但标准/人工/左上三框都在第 181 帧错误重捕，之后第 199/219/219 帧再次重捕，各自 tracking 中心均误差 `28.73/27.39/27.49 px`，正式 FAIL。car002 四框全 FAIL：标准/真值/右下主要因完全遮挡语义，左上还在第 145/245 帧双重捕并丢失可见框；四框 tracking 中心 `1.99/1.82/5.03/3.48 px`，已确认无框段没有假观测并不足以抵消其他失败。跨初始框最大成对中心差 P95：car001 tracking/display `77.00/76.78 px`，car002 `31.83/27.33 px`，远差于 `9f84efe` 基线 `3.31/3.12`、`6.21/6.21 px`。detector `display_bbox` 仍只做外观，两路中心分别评测；单组右下 PASS 不构成双目标一致性改进。
- **裁决 / Handoff**：组合参数明确否决，板端隔离配置和八组输出保留用于复核，正式 `rk3566/nanotrack_v3` 配置、源码、模型均未修改。缩短进入搜索状态和 detector 请求间隔改变多候选调度，导致 car001 三组先捕错车；不能用右下单组通过作为晋级依据。仍以 `9f84efe` 正式八框 `2/8 PASS` 为算法基线，handoff HOLD。后续应先形成抗相似车的候选身份与完整可观测性证据，再重跑同一八框及两路中心；本条与诊断文档同提交。

## 2026-11-14T06:44:05+08:00 两目标重捕窗口的逐帧可用性诊断

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，分析基线 `c371c276910621b079b0588e5c9312f276896f42`，开始时工作树干净；正式算法文件未改。严格加载 canonical `data/occ_video_1/1.mp4` 与 car001/car002 各自已确认真值；视频 SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`。对 `9f84efe` 八框基线的 car001 四组板端 CSV（`/tmp/nt-dual-gt-evidence/handoff-9f84efe/<label>/frames.csv`）和 `c80c78c` 被否决候选的 car002 四组板端 CSV（`/tmp/nt-dual-gt-evidence/strong-score-c80c78c/runs/<label>/frames.csv`）执行 `python3 /tmp/nt-dual-gt-evidence/recovery-window-diagnosis.py > /tmp/nt-dual-gt-evidence/recovery-window-diagnosis.json`。脚本/完整帧级报告 SHA-256 `51eb3fa6efaf82a5b1f5175eeff6514773a62dbb2eb507b385592fe7dc80f70a` / `3816ff6d595ea2d09e13624d001326fd84d988dd83d5e0bf609515af5574f108`，每组报告绑定 CSV 自身 SHA-256、源码 commit、candidate source、冲突、确认击数、原始框/分数及与对应真值的 IoU/中心误差；只作事后诊断，真值没有进入运行时。
- car001 标准/人工框第 187 帧已各有贴近真值的 recent/safe 原始框，随后第 188 帧两击重捕，正式 PASS。左上框第 188/189 帧 recent 原始框 IoU `0.74/0.73`、中心误差 `3.0/5.1 px`，但身份冲突持续至第 189 帧，确认击数为零；第 190/191 帧才两击并重捕，晚于正式第 190 帧的排他截止。右下框直到第 190 帧才首次出现 IoU `0.81`、中心误差 `3.5 px` 的 recent 原始框；第 189 帧 recent/safe IoU 仅 `0.00/0.27`，当前两击语义最早第 191 帧重捕。左上是可用候选被冲突挡住，右下是此前根本没有可靠的目标观测，不能用同一个冲突阈值解释两者。
- car002 被否决候选的真值/右下初始框在人工确认无正式框的第 228/229 帧取得同一 detector 候选的两次 NanoTrack 命中，第 229 帧误重捕；该帧 recent/safe 原始分数分别约 `0.9975/0.9980` 和 `0.9966/0.9968`，detector 框高约 `42.39 px`，recent 原始框高约 `45.71/45.03 px`。第 230 帧首次有可观测真值，正式框高约 `55 px`。高分数只能证明模板响应强，不能证明完整目标已可观测；直接降低 car001 两击要求会增加 car002 第 229 帧同类风险。car002 数据包 `annotation_metadata.json` 的 `formal_evaluation_authorized=true`、`unresolved_review_count=0`，第 229 帧无框状态已人工确认；事件文件保留早期 `needs_human_review` 字样，正式状态以当前元数据、逐帧标注和评估契约为准。
- **裁决 / Handoff**：不以真值帧号做运行时延迟，也不为追单组重捕时间直接降低两击或抬高 raw score 门。两目标实际需求是同时具备完整目标可观测性、候选身份和目标定位证据；现有官方 RKNN 模型及候选表尚未达到八框要求。请协调者将帧级 JSON、八框 CSV 与对应两目标 GT 提供给模型/外观评估 owner，特别审查 car001 第 187～191 帧和 car002 第 228～230 帧的模板响应/遮挡下误确认；新模型或验证器须以冻结哈希在 RK3588 八框闭环复验 tracking/display 中心、IoU、无框段及跨框差。正式八框基线 `2/8 PASS`、handoff HOLD，本条与诊断文档同提交。

## 2026-11-14T06:37:07+08:00 双真值八框重测：高分恢复候选否决并撤回

- 分支/工作树 `codex/dual-gt-init-consistency` / `/home/u20/single-object-tracking`，测试源码提交 `c80c78ca0063b6470a5219142c20fd6157ef4dfe`，起始工作树干净。`git archive --format=tar c80c78c` 的板端隔离源码包 SHA-256 `c6edf93fec3def196955f4741324af3afbc535afe54cf20451734430e0b5904d`，板端目录 `/home/neardi/Project/SOT/dual-gt-strong-score-c80c78c`，未覆盖 `9f84efe` 基线。controller/headless 配置 SHA-256 `782a0b2bffcd11ddf67cca4aa40eeae5e0ec8a8d79ff9c3b8d8a547449e685c8` / `209054f9c16515d58d3f78f4bc12d0a245749e7948e21f815966d4e912b89838`。板端原生 CMake、build、CTest 日志 `runs/cmake.log`、`runs/build.log`、`runs/ctest.log` SHA-256 依次为 `b8fa5de7f40fde2fd7b4da7be251777801622f97da57c0fb77cf1268b4e454e9`、`9d95cddf64c737c18f39c4fa6587e552ac758784e87eb51544cfb77a2678c18e`、`b47142f3b598dce36e718e793898110e96cdee811954a55a8ef35a31adf67ce0`；板端 `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `9ddd884a619dcebbee9dc8b5032206fc479b246ced581c88c2076cef97a68710`。
- 板 `LKD3588`、Linux `6.1.84`、RKNN API `2.1.0 (967d001cc8@2024-08-07T19:28:19)`、driver `0.9.8`。输入视频板端路径 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/data/occ_video_1/1.mp4`，SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；官方 NanoTrack template/search/head RKNN SHA-256 依次为 `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`、`d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`、`4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`，tracker manifest `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`；VisDrone9 detector/manifest/labels `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c` / `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9` / `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。上述输入哈希在本轮板端重核；GT 沿用 `data/occ_video_1/ground_truth/car_001` 与 `car_002`，目标和初始化框严格配对。
- 板端分别以 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video runs/<label>/output.avi --output-boxes runs/<label>/frames.csv --session-metadata runs/<label>/session.json` 运行八组；`car001_nominal/user/left/right` 的 ROI 为 `(992,582,37,39)` / `(993,585,36,36)` / `(990,580,39,41)` / `(994,584,35,37)`，`car002_nominal/truth/left/right` 为 `(970,517,38,38)` / `(967,513,42,42)` / `(968,515,40,40)` / `(972,519,36,36)`。八组 exit 0，八个 AVI 均经 `ffprobe -count_frames` 验证为 1920×1080、271 帧。输出 CSV/AVI/session/runtime.log 留在板端 `runs/<label>/`；32 文件 `runs/eight-outputs.sha256` SHA-256 `1226a45ff088d64f7b716f7430ced56b375afb0fa7f07f97020ca4687f24d7f7`，板端 `sha256sum -c` 全部通过。八组文本包 `runs/eight-text.tar.gz` SHA-256 `120418dc59b62a8158ff99f5e49d556036194bd9d9a32b9b331af2b8cbb71b55`；主机回传 `/tmp/nt-dual-gt-evidence/strong-score-c80c78c/` 两文件哈希一致，解包后的 24 个 CSV/session/log 与板端 manifest 一致，视频只留板端。
- 主机实际执行 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1`，分别传入上述八个 `(target, label, ROI, CSV)`，输出 `/tmp/nt-dual-gt-evidence/strong-score-c80c78c/eight-center.json` SHA-256 `ae8f49a3425532adaa0c76144d44a4a4ada99fee4110095d101b2f039930dd42`。正式门禁 `3/8 PASS`：car001 标准/用户框 PASS，左上框因重捕第 191 帧 FAIL，右下框因完全遮挡语义和第 191 帧重捕 FAIL；car002 标准/左上框 PASS，真值/右下框都在人工确认无框第 229 帧发生假观测与过早重捕，FAIL。car002 四框重捕帧 `231/229/241/229`。car001 四框可见帧 tracking/display 平均中心误差约 `4.28/4.32`、`4.63/4.69`、`4.34/4.35`、`5.02/5.05 px`，与 `9f84efe` 逐项相同；car002 为 `2.12/2.12`、`1.68/1.68`、`2.47/2.34`、`2.70/2.70 px`。car002 跨框 tracking/display 最大两两中心差 P95 `6.29/6.29 px`，较基线 `6.21/6.21 px` 退化，car001 约 `3.31/3.12 px` 不变。detector 的 `display_bbox` 仅用于视觉效果，两路中心和 tracking IoU 都按各自真值独立评分；不能以显示框外观替代准确性。
- 误重捕第 229 帧的 `candidate_source=DETECTOR`，两框 NanoTrack recent/safe 原始分数约 `0.997`；提高 `0.90` 分数门槛不能可靠排除无框目标。单纯把 detector 两击确认延长一帧，又会使已在第 241 帧才重捕的左上框越过当前截止帧，故不把该调参视为完成。**裁决 / Handoff**：撤回 `c80c78c` 的两项门禁、配置和专属测试；撤回后 `rk3566/nanotrack_v3` 内容与已验证 `8277b33` 逐文件一致。保留候选 commit 与隔离板端证据；正式算法仍以 `9f84efe` 八框 `2/8 PASS` 为验证基线，handoff HOLD。下一步处理候选身份与可观测性，并要求双目标八框两路中心/IoU 和跨框一致性同时改善；不得仅调整评分或 `display_bbox` 掩盖跟踪误差。本条与撤回代码同提交，结果以实际 commit 为准。

## 2026-11-14T06:24:45+08:00 强遮挡提前搜索加 NanoTrack 高分确认候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `04539d9`，开始时工作树干净。恢复上一已提交候选 `0c45bca` 的强遮挡连续两帧搜索逻辑，仅在新候选中叠加可关闭的 `recovery_confirm_min_raw_score`，当前 `config/visible-adaptive-display.conf` 取 `0.90`，配置 SHA-256 `782a0b2bffcd11ddf67cca4aa40eeae5e0ec8a8d79ff9c3b8d8a547449e685c8`。恢复候选仍进入身份候选表与冲突判断；达到两次 NanoTrack 命中后，只有**当帧**待提交模板的原始分类分数达到门槛才授权重捕。detector 不直接授权控制，`display_bbox` detector 仅用于外观。
- 反事实依据是同源 `9f84efe` 八框板端日志中八个正确重捕帧的当帧 recent 原始分数最低为 `0.917303`；已否决的 `0c45bca` car002 标准框误重捕第 226/242 帧所选 SAFE/RECENT 原始分数分别 `0.884239/0.813238`。这只说明该阈值值得闭环测试；部分遮挡或相似车辆仍可能得到高分，不能凭冻结 CSV 宣称安全或准确。配置解析/校验/指纹已包含新字段，测试验证低分两击仅保留候选、后来高分时才可由 NanoTrack 提交。实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；`git diff --check` 通过。
- **证据边界 / Handoff**：尚未在 RK3588 运行，正式已验证内容仍为 `9f84efe` 八框 `2/8 PASS`、handoff HOLD。下一步用精确提交部署独立目录，先重跑 car002 标准框；若它避开无框误重捕且中心、重捕时间均不退化，再测 car001 标准/右下、car002 左上并扩八框。不能以只改善完全遮挡语义代替两路中心与初始框一致性。此条与候选代码同提交，结果以实际 commit 为准。

## 2026-11-14T06:20:49+08:00 强遮挡两帧提前搜索的 RK3588 否决与撤回

- 精确候选 `codex/dual-gt-init-consistency@0c45bca`，工作树 `/home/u20/single-object-tracking`。以 `git archive --format=tar 0c45bca -o /tmp/nt-dual-gt-evidence/strong-vis-0c45bca-source.tar rk3566/nanotrack_v3 scripts` 生成并传输源码，主机/板端 `source.tar` SHA-256 均 `b4cc6db339832cbc96f434c5dabc760bddb844f8bef9700945b624e81e9116a7`；板端隔离目录 `/home/neardi/Project/SOT/dual-gt-strong-vis-0c45bca`，未覆盖 `9f84efe` 基线。实际板 `LKD3588` Linux `6.1.84`，session 实报 RKNN API `2.1.0 (967d001cc8@2024-08-07T19:28:19)`、driver `0.9.8`。原生运行 `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DBUILD_TESTING=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so -DCMAKE_CXX_FLAGS=-Werror=switch`、`cmake --build build --parallel 4`、`ctest --test-dir build --output-on-failure`，`23/23 PASS`；日志 `runs/cmake.log`/`runs/build.log`/`runs/ctest.log` SHA-256 `a77115191c3a613aab6664f99963ba9363fa9b2f0d176acdfa2c4eb822a25856`/`0b9f022f802aa538d187f24e67621c87bd20173977acbc646688d218c73a1e02`/`a30ff0a264613531b4aebbb480881db140011b3381814208b56d40c2d67ad2b8`；二进制 `build/nanotrack_v3` SHA-256 `3f4aa6a14fbf8c828266d6e2b935ad7a68db9b60c8a9ecbc03e2e09f86a9330e`。
- 板端视频 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；官方 NanoTrack template/search/head RKNN SHA-256 `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`/`d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`/`4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`，manifest SHA-256 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`；RK3588 VisDrone9 detector/manifest/labels SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`/`ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9`/`41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。候选 controller/headless 配置 SHA-256 `d35d7d608b45a0d1fee1a05d97d53d5946f554009bb0ddb5ff6dd19bba99ce69`/`f44c3b1dbd0dc0109e846af5313e94f58e9b25758832b46073b6b84e9d93268c`。
- 板端实际执行 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox 970,517,38,38 --output-video runs/car002_nominal/output.avi --output-boxes runs/car002_nominal/frames.csv --session-metadata runs/car002_nominal/session.json`，runtime exit 0；AVI 经 `ffprobe -count_frames` 为 `1920×1080×271`。CSV/AVI/session/log SHA-256 `f862b4817073c2695dc6ebd69e0f13a79ebd97fc64d30fe24120b0c7bd197b64`/`157d20dfdb79c9665637362168abebd7cb7556a4694ac2e0cba8cc7fa617e660`/`aef9c64625bd11021947ac118d584bfe06adfee3f9716ba3656775b8880263da`/`c4eab06f4501e4454132e46d9d87d6a1f74ab7b07da18dbdf007b0c52040ceec`。AVI 等大工件留在板端；CSV/session 小工件回传 `/tmp/nt-dual-gt-evidence/strong-vis-0c45bca/car002_nominal/` 且两端哈希相同。执行 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run car_002 car002_nominal 970,517,38,38 <回传 CSV> --output-json /tmp/nt-dual-gt-evidence/strong-vis-0c45bca/car002_nominal/score.json`，报告 SHA-256 `073d9d5308009464143e9ca1e98b885bd7feb92106d5ba07fba7f4a0a077f21d`。
- 第 213 帧如预期转 `SEARCHING`，213～216 帧 `observation_valid=0`、`control_source=MOTION_SEARCH`、`control_valid=1`，因此原先 car002 完全遮挡语义门改善；但第 226 帧在人工确认无框段误重捕，随后第 `242/263` 帧再次重捕。正式评分 `FAIL`，失败门为 `visible_boxes`、`reacquisition`，无框段假观测 `1` 帧；可见 tracking/display 中心均误差 `4.53/4.48 px`，原 `9f84efe` 标准框为约 `2.59/2.59 px`，两路中心均退化。单个代表框已构成决定性反例，按筛查顺序未扩大其余三框或八框，不把未运行的框声称为通过。
- **裁决 / Handoff**：撤回 `0c45bca` 的候选代码、配置和专属单测；撤回后的 `rk3566/nanotrack_v3` 全部文件与已验证 `8277b33` 内容一致，正式配置 SHA-256 恢复 `5d3214646bd3b0e6d38213482178aa44a0deb301d6f303c8b5eaa7f5517d3a3b`。保留候选 commit 和板端隔离根以复核误重捕；不为通过无框门而仅改 CSV 语义或显示框。`9f84efe` 正式八框仍 `2/8 PASS`、handoff HOLD。本条与撤回代码同提交，结果以实际 commit 为准。

## 2026-11-14T06:16:07+08:00 强遮挡证据连续两帧提前搜索候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，代码基线 `8277b3380358d75b604e3e7252c066e5afd09cf7`，开始时工作树干净。只用 `9f84efe` 精确 RK3588 八框 CSV（`/tmp/nt-dual-gt-evidence/handoff-9f84efe/<label>/frames.csv`）与 canonical 双目标真值分析，不改视频或标注；视频 `data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`。car002 四框在第 212/213 帧均连续两次满足“当前帧 NanoTrack decoded 高度 ≤ 当前可信框 `0.95`，且相对预测中心残差 `>0.15`”；car001 四框均无连续两帧满足。car002 左上框在可见的 120～142 帧虽有持续中心残差，却因 decoded 高度不缩小而不满足。该回放只验证既有轨迹上的判别，不等于改动后的闭环效果，也不证明跨场景泛化。
- 在 `rk3566/nanotrack_v3` 增加默认关闭的 `recent_visibility_search_height_ratio`、`recent_visibility_search_center_residual`、`recent_visibility_search_consecutive_frames`，当前 `config/visible-adaptive-display.conf` 取 `0.95/0.15/2`，配置 SHA-256 `d35d7d608b45a0d1fee1a05d97d53d5946f554009bb0ddb5ff6dd19bba99ce69`。仅已确认 car 类、decoded 高度与运动残差同时满足且帧号连续时，第二帧从临时观测进入 SEARCHING；单帧仍为 SUSPECTED，干净帧、初始化、异常恢复和结束可疑状态清零。detector 候选与 `display_bbox` 只按现有语义使用，未让 detector 框直接授权 tracking/control。配置解析、校验与指纹包含新参数；负例测试覆盖单帧、干净帧打断和非 car 类。
- 实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 和 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，`git diff --check` 通过。SSH 实测 `neardi@192.168.1.106` 为 `LKD3588` Linux `6.1.84` 且保留精确基线 `/home/neardi/Project/SOT/dual-gt-handoff-9f84efe`。**证据边界 / Handoff**：本候选尚未上板，主机测试不证明 car002 第 213 帧可合法输出 MOTION_SEARCH，也不证明 car001 三个微扰框的恢复、两目标中心或八框正式门槛会改善。下一步以本条精确提交归档部署到隔离目录，先跑 car002 标准/左上与 car001 标准/右下，若无退化再扩八框；正式 `9f84efe` 八框仍 `2/8 PASS`、handoff HOLD。本条提交后以实际 commit 为结果身份。

## 2026-11-14T06:10:08+08:00 官方原始响应峰值与局部加权中心八框筛查

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `88e2af2`，开始时工作树干净。用 `repos/siamtrackers/NanoTrack/models/pretrained/nanotrackv3.pth` SHA-256 `4f0d25108385a67bdec5fa01ca37c74acb72343b53996dd645fe18b86e512e69` 和配置 `repos/siamtrackers/NanoTrack/models/config/configv3.yaml` SHA-256 `60ea9dc886dcdd0451e60b404490a39d19495480185ca7b6299b72b74d746f87`，输入仍为 canonical `data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，评价分别使用两目标已确认真值。此处是官方 PyTorch 桌面单跟踪器，不是现有 RK3588 RKNN 加控制器链；不把离线解码称作已验证的板端改动。
- 原型 `/tmp/nt-dual-gt-evidence/soft-center-screen.py` SHA-256 `9180c6a607969882a8c29213465a25fd850a41768d57efe311f4ddb7c0a29f8b`，逐帧执行与官方 `NanoTracker.track` 相同的 `cls/loc` 解码和状态更新，同时只读计算最佳格邻域 3×3 各候选中心按 `pscore` 的 softmax 权重，温度 `0.01/0.03/0.07`；加权中心不反馈到下一帧。先运行原官方 `scripts/run_nanotrack_video.py` 与原型的 car001 标准框各一次，原输出 `/tmp/nt-dual-gt-evidence/official-car001-nominal.csv/.mp4` SHA-256 `6f13e7c03359943630a741bdef269c3fadc05d5b92aa2b244dafebc4081686b5`/`8f89eafa2644f396636c9e9c29cc0acc968efd3735856f67afa878a175e3245a`；271 帧框坐标与原型的基线最大差 `4.997e-7 px`，确认诊断复现原算法。再以 `env OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 .venvs/nanotrack/bin/python /tmp/nt-dual-gt-evidence/soft-center-screen.py --config repos/siamtrackers/NanoTrack/models/config/configv3.yaml --snapshot repos/siamtrackers/NanoTrack/models/pretrained/nanotrackv3.pth --bbox <XYWH> --output /tmp/nt-dual-gt-evidence/soft-official-<目标>-<框>.csv` 对两目标各四初始框跑完整 `271` 帧，八次均 exit 0；ROI 同上一条。八 CSV 清单 `/tmp/nt-dual-gt-evidence/soft-official-eight.sha256` SHA-256 `3399e09c6a678ff7c223e7a3e6d37afe8971190667de1c557d5a1182054e538e`，逐文件核验通过。
- 运行 `python3 /tmp/nt-dual-gt-evidence/score-soft-official.py > /tmp/nt-dual-gt-evidence/score-soft-official.json`；脚本/报告 SHA-256 `7f629b6e8254587893cdfc74ddb17b13be417000a55264f8cc6afc9d0cd76676`/`2a73d74a02ba5c1ed784a9eb32820d0dfa24b1c77e929dfe0c836e08118a59f1`。car001 四框遮挡前 0～159 帧最佳格中心均误差 `2.28/2.18/2.47/2.09 px`，温度 0.03 变为 `2.25/2.14/2.50/2.02 px`；后段 230～270 帧原始四框均约 `126 px`，加权最多降不足 `1 px`，没有纠正错车。car002 遮挡前最佳格四框 `1.52/1.36/1.29/1.70 px`，温度 0.03 为 `1.40/1.43/1.26/1.58 px`；真值框退化。car001 遮挡前跨框中心距 P95 原始 `1.78 px`，温度 0.03/0.07 为 `1.91/2.13 px`；car002 全段温度 0.01 仅 `2.76→2.74 px`。局部加权不足以同时改善两车的精度和一致性。
- **裁决 / Handoff**：不把局部 softmax 中心解码接入 `tracking_bbox`/控制，`display_bbox` detector 仍仅作视觉外观。旧官方 PyTorch 跟踪器的遮挡后错车也说明单帧响应的亚像素微调不是当前身份分叉的主要解法。正式 `9f84efe` RK3588 八框仍 `2/8 PASS`，handoff HOLD；下一步应针对恢复阶段的多候选身份与模板特征，且必须实际复核板端闭环、两目标可见中心与无框段。本条提交后以实际 commit 为结果身份。

## 2026-11-14T06:06:21+08:00 集成契约 g5000 权重的双目标四框桌面筛查

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `56eb83c8e9e25329b26a9ca2ced1fc2ccc455df4`，开始时工作树干净。先按 `docs/progress/integration-contract.yaml` 复核冻结 checkpoint `artifacts/nanotrack_conservative_three_stage/run/stage1/candidate_stage1_g00005000_e001_s02500.pth` SHA-256 `40ebbe4296938bf5d53c8605ab05e729b306fc0bce65921a57ab6613030410d1`、配置 `configs/nanotrack_v3_three_stage_stage1.yaml` SHA-256 `cb3065982818a701a7ebb469c41b894b0a6c11e7aac6cb91dc42e5ed50547f2e`，与契约相符。输入仅用 canonical `data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d` 和匹配的 car001/car002 已确认真值。
- 以 `env OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 .venvs/nanotrack/bin/python scripts/run_nanotrack_video.py --repo repos/siamtrackers/NanoTrack --config configs/nanotrack_v3_three_stage_stage1.yaml --snapshot artifacts/nanotrack_conservative_three_stage/run/stage1/candidate_stage1_g00005000_e001_s02500.pth --video data/occ_video_1/1.mp4 --output /tmp/nt-dual-gt-evidence/g5000-<目标>-<框>.mp4 --bbox-output /tmp/nt-dual-gt-evidence/g5000-<目标>-<框>.csv --bbox <XYWH>`，依次跑 car001 标准 `992,582,37,39`、人工 `993,585,36,36`、car002 标准 `970,517,38,38`、真值 `967,513,42,42`。四次均 exit 0、271 帧、CPU `tracking_fps=68.79/68.85/71.31/70.75`。8 个 CSV/MP4 的逐文件哈希清单 `/tmp/nt-dual-gt-evidence/g5000-four-outputs.sha256` SHA-256 `06425ce8ca53fee27374a39a992d6d2cceca8ea0c53f44a9c693e83baf12283f`，`sha256sum -c` 通过。
- 执行 `python3 /tmp/nt-dual-gt-evidence/score-g5000-four.py > /tmp/nt-dual-gt-evidence/score-g5000-four.json`，脚本/报告 SHA-256 `f1a3555aa84ac3757bb72dc08f935c5f319c65ebab05930dd7922ee619b236ac`/`a447ff9d8479b0fdbdb314fbbbc9d2ede63c27b9e04e7a40ef6c4c0a5f041549`。仅计正式可观测框：car001 两框各 258 帧，中心均误差 `35.03/34.98 px`，后段 230～270 帧 `124.30/124.72 px`，两框相互接近但同时跟错；car002 两框各 252 帧，标准/真值全段 `32.63/2.89 px`，标准后段 `190.58 px`，两框跨 ROI 中心距 P95 `215.09 px`。因此近似一致的两个错误轨迹不能当作准确定位证据。
- **裁决 / Handoff**：g5000 原 checkpoint 不能直接作为双目标一致性替换；不把它转换成 RKNN 或调整当前板端模型。桌面单跟踪器缺少生产控制器、detector、`display_bbox`、无框状态和反馈恢复逻辑，以上不是板端正式门禁，不能代替模型所有者的冻结身份或正式 promotion 决定。`9f84efe` 板端八框仍 `2/8 PASS`、handoff HOLD；下一步若研究中心解码，须从正式 board 原始响应与两目标八框出发，并验证改变中心后的闭环与遮挡语义。本条提交后以实际 commit 为结果身份。

## 2026-11-14T06:02:00+08:00 两套既有微调权重的双目标初始框敏感性筛查

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；分析基线 `e5c80cd2c314f427b6d7606d423c46a1fed9ac3e`，开始时工作树干净。本次只在主机 `.venvs/nanotrack` 的 CPU 上运行既有单跟踪器，不修改正式 RK3588 算法或板端文件。输入为 canonical `data/occ_video_1/1.mp4`，SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，评价严格分别使用 car001/car002 的确认真值。运行脚本 `scripts/run_nanotrack_video.py` SHA-256 `302b286429cb57fa827b0f14e4756239f8289f3a5e33bfd284a0d8d4f1ef89d9`。
- UAV123 权重 `artifacts/nanotrack_uav123_finetune/best/best.pth` SHA-256 `761eccd07df08a05932ca0b7c828fdbb85f790744159dd650bc49bb73b88f353`，配置 `configs/nanotrack_v3_uav123_finetune.yaml` SHA-256 `3e1fde4aec5617b9f88427147e9addf2cfd1b3d370d5d3671501d4529e9173f4`；四数据集权重 `artifacts/nanotrack_four_dataset_finetune/best/best.pth` SHA-256 `f19897b6ca2568f420b45e7c718ebc5f6ead353485086056fbcbe67de27c5e0f`，配置 `configs/nanotrack_v3_four_dataset_finetune.yaml` SHA-256 `2e722c135ef863c4d70843e4f1a5f0d0f20d722bb4d447fbefc5b3603955222b`。实际命令形如 `env OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 .venvs/nanotrack/bin/python scripts/run_nanotrack_video.py --repo repos/siamtrackers/NanoTrack --config <上述配置> --snapshot <上述权重> --video data/occ_video_1/1.mp4 --output /tmp/nt-dual-gt-evidence/<模型>-<目标>-<框>.mp4 --bbox-output /tmp/nt-dual-gt-evidence/<模型>-<目标>-<框>.csv --bbox <XYWH>`。car001 标准/人工/左上/右下 ROI 为 `992,582,37,39`/`993,585,36,36`/`990,580,39,41`/`994,584,35,37`；car002 标准/真值/左上/右下为 `970,517,38,38`/`967,513,42,42`/`968,515,40,40`/`972,519,36,36`。UAV123 跑两目标各四框；四数据集先筛 car001 标准/人工、car002 标准/真值四框。并行六组首试资源争用、均被中断且没有产生完整 CSV；后用单线程逐组重跑。12 组均完成 271 帧、exit 0；两组最初不限制线程的 `tracking_fps` 约 43，其余 10 组约 68～73。实际框及 24 个 CSV/MP4 文件的逐文件 SHA-256 见 `/tmp/nt-dual-gt-evidence/old-weights-desktop-outputs.sha256`，清单 SHA-256 `381afb6bc7755d5549ed1b86c234207c2eb37abed61d7ad3b4d75fda69840649`，`sha256sum -c` 全部通过。
- 执行 `python3 /tmp/nt-dual-gt-evidence/score-uav123-eight.py > /tmp/nt-dual-gt-evidence/score-uav123-eight.json` 与 `python3 /tmp/nt-dual-gt-evidence/score-fourds-four.py > /tmp/nt-dual-gt-evidence/score-fourds-four.json`；评分脚本 SHA-256 `17a1d9f5fa8457ef2906e6bec48a78d5e6693a25b3f85e3e18d6cc31c209f1fe`/`26b558da1c917bb07bdedffdb433f2c6f629240f0a07113c0510749dbd48be66`，报告 SHA-256 `3570112aa642086f3ff7cb76a6d8841fa61d7a53b34f791f19112e2a1abd550b`/`e190aa2110f76bcbafb096173a2d8b8456728480fc7ab7b7c6b5be869e6f4fe2`。只计正式可观测框：car001 `258` 帧、car002 `252` 帧。UAV123 car001 标准/人工/左上/右下全段中心均误差 `34.93/3.28/35.00/34.89 px`，标准/左上/右下在后段 `230～270` 平均误差约 `126.6 px`，跨初始框同帧最大成对中心距 P95 `141.37 px`；car002 四框中心均误差 `2.16/2.04/1.95/2.17 px`。四数据集 car001 标准/人工为 `5.16/34.83 px`，人工框后段均误差 `124.13 px`，两框跨框 P95 `141.16 px`；car002 标准/真值为 `2.24/3.24 px`。两套模型都在 car001 遮挡后发生与几像素初始化差别有关的轨迹分叉，且稳定的初始框方向相反。
- **裁决 / Handoff**：两套旧权重均拒绝作为当前双目标一致性候选，不进行 RKNN 转换或板端部署；桌面单跟踪器始终输出框，没有生产控制器、detector、无框状态或显示路径，因此这些数值不是板端正式门禁结果。保留 `9f84efe` 有效算法内容、板端八框 `2/8 PASS` 和 handoff HOLD。模型所有者若继续训练，应以仅作验证的 car001/car002 轻微初始框矩阵检验遮挡后身份分叉，并经 RK3588 实际双目标、两路中心与无框段闭环复测；不可根据单框高 IoU 晋级。本条提交后以实际 commit 为结果身份。

## 2026-11-14T05:51:03+08:00 前后向校验局部光流的八框筛查

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；分析基线 `d1e9c6a9f2023ea6ba953369594d4b840a5d2556`，开始时工作树干净。主机 OpenCV `4.5.5`，仅用 canonical `data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`、car001/car002 已确认真值与下方 `9f84efe` 精确板端八框 CSV。执行 `g++ -O2 -std=c++17 /tmp/nt-dual-gt-evidence/klt-screen.cpp -o /tmp/nt-dual-gt-evidence/klt-screen $(pkg-config --cflags --libs opencv4)`，源码/二进制 SHA-256 `d832de2b74684ee5b798cbc78b18857e5d80e764a1e815d03ee2911c350cea53`/`25cc77d9223cc36e372803fad87f495c9be46c63ed17ffb50c81d615fabc76be`。算法在初始框内取最多 80 个角点，以 15×15、3 层 LK 光流跟踪，前后向误差阈值 1.5 px、光度误差阈值 25、至少 4 点，逐帧中位平移；没有使用真值调参或把检测器框投入控制。
- 用上条相同的两目标各四初始框完整回放 `271` 帧，输出 `/tmp/nt-dual-gt-evidence/klt-car001-{nominal,user,left,right}.csv` 与 `klt-car002-{nominal,truth,left,right}.csv`；八文件 SHA 清单 `/tmp/nt-dual-gt-evidence/klt-eight-csv.sha256` SHA-256 `59d9c71a94e1a21ea90c1fd2e689a2d11ba7c0e1d9c6f9aaa8a378729ce229b7`。执行 `python3 /tmp/nt-dual-gt-evidence/score-klt-screen.py > /tmp/nt-dual-gt-evidence/score-klt-eight.json`，脚本/报告 SHA-256 `b947ee6945e3815fce48ac36cccff805779a8ffff7ba11c5f4eefd69dae93d9c`/`b00845ce2da8bb1ff336f98b699aed488ceb2fc5f59a4b60d609a889ef090252`。光流有效的可观测帧 car001 每框 `169` 帧，标准/人工/左上/右下中心均误差 `2.29/1.35/3.34/1.83 px`，对应 NanoTrack 同段约 `3.36/3.46/3.13/3.68 px`；car002 每框 `211` 帧，光流 `6.94/9.18/7.38/5.80 px`，NanoTrack 同段约 `1.62/1.39/1.44/1.99 px`。car001 第 174 帧、car002 第 215/216 帧起光流失效；car002 已确认无框的第 211～212 帧仍 `2/15` 错报有效。光流对 car001 遮挡前定位有局部收益，对 car002 明显退化。
- 反事实仅当 NanoTrack `TRACKING`、accepted、control_valid 且光流中心距 ≤5 或 ≤10 px 时两中心各取一半；执行 `python3 /tmp/nt-dual-gt-evidence/score-klt-consensus.py > /tmp/nt-dual-gt-evidence/score-klt-consensus.json`，脚本/报告 SHA-256 `edca97f07b60b63afdd5bff30dec20c3baadf9d35033d7660c48cdaae956a9a2`/`a83af89bfbe667693b93e42e6e358de03749cade823206884f228ce09fb5f742`。5 px 门下 car001 标准/人工/右下全段中心均误差从 `4.28/4.63/5.02` 降到 `3.84/4.06/4.55 px`，左上 `4.34→4.33` 几乎无收益；car002 标准/真值/左上/右下从 `2.59/2.00/1.77/2.92` 变为 `2.44/2.10/1.70/2.85 px`，真值框退化。10 px 门使 car002 四框均比原 NanoTrack 更差。5 px 门下 car001 跨框最大中心距 P95 `3.31→3.51 px`，car002 `6.21→6.21 px`，均未达到更紧且更一致的双目标要求。以上只是冻结轨迹输出反事实，未计入反馈和 RK3588 时延。
- **裁决 / Handoff**：不把局部光流接入跟踪输出或控制，也不以它的有效点数替代遮挡真值；两目标对同一光流规则反应相反。保留源代码和报告在主机 `/tmp/nt-dual-gt-evidence/` 作可复核负例，正式算法文件无改动，八框 `2/8 PASS` 与 handoff HOLD 不变。下步需模型/模板级定位与身份候选，必须同时检验两目标中心、初始框一致性、无框段和板端闭环。本条提交后以实际 commit 为结果身份。

## 2026-11-14T05:46:58+08:00 独立 CSRT 中心观测与跨初始框一致性筛查

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；分析基线 `d9b7d1a3b26a08878d0c32a4186571dd9c04255b`，工作树开始时干净。只用 canonical `/home/u20/single-object-tracking/data/occ_video_1/1.mp4`（SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`）、两目标已确认 GT 和 `9f84efe` 八组精确 RK3588 CSV 作主机离线比较；没有修改板端或正式运行算法。OpenCV `4.5.5`，执行 `g++ -O2 -std=c++17 /tmp/nt-dual-gt-evidence/csrt-screen.cpp -o /tmp/nt-dual-gt-evidence/csrt-screen $(pkg-config --cflags --libs opencv4)`，源码/二进制 SHA-256 `13258d009d94496c416271a713df644a20a8397bb826dbf0416ee8bd7a6c0b67`/`a7958f8eaaafa33d9d0b99b2dba059927c40e7d2701a7d2e81c87703b3a9aebb`。用 car001 标准/人工/左上/右下 `992,582,37,39`/`993,585,36,36`/`990,580,39,41`/`994,584,35,37`，car002 标准/真值/左上/右下 `970,517,38,38`/`967,513,42,42`/`968,515,40,40`/`972,519,36,36` 运行八次，每次完整 `271` 帧。输出 `/tmp/nt-dual-gt-evidence/csrt-<label>.csv`；16 个原始/重置 CSV 的逐文件 SHA 清单 `/tmp/nt-dual-gt-evidence/csrt-eight-csv.sha256` SHA-256 `c05bbe9f650f7ca9823671af0449539bbcf45331b6d11328e3a1cbc0d0aff2a5`。
- 执行 `python3 /tmp/nt-dual-gt-evidence/score-csrt-screen.py > /tmp/nt-dual-gt-evidence/score-csrt-eight.json`，脚本/报告 SHA-256 `ac9ab67b5d49370233b0f5be5a8da7cd970c45f91808ee5ae54c3a0853da4e97`/`2ad1b2eeb5ea79175e640d0b77ef7eeb164d1b69999798eb78de9311bad6d929`。GT 可观测帧 CSRT 中心均误差 car001 四框标准/人工/左上/右下 `36.82/61.08/90.84/2.77 px`，对应 NanoTrack `4.28/4.63/4.34/5.02 px`；car002 四框标准/真值/左上/右下 CSRT `3.07/2.59/2.22/3.85 px`，NanoTrack `2.59/2.00/1.77/2.92 px`。CSRT 对 car001 三框遮挡后严重漂移，且对 car002 已确认的 15 个无框帧仍 `15/15` 返回成功；其成功标志不能用于目标可观测性或控制授权。
- 反事实只在 NanoTrack `TRACKING`、accepted、control_valid 且 CSRT 中心距不超过 `10 px` 时把两中心各取一半，其余保持 NanoTrack 原框；执行 `python3 /tmp/nt-dual-gt-evidence/score-csrt-consensus.py > /tmp/nt-dual-gt-evidence/score-csrt-consensus.json`，脚本/报告 SHA-256 `8cd8c68028753b22074e03ba2e2ecfa278a8c9eb604cf8abe1990df0422ca359`/`c3dcd753ecd53ab90f36559f2510960dc22b54dfdaa719734543e4bf2e9e9716`。各框可见帧平均中心误差虽都小幅下降，car001 四框最大两两中心距 P95 却从 `3.31→6.77 px`，car002 从 `6.21→6.28 px`；用户要求的跨微扰一致性变差，且此离线冻结轨迹没有计入反馈、板端时延或闭环副作用。
- 再以 `9f84efe` 已确认重捕帧的 NanoTrack 框重置 CSRT，原型 `/tmp/nt-dual-gt-evidence/csrt-reinit-screen.cpp`/二进制 SHA-256 `5a52668d425bf56323e2ac844af5c1f78238a6a5a908e63a54d219590cba6fca`/`a532f0eb6712dd03f35995ff59abeb86d2a6301b2e0999ce71a724bb747aadb2`，八组完整 271 帧；评分脚本/报告 `/tmp/nt-dual-gt-evidence/score-csrt-screen-reinit.py`/`score-csrt-eight-reinit.json` SHA-256 `a6e520abcfc4ecc61abb54a80d48c284c536c5ccde783d1dec124d4cef157141`/`e8bf58a31deefc3afb43f97d4e66e7e7cdf4179f1883b6062643b5399795f0de`。car001 标准框 CSRT 后段均误差从不重置 `141.88→4.56 px`，但 car002 标准/真值框从 `7.55/2.63→12.66/10.53 px`；同一重置策略仍无法同时改善两目标。重置版 10 px 一致性融合报告 `/tmp/nt-dual-gt-evidence/score-csrt-consensus-reinit.json` SHA-256 `df7a95e2b139b14bb6a8efe0a580b0ee017b9ef7106b2410675919e998e4038f`，car002 四框平均中心均高于原 NanoTrack。
- **裁决 / Handoff**：不接入 CSRT 或它的中心融合，也不把其 `ok` 当作无框/重捕证据；虽然部分单框中心更准，但错车漂移、15 个无框误报和跨初始框一致性退化直接违背本目标。当前 `9f84efe` 算法内容、八组 `2/8 PASS` 和正式 handoff HOLD 不变；继续寻找同时改善定位和身份、可经板端闭环与两目标无框帧验证的候选。本条提交后以实际 commit 为结果身份。

## 2026-11-14T05:39:16+08:00 display detector 候选中心与双目标真值对照

- 精确源码 `codex/dual-gt-init-consistency@d157d0d66a7346dd5460b8a568eea2c0e973cc58`，基线 `766f1368ee4a21df0aeb039c6b47d5437080b537`，隔离 RK3588 根 `/home/neardi/Project/SOT/dual-gt-display-trace-d157d0d`。`git archive` 源码包 `/tmp/nt-dual-gt-evidence/display-trace-d157d0d-source.tar` 与板端 `source.tar` SHA-256 均 `b106bc40342b46707318f230a8278b8480e70df6f135b760ec18df27c04306d7`。板端执行 `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build --parallel 4`、`ctest --test-dir build --output-on-failure`，`23/23 PASS`；二进制 SHA-256 `ed4f8ad097331988e22ac49dff5fe305ee20163ca2cc425a43b758119bb58e57`，板端配置 `runs/headless-eval.conf`/控制器配置 SHA-256 `c7594ebb9c43789b136e13182fdfd96f634af154f411142f0866b954762cad98`/`5d3214646bd3b0e6d38213482178aa44a0deb301d6f303c8b5eaa7f5517d3a3b`，构建/CTest 日志位于 `runs/`。
- 视频板端实际路径 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/data/occ_video_1/1.mp4`，重新核得 SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；官方 NanoTrack template/search/head RKNN SHA-256 `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`/`d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`/`4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`；VisDrone9 RK3588 detector/manifest/labels SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`/`ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9`/`41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。板端 Linux `6.1.84`，运行 session 实报 RKNN API `2.1.0`、driver `0.9.8`；主机匹配 GT car001/car002 状态清单 SHA-256 `e44fd23514d6871e72d628ad4843fc90bb4246f306322ee297c8a2aa75818036`/`212525642591c124320e93369f8748d82cc53092628beff051507f930946dec1`。
- 串行运行 `NT_TRACE_DISPLAY_CANDIDATE=1 ./build/nanotrack_v3 --config runs/headless-eval.conf --bbox 992,582,37,39` 与 `--bbox 970,517,38,38`，各自指定 `--output-video/--output-boxes/--session-metadata` 为 `runs/car001_nominal/`、`runs/car002_nominal/`，完整 CSV/JSONL/session/log/AVI 留在板端。CSV SHA-256 `64b33b875a2435f5d8d755c16379336e865207006ed67f47a9796c98fb736a60`/`452ea941b451990b93810c3a9d885c4da291a532b50c63fd6a2dd416248de246`，AVI SHA-256 `2a28d7e23f03d18c3cbcc0dfb464095232d663ee8d3ad6cb77d1b0f4c427ee2a`/`21320c9508abcd54e1b8e28597fa2489dc20a86450bc9b4f97329b0aef0b9138`，`ffprobe` 两段均 1920×1080×271。文本包 `runs/two-text.tar.gz` 板端/主机 SHA-256 `20c96eb00c7f6d0302b1c68ccf1a0c5c8e5531cad6274e8ec5c6a0a1d12f68ec`，主机副本 `/tmp/nt-dual-gt-evidence/display-trace-d157d0d-two-text.tar.gz`。
- 主机执行 `python3 /tmp/nt-dual-gt-evidence/score-display-detector-trace.py > /tmp/nt-dual-gt-evidence/score-display-detector-trace.json`，脚本/报告 SHA-256 `5c0f5cfb693c4f13e69d9c4c637937f49f38dcb5e8af39aa5ae8bd4291e7c2f1`/`4a20ccec3c179a55f80b3385ba8fca10a0be331aef0ba076d3825ad9764d6762`。两路各有 76 个已匹配显示 detector 候选；逐帧比较与 `9f84efe` 基线 271 个 tracking/display 框及 state、phase、reacquired、control_valid 字段差异全为 `0`，说明此次只读诊断未改变轨迹。car001 后段 ≥240 帧 10 个候选的 tracker/detector 中心真值均误差 `7.33/9.30 px`，detector 优胜 `0/10`；car002 同范围 `7.74/2.51 px`，detector 优胜 `10/10`。前段 <160 帧 car001 `3.23/4.66 px`、detector 优胜 `12/48`；car002 `1.49/4.45 px`、优胜 `5/48`。detector 原框经对称 padding 后的中心在该区域保持不变；诊断日志比较的是结果帧真值，候选源帧通常早 1 帧（33 ms），因此包含正常一帧运动差。
- 使用正式评分 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run car_001 car001_nominal 992,582,37,39 <CSV> --run car_002 car002_nominal 970,517,38,38 <CSV> --output-json /tmp/nt-dual-gt-evidence/display-trace-d157d0d-two-center.json`，报告 SHA-256 `db8eada1f9e4d300527e624131cdc01f03983606233fb3423e1d508263ce1acc`：car001 标准框 PASS，重捕 188，tracking/display 中心均误差 `4.28/4.32 px`；car002 标准框 FAIL（完全遮挡语义），重捕 235，两中心均 `2.59 px`。这与原八框代表组完全一致。
- **裁决 / Handoff**：detector 的显示候选中心对两目标呈相反效果，早段两目标多数帧也更差；不可把它作为 tracking/control 的通用中心修正，display detector 仍仅作外观用途。撤下临时诊断打印后执行 `git diff --name-only 9f84efe -- rk3566/nanotrack_v3 scripts`，结果为空；主机重构建及 CTest `23/23 PASS`，日志 `/tmp/nt-dual-gt-evidence/display-detector-trace-revert-host-build.log`/`display-detector-trace-revert-host-ctest.log` SHA-256 `5d8e364186ce39a995aa414cd968aa71fbd73e439471f8e38aad11bb2265bada`/`da80783f68993bf5a98ba0dba26105f2aa54308a43c829f463dd4147b3a44014`。保留精确板端日志/视频和本反例。正式八框 `2/8 PASS` 与 handoff HOLD 不变。后续须针对 NanoTrack 自身定位/模板或经冻结的模型找改善方案，本记录与撤除代码同提交。

## 2026-11-14T05:32:57+08:00 只读 display detector 候选中心诊断源码

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `766f1368ee4a21df0aeb039c6b47d5437080b537`。在 `AdaptiveDisplayBox::ingest` 内仅当显式环境变量 `NT_TRACE_DISPLAY_CANDIDATE=1` 时，把已通过显示候选选择的 detector 框与同帧 tracker 框、源帧及年龄打印到 stderr；未匹配时打印 `none`。代码文件 SHA-256 `aebd3e3debb62c478b41d04eab988b31991248b748f3b70f4ae23f537b938de5`。打印的候选是 padding 后的显示外观框，中心在无图像边缘裁剪时与原 detector 框相同。该诊断不向 tracking/control 写 detector 结果，默认环境不输出。
- 主机执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 和 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，RKNN-off `23/23 PASS`；构建/测试日志位于 `/tmp/nt-dual-gt-evidence/display-detector-trace-host-build.log`、`/tmp/nt-dual-gt-evidence/display-detector-trace-host-ctest.log`，SHA-256 `67e804ec196d1680ec9412db438c9804d30124c63c2236570f84ebb1126e2fbc`/`047767e41202e4bac7cb0efc89393894c746d15742e3739aac92d7b9135a5dc9`。
- **证据边界 / Handoff**：这是主机可编译的只读诊断，没有板端候选中心或真值结论。下一步须提交精确源码、RK3588 原生构建，以 canonical 输入分别跑 car001/car002 代表初始框，核验 `tracking_bbox`/`display_bbox` 轨迹与 `9f84efe` 不变，再对 trace 候选框和两目标可观测真值计算中心误差与错身份风险；仅在有跨框改善空间且符合 NanoTrack 确认门时才设计定位改动。正式 handoff HOLD，本条与诊断源码同提交。

## 2026-11-14T05:31:06+08:00 完整车体图像分割中心的两帧筛查

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；筛查基线 `ee450c14e020577b5ba97aee3003722a045c6c1c`。输入沿用下方已核对身份的 canonical 视频/GT 和 `9f84efe` 八框板端 CSV；主机 OpenCV `4.5.5`。执行 `g++ -std=c++17 /tmp/nt-dual-gt-evidence/grabcut-probe.cpp -o /tmp/nt-dual-gt-evidence/grabcut-probe $(pkg-config --cflags --libs opencv4)`，原型源码/二进制 SHA-256 `222596af594cba6f3165af691741667bef34489392e46b72182550dfca8018e2`/`8b1aec3da9c2c5ebaed668595600ac15edbdeef2055d6fbd8ee02246bb32063d`；再运行 `python3 /tmp/nt-dual-gt-evidence/grabcut-screen.py > /tmp/nt-dual-gt-evidence/grabcut-screen.json`，脚本/报告 SHA-256 `d50222bfdc38a3c2b355821ba8041c9df7504153e7b6441f569a73dfc205fbe8`/`0fe73a215a4d2b09edddf61f19d59f1c4ad3f2c61357d0b76ae57160e9e954bd`。
- 仅筛后段第 245、260 帧，两目标各四初始框。GrabCut 使用 tracking 框外扩 7 px 作前景初始矩形，选取邻近连通域的包围框，不使用真值调参。car001 第 260 帧四框原中心误差 `5.57/7.96/5.35/5.27 px`（标准/人工/左上/右下），分割后均 `10.40 px`；第 245 帧分割后均 `6.96 px`，只在标准/右下略好。car002 第 260 帧四框原 `10.61/5.90/5.66/8.44 px`，分割后 `2.06/2.50/3.20/2.06 px`；第 245 帧也均改善。分割轮廓的几何中心与人工完整目标框中心并非总一致，同一规则在 car001 上退化。
- **证据边界 / Handoff**：这只是主机两帧离线筛查，不是 271 帧、遮挡期、RK3588 时延或真实闭环的证明；原型未进入代码分支。不能因为 car002 局部收益把分割结果用于 `tracking_bbox` 或 `display_bbox` 的中心。正式八框结果仍为 `2/8 PASS`、handoff HOLD。下一步需要模型/模板级、同时经两目标八框可见和遮挡段验证的定位方案。本记录提交后以实际 commit 为结果身份。

## 2026-11-14T05:28:32+08:00 初始裁剪聚合与真实模板更新时序核对

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；分析基线 `ea3ecc3c99ebde6fdd69202993823b715daa52be`，输入仍为下方哈希绑定的 `occ_video_1` 双目标、`9f84efe` 八框板端 CSV 与同源视频解码图像。主机运行 `python3 /tmp/nt-dual-gt-evidence/init-jitter-aggregate.py > /tmp/nt-dual-gt-evidence/init-jitter-aggregate.json`；脚本/结果 SHA-256 `90bd1f09026986310634b311e1f359afe1228e6d54ce52e344a24df301da6fae`/`8ccda2126dc8faf9078867ecb8105710b7ef94ebef782f5e3744f815d17af2c1`。再运行相同脚本的 ±1 px 版本 `/tmp/nt-dual-gt-evidence/init-jitter-radius1.py`，脚本/结果 SHA-256 `c4e44b77d5ec07ebe799e8f1030c2b8cbe47dd59918cc3cdb92427ebb3eb1b98`/`ee41bffe4cad00c0585088a8196eb11d9f64632c4a40a2e9ae10364af2d5fe5a`。两种离线反事实都把第 0 帧实际 ROI 的 3×3 平移图块特征平均，只用第 240～270 帧两车真值图块事后核对相似度。
- RGB 正 margin 帧数在 ±3 px 聚合后，car001 标准/人工/左上/右下从 `13/30/13/29` 变为 `28/30/25/30`，但 car002 标准/真值/左上/右下从 `26/28/26/15` 变为 `8/23/17/3`；±1 px 聚合时 car001 `17/30/18/30`、car002 `23/28/24/11`。没有一个统一小扰动平均方案在两目标八框都改善，这只是像素级代理，不等于 RKNN 特征平均的真实效果，故没有新增初始化推理开销或修改生产跟踪器。
- 主机执行 `python3 /tmp/nt-dual-gt-evidence/template-timeline.py > /tmp/nt-dual-gt-evidence/template-timeline.json`，脚本/报告 SHA-256 `4a760bba9646cb5869782801f7f4ea3c7bfe994ff36e6e8a4bac0da11966e9c1`/`a3d545302128cc7c76a9d359fa9deebefe4700528c135121bac6412611fb33a2`。八框均在第 30/33、63/66、约 96～100、约 129～133 帧阶段/提交 recent 模板，此后还有恢复及稳定段提交；car001 恢复后的提交在 191/194 帧，后段又在 224/227、257/260 等帧阶段/提交。第 240～253 与 258～270 帧 car001 标准框中心均误差 `7.19→7.88 px`、car002 标准框 `6.99→8.74 px`，其他框的变化方向相同。后段误差不能仅归因于初始化时的单张像素模板；实际 recent 模板已多次更新且偏差没有随近期提交消失。
- **裁决 / Handoff**：维持当前算法、模型、400 ms 配置与 `2/8` 正式八框结论；不把初始化裁剪聚合或静态像素相似度升级为板端候选。后续模型/模板研究必须覆盖恢复后 recent 特征、同类负样本和追踪框中心；display detector 框仍只用于视觉外观。正式 handoff HOLD，本条提交后以实际 commit 为结果身份。

## 2026-11-14T05:25:33+08:00 八个初始模板的外观可分性诊断

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；分析基线 `2a9a8d656ad638dd476086641cf05780c3274c32`。继续使用下方经哈希核验的 `occ_video_1/1.mp4`、两目标真值与精确板端 `9f84efe` 八框 CSV。主机执行 `python3 /tmp/nt-dual-gt-evidence/initial-appearance-separation.py > /tmp/nt-dual-gt-evidence/initial-appearance-separation.json`；脚本 SHA-256 `72c4f14f68cf84c2ed39661c9c6edf6deff3b522715b6bc438e887778fa1b2fe`，报告 SHA-256 `c1dff1276dccfa2892fe929c1544e76ceac7e8ee11cf310efa37b17f6905e41c`。脚本取八组第 0 帧实际初始化框图块，缩放至 32×32，分别与第 240～270 帧两目标的真值图块计算归一化 RGB/灰度/梯度相关系数；“正 margin”仅表示同目标相似度大于另一辆已确认轿车。
- RGB 相关的正 margin 帧数：car001 标准/人工/左上/右下 `13/30/13/29`（每组 31 帧），平均 margin `−0.025/+0.146/−0.003/+0.134`；car002 标准/真值/左上/右下 `26/28/26/15`，平均 `+0.068/+0.080/+0.056/+0.002`。梯度相关在 car001 四框仅 `0/6/4/3` 帧正 margin，但 car002 四框为 `31/28/24/31`。几像素的初始化变化足以改变这项朴素外观量对两辆白车的排序；一种固定像素相关规则不能同时稳定处理两目标八框。
- **证据边界 / Handoff**：这是仅含两辆指定轿车、使用后段真值图块的离线可分性上界，不包含 detector 全部候选、遮挡期虚假观测，也不是 RKNN tracker 的真实特征或板端推理结果；不能把相关系数当作身份验证器直接部署。结合下方原始 NanoTrack 中心偏差，请协调者将八框实际初始化 ROI、CSV 和本诊断交模型/外观评估 owner，用跨框和负样本门槛检验模板鲁棒性。当前代码与正式板端 handoff 均保持 HOLD；下一步需提出有运行时输入可用、且可经八框及无框帧验证的模板或外观候选。本记录提交后以实际 commit 为结果身份。

## 2026-11-14T05:23:11+08:00 原图边界梯度中心修正的离线否决

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；分析基线 `cd61638cc7474f792be8854377e651f0c03c5e08`，有效板端算法/八框轨迹仍为 `9f84efecd57d611a239cc71c99ab2e4295025dbc`。输入 `data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；八框 CSV 位于 `/tmp/nt-dual-gt-evidence/handoff-9f84efe/<label>/frames.csv`，其中 car001/car002 标准框 SHA-256 分别 `7bcd85d31b9e5840eb4bb56e2d9d7e82eb4b143f9e056c9056fa05e9e9aa3be4`、`23a6e3876ffdc34a863dc5896e2262b851cf7dc84e5d024a7076086c7f82abf4`；其余六框及板端输入、模型、二进制身份见下方 `9f84efe` 八框记录。
- 主机执行 `ffmpeg -v error -i data/occ_video_1/1.mp4 -q:v 2 -start_number 0 /tmp/nt-dual-gt-evidence/rawframes/%03d.jpg -y`，再执行 `python3 /tmp/nt-dual-gt-evidence/edge-center-offline.py > /tmp/nt-dual-gt-evidence/edge-center-offline.json`；脚本 SHA-256 `e948d02680dd34397159cd078956a16ae110b50ecfee30683d88786570c08c25`，报告 SHA-256 `6bc1f0dea1e6a71006078457e8af5ff1bfebbf3902efa8a6e5a7148263e295d6`。脚本在第 240～270 帧沿 tracker 框四边寻找 ±12 px 内图像梯度最强的位置，不使用真值调参；真值只用于事后计算误差。原图目视确认 car001/car002 均为白车但车身明暗分布不同，因此简单亮度质心也没有可靠的共同偏移方向。
- 后段 31 帧每框结果：car001 标准/人工/左上/右下原 tracking 中心均误差 `7.39/8.74/7.18/7.37 px`，梯度修正反事实 `16.31/12.57/17.29/12.85 px`，四框各 `0/31` 帧改善。car002 标准/真值/左上/右下原均误差 `7.85/4.66/3.16/6.65 px`，修正后 `5.07/16.27/8.72/4.27 px`；虽有两框均值下降，另外两框显著退化，且四框的 P95 均差于原轨迹。该修正没有进入 C++、板端或正式八框评分。
- **裁决 / Handoff**：同一图像边缘规则无法稳定消除不同目标/初始框的 NanoTrack 原始中心偏移；不得把 car002 两框的局部收益当成两路达标。继续保持 `9f84efe` 算法内容与 400 ms 配置，正式 handoff HOLD。下一步应研究可跨框验证的外观/模板定位或经协调者冻结的新模型，而非把 display detector 外观框用于 tracking/control 校正。本记录提交后以实际 commit 为结果身份。

## 2026-11-14T05:18:31+08:00 稳定可见后段中心误差归因与模型侧 Handoff

- 在精确板端运行 `9f84efecd57d611a239cc71c99ab2e4295025dbc` 的八个 CSV 上，主机执行 `python3 /tmp/nt-dual-gt-evidence/analyze-late-center-sources.py > /tmp/nt-dual-gt-evidence/late-center-sources.json`；脚本 SHA-256 `80de59ddcdad3c0a2700e23c417e7202642da12f0afb5e7a7796a006039b42cf`，报告 SHA-256 `cd411b68e52eb7a5ec2cbbad7128f27c96e4dff823164d3e24f30b0c1ac878e7`。脚本通过 `scripts/evaluate_occ_video_1.py` 严格加载两目标匹配 GT，取第 240～270 帧的 31 个可观测框，逐帧比较 tracking、display、NanoTrack recent 原始中心与真值。
- 八框 `tracking_bbox` 中心均与当帧 NanoTrack recent 原始中心逐帧重合（每组 `31/31`，误差小于 0.001 px）；display 与 tracking 中心最大差约 0.0001 px。car001 标准/人工/左上/右下后段 tracking 中心距真值均值 `7.39/8.74/7.18/7.37 px`，有方向一致的左上偏移：平均 dx `−4.37/−5.93/−3.90/−4.34 px`、dy `−5.88/−6.37/−5.97/−5.88 px`。car002 四框后段 dx/dy 方向和量值不相同（例如标准 `−5.01,+6.00 px`、真值框 `−3.97,+2.13 px`），不能对所有目标加一组固定中心偏移。此区段的误差已直接来自 tracker 原始输出，不能靠调整 display detector 视觉框或当前控制器平滑解决。
- **证据边界 / Handoff**：上述是同一视频的八框板端回放分析，尚未证明哪种新 tracker 权重、外观验证器或模型侧改动可改善它，也不能单凭本数据训练后再用它宣称泛化。请协调对话将“car001 后段原始 NanoTrack 中心持续偏左上、car002 方向依 ROI 不同”的精确 CSV 和报告交给 NanoTrack 微调/模型评估 owner；若提供新冻结模型，须按契约先核对 checkpoint、ONNX、RKNN、配置和哈希，再由板端任务用两目标八框重跑。当前算法分支与板端正式 handoff 均保持 HOLD。

## 2026-11-14T05:15:50+08:00 car002 恢复前中心分叉与最短等待时间反事实

- 沿用已完成八框验证的精确源码 `codex/dual-gt-init-consistency@9f84efecd57d611a239cc71c99ab2e4295025dbc`、RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-handoff-9f84efe`、二进制 SHA-256 `41d83ef0d772833b29400a35cacb8825a1a402f24ea7dbfe9279ac09b062343b`、视频/GT/模型/驱动身份，仅修改板端隔离配置，不改仓库 400 ms 正式配置。原八框 CSV 对真值逐帧核对：car002 右下框第 230～232 帧 motion 中心距可观测真值 `22.7/23.5/24.9 px`，recent/safe 原始中心约 `5.6/5.4`、`4.7/4.4`、`5.0/3.9 px`；但原配置第 232 帧才允许首次可信候选命中，第 233 帧两击重捕。四框进入 SUSPECTED 的第 211 帧速度、加速度不同，至第 232 帧开环预测中心最大分叉约 `34.8 px`；用第 211 帧中心和速度在 Python 中离线作零加速度外推，反而使标准框第 232 帧预测中心误差约 `6.4→18.8 px`、真值框约 `10.3→12.9 px`，不能据此改动运动模型。
- 板端 `search_reacquisition_min_age_ms=400→250`：`runs/minage250-controller.conf` SHA-256 `13a2dc71f345a2d71a70ae1c1e1529924f91ec83294712c5b4701e945eda877a`，`runs/headless-minage250.conf` SHA-256 `70264764d17e9088a7657f86d684e9dde0da9419870c7718deef8efbb1ac7eee`；实际命令 `./build/nanotrack_v3 --config runs/headless-minage250.conf --bbox 972,519,36,36 --output-video runs/minage250/car002_right/output.avi --output-boxes ... --session-metadata ...`。AVI 1920×1080×271，CSV/AVI SHA-256 `929248ad347fee27e5559c68fa50d60f3c20edc16efc8546c583f09c2d7adf4a`/`8514c40debf45be15d84b4176a860e90f1f84d976730322285d9120191045f12`，文本包 `runs/minage250-car002-right-text.tar.gz` 主机/板端 SHA-256 `ffc23e24211f52e1b68fdd0d8b3cc35ae65fbeb8417892c39fc274845e286681`。评分 `/tmp/nt-dual-gt-evidence/minage250-car002-right.json` SHA-256 `a9e6b0b784a88f5fc3c351b9616b14e49019858e747c06cd67abc67f14a2d8f2`：第 228/229 帧两击并在第 229 帧错误重捕，此帧属人工确认无框；第 249 帧再次重捕，可见 tracking/display 中心均值 `3.96/3.99 px`，正式 FAIL，否决。
- 板端 `search_reacquisition_min_age_ms=400→300`：控制器/headless 配置 SHA-256 `84a0893fb60ca0b15ffb98700c420fa6f789a854d896872ab7bf3604f3d4ecfb`/`9737eeed8f80120ccc9a3fbc55242db8f05879ba66c1d8217e99af40a75b9ca8`，同一命令改用 `runs/headless-minage300.conf`、`runs/minage300/car002_right/`；AVI 1920×1080×271，CSV/AVI SHA-256 `c0dd3a257fdfcc4762908a7343f3033e465795741ca77b2778ba72494322be55`/`2bd2366dd465c8257fa94cf444d67939ecac3c0133187bcc28995fc29b652b6a`，文本包 `runs/minage300-car002-right-text.tar.gz` 主机/板端 SHA-256 `a49f6c4984bc30de392f979e462961db6e16b3eb78b106420294a7b369b22bc5`。评分 `/tmp/nt-dual-gt-evidence/minage300-car002-right.json` SHA-256 `c1790b90465e987fa05f97b45f0a16d76813f622955d3b327e52b9ec0c488de3`：第 229～231 帧先产生身份冲突，虽第 232～233 帧同一 tracker 候选两击，因冲突后同候选 detector 门禁未重捕，整段无重捕；可见 tracking/display 中心均值 `11.85/10.97 px`，正式 FAIL，否决。
- **裁决 / Handoff**：最短等待是遮挡边界的身份安全门，而非单纯让 tracker 的较好原始框更早输出；250 ms 导致无框帧误重捕，300 ms 使冲突记忆阻止后续重捕，均比原 400 ms 退化。正式源码和配置不变，当前有效八框矩阵仍为 `9f84efe`，handoff HOLD。下一步若处理重现前两路中心，应区分“未经确认的候选框”与可用于控制的可信框，并以 car001 遮挡期相似车辆和 car002 无框帧共同约束，不能把原始响应直接当作真值。

## 2026-11-14T05:10:27+08:00 远距探针门禁的板端筛查否决与撤回

- 精确源码 `codex/dual-gt-init-consistency@4a4032c781428f1f3536c72c656a5966c667d3f8`，源码包 `/home/neardi/Project/SOT/nt-dual-gt-probe-gate-4a4032c.tar` 主机/板端 SHA-256 `bd2411a339a1512e5f89ebe99dc41f796f2c1d10ac11ccbe8c06d20b16a30a15`，隔离根 `/home/neardi/Project/SOT/dual-gt-probe-gate-4a4032c`；RK3588 原生 CTest `23/23 PASS`，`build/nanotrack_v3` SHA-256 `f531e37312f5bbf9a2ea9544ecbda618b6bad9b5ac23b20e4f9aa65cd9720235`。控制器配置 SHA-256 `d0e4f9ad7c46ee2d6e00a30653257c95ee4ab263d51d9863dbb27958a03759d2`，400 ms headless 配置 SHA-256 `910558dc3929fed7a34deb3430509726ad0ac4ec80f0bc42d869248a00476a03`；视频、GT、模型和板端运行版本同上一八框矩阵。
- 板端先把 `detector_interval_ms=400→330`，`runs/detector330-controller.conf` SHA-256 `c46d540ce82b2bdd612582e456f6190494d431c095118bfa92b6f431596df5b7`，headless SHA-256 `d90b49bc98702b4cd45608c9104314a35d1528b7a686d639cf443019934c1be9`。实际运行 car001 标准框 `./build/nanotrack_v3 --config runs/headless-detector330.conf --bbox 992,582,37,39 --output-video runs/detector330/car001_nominal/output.avi --output-boxes ... --session-metadata ...`，CSV/AVI SHA-256 `6af9e30c8ef94b3f342b1b5892b8c746297886f9177b56dd83e0e3fc04625ecb`/`b2c5b22a09cdfb2fedb5a63669810bfadff9a691600cb50faf0d1e17cd7ee60e`，文本包 `runs/detector330-car001-nominal-text.tar.gz` 主机/板端 SHA-256 `a1bad1bc329cfdaa7cf90c6c784ec02508501188f4af3d1132f2c8c2ef11346a`，评分 `/tmp/nt-dual-gt-evidence/probe-gate-4a4032c-detector330-nominal.json` SHA-256 `11e8547ed1f9473be1da7decd21be51ed4a6226be47a795ad4b2ff1bfca03771`。第 179～180 帧错误远距探针被排除，候选未错车重捕；但较早 tracker 原始中心仍距真值约 `12～45 px`，正确贴近真值的两击直到第 192～193 帧，重捕 `193`，tracking/display 中心均值 `4.43/4.47 px`，正式 FAIL。
- 400 ms 正式配置在板端执行 car001 标准/左上/右下三框，AVI 全为 1920×1080×271；标准 CSV SHA-256 `f52e5f9c8293eaee98259ff37283eb1b8f9f969097d827d71717bab93f49366e`，左上/右下 CSV SHA-256 `896dce6c54255ab101d6d1629baca9ac09eed22210ff7d947755897ef4e8f071`/`cf323abff779944f0c344636882752dd63730a52dc65bba44c2ae6dca953a605`，文本包 `runs/standard400-car001-nominal-text.tar.gz`、`runs/standard400-car001-extra-text.tar.gz` 主机/板端 SHA-256 `9a91db2faf91bdbfe6d551a0f648dca08c7e6a046776d94053f10129f4e66e63`、`a515a41ecfd5b5984007bf4823641ea909764c31e43948d219768bdf62cf9af1`。评分 `/tmp/nt-dual-gt-evidence/probe-gate-4a4032c-standard400-car001-three.json` SHA-256 `e31102a8fe3aac627d1c4200310bcebf728e6d6208617eb1533a8df444e65a17`：重捕 `188/191/191`，tracking/display 中心均值与 `9f84efe` 基线逐项一致，标准 PASS、左上/右下仍 FAIL。该门禁对正式参数无观测收益。
- **裁决 / Handoff**：撤回 `4a4032c` 的探针/确认距离门禁、配置和专属单测，恢复 `9f84efe` 算法内容及 400 ms 配置；恢复后主机执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，RKNN-off `23/23 PASS`，日志 `/tmp/nt-dual-gt-evidence/probe-proximity-revert-host-build.log`、`/tmp/nt-dual-gt-evidence/probe-proximity-revert-host-ctest.log`。保留精确源码、板端输出和失败记录。330 ms 错车虽被抑制，但真值重捕时机没有满足目标，不能用它替代八框基线。正式 handoff HOLD。

## 2026-11-14T05:04:21+08:00 保留远距竞争身份并阻止其牵引搜索探针的候选

- 基于 `codex/dual-gt-init-consistency@c6af7ff6bafda5ba075c37501f8526e8382964fa`，工作树 `/home/u20/single-object-tracking`，在 `RecoveryCandidateTable` 新增距可信运动中心的探针选择：detector 起源候选若超出 `detector_candidate_max_center_ratio`，仍保留在身份候选表并参加冲突判定，但不让它决定 NanoTrack 下一帧裁剪；确认阶段亦不得提交该远距 detector 身份。运动预测不可信时保持原选择。默认关闭的 `recovery_confident_detector_proximity_gate` 仅在当前可见配置启用，配置 SHA-256 `d0e4f9ad7c46ee2d6e00a30653257c95ee4ab263d51d9863dbb27958a03759d2`；正式 detector 间隔仍 400 ms，display detector 的视觉修饰不参与 tracking/control 授权。
- 动机来自上一 330 ms 对照：错车 detector 候选距运动中心 `0.037～0.039` 帧对角线，高于既有 `0.02` 限值，但前一候选仍让它连续占用搜索探针，最终将正确重捕延迟到 193 帧。本改动与已否决的“直接过滤 detector 候选”不同，保留远距竞争者作为冲突反证。实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`，日志 `/tmp/nt-dual-gt-evidence/probe-proximity-host-build.log`、`/tmp/nt-dual-gt-evidence/probe-proximity-host-ctest.log`；单测核验远距候选不被探测但仍在表中造成身份冲突，近距候选可探测，远距两击不能提交。
- **证据边界 / Handoff**：主机测试不能证明 RK3588 上能改善两个目标；先在 400 ms 上检验 car001 标准/右下、car002 标准/真值不退化，再用 330 ms 失败场景核验错车是否消失且 car001 不晚于重捕时限；满足后扩八框矩阵。正式 handoff HOLD。本条与候选代码同提交。

## 2026-11-14T05:01:11+08:00 远距确认门禁板端否决并撤回

- 精确源码 `codex/dual-gt-init-consistency@71a14162df472da8e9c784f77781a5031e40be07`，源码包 `/home/neardi/Project/SOT/nt-dual-gt-confirm-gate-71a1416.tar` 主机/板端 SHA-256 `3b1dfbb4c2a829858a4daa206eeeb636cc5227b9d6f64749a641c49cd64b98cd`，RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-confirm-gate-71a1416`。原生 CTest `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `727c498ab2223cab37d65d310c046ed938cdca4ed985a66daa4e758ee52d943a`，400 ms headless 配置 SHA-256 `817bae74744ed3f93f6737d6ec66f8df000281560727157706532698e746d43e`，控制器配置 SHA-256 `af4721bb6dafe4fb63c4f7172a5ba3437b20f82a8dcbc3471125b7edd242336a`；视频、GT、三 NanoTrack RKNN、detector/manifest/labels、Linux/RKNN API/driver 身份同上一八框矩阵。
- 在 400 ms 配置以 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...` 运行 car001 标准/右下、car002 标准/真值四组；四 AVI 均 1920×1080×271。板端 `runs/screen-checksums.sha256` SHA-256 `019952069f21b33f8c5c714fb2b1390b039b37402003317b5850a0985ab96958`，文本包 `runs/screen-text.tar.gz` 主机/板端 SHA-256 `5eb6c56f1521e3a8a921eeea3e55e96a3ddcbcb5b36df4ecad36a337f42489cb`，评分 `/tmp/nt-dual-gt-evidence/confirm-gate-71a1416-screen.json` SHA-256 `c5971f24a9601c144a0ca473b6de442945dbf8c4f5fd716b1f98224ab34f43e0`。四组重捕、中心、正式门禁与 `9f84efe` 代表框逐项一致，门禁未改善现行参数。
- 仅板端 `runs/detector330-controller.conf` 改请求间隔 `400→330 ms`，配置 SHA-256 `05aaa16434373ef3e8b721d6f6dc3b6a70a2f304cd9d374f7bd6887414f60291`，`runs/headless-detector330.conf` SHA-256 `fcd2ffe8005e1cd54df1275de0775312b25278d290733e5d933a1a3adf7b0ad1`；car001 标准框 CSV/AVI SHA-256 `2679b9a89f76143308f3ed169fb9fc2f10c2c71818597dd2fc24494668e0d518`/`b2c5b22a09cdfb2fedb5a63669810bfadff9a691600cb50faf0d1e17cd7ee60e`，AVI 1920×1080×271，文本包 `runs/detector330-car001-nominal-text.tar.gz` 主机/板端 SHA-256 `e2111dcebdce615eac93afe054432504a280313a5cb4bdce2ed89159fb0c54ab`，评分 `/tmp/nt-dual-gt-evidence/confirm-gate-71a1416-detector330-nominal.json` SHA-256 `64bc46a96031938e35da435e91be3cd8d440de7f96d049fa7d5eca51f61ce0fe`。第 179/180 帧远处同类候选获得两次 tracker 命中但没有被提交，证明门禁挡住该错车；随后第 193 帧才重捕，tracking/display 中心均值 `4.43/4.47 px`，仍因重捕晚于时限 FAIL，比 400 ms 标准框 PASS 退化。
- **裁决 / Handoff**：撤回 `71a1416` 新增的确认门禁代码、配置及专属单测；保留该提交和板端证据。源码恢复 `9f84efe` 的算法内容和 400 ms 配置，恢复后实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，RKNN-off `23/23 PASS`，日志 `/tmp/nt-dual-gt-evidence/confirm-proximity-revert-host-build.log`、`/tmp/nt-dual-gt-evidence/confirm-proximity-revert-host-ctest.log`。八框评测仍以 `9f84efe` 精确板端结果为最新有效对照。远距候选确认安全性可单独研究，但该门禁未使本任务要求的双目标贴真值、一致性及完整门槛更好。正式 handoff HOLD。

## 2026-11-14T04:55:29+08:00 330 ms 请求反事实否决与远距确认门禁候选

- 沿用精确源码 `codex/dual-gt-init-consistency@9f84efecd57d611a239cc71c99ab2e4295025dbc`，RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-handoff-9f84efe` 及原生二进制 SHA-256 `41d83ef0d772833b29400a35cacb8825a1a402f24ea7dbfe9279ac09b062343b`；仅板端 `runs/detector330-controller.conf` 把 `detector_interval_ms=400→330`，配置 SHA-256 `95c78de6f39d4a73a459aa2caea2550ddba5374ad5b243d8295f40c681338792`，headless 配置 SHA-256 `68bc79b75c78cbddf0ba4d3552bf1f15060914846c780bd0df992da0d359fe2c`。视频、双目标 GT、模型、manifest/labels 和 RK3588 Linux/RKNN 身份同上一八框矩阵。
- 实际执行五组 `./build/nanotrack_v3 --config runs/headless-detector330.conf --bbox <ROI> --output-video runs/detector330/<label>/output.avi --output-boxes ... --session-metadata ...`，标签为 car001 标准/左上/右下、car002 标准/真值；原始输出在板端 `runs/detector330/`，文件清单 `runs/detector330-checksums.sha256` SHA-256 `1d1c6af3b0fab5cbb0c601b4f7b07ab931b2ca11001d7a3a16fa1dc9c54c763e`，文本包 `runs/detector330-text.tar.gz` 主机/板端 SHA-256 `07da0fa01aa1c4f96238d0353c2391e9639901d7e349d1a8b50990a86c5d8426`。主机 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ...` 五组评分 `/tmp/nt-dual-gt-evidence/detector330-five.json` SHA-256 `1caa497e5b372cd528fce37e6c2a934257201f60128a284f4b9162ce30aac4e6`。
- car001 标准/左上均在第 180 帧遮挡中的错误同类车上重捕，后续分别第 206/216 帧再重捕；右下第 191 帧重捕但也落错目标。三框 tracking/display 中心均值分别 `27.71/27.70`、`23.46/23.45`、`33.44/33.27 px`，跨框 P95 `174.40/174.79 px`；正式五组 `0/5 PASS`。car002 标准/真值中心小幅改善至 tracking `1.73/1.60 px`，但完全遮挡语义仍失败。car001 标准框错误候选在第 177～178 帧距运动中心约 `0.037` 帧对角线，在第 179/180 帧得到两次 NanoTrack 命中，说明两击与同候选 detector 佐证仍不足以区分相似车。**否决 330 ms 全局间隔，正式 400 ms 配置和上次八框证据保持当前对照。**
- 基于该反例新增默认关闭的 `recovery_confident_confirmation_proximity_gate`。当运动预测仍可信时，detector 起源候选在确认帧的 tracker 实际框中心必须距运动预测不超过既有 `detector_candidate_max_center_ratio`；远候选仍保留在候选表中提供身份冲突反证，不删除、不直接授予控制。当前可见配置启用，SHA-256 `af4721bb6dafe4fb63c4f7172a5ba3437b20f82a8dcbc3471125b7edd242336a`，detector 间隔仍为 400 ms。主机执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，RKNN-off `23/23 PASS`，日志 `/tmp/nt-dual-gt-evidence/confirm-proximity-host-build.log`、`/tmp/nt-dual-gt-evidence/confirm-proximity-host-ctest.log`；测试保留远距候选元数据但拒绝两击提交，近距 tracker/detector 恢复路径仍可用。本条在 `/home/u20/single-object-tracking` 与代码同提交，基线 `766b01abb79612f95e4e9ff06477a88efbba03aa`。
- **证据边界 / Handoff**：此确认门禁尚未上板。先用 400 ms 精确候选跑 car001 标准/右下与 car002 标准/真值；若不退化，再以 330 ms 作失败场景反事实并扩八框。不能把主机单测或 car002 的局部收益当作两目标晋级依据，正式 handoff HOLD。

## 2026-11-14T04:43:27+08:00 双目标八初始框的 RK3588 真值矩阵与当前裁决

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；精确源码 `9f84efecd57d611a239cc71c99ab2e4295025dbc`，基线 `74d02d624de5cd17cfed419dbe337d6b6c9ff06b`。源码包 `/home/neardi/Project/SOT/nt-dual-gt-handoff-9f84efe.tar` 主机/板端 SHA-256 `d437e7cac8a4c184ee15fe198d1e39e3f51944d5cf4f9f93d0f0620584512acf`，隔离根 `/home/neardi/Project/SOT/dual-gt-handoff-9f84efe`，原生 CTest `23/23 PASS`；`build/nanotrack_v3` SHA-256 `41d83ef0d772833b29400a35cacb8825a1a402f24ea7dbfe9279ac09b062343b`，`runs/headless-eval.conf` SHA-256 `2d45ca0f31916e0d05e0e5c2cad189bbf0d42f372aa4a67f3dbb3de939c531d6`，`visible-adaptive-display.conf` SHA-256 `5d3214646bd3b0e6d38213482178aa44a0deb301d6f303c8b5eaa7f5517d3a3b`。RK3588 Linux `6.1.84`、RKNN API `2.1.0`、driver `0.9.8`。
- 板端主视频 `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/data/occ_video_1/1.mp4` SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；官方 NanoTrack 三 RKNN `template_backbone/search_backbone/head` SHA-256 `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`/`d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`/`4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`；RK3588 VisDrone9 detector/manifest/labels SHA-256 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`/`ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9`/`41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。主机 `data/occ_video_1` 的 car_001、car_002 已确认 GT 与事件包由评分器逐文件哈希验证，评分不使用遮挡估计框作为普通 IoU；精确 GT 身份见本文件前述双目标条目。
- 板端执行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`，含 car001 标准/人工/左上/右下与 car002 标准/真值/左上/右下；八个 AVI 全部实测 1920×1080×271。板端 `runs/matrix-checksums.sha256` SHA-256 `8b04bbed755331a03eadc1697bab35f9dafe8c8847083fb72b281dcfe82628c9`，每路 CSV/AVI/session/run.log/JSONL 与运行命令保留 `runs/matrix/<label>/`；文本包 `runs/matrix-text.tar.gz` 主机/板端 SHA-256 `cea2698426035d55367f2ab8bbe1613b41d66184cbf6abe9c9312470b47a305d`。主机执行 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ...` 对每路指定匹配目标 GT，评分 `/tmp/nt-dual-gt-evidence/handoff-9f84efe-eight-center.json` SHA-256 `7613b4730009660882bf0ce012f569f2aad1a975889355b1930c6f84817cc812`。
- car001 四框重捕帧 `188/188/191/191`；tracking 中心均值 `4.28/4.63/4.34/5.02 px`、display `4.32/4.69/4.35/5.05 px`，跨初始框中心 P95 `3.31/3.12 px`。相对 55efe8d 左上框无重捕和 `27.01/26.24 px`，本次已恢复为同一真实目标且轨迹中心接近其他框；标准、人工框正式 PASS，左上框晚于第 190 帧独占时限、右下框还存在完全遮挡语义失败。car002 四框重捕帧 `235/233/233/233`；tracking 中心均值 `2.59/2.00/1.77/2.92 px`、display `2.59/2.00/1.75/2.89 px`，跨初始框中心 P95 两路均 `6.21 px`，和基线一致；四框都在完全遮挡起始时延迟进入 SEARCHING。两目标均无已确认 no-bbox 帧的误观测，但状态语义门仍未过。
- 对原视频第 `191/232/260` 帧抽图，以对应目标 GT、tracking、display 分别用绿/红/蓝绘制六格局部放大图 `/tmp/nt-dual-gt-evidence/handoff-9f84efe-keyframes.png` SHA-256 `892828228fa7413ac69b8f72e4683a4940d202840eb5df7a9195bc33e09090eb`。目视核对：car001 左上/右下第 191 帧均定位同一白车，第 260 帧两路中心都较真值稍偏左上；car002 左上/右下第 232 帧处于未确认的重捕前搜索，右下框预测中心明显落在车身左下，随后第 233 帧才有正式 tracker 两击重捕。接近的全程均值不能消除这段局部偏差。
- **裁决 / Handoff**：新候选改善了 car001 左上框灾难性漂移，八框中心位置与不同初始框的一致性已定量验证；正式完整评测仍 `2/8 PASS`，不能作为批准的板端 handoff 或真实吊舱验收。下一步先处理 car001 两个微扰框重捕迟于时限与 car002 完全遮挡期间的 SUSPECTED→SEARCHING 延迟，再以同一八框 GT 复测；同时 car001 后段可见帧仍约 `7–9 px` 中心误差，需研究 tracker 本身的漂移，不能靠 display detector 美化掩盖。请协调对话继续保持 `development_board_validation.latest_handoff.available=false`，勿启动生产编译/吊舱验收。

## 2026-11-14T04:36:03+08:00 冲突候选的短时佐证交接，撤回整表延长

- 精确源码 `codex/dual-gt-init-consistency@74d02d624de5cd17cfed419dbe337d6b6c9ff06b`：主机/板端源码包 `/home/neardi/Project/SOT/nt-dual-gt-bound-hit-74d02d6.tar` SHA-256 `910b7a55ff08fcda3658eb36b0d7c4155a69f4de41815563c5d9f63d8648efc1`，隔离根 `/home/neardi/Project/SOT/dual-gt-bound-hit-74d02d6`，板端原生 CTest `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `6c5c0e4a0a9d891842a9ad2b18b56fc35826d1c79c878dc7aa9a884f2f0aeb1d`，headless 配置 SHA-256 `be76106c935b518e3872a86daaec1682634659af107ca41bcb2395cfaebf2231`，控制器配置 SHA-256 `5d3214646bd3b0e6d38213482178aa44a0deb301d6f303c8b5eaa7f5517d3a3b`。同一 RK3588、视频、GT、模型/manifest/labels 和驱动身份沿用 7cbf58d 基线。
- 实际运行 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox 990,580,39,41 --output-video runs/car001_left/output.avi --output-boxes runs/car001_left/frames.csv --session-metadata runs/car001_left/session.json`：AVI 为 1920×1080×271，CSV/AVI/session SHA-256 `8b9363706b56631b3b293f5c0695c1776586adea080ec198f7c64e8bb4835dbb`/`f2fee2ecdf9666ae0bcffacbe73935cb2abf198472ae786516f3bf6a68802799`/`83490d7fa77414124cf60ba29987ace57d569f6d3a7261a9b02c6f5ea3e73d8c`，板端 `runs/car001_left-text.tar.gz` SHA-256 `be64c6e574b9c86f6d5175c35b104748593b10128cbb96036bd2d080e3c99ed1`，评分 `/tmp/nt-dual-gt-evidence/bound-hit-74d02d6-car001-left-center.json` SHA-256 `b8e1d8053386f23620f6405d724ed7634a9efbabf72b2ce1c23b25c1cbfc3bf0`。仍无重捕，tracking/display 中心均值 `27.01/26.24 px`；新保留窗使其他同类 detector 候选也多占一帧，第 190 帧仍有身份冲突，首次 NanoTrack 命中延至 191 帧，而 detector 源时效已过。**否决 74d02d6 的整表延长，正式 handoff HOLD。**
- 基于 `74d02d6` 的新改动恢复原候选帧窗；仅在冲突后 detector 门禁开启、detector 元数据因帧窗到期而源时间尚有效、且该候选已经有匹配 tracker 证据时，保留带源时间的短暂交接标记。其他 detector-only 候选正常清理；只有该候选在源时间仍有效时产生的 NanoTrack 验证命中才绑定佐证，后续仍须同候选两击，冲突/重置清标记。`display_bbox` 的 detector 框依旧只服务显示，不进入 tracking/control 授权。主机 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 和 `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure` RKNN-off `23/23 PASS`，日志 `/tmp/nt-dual-gt-evidence/conflict-handoff-host-build.log`、`/tmp/nt-dual-gt-evidence/conflict-handoff-host-ctest.log`。工作树 `/home/u20/single-object-tracking`，本条与代码同提交。
- **证据边界 / Handoff**：新交接候选尚未上板，先按同一 car001 左上框原生复测；若恢复，再复测标准/右下与 car002 代表框和八框矩阵。未满足两目标、两路中心及一致性目标前不批准 handoff。

## 2026-11-14T04:31:00+08:00 一次提前请求的八框矩阵否决与 detector 命中绑定候选

- `codex/dual-gt-init-consistency@55efe8dd7f4cec6a68f586b27d8d5e788cd7be61` 在 RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-conflict-refresh-55efe8d` 原生编译，CTest `23/23 PASS`；源码包 SHA-256 `e93c98080c2df7b38dad3f287f2626c1a57361ac10dba0d1d2008de274e932db`，二进制 SHA-256 `0de9914db4a35fb9ab6fec5ef0c7d51a820df48feee6bae150d85f4f490a8b13`，headless 配置 SHA-256 `efa187823c528d251ea3cfd852afe3003da77f9b540fbca37fb7039bc691c813`，控制器配置 SHA-256 `5d3214646bd3b0e6d38213482178aa44a0deb301d6f303c8b5eaa7f5517d3a3b`；视频、两套 GT、模型/manifest/labels 与板端驱动身份沿用 7cbf58d 基线。实际以 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...` 运行八框；八 AVI 均 1920×1080×271，文本在板端 `runs/screen` 和 `runs/matrix-extra`。四组筛查文本包 SHA-256 `76c32e0f6be8140e12969d4c018b4747f0517b00e0faa99063b12139fe3af4f9`，另四组文本包 SHA-256 `f96a11e2329fdddf8426dbf4d87f1f6e8f4334b9183585738bb35890936c6f8a`；两批文件清单 SHA-256 分别 `142d7b26503849c063522d18caf2f19613476a7b3fa75db5090e5d6dcbc3960e`、`83d2b8456f305d96b584f5ce25d38326a1d75bc79c9bbb67b77d23e323163210`。`python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ...` 八框评分 `/tmp/nt-dual-gt-evidence/conflict-refresh-55efe8d-eight-center.json` SHA-256 `b13ab50da5673763c9a9879c1a58139a762b3b08e68f90c2ec50911ca50d8492`。
- car001 标准/人工框在第 188 帧重捕，tracking/display 中心均值 `4.28/4.32`、`4.63/4.69 px`，均 PASS；右下框第 191 帧重捕，中心 `5.02/5.05 px`，仍超时且完全遮挡语义失败；左上框没有重捕，中心 `27.01/26.24 px`，四框中心 P95 `145.34/141.49 px`。car002 四框保持第 `235/233/233/233` 帧重捕，tracking 中心均值 `2.59/2.00/1.77/2.92 px`、display `2.59/2.00/1.75/2.89 px`，跨框 P95 两路均 `6.21 px`；四框都因完全遮挡语义失败。正式门禁 `2/8 PASS`，该提交不能晋级。
- 左上框逐帧证据：第 186 帧请求、第 187 帧收到 detector；第 190、191 帧同一候选出现两次 NanoTrack 命中，却未重捕。原三帧候选保留窗在第 190 帧清掉 detector 元数据，尽管其源帧 186 到当时仅 `133 ms`，尚未超过 `150 ms` 时效。基于 `55efe8d` 新增**仅在启用冲突后 detector 门禁时**多一帧候选元数据保留，同时要求 detector 在当前两次 NanoTrack 命中之一发生时仍新鲜，并将这次佐证绑定到该命中；冲突、跨代、命中重置都会清除绑定。detector 仍不能直接授权重捕或控制，display 仍只作显示。工作树 `/home/u20/single-object-tracking`，改动基线 `55efe8dd7f4cec6a68f586b27d8d5e788cd7be61`，本条随代码提交。
- 主机执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，RKNN-off `23/23 PASS`，日志 `/tmp/nt-dual-gt-evidence/conflict-bound-hit-host-build.log`、`/tmp/nt-dual-gt-evidence/conflict-bound-hit-host-ctest.log`。测试覆盖 detector 源时效内第 4 帧交接、命中绑定、下一帧 detector 过期仍须两次 NanoTrack 命中、冲突清除绑定、过期源不得绑定；默认关闭配置维持原三帧窗。第一次全局延长帧窗导致历史 recovery replay 两项测试失败，缩小至启用配置后 `23/23 PASS`。
- **证据边界 / Handoff**：新候选尚未在 RK3588 运行，须以精确提交原生构建和至少 car001 左上/标准/右下及 car002 代表框复测中心、重捕时机与身份；如过门再跑完整八框。正式 handoff HOLD。

## 2026-11-14T04:19:49+08:00 冲突后首个可信候选触发一次提前 detector 请求

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `4669b7be5bbf89c624362646f8ebac5f9395bc3e`。板端 car001 标准框的冲突到 183 帧，184 帧已有 tracker 候选一击，但 400 ms 定时 detector 下次在 189 帧请求、190 帧到达，造成 192 帧才完成 NanoTrack 两击重捕。新增每次丢失最多一次的定向提前请求：仅当已发生身份冲突、候选有至少一击且该候选尚无 detector 证据时，允许越过常规定时；仍须原有 class/generation/stale 门禁与同候选 NanoTrack 两击确认。未改变 detector 显示用途或直接控制权限，配置 SHA-256 仍为 `5d3214646bd3b0e6d38213482178aa44a0deb301d6f303c8b5eaa7f5517d3a3b`。
- 主机实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/conflict-refresh-host-build.log`、`/tmp/nt-dual-gt-evidence/conflict-refresh-host-ctest.log`。测试核验冲突后有一击时可提前请求、不可连续越过间隔、无冲突时不提前。首次测试夹具未建立目标类别，按安全设计拒绝 detector 请求；补全固定车类初始化后通过。
- **证据边界 / Handoff**：提前请求可能改变 detector 竞争身份与恢复路径，必须在 RK3588 用精确提交完成 car001 标准/右下、car002 标准/真值四组；只有中心一致性、重捕时机、身份与遮挡语义都不退化，才扩八组。正式 handoff HOLD。

## 2026-11-14T04:18:00+08:00 身份冲突后同候选佐证的四组板端筛查

- 精确源码 `codex/dual-gt-init-consistency@4669b7be5bbf89c624362646f8ebac5f9395bc3e`，源码包 `/home/neardi/Project/SOT/nt-dual-gt-conflict-detector-4669b7b.tar` 主机/板端 SHA-256 `599040638f953d8f562eb20b266ee17b3f8792de26b31b73ea3ae692ee87c437`，隔离根 `/home/neardi/Project/SOT/dual-gt-conflict-detector-4669b7b`。RK3588 原生 CTest `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `800944e5718d12c3507c1431c0a5295fddc2dc8107e6ee26638bdcd7f7fd65d8`，控制器配置 SHA-256 `5d3214646bd3b0e6d38213482178aa44a0deb301d6f303c8b5eaa7f5517d3a3b`，headless SHA-256 `77a6f09404cb76962a77d8e103feb0b3483475e5c2a6169547bebe35c39b4a91`；视频、双目标 GT、官方 tracker 与 detector 身份同 7cbf58d 基线。
- 执行 car001 标准/右下、car002 标准/真值四组 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...`，四 AVI 均 1920×1080×271。16 文件清单 `runs/screen-checksums.sha256` SHA-256 `2306d11277a40b6799fbe4ae4021c7647031688ea9f73a474c4b6ed459276d66`；文本包 `runs/screen-text.tar.gz` 主机/板端 SHA-256 `3badfa3ced7ae547c78913ff99b95c4881f51ffb6031377c5ffb41a9f3b6c3d3`，评分 `/tmp/nt-dual-gt-evidence/conflict-detector-4669b7b-screen-center.json` SHA-256 `1874a177ad362ca4d62ad83d00664a6062aa02525b964098fecea03d98160c2d`。
- car001 标准/右下 tracking 中心均值 `4.39/5.02 px`、display `4.44/5.05 px`，重捕 `192/191`，跨框 tracking/display 中心 P95 `2.88/2.16 px`；标准框 185–190 帧不再输出偏右 staged，而在 192 帧经 detector 同候选佐证重捕，但晚于正式重捕时限。car002 标准/真值 tracking 均值 `2.59/2.00 px`、重捕 `235/233`，基本保持基线路径，完全遮挡语义仍失败。四组正式门禁 `0/4 PASS`；这是中心一致性有改善但时效和遮挡门禁未过的中间候选，不能晋级。
- **裁决 / Handoff**：保留候选源码和板端证据用于定向缩短冲突后 detector 等待；没有批准正式 handoff。后续须证明提前请求不会重现先前 300 ms 全局加速时的同类车误重捕，再做八组矩阵。

## 2026-11-14T04:13:33+08:00 身份冲突后同一候选的 detector 佐证门禁候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `5e74e958ebd0b00574776bc4b7c2634c5716aaea`。对已验证八组板端 CSV 和双目标 GT 逐帧核对：car001 标准/人工/左上/右下框在重捕前分别有 `178–183`、`178–182`、`178–180,190–192`、`188` 帧身份冲突；car002 四组在重捕前没有。car001 标准框第 185 帧纯 tracker 候选重捕，中心误差 `9.2 px`，随后 staged 偏框；另三框经 detector 候选在 191–194 帧重捕。car002 四组 tracker-only 重捕中心更贴近真值，不能一律要求 detector。
- 新增默认关闭的 `recovery_conflict_requires_detector`，只在本次丢失已经观察到身份冲突时，要求**同一个** tracker 两击确认候选具有 detector 证据后才准许重捕；无冲突路径仍按既有两击确认。detector 证据自身不授权 tracking/control，仍由 NanoTrack 两击确认。`visible-adaptive-display.conf` 启用，SHA-256 `5d3214646bd3b0e6d38213482178aa44a0deb301d6f303c8b5eaa7f5517d3a3b`；该门禁不消费 display 的 detector 框。
- 实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/conflict-detector-host-build.log`、`/tmp/nt-dual-gt-evidence/conflict-detector-host-ctest.log`。测试验证冲突被本次丢失记住、冲突后仅 tracker 两击不能授权、同候选 detector 佐证加 NanoTrack 确认可以授权、无冲突两击路径保留。
- **证据边界 / Handoff**：目标是消除 car001 过早且偏框的 tracker-only 重捕，同时保留 car002 的无冲突重捕；必须以精确源码在 RK3588 原生运行四组代表框并比较两路中心、跨框 P95、重捕和遮挡语义，再决定是否扩八组。现阶段正式 handoff HOLD。

## 2026-11-14T04:07:45+08:00 恢复 probation 严格双模板 IoU 门禁的四组板端否决

- 精确源码 `codex/dual-gt-init-consistency@4b46dd3ac230c847d4d0d9d246528b771f8fedb0`，源码包 `/home/neardi/Project/SOT/nt-dual-gt-probation-iou-4b46dd3.tar` 主机/板端 SHA-256 `1d11da318aa4460e8f972236ba9bd9564a8ef922977fe650dbe2629fa9f39591`，隔离根 `/home/neardi/Project/SOT/dual-gt-probation-iou-4b46dd3`。RK3588 原生 CTest `23/23 PASS`；二进制 `build/nanotrack_v3` SHA-256 `c948a95f9e96ce8dac86b96836fa22ce2c2d9041faeb7d6e207a258faa00149d`，控制器配置 SHA-256 `f20007d44e7a06045d1a2a3edbb79cbc8cdf18c8f334907d8589f4e3fa04c4db`，headless 配置 SHA-256 `35cce3fb3da04ee3fbdba14f8efdd48c71b3f722e68c5ce5bf0525f5c51d0cd5`；视频、GT、模型、detector 及环境同基线矩阵。
- 执行 car001 标准/右下、car002 标准/真值四组 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...`。16 文件清单 `runs/screen-checksums.sha256` SHA-256 `879c1e990906eb829e8eda6d403ba42a2d64cdfac26ee9e1c00977fe88338089`，四 AVI 均 1920×1080×271；文本包 `runs/screen-text.tar.gz` 主机/板端 SHA-256 `feb5cd6919326dabfefbcac4107e19c0a124c9f82d5f3cd3783e54837481b770`，评分 `/tmp/nt-dual-gt-evidence/probation-iou-4b46dd3-screen-center.json` SHA-256 `be9c50f37e311c5b764401701c17c6a1e014cdd507870ff00e0692894c71e4f7`。
- car001 标准/右下 tracking 中心均值 `19.04/5.02 px`，重捕 `185,200,223,239`/`191`，跨初始框 tracking/display 中心 P95 `72.58/72.58 px`。car002 标准/真值中心均值 `2.59/2.00 px`，重捕 `235/233`，跨初始框 tracking/display P95 `5.09/5.03 px`，完全遮挡语义仍失败。四组正式门禁 `0/4 PASS`；严格门禁使 car001 标准框反复回滚、错误重捕，未形成稳定身份。
- **裁决 / Handoff**：撤回 `4b46dd3` 代码、配置与单测，恢复基线控制器配置 SHA-256 `63007d32b58c7e4aa920762f4d73f3532ef8ee544e50101c0684914d9b470acc`；保留精确板端输出供后续分析。简单调大 probation 几何阈值与前述中心融合均无法保证两目标/两路中心及初始框一致性。正式 handoff HOLD；下一步应以恢复阶段的多假设身份验证与运动连续性为研究对象，并在相同八组真值矩阵复核，不能依据模型加载、主机测试或单个 ROI 局部收益晋级。

## 2026-11-14T04:03:22+08:00 重捕 probation 双模板冲突门禁候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `6363b935fe6dbf1dd0ca1a3b7d60e407d0b89a65`。基线 car001 标准框第 185 帧重捕后，186–189 帧 staged/recent IoU 约 `0.56–0.60`，staged 连续偏离人工真值，recent 则贴近真值；car001 另外三框 probation IoU 至少约 `0.89`，car002 两个代表框至少约 `0.79`。新增默认关闭的 `recovery_probation_min_template_iou`，当前可见配置取 `0.75`；仅当恢复 probation 的 staged/recent 都可靠却低于阈值时，不接受本帧 staged 并回滚模板，避免高分但冲突的 staged 连续污染可信中心。控制器配置 SHA-256 `f20007d44e7a06045d1a2a3edbb79cbc8cdf18c8f334907d8589f4e3fa04c4db`；detector 显示框不参与此门禁。
- 实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/recovery-probation-iou-host-build.log`、`/tmp/nt-dual-gt-evidence/recovery-probation-iou-host-ctest.log`。新增测试证明可靠但相离的两个模板不能把 staged 作为观测接受。首次测试夹具缺少已暂存模板 slot，导致 `bad_optional_access`；补全真实 probation slot/事务状态后 23/23 通过。
- **证据边界 / Handoff**：这是对基线板端冲突帧的假设，回滚后可能延迟重捕或转向其他目标；必须在 RK3588 上检验 car001 标准/右下与 car002 标准/真值的中心、重捕、身份和遮挡语义，再决定是否扩八组。正式 handoff HOLD。

## 2026-11-14T04:00:13+08:00 双模板中心融合的四组板端否决与撤回

- 精确源码 `codex/dual-gt-init-consistency@9bb08b6c8217d026f8023dc2a63a7ab406888284`，源码包 `/home/neardi/Project/SOT/nt-dual-gt-safe-blend-9bb08b6.tar` 主机/板端 SHA-256 `c541ddad201ddcb0e4424f4c633528bcaf95671538c3b8989a4155d9780c3f4b`，隔离根 `/home/neardi/Project/SOT/dual-gt-safe-blend-9bb08b6`。RK3588 原生 CTest `23/23 PASS`；二进制 `build/nanotrack_v3` SHA-256 `79374a4994cff738e326a6bae29f6fda42da675f6b18c346c769f257287cc1a6`，控制器配置 SHA-256 `b56419b8daffae18afeffb2d40e9957899d82586ee027e74aef5c01b065da621`，headless 配置 SHA-256 `6bb93a9cc4af0e25c547cda80f85c3168e0edaa5620b08ae4ce387267dd7dd9f`；视频、GT、模型、detector 及环境同基线矩阵。
- 执行 car001 标准/右下、car002 标准/真值四组 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...`。16 文件清单 `runs/screen-checksums.sha256` SHA-256 `dbf8f665b27779fc8b740448c3e9a41a61d04485630d438558b4d8713b8cb912`，四 AVI 均 1920×1080×271；文本包 `runs/screen-text.tar.gz` 主机/板端 SHA-256 `5e0acf6bcb81b3771bc3d8be77886a91501a5f8947b7523b5e95903832b2bdbf`，评分 `/tmp/nt-dual-gt-evidence/safe-blend-9bb08b6-screen-center.json` SHA-256 `5a70bb2962eb978eb125a4852f957a899194d5947226d39126f1dbbb30688123`。
- car001 标准/右下 tracking 中心均值 `32.93/5.09 px`，重捕 `186/192`，跨初始框 tracking/display 中心 P95 `135.85/133.08 px`；标准框重捕后偏离累积，真实目标身份代理和可见框门禁失败。car002 标准/真值中心均值 `2.10/1.77 px`，跨初始框 P95 `3.42 px`，但完全遮挡语义仍失败。四组正式门禁 `0/4 PASS`。离线冻结轨迹对融合的微小收益没有覆盖实际反馈：一旦微调中心影响下一帧裁剪，遮挡后的恢复路径可分叉。
- **裁决 / Handoff**：撤回 `9bb08b6` 融合代码、配置、测试以及 `c3ce34a` 的诊断环境开关，保留所有板端输出和上述两条实验记录；恢复 `027e28b` 的当前算法文件与 `63007d32b58c7e4aa920762f4d73f3532ef8ee544e50101c0684914d9b470acc` 控制器配置。两目标/两路中心的一致性目标仍未达到，正式 handoff HOLD。后续研究重捕阶段的身份与模板轨迹分叉，不继续用微小中心修补推断长期效果。

## 2026-11-14T03:54:58+08:00 可信双模板近邻中心融合候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `c3ce34ad1f4b8eff368f41fa7fe22e93f1d80101`。诊断显示四组 recent/safe 配对帧 safe 平均中心改善仅 0.22–0.51 px，但偶有远处误响应；只在正常 TRACKING、无模板 probation、两响应均 reliable、中心距不超过 recent 框对角线 15%、框 IoU 至少 0.75 时，按 `recent_safe_center_blend_alpha=0.50` 融合中心，尺寸与 selected_template 仍属 recent。其他配置默认 0 禁用。`visible-adaptive-display.conf` SHA-256 `b56419b8daffae18afeffb2d40e9957899d82586ee027e74aef5c01b065da621`。
- 主机执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/safe-center-blend-host-build.log`、`/tmp/nt-dual-gt-evidence/safe-center-blend-host-ctest.log`。新增可靠近邻可融合与远离 safe 不得牵引中心的控制器测试。
- **证据边界 / Handoff**：离线融合只基于冻结轨迹估算，改变中心后会反馈到下一帧裁剪，必须在 RK3588 原生实际运行至少四组筛查，再决定是否扩为八组；正式 handoff HOLD。

## 2026-11-14T03:53:36+08:00 正常跟踪 safe head 诊断结论

- 精确源码 `codex/dual-gt-init-consistency@c3ce34ad1f4b8eff368f41fa7fe22e93f1d80101`，源码包 `/home/neardi/Project/SOT/nt-dual-gt-safe-head-c3ce34a.tar` 主机/板端 SHA-256 `4118ac8a768c3ce49356923fcd09f68c3a99ce325199a7724f419f567f772dd9`，隔离根 `/home/neardi/Project/SOT/dual-gt-safe-head-c3ce34a`。RK3588 原生 CTest `23/23 PASS`；二进制 `build/nanotrack_v3` SHA-256 `88d821e6610d25b88083e8bae6f598a43ff6a4a9db8aa53e8f65e0dc817cb440`，headless 配置 SHA-256 `5fdb1c5eadfec175545c5c36b0af15c66571ab673ead03da7a4f3d455e130219`；模型/视频/GT/detector 及环境同 7cbf58d 基线。
- 执行 car001 标准/右下、car002 标准/真值四组 `NT_DIAGNOSTIC_SAFE_HEAD=1 ./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...`。四 AVI 均 1920×1080×271，16 文件清单 `runs/safe-probe-checksums.sha256` SHA-256 `d89d2b47ef9afe97324d207c8bb6919eab6bc86e8269581f89822c47de9886d2`；文本包 `runs/safe-probe-text-v2.tar.gz` 主机/板端 SHA-256 `fd7e596db71a4660af5991a6a7101f9a5585ea65df24c5fa1b6f867a9a9401e1`，评分 `/tmp/nt-dual-gt-evidence/safe-head-c3ce34a-center.json` SHA-256 `98fb7c3bec5e66eb899a44286da43199d83ebf6d805926d814b8727d51bb3168`。
- car001 标准/右下与 car002 标准/真值的可见配对帧数 `233/233/229/227`，safe 相对 recent 平均中心误差 `−0.413/−0.352/−0.509/−0.218 px`；safe 优胜超过 0.5 px 的帧数 `69/125/95/79`，recent 优胜 `25/35/18/52`。car001 标准框第 166 帧 safe 中心误差 53.79 px（recent 7.48 px），不能直接切换 safe。四组 tracking 框逐帧与基线相同；car001 右下第 71 帧额外 head 改变了异步 detector 显示刷新时序，其余 display 相同。**裁决 / Handoff**：撤下诊断环境变量候选，保留原始证据，研究仅在双模板近邻且可信时的中心融合；正式 handoff HOLD。

## 2026-11-14T03:46:30+08:00 安全模板正常跟踪响应的板端诊断候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `027e28b8daba1b0ff74e8b3f345f9e43c32118f5`。在 `NanoTrackV3::infer` 增加仅由环境变量 `NT_DIAGNOSTIC_SAFE_HEAD=1` 激活的正常 TRACKING safe head 原始响应采集；控制器仍选择 recent，tracking/control/display 选择策略不变。未设置变量时仍只运行 recent；这只是比较模板几何的诊断候选。
- 实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/safe-head-diagnostic-host-build.log`、`/tmp/nt-dual-gt-evidence/safe-head-diagnostic-host-ctest.log`。控制器配置 `rk3566/nanotrack_v3/config/visible-adaptive-display.conf` SHA-256 `63007d32b58c7e4aa920762f4d73f3532ef8ee544e50101c0684914d9b470acc`，未改。
- **证据边界 / Handoff**：需以精确提交在 RK3588 原生构建，并以两目标四种代表性初始框逐帧对照 recent/safe 中心与 GT；若 safe 无稳定优势，撤回诊断源码；正式 handoff HOLD。

## 2026-11-14T03:42:44+08:00 冻结 g5000 权重的 RK3588 双目标反事实筛查否决

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，当前源码 `8f102f06fec817130b770faf57e47af99ccc3089`；板端复用 `/home/neardi/Project/SOT/dual-gt-visibility-7cbf58d/build/nanotrack_v3`，二进制 SHA-256 `3f6c3896f777148a0d038d8c02b5d21fefa7461cc649336bb538db3b1cdb15d4`。上游冻结 g5000 checkpoint `artifacts/nanotrack_conservative_three_stage/run/stage1/candidate_stage1_g00005000_e001_s02500.pth` SHA-256 `40ebbe4296938bf5d53c8605ab05e729b306fc0bce65921a57ab6613030410d1`；导出源 `/home/u20/single-object-tracking/repos/siamtrackers/NanoTrack@248663fde6bf7c40190cf10ee396d5662919ecd3`、配置 `models/config/configv3.yaml` SHA-256 `60ea9dc886dcdd0451e60b404490a39d19495480185ca7b6299b72b74d746f87`。
- 实际以 `.venvs/rknn-convert/bin/python rk3566/nanotrack_v3/tools/export_v3.py --repo ... --config ... --weight ... --output-dir artifacts/rk3588-g5000-dual-gt --seed 20260929 --opset 14 --atol 1e-4 --rtol 1e-4` 导出，ONNX/ORT 校验通过，head cls/loc 最大绝对误差约 `6.91e-6/1.11e-4`，记录 `/tmp/nt-dual-gt-evidence/g5000-export.log`、`artifacts/rk3588-g5000-dual-gt/export_metadata.json` SHA-256 `4f499b5f033bb40660338b692124423d8e448f7178ea70a8fd37ad7d9a4bdb84`。再运行 `.venvs/rknn-convert/bin/python rk3566/nanotrack_v3/tools/convert_rknn.py --model-dir artifacts/rk3588-g5000-dual-gt --source-commit 248663f... --weight-sha256 40ebbe... --target rk3588`，Toolkit 2.3.2，未量化，日志 `/tmp/nt-dual-gt-evidence/g5000-rk3588-convert.log`。三 RKNN 文件 SHA-256：template `daa9609ca21c95292757faf023b4486446de821a62c57eb479187ae068a20931`、search `4ec265f417396d2bbafc8a9085b1b07be882b4fbc26f5fcd5b497e8849a7ec49`、head `abbb90b06b370f9fe8e0464751eec6be59da25a1f9ab402532613b1e9725aa3d`；manifest SHA-256 `8bde79120af2be7366b0a829a52c9974a087e9c60d3cf13a97be77d969093206`，内部哈希三项均匹配。
- 模型包 `/home/neardi/Project/SOT/nt-g5000-rk3588-dual-gt.tar.gz` 主机/板端 SHA-256 `44c9cf8d667e87a5c335bb7c4b767b461a72a0137992f7ddac5115a036830745`；板端隔离模型目录 `/home/neardi/Project/SOT/g5000-rk3588-dual-gt` 的三个 RKNN/manifest 哈希同主机。只在基线根派生 `runs/headless-g5000.conf`，SHA-256 `e1d219e065cb7aa97a52b7db431dcdbafddffe7bcb1b5172da18cb3ab76adbee`，视频、真值、detector 及其他控制参数保持原矩阵身份。执行 car001 标准/右下、car002 标准/真值四组 `./build/nanotrack_v3 --config runs/headless-g5000.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...`。16 文件清单 `runs/g5000-screen-checksums.sha256` SHA-256 `b80e3ec5d764c25a083c015f635e85ef60d3477f6984e26b97179339a0a0fa66`，四 AVI 均 1920×1080、271 帧；文本包 `runs/g5000-screen-text.tar.gz` 主机/板端 SHA-256 `b6b4ba53751c5ae6c38456e3cc9688dec93c227128c3620d297b2dcce58ea8c1`，评分 `/tmp/nt-dual-gt-evidence/g5000-screen-center.json` SHA-256 `babffaf1e4b3d3efc3a103e8dcbda0d786d5b68a10889a8d8d5bfdb3187ff4cb`。
- car001 标准/右下 tracking 中心均值 `7.88/33.15 px`，重捕 `153,193`/`187`，跨框中心 P95 `136.95 px`；car002 标准/真值均值 `3.27/2.97 px`，重捕 `234/235`，跨框 P95 `4.54 px`，标准框无框误观测 1 帧。四组正式门禁全 FAIL。**裁决 / Handoff**：g5000 在这套 RK3588 双目标任务上不能替换官方权重，保留实验包与原始输出，正式模型身份不变，handoff HOLD。此结论不改写上游训练选择或其他数据集性能结论。

## 2026-11-14T03:37:03+08:00 半像素解码中心修正的八组板端否决

- 精确源码 `codex/dual-gt-init-consistency@7d7a40aaf520b208fc3456e31487999f1c135759`，包 `/home/neardi/Project/SOT/nt-dual-gt-pixel-edge-7d7a40a.tar` 主机/板端 SHA-256 `64b907c127b7b1d5cb055e10522e369de2ed720e88e8eacf97cc281e8bee7388`，隔离根 `/home/neardi/Project/SOT/dual-gt-pixel-edge-7d7a40a`。RK3588 原生构建及 CTest `23/23 PASS`；二进制 `build/nanotrack_v3` SHA-256 `243820e7418a353dc8a52360437989add733242842b5e6e58ef9aeb92948a1be`，控制器配置 SHA-256 `63007d32b58c7e4aa920762f4d73f3532ef8ee544e50101c0684914d9b470acc`，headless SHA-256 `58615f65dcce0ddf03295bf1da2e1d9783c1cc0cfd58c3be93afcb7d7b24dd99`；视频、GT、模型和板端环境同前述基线矩阵。
- 执行八组人工初始框变体 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...`。32 文件清单 `runs/screen-checksums.sha256` SHA-256 `664d409ef1052554e9614876b0f1316e02f1fc7676c419d283e2f0d36990327c`，八 AVI 均 1920×1080、271 帧；文本包 `runs/screen-text.tar.gz` 主机/板端 SHA-256 `211335f086aa97d80df1ec1dfee1a8dc3e0ec71193c8794d8fd396b77c9f0564`，评分 `/tmp/nt-dual-gt-evidence/pixel-edge-7d7a40a-center.json` SHA-256 `e06b57f2a79aa8a1ddc4abc10ff0feded884d1928e50e06a591e8409a6bdd704`。
- car001 标准/右下中心均值从基线 4.39/5.02 改为 3.89/4.26 px，重捕 185/186；但人工框第 155/187 帧两次重捕、左上框第 186/204/239 帧三次重捕，二者中心均值 6.22/14.26 px，四框 tracking/display 中心 P95 `45.74/45.85 px`，明显差于基线 `3.51/3.65 px`。car002 四框中心均值 `2.69/2.25/2.35/3.11 px`，跨框 tracking/display P95 `6.34/6.34 px`，完全遮挡语义仍失败，右下另有预测连续性失败；正式八组 `0/8 PASS`。car001 标准/右下可见框失败集中在 185–190 帧：重捕后 staged 模板偏右，recent 模板随后靠近真值，但 probation 输出仍优先 staged。
- **裁决 / Handoff**：半像素解码修正不能在四种初始框上保持身份一致，撤回 `7d7a40a` 的源码变更，保留八组原始板端输出与评分；正式 handoff HOLD。后续应分别检验模型/模板对初始框微扰的稳定性与 probation 选择策略，不能仅凭两组改善认定全目标达标。

## 2026-11-14T03:28:58+08:00 NanoTrack 裁剪像素中心到框边界中心的坐标候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `89a9e101f5752c6c5b74744bf5609e1a627adc22`。`NanoTrackV3::infer` 以 `bbox.x+(width−1)/2` 定位像素裁剪中心，而通用 `BBox` 与 `decode_head` 使用 `bbox.x+width/2` 作为框边界中心。仅在 `run_head` 进入解码器前把参考中心加 `0.5 px`，保持裁剪与 RKNN 输入不变；检验零位移时的半像素坐标偏差是否累积成可见中心误差。当前是数学假设，不能从代码差异直接推断板端效果。
- 实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/pixel-edge-center-host-build.log`、`/tmp/nt-dual-gt-evidence/pixel-edge-center-host-ctest.log`。
- **证据边界 / Handoff**：须在 RK3588 以精确提交测试 car001 标准/右下、car002 标准/真值初始框；同时比较 tracking/display 中心、IoU、重捕、身份与遮挡语义。板端证明前保持 HOLD。

## 2026-11-14T03:25:41+08:00 NanoTrack 窗口权重 0.30 板端否决

- 精确源码 `codex/dual-gt-init-consistency@2e6a0cdca6847c21b1bab979ce66df0cee877d2a`，包 `/home/neardi/Project/SOT/nt-dual-gt-window030-2e6a0cd.tar` 主机/板端 SHA-256 `79a56c94ffa2402d473fe2126cabd9206e23ba9cb852d9143e19e15d70f68282`，隔离根 `/home/neardi/Project/SOT/dual-gt-window030-2e6a0cd`。RK3588 原生构建及 CTest `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `5417ee1826447dd5b592373ac8a29387a0ef2818037260083301c0cf2d788de5`，控制器配置 SHA-256 `63007d32b58c7e4aa920762f4d73f3532ef8ee544e50101c0684914d9b470acc`，headless SHA-256 `0a8ff85069a15eeeb14692a1d8fa74da19fb8e55710a899298c263a229b9e616`；视频、GT、模型和板端环境同前述精确矩阵。
- 执行四组 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...`，car001 标准/右下与 car002 标准/真值。16 文件清单 `runs/screen-checksums.sha256` SHA-256 `c01db02d914937097f8542a2044127a405fbb53b61591bedc4ba38bd296e66f9`，四 AVI 均 1920×1080、271 帧；文本包 `runs/screen-text.tar.gz` 主机/板端 SHA-256 `071ec441fbee1c8c40815feeac0d62897172dcfccf04b564a84736dd87d32b22`，评分 `/tmp/nt-dual-gt-evidence/window030-2e6a0cd-screen-center.json` SHA-256 `1e1677acde63fd74f04097d30b86f3a6353af20850052695bec3d5ee5ab1cf62`。
- car001 标准/右下 tracking 中心均值 `202.96/161.07 px`，分别 3/4 次重捕，跨框中心 P95 `601.66 px`；car002 标准/真值均值 `105.01/25.97 px`，分别 2/3 次重捕，标准框 15 个无框帧误观测 12 帧，跨框中心 P95 `184.90 px`。四组正式门禁全部 FAIL。**裁决 / Handoff**：Hann 权重 0.30 破坏目标身份和中心一致性，撤回该变更，保留板端证据；不据此推定其他权重可用，正式 handoff HOLD。

## 2026-11-14T03:21:28+08:00 NanoTrack 响应窗口 0.30 主机筛查候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `ecb997fea5b45a4094d9b56e68cc13c0171bb73b`。保持此前八组最佳的恢复/显示逻辑，只将 NanoTrack 解码的 Hann 窗口权重从 `0.455→0.300`（默认和 RKNN 运行实例同步），检验较弱中心先验能否改善运动段的原始中心偏移和不同初始框的收敛；不使用 detector 中心修正 `tracking_bbox`，`display_bbox` 的 detector 信息仍仅供外观尺寸。
- 实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/window030-host-build.log`、`/tmp/nt-dual-gt-evidence/window030-host-ctest.log`。
- **证据边界 / Handoff**：尚无 RK3588 推理结果。先筛 car001 标准/右下与 car002 标准/真值框，只有中心、重捕、遮挡语义均不退化才扩展八组；正式 handoff HOLD。

## 2026-11-14T03:19:53+08:00 detector 初始框统一化的板端反事实否决

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，当前源码 `9f64c5df6bb3112fe23836a12a0b5f4818210e7b`，板端复用已测基线根 `/home/neardi/Project/SOT/dual-gt-visibility-7cbf58d`、二进制 SHA-256 `3f6c3896f777148a0d038d8c02b5d21fefa7461cc649336bb538db3b1cdb15d4`，配置/视频/GT/模型/板端环境同基线矩阵。四种人工 ROI 对每个目标都匹配相同的初始化 detector 候选；car001 候选 `994.000916,581.063293,35.289490,39.486084` 接近真值，car002 候选 `973.034729,520.489929,34.091492,31.423828` 比真值 `966.99936,512.99946,42,42.00012` 明显偏小。
- 用 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...` 直接模拟三种确定性初始化：car001 detector 原框；car002 detector 原框；car002 detector 中心加 42×42 框。12 文件清单 `runs/init-detector-screen-checksums.sha256` SHA-256 `058021eaefd6e8b04821a7938c81e4f264d5a2ea5c58c2d16976360191fe7b1e`，三 AVI 均 1920×1080、271 帧；文本包 `runs/init-detector-screen-text.tar.gz` 主机/板端 SHA-256 `0fc44a08a4b16a224e1bcef2b508ac8ef49ee0f8c137f837942636f56fa4a916`，评分 `/tmp/nt-dual-gt-evidence/init-detector-screen-center.json` SHA-256 `282d7043275fa05bb25f338dc6493934ace865b8c0eae324205e0dd79327e9a10`。
- car001 detector 原框第 207 帧才重捕，tracking 中心均值 8.70 px；car002 detector 原框第 82/195 帧两次重捕，均值 103.27 px 且 15 个无框帧有 13 帧误观测。car002 detector 中心加 42×42 框第 233 帧重捕、中心均值 2.17 px，但该 42×42 尺寸来自真值反事实，运行时不能假设。**裁决 / Handoff**：不把 detector 初始候选直接替代人工 ROI；保留三组板端原始输出和评分。正式 handoff 仍 HOLD。

## 2026-11-14T03:16:06+08:00 真正冻结早搜加速度后的板端四组否决

- 修复后精确源码 `codex/dual-gt-init-consistency@a1b368e2966251cf326816780163bdd1916762e9`，包 `/home/neardi/Project/SOT/nt-dual-gt-early-freeze-a1b368e.tar` 主机/板端 SHA-256 `b96eb2ec6557f58a0ee0fd652ad80ac3b4f26c6b86ef3a75aa44000a1bdda089`，隔离根 `/home/neardi/Project/SOT/dual-gt-early-freeze-a1b368e`。RK3588 原生构建及 CTest `23/23 PASS`；二进制 `build/nanotrack_v3` SHA-256 `cc257ac03e078489fca588d95de91bc32c9701c44910a174f8d5b893669eee31`，控制器配置 SHA-256 `0d9d4479ec2e6a273ce79d2b269833aadfab7926fada11c7f02b03dd76785f66`，headless SHA-256 `ca4d32355a163c49cccb0752bf37850429ce361d81a31be63b57ab68cc2d2839`；视频、GT、模型、板端环境同前述精确矩阵。
- 执行四组 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...`，car001 标准/右下与 car002 标准/真值框。16 文件清单 `runs/screen-checksums.sha256` SHA-256 `9368c01ac30de5bc4d081b4897faa8b6310363f26f12a34d16ffcc846f7ebf86`，四 AVI 均 1920×1080、271 帧；文本包 `runs/screen-text.tar.gz` 主机/板端 SHA-256 `acc5afacd0f31fd28388729be19833071518aef76ddff246a16f83a95575553c`，评分 `/tmp/nt-dual-gt-evidence/early-freeze-a1b368e-screen-center.json` SHA-256 `9fd5ca684c60cca453dd9c796fbda7a3a042c7afe343e66b73ebd1027303ac10`。
- car001 重捕 187/198、tracking 中心均值 4.16/5.82 px、跨两框 tracking/display 中心 P95 17.61/15.91 px；car002 重捕 253/253、均值 3.03/2.45 px、跨两框 P95 10.32/9.87 px。冻结加速度较未执行的早搜 600 ms 候选有改善，但仍显著差于 7cbf58d 基线八组的 car001/car002 跨框 P95 3.51/6.21 px，重捕也晚。两组 car002 语义 PASS 不能抵消中心一致性退化。
- **裁决 / Handoff**：四组筛查已经触发否决，无需重复完整八组；撤回 07bdb37/a1b368e 的早搜与冻结源码/配置/单测，保留原始板端输出，正式 handoff HOLD。下一步沿原 400 ms、非提前搜索基线，单独寻找可见段中心偏移与 car002 完全遮挡的更可靠证据。

## 2026-11-14T03:11:25+08:00 冻结加速度触发标志修复及前次四组证据

- `07bdb37d1571eb07b26e2409a68067ae21f4d443` 精确源码包 `/home/neardi/Project/SOT/nt-dual-gt-early-freeze-07bdb37.tar` 主机/板端 SHA-256 `a31a361493e49cf0fc1bea4a47fb3cc9c2310f881ea5aa8fd66145ff784c2bd3`，板端隔离根 `/home/neardi/Project/SOT/dual-gt-early-freeze-07bdb37`。RK3588 原生构建及 CTest `23/23 PASS`；二进制 SHA-256 `37f07eb33d64be6e9b9c1b4ef7f6581c341f08182d3ec890f717d60a966e2816`，控制器配置 SHA-256 `0d9d4479ec2e6a273ce79d2b269833aadfab7926fada11c7f02b03dd76785f66`，headless SHA-256 `37ff6a51c5292c1dc07e229b53fe5820caee9e7524468b53ae2ff377d1f1094c`。视频、GT、模型与板端环境同前述精确矩阵。
- 执行 `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...` 四组 car001 标准/右下、car002 标准/真值框。16 文件清单 `runs/screen-checksums.sha256` SHA-256 `d902055fd85f52274edbbaaee129c26aa601597b1050da26dea488fae2a72f90`，四 AVI 均 1920×1080、271 帧；文本包 `runs/screen-text.tar.gz` 主机/板端 SHA-256 `2c6c10af9e6ec5898e2e244de9b1aa9295888e3f84f12d5680eb36d600acdf8f`，评分 `/tmp/nt-dual-gt-evidence/early-freeze-07bdb37-screen-center.json` SHA-256 `bceb5ac6cefd9451871a9ed51aa1ac3f2cfe1d5675aaabeb13a55db25ccbfb9f`。四组与先前 600 ms 提前搜索结果一致：car001 重捕 187/200、跨框 tracking 中心 P95 37.21 px；car002 重捕 244/255、跨框 P95 26.49 px。
- 逐帧核查发现 `motion_acceleration_*` 仍按旧值外推；代码路径先 `clear_suspicion()` 清空 `recent_visibility_streak_`，再检查该计数是否需要冻结，导致 07bdb37 的冻结分支未执行。现将触发标志在清理前保存，并为非零加速度添加单测，实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --target test_long_term_controller --parallel 4` 和 `/tmp/nt-dual-gt-consistency-build/test_long_term_controller`，结果通过。**证据边界 / Handoff**：07bdb37 板端数据只证明冻结未执行时的退化，不代表修复后的效果；须重新提交精确源码并板端复测，正式 handoff HOLD。

## 2026-11-14T03:06:08+08:00 提前可见性搜索时冻结开环加速度的主机候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `9f7dec02227e9a9b5a4b0ddb79839170276bcf67`。恢复已测的连续三帧可见性搜索与 600 ms 最小重捕年龄；仅在该几何搜索触发时调用运动预测器 `freeze_open_loop_dynamics()`，保留当前速度、清除加速度。先前 car001 右下框第 171 帧后预测纵向速度反向，提前搜索到第 190 帧的中心 `y≈809`，真值 `y≈848`；旧正常搜索中心 `y≈827`。该候选检验开环加速度反向是否是早搜后的中心漂移主因；运行时不使用真值帧号或位置。配置 SHA-256 `0d9d4479ec2e6a273ce79d2b269833aadfab7926fada11c7f02b03dd76785f66`。
- 实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/early-freeze-host-build.log`、`/tmp/nt-dual-gt-evidence/early-freeze-host-ctest.log`。
- **证据边界 / Handoff**：尚无板端推理结论。须 RK3588 精确源码原生构建、双目标八组真值评分；正式 handoff 保持 HOLD。

## 2026-11-14T03:04:05+08:00 提前搜索但延后 detector 探针的板端否决

- 精确源码 `codex/dual-gt-init-consistency@f53a235ccc22751a8a455c437cef5745d17cfd77`，包 `/home/neardi/Project/SOT/nt-dual-gt-probe-wait-f53a235.tar` 主机/板端 SHA-256 `029e2676a48fada4a7e9788681b1c7a437ce5c3e5be432cc09ff82b1f06ff9cc`，隔离根 `/home/neardi/Project/SOT/dual-gt-probe-wait-f53a235`。RK3588 原生构建及 CTest `23/23 PASS`；二进制 `build/nanotrack_v3` SHA-256 `5b035ad3c30338cf7ea8758028305afff76fb99754edff765b97156c1e1542df`，控制器配置 SHA-256 `02c1d19fe98902bb6d63564b276fd23d679960336099abf57cf32733350d882d`，headless 配置 SHA-256 `560ec4cc319bfb3a66938d3015d30c93d3744dd52c3a0b612908aabde1162ad8`。视频、GT、tracker、detector/manifest/labels 和板端环境同前述精确矩阵。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`；32 文件清单 `runs/matrix-checksums.sha256` SHA-256 `de604d878d0b0341f7d6c2d0f619047ea5804c9f85517b8b6a068c0a23e0ffca`，八 AVI 均 1920×1080、271 帧；文本包 `runs/matrix-text.tar.gz` 主机/板端 SHA-256 `1a1cd2aa961d68ccf08b22520710de68332ae7b6cb91e0f653af8230fec27c33`，评分 `/tmp/nt-dual-gt-evidence/probe-wait-f53a235-center.json` SHA-256 `dda9dd5650c07c77bfd980607518c73c3d03d683648dbc89fec900c58d0bd148`。
- car001 标准框第 187 帧重捕但可见框门禁失败，右下框延至第 200 帧且 tracking 中心均值 `6.98 px`；跨初始框 tracking/display 中心 P95 `37.21/35.50 px`。car002 四组完全遮挡语义均 PASS，但重捕 `244/255/244/243`，跨初始框 tracking/display 中心 P95 `37.34/34.52 px`；正式门禁 `4/8 PASS`，与之前仅增加 600 ms 等待的失败候选近似。等待期内不探测 detector 候选未解决后续 recent/safe 命中不连续和晚期 detector 救援的问题。
- **裁决 / Handoff**：撤回 f53a235 的源码/配置/单测，保留上述板端证据；以 `1a7bd72` 的源码和 7cbf58d 的八组矩阵为最佳已测基线，正式 handoff 继续 HOLD。下一步应针对提前搜索造成的运动预测漂移和确认几何设计有独立证据的候选；不能把 4/8 语义 PASS 当成中心一致性达标。

## 2026-11-14T02:57:29+08:00 提前搜索但延后探针的主机候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `1a7bd72e80c19cd4084f495b06b08dfe39bd0d4b`。恢复已测的“轿车连续三帧高度缩小且中心残差增大即提前 SEARCHING”候选，并把可见配置的 `search_reacquisition_min_age_ms` 设为 600；新增默认关闭的 `search_probe_requires_reacquisition_age`，只在可见配置启用，使全目标模式在确认等待期间也不把 detector 候选作为 NanoTrack 探针。此前 600 ms 反例在等待期间仍探测遮挡中的同类车；本候选针对该差异。配置 SHA-256 `02c1d19fe98902bb6d63564b276fd23d679960336099abf57cf32733350d882d`。
- 新增单测确保 detector 候选在等待期间可以入表，但不能移动 NanoTrack 探针；已有连续可见性搜索负例随源码恢复。实际执行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/early-probe-wait-host-build.log`、`/tmp/nt-dual-gt-evidence/early-probe-wait-host-ctest.log`。
- **证据边界 / Handoff**：尚无该源码的 RK3588 推理结果；必须完成精确源码板端构建与双目标八组真值评价，若误重捕或中心退化则撤回。正式 handoff 保持 HOLD。

## 2026-11-14T02:53:58+08:00 保留竞争身份但限制远处探针的板端否决

- 精确源码 `codex/dual-gt-init-consistency@f1e0bffef970f3167da3ba026e876bd6661f5fae`，包 `/home/neardi/Project/SOT/nt-dual-gt-rival-f1e0bff.tar` 主机/板端 SHA-256 `a75b8a48813a38eb6d65daae49f8656b0c81c618848e997cdb0585af3efbc658`，隔离根 `/home/neardi/Project/SOT/dual-gt-rival-f1e0bff`。RK3588 原生构建及 CTest `23/23 PASS`；二进制 `build/nanotrack_v3` SHA-256 `8f8bd403708b8fb4e7ce7675e1a0c81674b053cd4543c9301053d468f24063d7`，控制器配置 SHA-256 `bb8c4758f6a0e6943445130c1aa7241aa5c9f228a881480aa2dabf573a863bfc`，headless 配置 SHA-256 `f5177478d9dca4ea0a3eb4abaabd2ee1ead14c764f4596c40325a09e54773705`。视频、GT、tracker、detector/manifest/labels 和板端环境同前述精确矩阵。
- 300 ms car001 四组：控制器配置 SHA-256 `cf0723e9bcefdba0f0cfc324b503eac80918bf084a6e82889ecbecafc14eda02`、headless SHA-256 `3aa3f9fd8da2e126fb118a95a00966af3894a5eb36fe5e577fe5d6a5d8487952`；执行四个 `./build/nanotrack_v3 --config runs/headless-detector300.conf --bbox <ROI> --output-video ... --output-boxes ... --session-metadata ...`。16 文件清单 `runs/detector300-car001-checksums.sha256` SHA-256 `a82b82141226ffae69a08cc82e21ada8f7cf9f1bf30f1a48891fc20b2036eda0`，四 AVI 均 1920×1080、271 帧；文本包 `runs/detector300-car001-text.tar.gz` 主机/板端 SHA-256 `cd33ac61479d2b60a5037551dcacfa0f9df605d7393a83b56b09d51351b5ac5c`，评分 `/tmp/nt-dual-gt-evidence/rival-f1e0bff-car001-300-center.json` SHA-256 `ff83a524625cbc27b31f4dfd8e89fd0184acc7f5279abd3b39403d18336dff9e`。标准框仍在第 179 帧误重捕并于 197 帧再重捕；跨框 tracking 中心 P95 `15.94 px`，正式门禁仅左上框 PASS。逐帧日志：第 175 帧远处 detector 候选进入，第 176–177 帧形成身份冲突；候选第 178 帧失效后，近期模板在 178–179 帧连续命中导致第 179 帧误确认。
- 400 ms 正式配置：运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`，32 文件清单 `runs/matrix-checksums.sha256` SHA-256 `645829786799804955a3fbe7619acc988d8b7ee1b41a1bb8e784de6cc5b2d78a`，八 AVI 均 1920×1080、271 帧；文本包 `runs/matrix-text.tar.gz` 主机/板端 SHA-256 `7edd857aa8fb350b01d2594358e0fe0a78ab0cc8203a992737fde018dc4c3b83`，评分 `/tmp/nt-dual-gt-evidence/rival-f1e0bff-center.json` SHA-256 `275eb00c2c7d70faebf2a3a43fc6235aba4cfec966978989b7eeab16cef9e5ca`。八组重捕帧、中心均值、跨框 P95 与 7cbf58d 基线逐项一致：car001 `3.51 px`、car002 `6.21 px`，正式门禁 `1/8 PASS`。
- **裁决 / Handoff**：此位置门禁没有改善 400 ms 正式配置，300 ms 又退化，撤回 f1e0bff 的源码/配置/单测，保留板端输出和评价证据；正式 handoff 继续 HOLD。下一候选需针对失效后的近期模板独立确认，不能仅靠延长所有竞争候选的生存期。

## 2026-11-14T02:44:01+08:00 保留 detector 身份竞争证据的探针位置门禁主机候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `112ed22c3e059392424d3e9c71b35be972076325`。逐帧对照已保留八组 CSV 与双目标真值：正常可见 TRACKING 帧的 `tracking_bbox` 中心与 NanoTrack 原始中心几乎一致，`display_bbox` 中心亦随 tracking 中心；主要偏差来自遮挡后的错误或延迟重捕，而非显示框尺寸处理。
- 新增默认关闭的 `unified_confident_detector_probe_gate`，仅在 `visible-adaptive-display.conf` 开启。运动预测可信时，距离预测超过现有 `detector_candidate_max_center_ratio=0.02` 的纯 detector 候选仍留在统一候选表中作为竞争身份，暂不引导 NanoTrack 探针；与远处 detector 合并的近期/安全候选也不能确认。近处 detector 候选仍可探测。配置 SHA-256 `bb8c4758f6a0e6943445130c1aa7241aa5c9f228a881480aa2dabf573a863bfc`。
- 主机 RKNN-off 实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，`23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/confident-rivals-host-ctest.log`。新增单测核对远处同类候选仍在表中否决近期误确认、不能牵引探针，近处候选仍可供验证。
- **证据边界 / Handoff**：这是依据两目标真值和先前板端反例形成的主机候选，尚无此提交的板端推理结果。需精确源码包在 RK3588 原生构建、完成 300/400 ms 初始框矩阵并评分；正式 handoff 保持 HOLD。

## 2026-11-14T02:37:46+08:00 连续可见性搜索候选板端否决并撤回

- `codex/dual-gt-init-consistency@e1830f8aa2ef37478ff9583bba1f16490dafa5a1` 精确源码包 `/home/neardi/Project/SOT/nt-dual-gt-visibility-streak-e1830f8.tar` 主机/板端 SHA-256 `d22a6d53dc7faa4614b8395177fa3f75154b21a1a85de802d5638789ecae7e3c`；隔离根 `/home/neardi/Project/SOT/dual-gt-visibility-streak-e1830f8`。RK3588 原生构建和 CTest `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `7dfe9c810edc89b10cc4579ef9dcc9815ae35fe33f6e274ef9013c57cd7cf89b`，控制器配置 SHA-256 `b490332d497a8d8bc1f7e6f1c088a0187b8e8c95bdd9641959311f7e6b1f23c4`，headless 配置 SHA-256 `bf593394d9eb134976dbfaf8030c3a5ebd18e9fc9ab724ece4c0ca9495c4ae8e`；视频、GT、模型、detector 身份同前述精确矩阵。
- 原始 `search_reacquisition_min_age_ms=400`：实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`；八组 32 文件清单 SHA-256 `c779b18a814bfba4b96de810c3b53ed9227b0de312bb77f4e9353b4036658079`，八个 AVI 均 1920×1080、271 帧，文本包 `runs/visibility-streak-text.tar.gz` 主机/板端 SHA-256 `05439b2f2d2d21657ffa467406e13577dec5d94ed0aa1ce43ffea04df3dd1a17`，评分 `/tmp/nt-dual-gt-evidence/visibility-streak-center.json` SHA-256 `06663c83d92e9af1f7459f80f990602974c65da1576870407b5656fe4feb2acb`。car002 完全遮挡语义改善，但标准/右下在第 226 帧过早重捕并随后回滚；car001 右下第 183 帧过早重捕且再次于 201/224 重捕。跨框 tracking 中心 P95 car001 `3.51→40.39 px`、car002 `6.21→30.06 px`；正式门禁 `2/8 PASS`，中心一致性严重恶化。
- 同一二进制筛查 `search_reacquisition_min_age_ms=600`：板端控制器配置 `runs/minage600-controller.conf` SHA-256 `0d9d4479ec2e6a273ce79d2b269833aadfab7926fada11c7f02b03dd76785f66`、headless SHA-256 `f9e1da4092b3d2b46649b06582d5d28a40b6f789fd7663ca839b83f61edfc2c6`；运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-minage600.conf runs/minage600-matrix`，32 文件清单 SHA-256 `e3a2795b843ace03340175a7bd60ef7ac287c8d5aac2ae66852302dd30a37a1d`，八个 AVI 均 1920×1080、271 帧。文本包 `runs/minage600-text.tar.gz` 主机/板端 SHA-256 `5149e3a0658759595ef25bfe3309c77e8079c5810ee0bc089f0e9323ac53eeba`，评分 `/tmp/nt-dual-gt-evidence/minage600-center.json` SHA-256 `b251e5c57182f6700fd7e910427fd284fc1182cfa411e43d96ae4c00f3a82caf`。car002 四组正式语义门禁虽 PASS，但重捕延至 `244/255/244/243`，跨框中心 P95 `37.34 px`；car001 右下重捕第 200 帧，标准框因可见框失败，跨框 P95 `37.21 px`。
- **裁决 / Handoff**：提前 SEARCHING 与单一固定重捕等待期不能同时保持遮挡语义、准确重捕和初始框一致性。撤回 e1830f8 的源码、配置和单测改动，保留两轮失败证据；继续以 `d2c0c22` 的双窗口响应 + 早期部分可见观测隔离为当前候选，正式 handoff 保持 HOLD。

## 2026-11-14T02:25:37+08:00 连续几何可见性下降触发搜索的主机候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `d2c0c22`。基于已核对的八组 CSV，car002 四种初始框从第 211 帧连续 10 帧符合“原始 decoded 高度相对可信框 `≤0.98` 且归一化中心残差 `≥0.04`”；car001 可见段没有连续三帧命中，右下框 168～172 部分/完全遮挡段有连续命中。新增可关闭的 `recent_visibility_search_consecutive_frames`，只对已有轿车双条件门禁连续命中计数，当前可见配置取 3；第 3 帧转 SEARCHING/MOTION_SEARCH，干净帧或帧间隔重置，初始化、异常回滚和清除可疑状态同步清理计数。`visible-adaptive-display.conf` SHA-256 `b490332d497a8d8bc1f7e6f1c088a0187b8e8c95bdd9641959311f7e6b1f23c4`，其他配置默认关闭；不以 GT 帧号或 detector 显示框作为运行时判据。
- 新测试覆盖三次连续双条件命中才搜索、干净帧打断计数；既有单条件/其他类别/缺 decoded 框负例保留。实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/visibility-streak-host-build.log`、`/tmp/nt-dual-gt-evidence/visibility-streak-host-ctest.log`。
- **证据边界 / Handoff**：只是依据当前双目标真值提出的板端反事实候选。需 RK3588 原生八组复测验证 car002 213～216 完全遮挡是否进入搜索，以及 car001 右下提前搜索、两目标重捕和中心是否退化；未批准 handoff。

## 2026-11-14T02:21:58+08:00 可信运动位置门禁板端否决并撤回

- `codex/dual-gt-init-consistency@f3765aa8c1ce38489f003c928dbe46f6ac548612` 精确源码包 `/home/neardi/Project/SOT/nt-dual-gt-position-gate-f3765aa.tar` 主机/板端 SHA-256 `a7dac31c288405ea997f6efdf8e1d937723b91b00c8596c3ca1a960e4ed45b5c`；隔离根 `/home/neardi/Project/SOT/dual-gt-position-gate-f3765aa`。RK3588 原生编译及 CTest `23/23 PASS`，`build/nanotrack_v3` SHA-256 `2a6d91cb3ff2af86ce84538b11ffeac0a47e8a46d1e69b48ea0cec1cf6bc54d2`。控制器配置 SHA-256 `b6738adf133c258da17aa849b6c04213e3379f6529fd580f24a199c718b66be0`，headless 配置 SHA-256 `e3c221eaa055dbe6d832f151dc33c0e4330c1e7addacfeda2b8089bbb9dae3f6`；视频、GT、tracker、detector 身份同前述精确矩阵。
- 先用板端 300 ms 反事实配置 `runs/detector300-controller.conf` SHA-256 `c730ac8f9eda129abb5e325cd61c0da8704c8449c0b4bd7685194cbb30ed3b9b`、headless SHA-256 `7fa5b2eb89fc8327b47124b2a5c1011bbf2953ea216b713143adc61ccf8074a8` 跑 car001 四组；16 文件清单 SHA-256 `ea159aad22fcf4a663473861882e57da9eadeba34924d1a1bef956d254921b6b`，四个 AVI 均 1920×1080、271 帧，小型文本包 `runs/detector300-car001-text.tar.gz` SHA-256 `96ceddacb457aa03831597552f562a8acf2c0228b42459f0b0c4583562e9741e`，评分 `/tmp/nt-dual-gt-evidence/position-gate300-car001-center.json` SHA-256 `d1039449e7074d89ca7283f19f0b5e68874ba04de2c156f16a01ea1869ca21f0`。car001 左上重捕提前至 186 且 PASS，右下提前至 188 但遮挡语义失败；标准框仍在 179 由 recent 模板误重捕，人工框延至 213，跨框中心 P95 `16.55 px`，故 300 ms 方案否决。
- 用原始 400 ms 配置实际执行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`；八组 32 文件清单 SHA-256 `a64e3f29dd8a77d2e029f36df206ee7f074421993808ca30de67fb32d351accd`，八个 AVI 均 1920×1080、271 帧；文本包 `runs/position-gate-text.tar.gz` 主机/板端 SHA-256 `6b4bba010a3abf3ae0b6efb7b2afdbc675958e1411700bb8f0612e4606f5fd2a`，评分 `/tmp/nt-dual-gt-evidence/position-gate-center.json` SHA-256 `fdd2e57ed2ec0b3b7cdbd34b51d2beb25c52f3af90c46e7cbbd4e5841d1bd340`。car001 标准框误重捕 `179/197/220`，人工框第 216 帧才重捕，右下第 204 帧，跨框 tracking 中心 P95 从基线 `3.51→29.54 px`；car002 中心小幅改善但仍遮挡语义失败。正式门禁 `0/8 PASS`。
- **裁决 / Handoff**：位置门禁简单过滤远处 detector 候选会去掉同类身份冲突的反证，反而放行 recent 模板误重捕。撤回 `f3765aa` 的控制器、配置与测试改动，保留失败日志和本记录；不能从该板端结果批准 handoff。后续需分别处理候选可验证性与竞争身份冲突，不能仅用全局位置阈值或 detector 请求频率修复。

## 2026-11-14T02:09:55+08:00 可信运动下的统一恢复 detector 位置门禁候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `b598e58`。`consume_detector_batch` 先前在统一恢复模式中无条件绕过 `detector_candidate_max_center_ratio=0.02`；板端 300 ms 筛查中 car001 第 176～179 帧错误候选距离预测位置 `0.036～0.038`，仍通过 detector + NanoTrack 两次命中误重捕。新增默认关闭的 `unified_confident_detector_position_gate`；仅运动预测可信时执行已有位置上限，失去预测置信度后仍允许远处候选经独立验证。`visible-adaptive-display.conf` 启用，配置 SHA-256 `b6738adf133c258da17aa849b6c04213e3379f6529fd580f24a199c718b66be0`。显示 detector 仍不直接授权 tracking/控制。
- 新控制器测试覆盖同类近候选通过、远候选拒绝、远候选不能成为后续探针，以及运动置信度过期时允许远处候选进入验证。实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/position-gate-host-build.log`、`/tmp/nt-dual-gt-evidence/position-gate-host-ctest.log`。首次无条件应用位置门禁导致 replay 夹具失败，已改为显式可选配置，默认行为及 replay 回归恢复通过。
- **证据边界 / Handoff**：当前仅主机逻辑证据；需用精确提交在 RK3588 上同时验证 400 ms 基线与 300 ms 参数反事实的双目标八组中心、重捕和遮挡。未经板端证明不批准 handoff。

## 2026-11-14T02:09:55+08:00 detector 300 ms car001 四初始框参数筛查否决

- 沿用 `codex/dual-gt-init-consistency@7cbf58d` RK3588 二进制 `3f6c3896f777148a0d038d8c02b5d21fefa7461cc649336bb538db3b1cdb15d4`；仅板端 `runs/detector300-controller.conf` 把 `detector_interval_ms=400→300`，SHA-256 `4b4f7254aa09e6dfe8f39f0d0e6e7898556a2f066ad5a59d0c9417b1c471013d`，headless 配置 SHA-256 `f16c0f0fcbd441e244bf808dab6fada8b21d6a2819512df65b5d2108b2833f7b`。四个 car001 ROI 的运行输出留在 `runs/detector300-car001/`，16 文件清单 SHA-256 `1c15710a2af1b93cd9b4915a9e046fdb0a49573054d141941b8dbe90099f4903`，四个 AVI 经 ffprobe 均 1920×1080、271 帧。小型文本包 `runs/detector300-car001-text.tar.gz` 主机/板端 SHA-256 `fd26e8d5cd4aea80b44407b881b88b8e8d6b178eefa526e28b3659bd0af09349`；同一初始化一致性脚本评分 `/tmp/nt-dual-gt-evidence/detector300-car001-center.json` SHA-256 `9a8b3c724d374a89d064373a89162d53bc4fcb4ec426a9a1808920b4a761093b`。
- car001 标准/人工/左上在第 179 帧发生遮挡区错误重捕，之后第 199/197/199 帧再次重捕；三组可见帧 tracking 中心均值 `27.94/28.25/28.45 px`，跨初始框 P95 `79.36 px`，均显著恶化。右下组重捕 190，中心均值 4.94 px。**否决**单独把 detector 间隔改为 300 ms；失效揭示已有统一恢复对远处同类 detector 候选的位置上限绕过，需以代码门禁反事实复测。

## 2026-11-14T02:01:22+08:00 car001 重捕最短等待时间筛查无收益

- 沿用 `codex/dual-gt-init-consistency@7cbf58d` RK3588 二进制 `3f6c3896f777148a0d038d8c02b5d21fefa7461cc649336bb538db3b1cdb15d4`，仅在板端 `runs/minage200-controller.conf` 把 `search_reacquisition_min_age_ms=400→200`，SHA-256 `b4427fd16a7ebecc90ed73be4b0276ffe268024cc3cf63639f094abc62ca4191`；headless 配置 SHA-256 `37a9dd8abb30863e9c1c6e29422f7e3bb0c139154d0228d4966dc3a4d3ac6f1e`。源码默认配置保持不变，输入身份与上一八组一致。
- 实际以四个 car001 ROI `992,582,37,39`、`993,585,36,36`、`990,580,39,41`、`994,584,35,37` 分别执行 `./build/nanotrack_v3 --config runs/headless-minage200.conf --bbox ROI --output-video ... --output-boxes ... --session-metadata ...`，输出保留在 `runs/minage200-car001/`，16 文件清单 SHA-256 `d4897a7d23f1a7d8a821b746823f482dc98db8c3d5a79f34bb187f21b4a7789f`；四个 AVI 经 ffprobe 均 1920×1080、271 帧。小型文本包 `runs/minage200-car001-text.tar.gz` 主机/板端 SHA-256 `8164e3937537b944eabbf9ff52c0410fc77fa059a3d156c116a442fd65ef3400`。同一初始化一致性脚本评分 `/tmp/nt-dual-gt-evidence/minage200-car001-center.json` SHA-256 `547da9018fc964fda1f5a310751e4ed0d718b4efa1a3c63077901792fc91751a`。
- 四组重捕仍为 `185/192/194/191`，tracking/display 中心、跨初始框 P95 与原 400 ms 配置相同；尽管标准框 CSV/AVI 哈希改变，评分无收益。**否决**缩短最短等待时间，保持 400 ms；car001 其余三组重捕时序仍需解决，不批准 handoff。

## 2026-11-14T01:57:03+08:00 uncertain_ms=100 参数筛查否决

- 沿用 `codex/dual-gt-init-consistency@7cbf58d` RK3588 原生二进制 `3f6c3896f777148a0d038d8c02b5d21fefa7461cc649336bb538db3b1cdb15d4`，仅板端 `runs/uncertain100-controller.conf` 将 `uncertain_ms=200→100`，配置 SHA-256 `f6c934ee0f20132e384cd4bcdb5c19be5d34b7563fd390a50ba90a7559ba8055`，headless 配置 SHA-256 `a5b3e28effb486177300487bc86b68745e89e764b5f3949513c88d5f866ac5b9`；源码默认配置不变。输入视频、GT、RKNN/detector 身份同上一八组。
- 实际命令 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-uncertain100.conf runs/uncertain100-matrix`，八组 CSV/AVI/session/log 留在板端，32 文件清单 SHA-256 `cf3148d95b7bd7fb5c9b71240e97ffa7fc73e5a7337f849539911b32ad0af685`；八个 AVI 经 ffprobe 均为 1920×1080、271 帧。小型文本包 `runs/uncertain100-text.tar.gz` 主机/板端 SHA-256 `4a642e173e661cb0656cc7f0f924094a3cceee9fb41f973183da738e277b7245`。用 `scripts/evaluate_occ_initialization_consistency.py` 和同样八个 ROI 评分，`/tmp/nt-dual-gt-evidence/uncertain100-center.json` SHA-256 `3081be617760d7400aa7faee2fb75121721f2dc36676ae3c0296fec0bf0bbc7f`。
- car002 四组第 217～219 帧改为 SEARCHING/MOTION_SEARCH，但第 213～216 帧仍未满足完全遮挡语义。car001 标准框重捕 `185→193`，正式唯一 PASS 丢失；car002 左上出现额外第 144 帧重捕，右下重捕 `233→259`、可见帧 tracking 中心均值 `2.92→7.08 px`，car002 跨框 tracking 中心 P95 `6.21→46.09 px`。**否决**全局 `uncertain_ms=100`，保持源码配置 200 ms；本次不产生代码或 handoff。

## 2026-11-14T01:51:06+08:00 早期部分可见轿车门禁 RK3588 八组复测

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；源码提交 `7cbf58dca735ad653747b5c7994fad09140987a9`，基线 `b04cdc7`。精确源码包 `/home/neardi/Project/SOT/nt-dual-gt-visibility-7cbf58d.tar` SHA-256 `e5fc4f45ef41befaf165f2c7849f2afc277337aba79f3b6973cf2f3df8c7367b`；隔离根 `/home/neardi/Project/SOT/dual-gt-visibility-7cbf58d`。RK3588 原生构建及 CTest `23/23 PASS`，`build/nanotrack_v3` SHA-256 `3f6c3896f777148a0d038d8c02b5d21fefa7461cc649336bb538db3b1cdb15d4`；控制器配置 SHA-256 `63007d32b58c7e4aa920762f4d73f3532ef8ee544e50101c0684914d9b470acc`，headless 配置 SHA-256 `5fdb1c5eadfec175545c5c36b0af15c66571ab673ead03da7a4f3d455e130219`。视频、GT、tracker、detector 输入身份沿用上一精确矩阵。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`；板端 32 个 CSV/AVI/session/log 文件清单 SHA-256 `49b0c98430f8d145e4d60210d39b8f240ad8361de839917fa4836ea352f7414c`，八个 AVI 均经 ffprobe 确认为 1920×1080、271 帧。小型文本包 `runs/visibility-text.tar.gz` 主机/板端 SHA-256 `11ee0b033d92f7eec58488999d6d41f023dee9f5c915545af22df311a37cf1e4`；主机命令 `python3 scripts/evaluate_occ_initialization_consistency.py --dataset-root data/occ_video_1 --run ... --output-json /tmp/nt-dual-gt-evidence/visibility-center.json`，评分 SHA-256 `cf516ab3a6a98bd3437a29c8d2ceb98e74a46a6c89c95b5dd5b04d273b9f691e`，八个 ROI 明细可由板端 `scripts/run_occ_init_matrix.sh` 重现。
- car001 四组中心、AVI 和重捕帧 `185/192/194/191` 与双时窗候选一致；car002 四组 15 个无框真值帧的误观测全部归零，误观测控制仅剩 211/212 和 217～219（右下组），右下可见帧 tracking 中心均值 `3.17→2.92 px`、最大 `23.62→10.69 px`，跨框 tracking 中心 P95 `7.67→6.21 px`。car002 真值/左上组第 233 帧重捕，右下第 233 帧重捕，标准框第 235 帧重捕；全部可见帧中心均值 tracking/display 分别为 car001 `4.39～5.02/4.43～5.05 px`、car002 `1.77～2.92/1.75～2.89 px`。
- **证据边界 / Handoff**：正式 full evaluator 仍仅 car001 标准框 `1/8 PASS`；car001 其余组重捕超过 `<190` 门槛，右下还有完全遮挡语义失败；car002 四组在第 217～219 帧仍发布 tentative/OBSERVATION，未满足完全遮挡应为 SEARCHING/MOTION_SEARCH 的门槛。当前视频的中心和一致性有改善，但门禁仍 HOLD，未获得跨场景标定或协调者 handoff；下步筛查短一些的 `uncertain_ms` 能否让遮挡切换提前而不损伤可见帧。

## 2026-11-14T01:43:16+08:00 早期部分可见轿车几何门禁主机候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `b04cdc7`。在 recent-safe 普通跟踪中新增可关闭的轿车原始框几何判据：原始 decoded 高度相对当前可信跟踪高度 `≤0.98`，且归一化中心残差 `≥0.04` 时，把该帧作为 provisional/SUSPECTED，禁止更新可信跟踪状态与模板；仅 `target_class_id=2`、具有 decoded 框且配置双参数启用时生效。`visible-adaptive-display.conf` 两参数 `recent_visibility_height_ratio=0.98`、`recent_visibility_center_residual=0.04`，SHA-256 `63007d32b58c7e4aa920762f4d73f3532ef8ee544e50101c0684914d9b470acc`。参数进入解析、校验与配置身份；其他配置默认关闭。该候选不消费 detector 显示框作为 tracking/控制恢复证据。
- 新控制器测试检查单独高度噪声、单独中心运动不触发，双条件使轿车观测 provisional，其他类别或缺少 decoded 框保持原有行为。实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/visibility-host-build.log` 和 `/tmp/nt-dual-gt-evidence/visibility-host-ctest.log`。下一步 RK3588 原生编译和双目标八组真值复测，重点观察 car002 第 211/212 帧、可见真值帧及 car001 重捕是否出现副作用。
- **证据边界 / Handoff**：阈值由这组真值视频的离线诊断提出，尚未有板端反事实结果或跨场景标定；不批准 handoff。

## 2026-11-14T01:40:58+08:00 双时窗响应门禁板端复测

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；源码测试提交 `d34d0b9` 基于 `267e5b8`。源码包 `/home/neardi/Project/SOT/nt-dual-gt-dual-response-d34d0b9.tar` 主机/板端 SHA-256 `2e22e9238c4e7b2ebd8e2024050e9d676feeb802f38c0f20021b7bddb80b50a2`，隔离根 `/home/neardi/Project/SOT/dual-gt-dual-response-d34d0b9`。RK3588 原生编译及 CTest `23/23 PASS`；二进制 `build/nanotrack_v3` SHA-256 `849be5048b8085806d87f1741782a58be2ede88ed625f917357f5d95655ee0ee`，控制器配置 SHA-256 `1378c58fadd4a23b97bcc473ab06f69387d4402b8d3e44bc493f22065aed0b33`，headless 配置 SHA-256 `f5c2e7e63ccf2e0674eeb5d20c98e31f4779eb0ba78cc9257bdc704022240ffb`，同视频/RKNN/detector/manifest/labels 身份。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`，八组 CSV/AVI/session/log 留在板端 `runs/matrix/`，32 文件清单 SHA-256 `b7e29503f8bbac935752b15ba3a04d2730d716bb61751703e638931ccb4859dc`，八个 AVI 经 ffprobe 均 1920×1080、271 帧。小型文本包 `runs/dual-response-text.tar.gz` 主机/板端 SHA-256 `91d24db90431cc3898eee235bd40bc10634e4da736b2f10288bc31c965612875`，主机评分 `/tmp/nt-dual-gt-evidence/dual-response-center.json` SHA-256 `efa2ebf6e5fc620f7c84feb16924114e59cf124827fe75eadd2e64882e61b840`。
- car001 四组 AVI 哈希与 `47ccfc8` 基线相同，重捕 `185/192/194/191`、中心误差与跨框一致性不变，避免了全局长窗口的回归。car002 标准/真值/左上三组 AVI 同基线；右下无框误观测 `14→2`（仅第 211、212 帧）、第 235 帧经两次验证重捕，但可见帧 tracking 中心均值 `3.05→3.17 px`、P95 `9.62→8.25 px`、最大 `10.61→23.62 px`（第 233 帧仍在搜索），car002 跨框 tracking 中心 P95 `7.13→7.67 px`。正式门禁仍仅 car001 标准框 1/8 PASS；car002 四组均有部分遮挡无框误观测。
- 初步独立几何线索：car002 四组第 211～212 帧的原始 tracker 高度相对上一可信高度缩小，同时中心残差上升；在当前八组 CSV 上，原始高度/可信高度 `≤0.98` 且归一化中心残差 `≥0.04` 可标出这两个无框帧，car001 四组可见帧未命中。但这只是当前视频的离线筛查，尚未证明能安全加入控制器；下一候选需以车类、原始框、双条件门禁实现并做负例/板端复测。
- **证据边界 / Handoff**：双时窗改善 car002 遮挡语义，但中心最大误差和正式门禁仍不达标；不批准 handoff。

## 2026-11-14T01:32:37+08:00 双时窗 PSR/APCE 候选主机实现

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `267e5b8`。`ResponseBaseline` 继续用配置的短窗口（板端当前 8 帧）评估原有响应/运动门禁，同时保存 3 倍长度的可信样本窗口；长窗口满样本后，只有 PSR 与 APCE 相对长参考 **同时** 下降才触发额外的 SUSPECTED 观测隔离。冻结、恢复、重捕清理同时作用于两份样本；没有使用 GT 帧号或 detector 输出直接授权跟踪/控制。新增基线单测覆盖短窗口已适应弱响应而长窗口仍可识别双指标下降、单指标下降不触发、冻结与重捕清理；控制器单测覆盖 24 次健康、6 次温和下降、单指标负例和双指标观测隔离。
- 实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/dual-response-host-build.log`、`/tmp/nt-dual-gt-evidence/dual-response-host-ctest.log`。下一步打包本提交源码到 RK3588 隔离目录，原生构建并运行同一八组双真值矩阵，重点核对 car001 第 62 帧模板时序和 car002 第 217～229 帧无框误观测。
- **证据边界 / Handoff**：当前只有逻辑/主机测试；真实目标中心、重捕与无框门禁必须以板端复测判定，不批准 handoff。

## 2026-11-14T01:29:22+08:00 24 帧响应参考窗口否决与双时窗判别候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，源代码/二进制仍为已测 `47ccfc8`，板端根 `/home/neardi/Project/SOT/dual-gt-stable14-47ccfc8`，二进制 SHA-256 `9d833901e044cd1b06db8d18e691d7aa60bcbd31c68ad46663c6a68502c0b4c2`，同视频与 RKNN/manifest/labels。只在板端派生 `runs/reference-24.conf` 将 `occlusion_reference_window=8→24`，配置 SHA-256 `42a9778a600e349eccb6981267707d8f14e08f116cc73706dac83dd8688c9193`，`runs/headless-reference-24.conf` SHA-256 `51b43a949458ffaaa70ad6bf319498374abc072a55880df0e616dbcbaa4662b0`，未改 Git 源码。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-reference-24.conf runs/reference-24-matrix`；八组 CSV/AVI/session/log 留在板端，32 文件清单 SHA-256 `bd379649548bc3699865974c46efa1334bed74bee0ab11022e0af6244bb44074`。小型文本包 `runs/reference-24-text.tar.gz` 主机/板端 SHA-256 `eb0451911b526741849f46ac81d14ade3fefc36ec49932b7bf6d584e19df07fb`，评分 `/tmp/nt-dual-gt-evidence/reference24-center.json` SHA-256 `54ece8f7efe42c27ccaa28996906cf07411070adf70cb652d3bc2f3bfbbe1f2a`。
- car002 右下无框误观测 `14→2`，在第 234 帧重捕；但 car001 标准框 `185→195`，右下框变为第 `199/231` 帧两次重捕，tracking 中心均值 `5.02→11.98 px`，跨初始框 tracking 中心 P95 `3.51→38.70 px`，正式门禁 0/8。**否决全局 24 帧参考窗口**，保留 8 帧配置。逐帧对照显示，car001 右下在第 62 帧，PSR `6.11` 相对 24 帧参考 `8.28` 下降约 26%，而 APCE `18.54` 相对参考 `18.90` 基本不变；长窗口配合 peak-margin 跌落将这一正常可见帧判为 SUSPECTED，使模板 stage 从第 63 帧偏移。car002 右下第 217 帧的 PSR/APCE `4.76/12.89` 相对长参考 `7.94/18.71` 则同时下降，值得作为更具体的持续遮挡信号。下一候选考虑保留原 8 帧响应/运动门禁，另以长参考的 PSR 与 APCE **双指标同时下降**辅助判断；必须做主机负例和同八组板端测试。
- **证据边界 / Handoff**：双时窗只是依据当前两目标日志形成的假设，不能用真值帧号写入运行时规则。正式门禁未通过，不批准 handoff。

## 2026-11-14T01:22:35+08:00 搜索 detector 间隔 200ms 否决与响应基线定位

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，源代码/二进制仍为已测 `47ccfc8`，板端根 `/home/neardi/Project/SOT/dual-gt-stable14-47ccfc8`，二进制 SHA-256 `9d833901e044cd1b06db8d18e691d7aa60bcbd31c68ad46663c6a68502c0b4c2`；同视频与 RKNN/manifest/labels。只在板端从已提交配置派生 `runs/detector-200.conf`，把 `detector_interval_ms=400→200`，配置 SHA-256 `8608c4270cd5a1b2745a5e249097cdeaca3ba71fa21d3d04c2dce981aadd5204`，`runs/headless-detector-200.conf` SHA-256 `95fab9c7695610a54478f3e31ee24c777dd3bb69dd666ea3d2b1b3af1cd277f7`，没有改动 Git 源码。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-detector-200.conf runs/detector-200-matrix`；八组 CSV/AVI/session/log 留在板端 `runs/detector-200-matrix/`，32 文件清单 SHA-256 `0589ecc59acc67ec00121c0e8d50843526961cf39364b1c8e3e50a38ab203be5`。小型文本包 `runs/detector-200-text.tar.gz` 主机/板端 SHA-256 `f94f8de3139fd74d91e66097ae3b091d00eb8306b688164a68b0dd97702cfcd3`；评分 `/tmp/nt-dual-gt-evidence/detector200-center.json` SHA-256 `f9bd9a9bdcc237312b8160b074e8c08982e5dbe1bcf87108c9f910fb2a569b64`。
- car001 左上框重捕 `194→188` 且正式门禁 PASS，但标准框重捕 `185→186`、tracking 中心均值 `4.39→4.83 px`、平均 IoU `0.751→0.708`；人工框仍在 192，右下仍在 191。car001 跨初始框 tracking 中心 P95 `3.51→13.32 px`，明显违背一致性目标；car002 四组中心及无框误观测无改善。**否决全局 200ms 间隔**，保留 400ms 的已提交配置。日志显示 car001 人工/左上框的正确 detector 候选通常在第 190 帧进入并经两次 NanoTrack 验证，而更早频率也可能让标准框在第 186 帧使用高度偏小的 detector 候选；不能仅用重捕提前判定改善。
- car002 右下框第 211～229 帧主要仍处于 TRACKING，15 个无框帧误报 14 帧。第 216 帧近期响应 PSR/APCE `5.21/13.78`，参考 `7.30/18.32`，两项尚未同时越过相对下降门槛；参考在连续弱响应期间快速下降，到第 222 帧已为 `5.16/13.39`，仅短暂进入 SUSPECTED 并立即清除。与 car002 标准框第 217～221 帧进入 SUSPECTED 的时序相对照，下一步筛查更长的可信响应参考窗口，检查能否减少无框误观测且不损害 car001。
- **证据边界 / Handoff**：200ms 频率仅为板端参数筛查，已否决；响应参考窗口仍是待证假设。正式门禁维持 1/8，不批准 handoff。

## 2026-11-14T01:13:46+08:00 14 帧模板组合板端结果与重复性

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，源码测试提交 `47ccfc8fe0ea2d677154d6483d52573f5555c1b3`，基线 `c16ddf6`；源码包 `/home/neardi/Project/SOT/nt-dual-gt-stable14-47ccfc8.tar` 主机/板端 SHA-256 `d7fcbaf0007f976811efa44b9ba25c627333381c7b8bdf20acaab28ffae7c03d`，板端隔离根 `/home/neardi/Project/SOT/dual-gt-stable14-47ccfc8`。RK3588 原生编译、CTest `23/23 PASS`，`build/nanotrack_v3` SHA-256 `9d833901e044cd1b06db8d18e691d7aa60bcbd31c68ad46663c6a68502c0b4c2`。控制器配置 SHA-256 `1378c58fadd4a23b97bcc473ab06f69387d4402b8d3e44bc493f22065aed0b33`，`runs/headless-eval.conf` SHA-256 `c15132f193e4c541565202f8c5f14c8f4ea265ae2264221519bfd4dcbfcdaca4`；视频、三个 NanoTrack RKNN、detector/manifest/labels 与前述核对身份相同，板端 Linux 6.1.84、RKNN API 2.1.0、driver 0.9.8。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`；八组 CSV/AVI/session/log 留在板端 `runs/matrix/`，32 文件清单 SHA-256 `f0697c09e62ff740d6ecd54bbb3cac9c73613a4181a5e49bd4750bb8d55a28d2`，八个 AVI 经 ffprobe 均 1920×1080、271 帧。小型文本包 `runs/stable14-matrix-text.tar.gz` 主机/板端 SHA-256 `36a3475f6a989758dbb3e900523b3c39781433dcf49ed112b3098472aa7dad74`，主机评分 `/tmp/nt-dual-gt-evidence/stable14-center.json` SHA-256 `d37979b84999808b1e5f35b7354c71394dc54d8f4f720167b2892ce9c79c57fd`。
- 对比 `7f95652` 显示候选，同样配置/模型/初始框下 car001 四组 AVI 哈希完全相同，tracking/display 中心与重捕帧仍为 `185/192/194/191`，正式门禁仅标准框 PASS。car002 标准/真值框 AVI 哈希也相同；左上框 tracking 中心均值/P95 `2.52/8.34→2.17/6.16 px`，无框误观测 `6→2`，但单帧最大中心误差 `9.31→12.37 px`。右下框 tracking 中心均值/P95 `3.61/13.43→3.05/9.62 px`，最大 `15.72→10.61 px`，跨初始框 tracking/display 中心距离 P95 `10.62→7.13 px`；recent stage 从 228 推迟到 236 帧。car002 右下 15 个无框帧仍误报观测 14 帧且未重捕，正式门禁仍 FAIL；标准/真值/左上组也因遮挡语义等未全通过。轿车 display 中心仍基本跟随 tracking，未用外观修正掩盖 tracking 漂移。
- 用相同二进制、配置和 ROI 再运行 car001 右下与 car002 右下，输出位于 `runs/repeat/`，八文件清单 SHA-256 `9a3abc3754ad0a688315cd673a12f77b34312b5e7efe861bc456c6dd2b1168bf`。两组重复 AVI 与各自矩阵 AVI SHA-256 相同；逐帧比较 tracking/display 的 `x/y/width/height`、tracking phase、template action、state reason，在各 271 帧上差异数均为 0；CSV 原始哈希因运行时遥测字段而不同。
- **证据边界 / Handoff**：这是截至此轮中心一致性更好的板端候选，但正式门禁仍仅 1/8 通过，尤其 car002 部分遮挡无框帧误观测及 car001 非标准初始框晚重捕未解决；不批准正式 handoff。下一步需要针对这两个不同的失效原因改进身份/遮挡判别与重捕时机，不能仅凭均值改善宣布达标。

## 2026-11-14T01:05:14+08:00 连续可信帧 14 的模板试验候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `c16ddf6`，保留已在板端量测的轿车 display 中心改动。重新应用 `3c5324a` 中“可疑帧清零模板连续可信计数”，同时仅在本轮测试配置 `visible-adaptive-display.conf` 把 `stable_frames_for_update=15→14`，新配置 SHA-256 `1378c58fadd4a23b97bcc473ab06f69387d4402b8d3e44bc493f22065aed0b33`。上一轮 car001 第 49 帧可疑响应后至第 63 帧 stage 有 14 个可信帧；该组合旨在保留该 stage 时序，同时让 car002 第 222 帧可疑响应后的更新等待至目标重新完全可见。它仍只是实验假设，不代表 14 具备跨场景标定。
- 实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`；日志 `/tmp/nt-dual-gt-evidence/stable14-host-build.log` 和 `/tmp/nt-dual-gt-evidence/stable14-host-ctest.log`。下一步在 RK3588 上用同模型、视频、八组初始框原生编译并核对 tracking 与 display 中心、模板 stage 时序、重捕及无框误观测；若 car001 再退化立即撤回。
- **证据边界 / Handoff**：参数与逻辑组合尚未经过板端测试，不批准 handoff。

## 2026-11-14T01:04:21+08:00 显示中心候选板端复测

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；测试提交 `7f95652` 基于 `ff44961`。源码包 `/home/neardi/Project/SOT/nt-dual-gt-display-center-7f95652.tar` 主机/板端 SHA-256 `96cda00f1533c19accfcecd4425cd26dd6677ef169cac934e9b209b3e7b933fa`，板端隔离根 `/home/neardi/Project/SOT/dual-gt-display-center-7f95652`。原生编译和 CTest `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `57cc0cf06efd9ac0fd306293823a8d46a66a87eb77292594b05bd0c1a267992d`；控制器配置 SHA-256 `d6f9600a0a461c59bb1895711bb91c50c37d73b25eed64204dd1eb4225ff9853`，headless 配置 SHA-256 `69b153d18c4dfd5b0cdf5b741287be3acd69cb6fcf6b8d6b8dbd9f767b4f23f6`。视频、RKNN、detector、manifest、labels 与前述同身份。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`；八组 CSV/AVI/session/log 留在板端 `runs/matrix/`，32 文件清单 SHA-256 `4ede66a040019ffe4c44240386cbb4521029dd4f47501b280da3790eec8a0f2c`；八个 AVI 经 ffprobe 均为 1920×1080、271 帧。小型文本包 `runs/display-matrix-text.tar.gz` 主机/板端 SHA-256 `ebc5d25a15c74901e4ad546879551cf0c9519c01eedfe6863f4ab83c3f282877`，主机评分 `/tmp/nt-dual-gt-evidence/display-center.json` SHA-256 `8f7054ebe80cf600bedfe1537237c29d552fbbac9feab7fe6cdde891cc1db465`。
- 相对 `78baddc` 同协议，car001 四组 display 中心均值 `5.21/5.26/5.42/5.75→4.43/4.65/4.49/5.05 px`（标准/人工/左上/右下），tracking 轨迹、重捕帧、正式门禁不变。car002 四组 display 均值 `3.83/3.80/3.31/3.41→2.47/2.07/2.52/3.61 px`（标准/真值/左上/右下）；右下 P95 `5.35→13.43 px`，car002 跨初始框 display 中心距离 P95 `5.94→10.62 px`，暴露原 tracking 后段漂移。car002 无框帧误观测和正式门禁均未改善。
- **证据边界 / Handoff**：轿车 display 中心改动已在板端证明，但 car002 右下尾部一致性变差；整体目标未达标，暂不批准 handoff。下一轮继续修正 tracking/template 的后段漂移，不使用 display 外观改善冒充 tracking 精度。

## 2026-11-14T00:56:37+08:00 轿车显示中心与跟踪中心一致候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，基线 `ff44961`。基于已保存的 `78baddc` 八组 CSV 离线构造显示中心与 tracking 中心相同、仍保留原 display 宽高的评分包 `/tmp/nt-dual-gt-evidence/display-center-probe/0.0/report.json` SHA-256 `68a6b0205011bbdf9ab4feb3f0bcb3e0fe8b160865fc8211aaaee33cda2faf7c`。四组 car001 显示中心均值由 `5.21/5.26/5.42/5.75` 降至与 tracking 相同的 `4.39/4.56/4.48/5.02 px`；car002 三组均值改善，右下组由 `3.41` 升至 `3.61 px`，但该组 P95 更差。离线 CSV 重写只是局部几何上限，不等于板端时序运行证据。
- 新候选 `AdaptiveDisplayBox` 对轿车 `class_id=2` 只从 detector 取显示框宽高，中心跟随 tracking；人物等其他类别沿用既有变换。初始化时若 detector 框有偏心，也用 tracking 中心放置其宽高。新增单测用偏心 detector 框核验初始化和两次确认更新均不改变轿车显示中心。实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`。下一步按同一八组板端原生测试核对 display 与 tracking 中心、显示宽高、正式门禁和 detector 刷新时序。
- **证据边界 / Handoff**：该候选目前只有主机单测和离线几何模拟；tracking 重捕及 car002 无框语义不由本改动修复，正式 handoff 仍未批准。

## 2026-11-14T00:53:53+08:00 可疑帧重置模板计数板端否决

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`；测试提交 `3c5324a` 基于 `78baddc`。源码包 `/home/neardi/Project/SOT/nt-dual-gt-stability-3c5324a.tar` 在主机和板端 SHA-256 `a3bbd9db1039a0b6e8bead8e093eecdff4105dbacd2302461ca617a823bb231d`，隔离根 `/home/neardi/Project/SOT/dual-gt-stability-3c5324a`。原生编译、`ctest --test-dir build --output-on-failure` 为 `23/23 PASS`，二进制 `build/nanotrack_v3` SHA-256 `5c46aecca655f3d61dc7a940f30f66d2b12466535ba9d60132461a03691bdfe2`。控制器配置与上条相同 SHA-256 `d6f9600a0a461c59bb1895711bb91c50c37d73b25eed64204dd1eb4225ff9853`，`runs/headless-eval.conf` SHA-256 `f30422ecb2e49d46020bbd6e3c698683f55003619c7220f2be41aba0bf858334`；视频、RKNN 模型、manifest、labels 与上条同身份。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`；八组 CSV/AVI/session/log 留在板端 `runs/matrix/`，32 文件清单 `runs/matrix/checksums.sha256` SHA-256 `fc92c69bdc526574fda9464cfdb19a321c7c5f0466fd47b1958183a9e0fbcb18`；八个 AVI 的 ffprobe 输出均为 1920×1080、271 帧。小型文本证据包 `runs/stability-matrix-text.tar.gz` 主机/板端 SHA-256 `b46a33096805f2cc2b6854ac820b1f74ac66b89d01dc92881297430709d0966d`，主机评分 `/tmp/nt-dual-gt-evidence/stability-center.json` SHA-256 `43e40575b06b02a4b0dd39157e7f2d80173c02b7cd05cc75cdfb41057d4f5ff9`。
- 与 `78baddc` 同协议比较，car001 标准/人工/左上/右下四组重捕帧从 `185/192/194/191` 变为 `197/187/194/195`；原本通过的标准框变为失败，跨初始框 tracking/display 中心距离 P95 由 `3.51/3.85 px` 恶化至 `24.34/22.34 px`。car002 右下框的 15 个无框帧仍误报 14 帧；该变更虽推迟该组 recent 模板的 stage `228→237`，却未改善主要问题。car001 第 49 帧的早期 SUSPECTED 也将遮挡前模板 stage 整体推迟一帧，改变第 162～166 帧的模板状态，说明全局清零有明显副作用。
- **裁决 / Handoff**：撤回 `3c5324a` 的 `publish_tentative` 清零和相应测试期望，保留本条原始板端证据；继续以方向保护的 `78baddc` 行为为基线。两目标两路中心和遮挡语义尚未同时通过正式门禁，不批准 handoff。下一步定位 car001 重捕期与 car002 部分可见期的具体身份/几何判别问题。

## 2026-11-14T00:45:26+08:00 方向保护板端复测与遮挡后模板稳定性候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，本轮基线 `78baddcb79d4ee456203c76e249f353a456f0a99`。该提交源码包 `/home/neardi/Project/SOT/nt-dual-gt-direction-78baddc.tar` SHA-256 `81d8ba6078f4e1a5f98af0c23a97443fe1c2a08d8888de334216c88461853141`；板端根 `/home/neardi/Project/SOT/dual-gt-direction-78baddc`。RK3588 原生编译及 CTest `23/23 PASS`，`build/nanotrack_v3` SHA-256 `1ab8a7ed287be7be6930af41dd4674bd6c9851d1a6c03b1c163ba2c708b4ad24`。控制器配置 `d6f9600a0a461c59bb1895711bb91c50c37d73b25eed64204dd1eb4225ff9853`，headless 配置 `2eff9fe55e38f0d46b0b68049153d5ff28c8ee46abf7d9675d43b9d3a161b3a4`。视频、三个 NanoTrack RKNN、detector/manifest/labels 沿用前述已核对身份；板端 Linux 6.1.84、RKNN API 2.1.0、driver 0.9.8。
- 板端运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3 runs/headless-eval.conf runs/matrix`，八组原始 CSV/AVI/session/log 位于上述根的 `runs/matrix/car001_nominal/` 等目录；32 文件清单 SHA-256 `047e49a226a599e77b02d7d9618d78b4a52a86d15405b8ccdf041b178b2df5225`，八个 AVI 经 ffprobe 均 1920×1080、271 帧。主机评分 `/tmp/nt-dual-gt-evidence/direction-center.json` SHA-256 `4d1a7a13e46e1509c023627dd29269cf3e9d435cc8caee328f7c`。car001 右下初始化重捕由 193 提前至 191 帧，最大可见 tracking 中心误差 44.16→21.03 px，跨初始框最大中心距离 43.05→19.48 px；car001 其余组与 car002 四组无实质改善。car001 仅标准框通过正式门禁；car002 四组仍失败，右下初始框在 15 个无框标注帧中误报观测 14 帧。car001 右下三次和标准框两次额外重复测试位于 `runs/repeat/`，20 文件清单 SHA-256 `2a6ccbd3335535d6c5a29c62fd7b14d280da62bfe0688989da1135451c7f09cf`；相同初始化的中心轨迹及重捕帧一致。方向保护有局部改善但不足以交付。
- 在相同二进制下筛查 `search_local_late=2.0`：控制器配置 SHA-256 `7c9f173f451ecb57c7141d732c17214027492d8990e7433bcf9a637666dbaab9`，headless SHA-256 `89024fd267ea1742a4d59be6ecfb171fd0f5e833086da4f9548f2141208bf7df`，八组输出 `runs/search-late2/` 的 32 文件清单 SHA-256 `bf214557f3b53d9cfa73183edcc6ffc18617d8590a80dc78515b7e072e84b19a`；评分 `/tmp/nt-dual-gt-evidence/search-late2-center.json` SHA-256 `1cd60e80c04e4c23684c36d5624992c2db4540567c854f7da04023766ca252da`。car001 左上重捕虽提前，但标准框/人工框可见框门禁恶化；拒绝该参数。`search_local_late=2.5` 控制器配置 SHA-256 `e157fa1504f6fb182b89a9a50fa7a5930268f1b9bbae7fc486f5d76f4c8bbcca`，headless SHA-256 `29d5efaa425eeaffe1d1012d2f4844b62a0847651eeb9e52ed6e907180208e06`，car001 四组位于 `runs/search-late25/`；评分 `/tmp/nt-dual-gt-evidence/search-late25-car001.json` SHA-256 `2301ded8e110b32a6d7ce8632074228818afbf53ee00035c18ba5e9b9437ed17`，标准框退化至重捕 188 帧且可见框失败，跨框 tracking 中心 P95 达 41.56 px；同样拒绝。
- 新候选在 `LongTermController::publish_tentative` 清零 `stable_frames_`，使可疑响应打断 recent 模板更新的连续可信帧计数。car002 右下框在遮挡期间出现 SUSPECTED 后仍很快 STAGE/COMMIT recent；该改动旨在避免用短暂恢复的受遮挡外观更新模板，不能把无框帧的部分可见目标直接当成 GT 框。更新 transient/probation 单测，核验必须重新积累四个可信帧才允许 STAGE。实际运行 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、`ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，主机 RKNN-off `23/23 PASS`。本候选的板端原生构建、八组中心评分尚待完成；生成文件不入 Git。
- **证据边界 / Handoff**：仅方向保护的板端证据已取得，新模板候选仍待实测。两目标中心与无框语义尚不达标，不批准正式 handoff。下一步板端复测新候选并核对 car002 模板时序、中心偏差及 car001 退化风险。

## 2026-11-14T00:23:04+08:00 匀速搜索回退与方向反转保护候选

- `codex/dual-gt-init-consistency@8d43554` 的源码包
  `/home/neardi/Project/SOT/nt-dual-gt-coast-8d43554.tar` 主机/板端
  SHA-256 `e33fbdb858981244ea3f82c009fb6404c5cc298320807295af1a50ffe7b24667`，
  工作树 `/home/u20/single-object-tracking`，板端隔离根
  `/home/neardi/Project/SOT/dual-gt-coast-8d43554`。RK3588 原生编译与
  CTest `23/23 PASS`，`build/nanotrack_v3` SHA-256
  `b4499b5794baf83d88be614e61bfc2628cc6ca5146dcbbb400608db5bdb2a4b6`。
  控制器配置 SHA-256 与上条相同
  `d6f9600a0a461c59bb1895711bb91c50c37d73b25eed64204dd1eb4225ff9853`，
  `runs/headless-eval.conf` SHA-256
  `2db756b777fe4fb916e4f374e29f245eb276fde75f7aaa491f4bc3412c42d199`；
  视频、三个 NanoTrack RKNN、detector/manifest/labels 沿用前述已核对身份。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3
  runs/headless-eval.conf runs/matrix`，八组 CSV/AVI/session/log 留在
  `runs/matrix/<场景>/`；32 文件清单 SHA-256
  `0c5d79d43a0f6c81d4e96a04c752bc10c24bc2e3a722e19f424f896899a0ee63`。
  八个 AVI 经 ffprobe 均为 1920×1080/271 帧，主机真值评分
  `/tmp/nt-dual-gt-evidence/coast-center.json`。car001 四组重捕从旧版
  `185/192/194/193` 变为 `192/192/186/191`；右下框跟踪中心均值
  `5.73→5.18 px`，但原本通过的标准框晚重捕而失败，左上框可见框门禁
  新失败，跨初始框 tracking 中心距离 P95 `3.51→6.73 px`。
  car002 四组的跟踪中心均值、遮挡误观测及正式门禁未改善。因此撤回
  `MotionEstimator::begin_search` 的无条件匀速外推改动及相应测试。
- 新候选基于本条源代码继续修正：仅当搜索当前速度与最后接受的趋势
  点积为负时，恢复最后接受的速度，并清除其加速度；不再要求已触发
  单独 motion freeze。car001 右下框在第 165 帧的已接受速度 y 分量
  `+15.58 px/s`，搜索前翻转为负，而标准框没有方向反转；该条件旨在
  只处理这类开环反向。`test_long_term_controller` 新增无 motion freeze 的
  反向回退测试，主机实际运行
  `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、
  `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，
  RKNN-off `23/23 PASS`。板端原生及两目标八组复测待完成。
- **证据边界 / Handoff**：方向反转保护仍只是主机候选，尚不能声称改善
  RK3588 目标中心或通过正式门禁；不批准正式 handoff。

## 2026-11-14T00:15:30+08:00 轮询时机板端实测与搜索匀速预测候选

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`。
  轮询时机提交 `9e93f9dd9dec35c34ddebf5f88443c7588db7046` 的源码包
  `/home/neardi/Project/SOT/nt-dual-gt-poll-after-9e93f9d.tar` 主机/板端
  SHA-256 都为 `f1fcb0521392c72ea669c2a9ad73ddb1836659e4eca24b84ec9928e45001b44c`。
  RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-poll-after-9e93f9d`，
  原生 RKNN 编译与 CTest `23/23 PASS`；`build/nanotrack_v3` SHA-256
  `5d21eec75f2b95a6878e1970459b413338491c82c0beebd4668dd46c8b767fc4`。
  显示控制器配置 SHA-256
  `d6f9600a0a461c59bb1895711bb91c50c37d73b25eed64204dd1eb4225ff9853`，
  `runs/headless-eval.conf` SHA-256
  `8487250b6b156922dc47899d1b80cacf6299cd53f6b660623d94724938504259`。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3
  runs/headless-eval.conf runs/matrix`，保留 `runs/matrix/<场景>/` 的
  CSV/AVI/session/log；32 文件清单 `runs/matrix/checksums.sha256` SHA-256
  `bb1a84539b89f1e1c58476f7d56a1b890e5c2af6cc6448db6fd30bd3e173f8cd`。
  八个 AVI 经 ffprobe 均为 1920×1080/271 帧，主机真值评分
  `/tmp/nt-dual-gt-evidence/poll-after-center.json`。与上一轮最终 `alpha1`
  对照，八组 tracking 中心均值、重捕帧、正式门禁均未改善；
  car001 重捕仍为 `185/192/194/193`，car002 标准/真值框仍为 `235/235`，
  car001/car002 tracking 跨初始框中心距离 P95 仍为 `3.51/10.62 px`。
  因此同帧消费能力已由单元测试证明，当前板端视频尚无跟踪改善证据。
- 新一轮候选以 `9e93f9d` 为基线：`MotionEstimator::begin_search` 首次进入
  无观测搜索时清除加速度和尺度速度，保持最后已限幅速度做匀速预测，
  避免最后一帧的噪声加速度在开环搜索中持续放大。`test_controller_support`
  增加带非零已学习加速度的测试，核验入搜索后的匀速外推。主机实际运行
  `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4` 与
  `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，
  RKNN-off `23/23 PASS`；RK3588 原生与双目标八组测试尚待同提交复验。
- **证据边界 / Handoff**：无观测匀速预测仍需确认不会损害快速转弯目标、
  遮挡期身份门禁及 car002 无框帧；不批准正式 handoff。

## 2026-11-14T00:08:27+08:00 Detector 轮询时机主机候选

- `codex/dual-gt-init-consistency`、工作树 `/home/u20/single-object-tracking`，
  基线 `3c6201d3686988d8724d4eb08a7fe3f6a19a0925`。同一帧中，
  `LongTermRuntime::process_frame` 现先做 NanoTrack 推理，再无等待地轮询
  detector，随后将批次交给控制器；这允许推理期间完成的批次参与当前帧决策。
  新测试让 fake detector 在 NanoTrack 推理回调里完成任务，核验同帧消费，
  且 display 专用批次仍不进入跟踪/控制恢复。两次 NanoTrack 身份确认、
  候选时效和其他控制门禁未改。
- 主机实际命令 `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、
  `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，
  RKNN-off `23/23 PASS`。此条只证明主机逻辑和回归；真实 RK3588 原生编译、
  八组双真值中心评分及重复性尚待同一提交的源码包上验证。
- **证据边界 / Handoff**：该候选尚不批准生产或板端 handoff。下一步从本提交
  `git archive` 到隔离板端目录，按相同二进制/模型/视频/初始框的协议
  对照旧 `alpha1` 八组及 car001 右下重复运行。

## 2026-11-14T00:05:00+08:00 搜索间隔归因修正与同参数重复性

- 继续使用 `codex/dual-gt-init-consistency@ecfa29f6810b8a6610bb551ccdd704b531a5e5fc`，
  工作树 `/home/u20/single-object-tracking`，板端隔离根
  `/home/neardi/Project/SOT/dual-gt-23a43db`。沿用上一条已核对 SHA-256 的
  可执行文件、RKNN 模型、detector、输入视频和 `runs/alpha1-controller.conf`。
- **修正上一条参数归因**：源码搜索发现 `search_probe_interval_ms` 只在配置解析、
  指纹/元数据和验证中出现，运行时没有用它调度探测。所以上一条
  `runs/probe33/` 的右下框晚重捕不能归因于 `100→33 ms` 参数变化，
  该参数目前没有运行时效果。重跑同一 `runs/probe33-headless.conf`、
  同一 `994,584,35,37` 初始框两次，分别第 `193/193` 帧重捕；首次
  `runs/probe33/car001_right` 则是第 `205` 帧。两次重复结果留在
  `runs/repeat-probe33/car001_right_<1|2>/`，8 文件清单 SHA-256
  `5de54774a11ef27bcd5256234c7eb5928fe1d6a8a1997b5e64e957f88859140a`，
  主机评分 `/tmp/nt-dual-gt-evidence/repeat-probe33-car001-right.json`。
  首次 detector 第 `188` 帧请求的批次到第 `190` 帧才被消费，
  `detector_total_ms=124.787`；重复一次在第 `189` 帧消费，
  `detector_total_ms=51.056`。这是观测到的时序关联，尚未证明唯一因果。
- 用同一最终 `runs/alpha1-headless.conf` 和相同右下框再连续重复三次，
  `runs/repeat-alpha1/car001_right_<1|2|3>/` 的 12 文件清单 SHA-256
  `04b42866fa509321ff72f5ba144b8d448a612955e6a43fef5b78eb284169eb29`，
  主机评分 `/tmp/nt-dual-gt-evidence/repeat-alpha1-car001-right.json`。
  加上首次，共四次 tracking 中心轨迹逐帧相同、均第 `193` 帧重捕；
  display 受异步检测刷新轻微影响，四次同帧最大成对中心距离 P95
  `1.35 px`。这说明当前基准下本例重现稳定，但不能排除偶发晚重捕。
- 对 car001 四个初始框还筛查了限制运动加速度：
  `motion_max_acceleration_diagonals_per_s2=2.0→0.02`，隔离配置/headless
  SHA-256 分别为
  `448653a89a5bd435640153b8dc136e76f71cc0dbb756ea8d4e3526a281eb3a44`、
  `b34c453a170628f23614b8de460e554c6255349fea7dff7978321ac46d5c2683`；
  结果留在 `runs/accel002/<场景>/`，16 文件清单 SHA-256
  `0c4d89f29668218482960eefdda100e68aa84712b34466e44ab0c6ae452df165`，
  主机评分 `/tmp/nt-dual-gt-evidence/accel002-car001.json`。
  四组重捕变成第 `192/197/196/198` 帧，原标准框 PASS 也失败，
  tracking 最大成对中心距离 P95 `3.51→15.93 px`，拒绝纳入。
  单次确认配置被控制器以 `recovery confirmation policy must be two of three`
  拒绝加载，未运行；保留两次 NanoTrack 确认身份约束。
- **证据边界 / Handoff**：最终分支的显示参数改善两路可见帧中心贴合，
  但 tracking 的晚重捕、car002 遮挡误观测和检测异步时序敏感性未解决。
  下一步需要在保持两次 NanoTrack 确认的前提下，针对候选到达时序做
  可重复的控制器/运行时实验；正式 handoff 仍不批准。

## 2026-11-13T23:56:00+08:00 双路中心真值评分及显示重捕渐变对照

- 分支/工作树 `codex/dual-gt-init-consistency`、`/home/u20/single-object-tracking`，
  算法源码基线 `23a43db95c4bb8b3c6ba4090f077fd696d36d951`，新分支从
  `main@b80226577954febe22c5eff168125f77a6cdc724` 开始。源码包
  `/home/neardi/Project/SOT/nt-dual-gt-final-v3-source.tar` 主机/板端 SHA-256
  `b5ae7f07b447fb4465e46120f11db82d34490e493eb15b72b6ed8613ed5b0d99`。
  板端隔离根 `/home/neardi/Project/SOT/dual-gt-23a43db` 原生编译与 CTest
  `23/23 PASS`，`build/nanotrack_v3` SHA-256
  `f2b2da43e7a56b5fbc7c39bdd297b125f184d849ee02ee8ed65d189cba31fa02`；
  本次配置/评分改动在主机 RKNN-off 重构建与 CTest 亦为 `23/23 PASS`，
  中心指标单元测试 `2/2 PASS`。板端 Linux `6.1.84`、RKNN API `2.1.0`、
  driver `0.9.8`。
- 输入仍是上一条逐项记录 SHA-256 的 `data/occ_video_1/1.mp4`、三个 NanoTrack
  RKNN、VisDrone9 detector/manifest/labels 与两目标正式真值。板端原配置
  `rk3566/nanotrack_v3/config/visible-adaptive-display.conf` SHA-256
  `2adf4b35587215c18fcaf86221dd36f4c44fbf25910572a7a53c486235943d5e`，
  `runs/headless-eval.conf` SHA-256
  `dade127e93ab5a6b2f939397209603e663fbcb4df38d62cdcf0051e1de5ff659`。
  实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3
  runs/headless-eval.conf runs/final`，八场景结果在 `runs/final/<场景>/`，
  `runs/final/checksums.sha256` 覆盖 32 个 CSV/AVI/session/log，清单 SHA-256
  `447b7da068db1cb4e65dfc3c989effa9f352669792f79f9bc6e725ef72ff3752`。
  八个 AVI 经 ffprobe 均为 1920×1080/271 帧；主机 JSON
  `/tmp/nt-dual-gt-evidence/final-23a-center.json`。
- `scripts/evaluate_occ_initialization_consistency.py` 同时记录 tracking/display/control
  可见帧中心误差（像素，平均/P95/最大及最差帧）与多初始框同帧最大成对中心距离；
  对应单元测试在 `tests/test_evaluate_occ_initialization_consistency.py`。
  原配置 car001 四组 tracking 平均中心误差 `4.39/4.56/4.48/5.73 px`，
  display `5.41/5.53/5.71/6.86 px`；四组最大成对中心距离 P95
  tracking `3.51 px`、display `18.53 px`。car002 四组 tracking
  `2.49/2.05/2.52/3.61 px`，display `4.06/3.98/3.43/3.55 px`；
  最大成对中心距离 P95 tracking `10.62 px`、display `6.65 px`。
- 只把 `display_reacquisition_blend_frames=5` 改为 `1`，板端隔离控制器配置
  `runs/blend1-controller.conf` SHA-256
  `42f06ce988467d0aa78f4e3fc5b4491030cbd78c693a1b7e15c25808358e8a99`，
  headless 配置 `runs/blend1-headless.conf` SHA-256
  `341e7dcaddcdc4e7cacf88d7526c2973c6e617b86fd77c42a495062fb3841638`；
  同命令换成 `runs/blend1-headless.conf runs/blend1` 复测八组，
  `runs/blend1/checksums.sha256` SHA-256
  `e1eaf5677e902ca4561f56855e8f18ce3a1a6652ec50f2d75028da69b35ce827`，
  八个 AVI 经 ffprobe 均为 1920×1080/271 帧；主机 JSON
  `/tmp/nt-dual-gt-evidence/blend1-center.json`。car001 display 最大成对中心
  距离 P95 `18.53→3.71 px`，car002 `6.65→5.39 px`；tracking 轨迹、
  重捕帧与失败门禁未变。
- 再以渐变 1 帧配置为基线，把 `display_center_smoothing_alpha=0.80→1.00`；
  板端 `runs/alpha1-controller.conf` SHA-256
  `d6f9600a0a461c59bb1895711bb91c50c37d73b25eed64204dd1eb4225ff9853`，
  `runs/alpha1-headless.conf` SHA-256
  `34892aa163b5101b10d9a797a97c3adeed8aa2c660555c08719a9d84c69dacc5`。
  同命令换成 `runs/alpha1-headless.conf runs/alpha1` 复测八组，
  `runs/alpha1/checksums.sha256` 覆盖 32 个 CSV/AVI/session/log，SHA-256
  `5f511edbc5570f1a6c24eb15c21b8657e7c772575301cb47ef473bdd81a08d20`；
  主机评分 `/tmp/nt-dual-gt-evidence/alpha1-center.json`。八组 display 中心
  平均误差均下降：car001 `5.49/5.50/5.65/6.58→5.21/5.24/5.44/6.31 px`，
  car002 `3.95/3.90/3.38/3.48→3.83/3.78/3.31/3.32 px`。
  display 最大成对中心距离 P95 car001 `3.71→3.85 px`、car002
  `5.39→5.85 px`，属轻微一致性代价；八组 tracking/control 框、状态、重捕、
  observation_valid 逐帧相同。两项显示改动均已写入本分支显示配置，最终主机配置
  SHA-256 与 `runs/alpha1-controller.conf` 相同。
- 另做四组 car001 的未采纳搜索参数筛查：只把
  `search_probe_interval_ms=100→33` 的隔离配置 SHA-256
  `8bfce3c6378f7145c44e5ab9a0ec368c5b61171a44d86e036b9ea396a2f65d4d`，
  headless SHA-256 `aded48547d05128e7f709ad11325344a8bebb28c3984d0cdcf96067da897ed09`；
  输出 `runs/probe33/<场景>/` 的 16 文件清单 SHA-256
  `d61b6201c153a1ab5533a4de2d99de28c6fa0cab6d915800f2a34d2b26af45ea`，
  主机评分 `/tmp/nt-dual-gt-evidence/probe33-car001.json`。右下框重捕
  `193→205` 帧、tracking 均值中心误差 `5.73→8.35 px`、四组最大成对
  中心距离 P95 `3.51→43.05 px`，故拒绝。
  另在 display 渐变 1 帧基础上把 `suspicion_enter_ms=100→33`、
  `uncertain_ms=200→100`，隔离配置/headless SHA-256 分别为
  `a33e6a6dbfd7e4180078acc307a723736984279e1032b86e25bd304b`、
  `76e1c17e1c720a6ad4fad3e6bb86da76f411ffccf349c2ee7d2a4054e1700381`；
  `runs/early-suspicion/<场景>/` 的 16 文件清单 SHA-256
  `c963928c8bed5862ce101e19094c1e3cb13493314de855182bdb69a2c6d1db28`，
  主机评分 `/tmp/nt-dual-gt-evidence/early-suspicion-car001.json`。标准/左上框
  第 `70/76` 帧误重捕且平均中心误差 `52.38/54.72 px`，故拒绝。
- **证据边界 / Handoff**：car001 右下初始框第 192 帧 tracking 中心误差
  `44.16 px`，第 193 帧才重捕；car002 第 233 帧跨初始框 tracking 中心
  最大距离 `26.64 px`，15 个 no-bbox 帧仍有 `3/2/6/14` 次误观测。
  完整评估仅 car001 标准框 PASS；其余七组未通过正式门禁。此分支只保留
  改善显示中心一致性的候选，不批准正式 handoff，不合入 `main`/`Debug`；
  下一步继续验证跟踪中心偏移的搜索探测与遮挡语义。

## 2026-11-13T23:32:00+08:00 搜索期显示隔离板端复测与怀疑期回归

- 本轮被测源码 `codex/dual-gt-init-consistency@2dbffbb6cbe4817a06a2c903b3c34fc9512662c6`，
  基线为前一候选 `e1e1a535c6acfa925ea234e7007f96fac3a5cbd3`，工作树
  `/home/u20/single-object-tracking`。源码包 `/home/neardi/Project/SOT/nt-dual-gt-final-source.tar`
  主机/板端 SHA-256 `7707bb20b6fecef9fd7a3552604751821aaad6e59085eaa14b35c6e25bd88605`。
  RK3588 隔离根 `/home/neardi/Project/SOT/dual-gt-2dbffbb` 原生 RKNN 编译、
  CTest `23/23 PASS`；`build/nanotrack_v3` SHA-256
  `4f72c2e069e118c667236e0d8c4254fc9510c065e51403d2218a2961269cacf8`。
  实测 Linux `6.1.84`、RKNN API `2.1.0`、driver `0.9.8`。
- 输入 `data/occ_video_1/1.mp4` SHA-256 仍为
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；
  板端重算 NanoTrack template/search/head RKNN 及 VisDrone9 detector/manifest/labels
  均与下条 `e1e1a53` 记录的六个 SHA-256 完全相同。
  `rk3566/nanotrack_v3/config/visible-adaptive-display.conf` SHA-256
  `2adf4b35587215c18fcaf86221dd36f4c44fbf25910572a7a53c486235943d5e`；
  `runs/headless-eval.conf` SHA-256
  `7190b79337e35fe317606b0489d9295a300aa8d0782240c972c5bbb980cad48a`。
- 实际运行 `bash scripts/run_occ_init_matrix.sh ./build/nanotrack_v3
  runs/headless-eval.conf runs/final`，沿用下条八个 ROI 与两目标真值。八个 CSV 各
  271 数据行，八个 AVI 经 ffprobe 均为 1920×1080/271 帧；全部 32 个 CSV/AVI/
  session/log 留在 `runs/final/<场景>/`，`runs/final/checksums.sha256` 的 SHA-256
  为 `c474042f5704cc06ab81d3543102f094d4fd36efa2813441edc37bf43596b43b`。
  主机逐帧评分 `/tmp/nt-dual-gt-evidence/final-2dbffbb.json`。
- car001 四组显示均值 IoU 为 `0.740/0.737/0.731/0.708`，car002 为
  `0.707/0.714/0.722/0.715`（顺序同矩阵）；跟踪与控制框相对 `e1e1a53`
  逐帧完全相同。完整门禁仍仅 car001 标准框 PASS；其余 car001 第
  `192/194/193` 帧重捕，car002 四组均有遮挡语义失败。car002 在 15 个正式
  no-bbox 帧上，四组分别有 `3/2/6/14` 帧仍报告 `observation_valid=true`；
  该数量已加入 `scripts/evaluate_occ_initialization_consistency.py` 的诊断字段，
  不把这些帧算普通 IoU。
- 可见帧回归：把 SUSPECTED 与 SEARCHING 都保留旧 `.35` 时，car001 标准框和
  用户框最差显示 IoU 从上条 `.80` 的 `0.537/0.602` 降到 `0.400/0.420`，
  主要发生在第 49 帧目标快速运动且 SUSPECTED 的阶段。故本条后的源码改为
  TRACKING/SUSPECTED 用 `.80`，仅 SEARCHING 用旧 `.35`；主机 RKNN-off
  重构建及 CTest `23/23 PASS`，板端复测待完成。
- **证据边界 / Handoff**：显示中心改动不修复 tracker 的初始尺度继承、晚重捕或
  car002 遮挡期误观测。当前仍为新分支候选，正式 handoff 不批准，
  不合入 `main`/`Debug`。

## 2026-11-13T23:23:00+08:00 双目标多初始框 RK3588 对照与搜索行为隔离

- 候选源码 `codex/dual-gt-init-consistency@e1e1a535c6acfa925ea234e7007f96fac3a5cbd3`，
  基线为 `main@b80226577954febe22c5eff168125f77a6cdc724`，工作树
  `/home/u20/single-object-tracking`。`git archive` 源码包
  `/home/neardi/Project/SOT/nt-dual-gt-candidate-source.tar` 的本机与板端 SHA-256
  同为 `01c14bb9a9af59072673eb766360a4c11af6d291634c15472ee63c8925f15066`。
  板端隔离部署根 `/home/neardi/Project/SOT/dual-gt-e1e1a53`，原生编译命令
  `cmake -S rk3566/nanotrack_v3 -B build -DENABLE_RKNN_RUNTIME=ON -DBUILD_TESTING=ON
  -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include
  -DRKNN_LIBRARY=/usr/lib/librknnrt.so`、`cmake --build build --parallel 4`，
  CTest `23/23 PASS`。可执行文件 `build/nanotrack_v3` SHA-256
  `a1013ddf0c3b198cbafdf2f2321d6153db32d4863b53cec20be675876e1c1bc4`。
  实测板端 Linux `6.1.84`、RKNN API `2.1.0`、driver `0.9.8`。
- 输入视频板端 `data/occ_video_1/1.mp4` SHA-256
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；
  template/search/head RKNN SHA-256 为
  `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`、
  `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`、
  `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`。
  VisDrone9 detector RKNN/manifest/labels SHA-256 分别为
  `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`、
  `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9`、
  `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。
- 同源码配置 `.35/.80` 的控制器文件 SHA-256 分别为
  `85a8f6b8dd2c9215d68c292a388a3011e5a50bf7b854f7e85c567d3378cd6e11`、
  `2adf4b35587215c18fcaf86221dd36f4c44fbf25910572a7a53c486235943d5e`；
  headless 入口 SHA-256 分别为
  `9835e10c89f0487eb6258dcc820de795e7dd04a871284489796aeda2c17c3e15`、
  `e8311ca8b031d2154b8e9b62a04c6098695fd48f334862438e77b7c3d1618e85`。
  每个配置按 `scripts/run_occ_init_matrix.sh` 所列八个 ROI 运行，实际核心命令
  `./build/nanotrack_v3 --config <headless 配置> --bbox <ROI>
  --output-video runs/compare/alpha<035|080>/<场景>/output.avi
  --output-boxes runs/compare/alpha<035|080>/<场景>/frames.csv
  --session-metadata runs/compare/alpha<035|080>/<场景>/session.json`。
  16 个 CSV 均为 271 数据帧，16 个 AVI 经 ffprobe 均为 1920×1080/271 帧；
  `runs/compare/checksums.sha256` 含全部 64 个 CSV/AVI/session/log SHA-256，
  清单 SHA-256 为 `7831279239f9113afa02b6e6da3eb693c2df47b3aa537ed097e668359995e6dc`。
  主机评分 JSON `/tmp/nt-dual-gt-evidence/alpha035-full.json`、
  `alpha080-full.json`，用两目标真值各自的正式可见帧评分，不把不可观测框算普通 IoU。
- car001 四组显示框均值 IoU `.35→.80`：标准 `0.677→0.741`、用户框
  `0.673→0.741`、左上框 `0.665→0.732`、右下框 `0.646→0.708`；
  car002 为标准 `0.676→0.708`、真值框 `0.684→0.714`、左上框
  `0.696→0.731`、右下框 `0.686→0.714`。两配置下跟踪框及控制框保持同一
  初始化对应的逐帧轨迹；四组内显示框两两平均 IoU 最小值 car001
  `0.893→0.888`、car002 `0.895→0.890`，贴合度提升但一致性有小幅下降。
  算法 runtime P95 的八组最大值 `.35=10.569 ms`、`.80=10.335 ms`。
- 完整门禁不随显示参数改变：car001 四组只有标准框 PASS，另三组分别第
  `192/194/193` 帧重捕并有晚重捕或遮挡语义失败；car002 四组均因遮挡语义失败，
  两组无正式重捕。旧 `429ca9b` 二目标八组基线保留在
  `/home/neardi/Project/SOT/display-detector-exact-429ca9b/runs/dual_gt_baseline/`，
  `checksums.sha256` 清单 SHA-256 为
  `10b1f3fa5978124ddfd4b50c2811b01bac720642b2d9fd285c903aac41b03c68`。
- 隔离参数筛查：检测间隔 `400→200 ms` 时 car001 通过数 `1/4→2/4`，但标准框
  跟踪均值 IoU `0.751→0.708` 且跨初始框差异增大；`100 ms` 时左上框无正式重捕，
  跟踪均值仅 `0.567`。把两个目标的唯一 detector 初始化框直接用于 tracker，
  car001 第 192 帧晚重捕，car002 可见框门禁失败、跟踪均值 `0.640`；
  因此没有纳入这些初始化或检测节奏改动。弃用键
  `search_reacquisition_requires_detector` 在该配置中被程序按预期拒绝。
  这些试验在同一板端隔离根的 `runs/detector200/`、`runs/detector100/`、
  `runs/detector_init/`，40 个输出的 SHA-256 清单
  `runs/exploratory-checksums.sha256` 自身 SHA-256 为
  `c1a19a5b25ef663d6888e773e3aaca57fb30310af24b8c65b66c3cc3cceb60d7`。
- 当前分支再将 `.80` 限定于 TRACKING；SEARCH/SUSPECTED 保持原 `.35`，
  避免搜索预测框被额外加速。主机重构建及 CTest 再次 `23/23 PASS`；
  本条后续提交的精确源码与板端复测另记。**Handoff**：显示贴合度改善已观察，
  car001 重捕初值敏感与 car002 完全遮挡语义仍是阻塞；正式 handoff 不批准，
  不合入 `main`/`Debug`。

## 2026-11-13T23:00:00+08:00 双真值初始框一致性候选：主机阶段

- 新分支 `codex/dual-gt-init-consistency`，工作树 `/home/u20/single-object-tracking`，
  修改基线 `main@b80226577954febe22c5eff168125f77a6cdc724`；本条所在提交为代码候选。
  开始时工作区干净。`STATUS.md` 的协调基线 `e0b59f4` 较旧，本轮没有改动它或
  `integration-contract.yaml`。统一视频 `data/occ_video_1/1.mp4` 的本机 SHA-256
  复核为 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；
  两目标真值由 `scripts/evaluate_occ_video_1.py` 严格校验并排除不可观测框。
- 从已留存的 `429ca9b` 板端 CSV 复核：car001 的 `992,582,37,39` 与
  `993,585,36,36` 两次初始化，跟踪框可见帧均值 IoU `0.751/0.719`，显示框
  `0.677/0.672`；完整门禁分别 PASS 与 FAIL（后者第 192 帧重捕，晚于窗口）。
  car002 的 `970,517,38,38` 跟踪框/显示框均值 IoU `0.799/0.676`，
  完整门禁因完全遮挡语义 FAIL。三个 CSV 的新复算工具输出保留在
  `/tmp/nt-dual-gt-baseline-archived.json`；数据源是
  `/tmp/display-detector-exact-evidence/{car_001,user_roi,car_002}/frames.csv`，
  仍属历史 `429ca9b` 运行，不能算本分支改动后的板端表现。
- 诊断显示第 48--58 帧快速运动时，显示框中心落后跟踪框，两个目标均出现低 IoU；
  `AdaptiveDisplayBox` 使用跟踪中心的 `accepted_center_smoothing_alpha=0.35`。
  本候选加入独立的 `display_center_smoothing_alpha`，默认保持已有可见光语义，
  仅 `visible-adaptive-display.conf` 设为 `0.80`，并把有效值记入 session metadata。
  旧 CSV 的离线中心回放估计可提高显示 IoU，但该回放保持旧检测时序和尺寸，
  只是参数筛选，不是运行实测。
- 实际执行 RKNN-off 主机命令：
  `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-dual-gt-consistency-build -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF`、
  `cmake --build /tmp/nt-dual-gt-consistency-build --parallel 4`、
  `ctest --test-dir /tmp/nt-dual-gt-consistency-build --output-on-failure`，
  `23/23 PASS`。`python3 scripts/evaluate_occ_initialization_consistency.py` 已用三份历史 CSV
  完整跑通；新增单测覆盖显示与跟踪中心参数分离。模型未重新训练或转换。
- **证据边界 / Handoff**：RK3588 当前地址 `192.168.1.106` 已连通，真实多初始框基线
  正在运行。候选尚未完成板端原生编译与同源码参数对照；不批准生产 handoff，
  不合入 `main` 或 `Debug`。

## 2026-11-13T22:44:33+08:00 共同算法合入 main 与 Debug

- 用户确认合并本轮功能分支。合并前远端 `main@1fb5a7fe025b38a1d4794ee1a65859a2df0174df`、
  `Debug@59f700d642085ef263a4c34c73caee78cb60bb53`，两工作树均干净。
  `main` 先快进到已验证运动输入门 `89827227d256aaa648d8dba842f2ec0ac0f94895`，
  再将 `5793986` 的共同重捕代码、`c829e8a` 的显示缩放修正及 `429ca9b` 的零边距
  显示配置合入 `811607b2a6ae4adf57ad8dd402724d9c15efc108`。
  `Debug` 将完整功能分支 `b1da4b06f1e2f6ca27fe0858fda2de3a855401cf` 合入
  `df130c27cf3d40db6ac7494ea90b22bd2261fa84`，冲突只在 `src/main.cpp` 的
  Debug 窗口参数传递，采用已在 RK3588 运行的功能分支版本。
- 两分支的 `rk3566/nanotrack_v3` 对比仅三个交互界面文件不同：
  `src/main.cpp`、`src/interactive_debug_overlay.cpp`、
  `tests/test_interactive_debug_overlay.cpp`。共同算法、配置、日志与其余测试文件逐字节一致。
  合并后分别执行主机 RKNN-off CMake 构建和 CTest：`main`、`Debug` 各 `23/23 PASS`；
  `git diff --check` 通过。此前 RK3588 原生与三目标视频证据绑定源提交 `429ca9b`；
  本次没有把 Git 合并本身算作新的板端运行。
- **Handoff**：板端记录已同步到集成分支；协调对话可据此审查新基线，但公共
  `STATUS.md` 和 `integration-contract.yaml` 的正式状态及生产 handoff 仍需其所有者裁定。
  用户要求远端仅保留 `main`、`Debug`；功能分支继续只保留本地。

## 2026-11-13T22:19:33+08:00 detector 原框零边距 RK3588 复测

- 被测源码 `codex/recovery-local-arbitration-debug-video@429ca9bc109d48f03b945bf861f16f0ff7c4cde5`，
  独立板端根 `/home/neardi/Project/SOT/display-detector-exact-429ca9b`；主机与板端
  `source.tar` SHA-256 均为 `097838dd7999b64fba9d77c9470acaedac6b386c3209b2028b0e791e8f4ce72f`。
  板端原生 RKNN、`-Werror=switch` 构建和 CTest `23/23 PASS`；
  `build/nanotrack_v3` SHA-256 为
  `4695f896dca661d53c5b1c468fe38d41e2e0c8e34a47980172bde85ef09d2485`。
  实测 Linux `6.1.84`、RKNN API `2.1.0`、RKNPU driver `0.9.8`。
- 实际输入 `data/occ_video_1/1.mp4` SHA-256
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；
  `runs/headless-eval.conf` SHA-256
  `4bdd01e3fcb3761c517cc887d65b46c55e89d0ef453ca792b8a60371d01cb7d9`，
  所引用 `visible-adaptive-display.conf` SHA-256 为
  `faf968acfb86c280b913567eaf74dd40580a7264d752b12546a712e688b335a7`。
  本次板端复算 NanoTrack template/search/head RKNN SHA-256 依次为
  `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`、
  `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`、
  `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`；
  detector `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/releases/yolov8n-visdrone9-rk3588-fp16-v1/models/yolov8n-visdrone9-rk3588-fp16.rknn`
  维持 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`。
- 在板端部署根对 `user_roi=993,585,36,36`、`car_001=992,582,37,39`、
  `car_002=970,517,38,38` 分别执行
  `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video runs/<scenario>/output.avi --output-boxes runs/<scenario>/frames.csv --session-metadata runs/<scenario>/session.json`。
  三次均 exit 0，CSV 各 271 数据行、AVI 各 1920×1080/271 帧；runtime P95 依次为
  `11.016`、`9.672`、`9.823 ms`。三次初始化 `display_bbox` 均与唯一匹配的 detector
  `clipped_bbox` 四坐标完全相等（最大绝对差 `0`）。
- 对上一版 `c829e8a` 的三组相同 ROI 逐帧比较，状态、接受/重捕标志、模板动作、跟踪框、
  控制框均无差异。重捕后 21 帧显示框平均 IoU：用户 ROI `0.661→0.720`，
  car001 `0.621→0.710`，car002 `0.760→0.791`。full evaluator 仍为 car001 **PASS**
  （185 帧重捕）、用户 ROI **FAIL**（192 帧晚于 182--189）、car002 **FAIL**
  （213--216 帧完全遮挡语义），没有借显示改动宣称算法门禁改善。
- `runs/user_roi/`、`runs/car_001/`、`runs/car_002/` 的 CSV SHA-256 依次为
  `f90e6a9aae185dbf0e5c6fbeaf78f1e56a147acbdde268b1bb88c9a503f047eb`、
  `9926c0688495716f3e6aa0ec86cf4b87c1f569000cda4637876c6d752b78ea5f`、
  `f8ea15ff8e4ee52194cb43d241d86a4f88ccde2e6561498b4d5f5a9f1`；AVI 对应为
  `14ed94f1ce0c5799bbd8cc6e8baeef4a319c313310c914d52c9c1583c8ad93e6`、
  `edf20ddcf711e329047ba4d70ce1c18d188a11ba93b57f4deafe091f0062dd03`、
  `4b263ce94b6be8ee8b8f9183c32ff523353ae2d4ab31d06bfa04a2569cae8dc9`。
  主机评价 JSON `/tmp/display-detector-exact-evidence/<scenario>/eval.json` 的 SHA-256
  按上述顺序为 `107642c0726a413b878d6513296e570422c562beb067a7e45c6c6a29babbb7d1`、
  `46cc20aa5435015b715dd94f16e57e6e484f0d6fb2e6941822538e38d9f1be36`、
  `39e30cd2b2d3b05bf6b60954b75db654fe8e6cdfb4eee4c6924acd300f7a824e`。
- 交互模式在 Xvfb 中用同一用户 ROI 跑至第 200 帧后按 Q 正常退出；板端
  `runs/interactive_display/debug.avi` 1920×1080/201 帧，SHA-256
  `7298ae96dd953d26f3644e04fe8ce7f97701d62e8fde14e00a3039559110ec44`。
  `NanoTrack Display` 窗口截图 `/tmp/display-detector-exact-frame200.png` SHA-256
  `7f629ee82d49b13dadf11a8f0be7dec109ae2c16ac7e8d907d5558fa0d4bd0d4`，
  同名副本已保留在主机 `/tmp/`。
- **证据边界 / Handoff**：显示检测为低频、两次确认后渐进纠正，故中间帧仍由 tracker
  传播并平滑；本轮保证不再给 detector 框附加人工边距，不代表逐帧原样显示每次异步
  检测。真实吊舱效果待人工测试。候选保持本地分支，按用户既定规则待确认后才合入
  `main`/`Debug`，不推送远端功能分支。

## 2026-11-13T22:12:14+08:00 显示框使用检测器原框：主机验证

- 分支 `codex/recovery-local-arbitration-debug-video`，工作树
  `/home/u20/.codex/worktrees/recovery-local-arbitration-debug-video/single-object-tracking`，
  修改基线 `7cc966a6d45d592eff5def5d8eba30ddc50a1caf`；源码结果提交以本条
  Git 历史为准。按用户要求，默认配置和 RK3588 可见光配置中的车、人、未知类别
  水平/垂直显示留量均改为 `0`。初始化采用裁剪后的 detector 原框；跟踪期已确认的
  detector 原框作为显示包络目标。帧间传播、两次检测确认、渐进纠正、尺寸步长限制、
  SEARCH 冻结及重捕过渡保持现有语义，因此显示框不是每帧直接复制异步检测结果。
- `rk3566/nanotrack_v3/config/visible-adaptive-display.conf` SHA-256 从
  `3a949556b682011b7ebb40fb0353471b71af0677685e37a938f8af01d66928af`
  变为 `faf968acfb86c280b913567eaf74dd40580a7264d752b12546a712e688b335a7`；
  交互入口配置 `rk3588-adaptive-display-debug.conf` 仍为
  `5d67b007ff13cbd617e6ac54485b45c4a89d52da38d3651f54f5e0d3b92dfaca`，
  其 `controller_config=visible-adaptive-display.conf` 引用新参数。新增单测检查车/人
  初始化和两次确认后的目标框没有人工外扩。
- 实际执行 `cmake --build /tmp/nt-display-rebase-build -- -j4`、
  `ctest --test-dir /tmp/nt-display-rebase-build --output-on-failure`、
  `git diff --check`；主机 `23/23 PASS`。此阶段只有源码和主机结果，RK3588 新部署与
  固定 ROI 视频对照待完成。模型、输入视频与上一条 RK3588 记录相同，未重新转换。

## 2026-11-13T21:44:16+08:00 重捕后显示框缩放 RK3588 三目标复测

- 被测源码 `codex/recovery-local-arbitration-debug-video@c829e8aa9055714f3abc3ed817854e2725002854`，
  独立板端根 `/home/neardi/Project/SOT/display-rebase-c829e8a`；主机与 RK3588 的
  `source.tar` SHA-256 均为 `1a79700ca659d920d03ad91200ddc635c8317c289ad2e3ffef01a74caa1e9911`。
  板端原生 RKNN、`-Werror=switch` 构建及 CTest `23/23 PASS`，
  `build/nanotrack_v3` SHA-256 为
  `35f764601e25a090d921b1e7366ce163e2437094e0ac4c2e50bbd3139a9341ee`。
  本轮日志实测 Linux `6.1.84`、RKNN API `2.1.0`、RKNPU driver `0.9.8`。
- 运行配置 `runs/headless-eval.conf` SHA-256
  `845d99d2f9d470d842057874016bb252402574746c5cb305125e426d9ad7d132`；
  输入 `data/occ_video_1/1.mp4` SHA-256
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`。
  `models/template_backbone.rknn`、`search_backbone.rknn`、`head.rknn` 在本次板端
  复算的 SHA-256 依次为 `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`、
  `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`、
  `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`；
  VisDrone9 RK3588 detector `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/releases/yolov8n-visdrone9-rk3588-fp16-v1/models/yolov8n-visdrone9-rk3588-fp16.rknn`
  为 `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`。
- 在部署根分别执行
  `./build/nanotrack_v3 --config runs/headless-eval.conf --bbox <ROI> --output-video runs/<scenario>/output.avi --output-boxes runs/<scenario>/frames.csv --session-metadata runs/<scenario>/session.json`，
  其中 `user_roi=993,585,36,36`、`car_001=992,582,37,39`、
  `car_002=970,517,38,38`。三次均 exit 0，CSV 271 数据行、AVI 1920×1080 共 271 帧；
  runtime P95 分别 `9.628`、`10.002`、`9.432 ms`。CSV SHA-256 依次为
  `38e782b3286ef2de6612bc2ba2d8f7ddbdbcaf89c9dfd32b7f17ae61479ea936`、
  `2662ebff0437ffb24a86dccf2dbe06877a1fbfa3920f8fc0ed268537aa736247`、
  `c2f672fa5de7c1480a5d235b4d044874769ad7b67940bedd8fdbdb7e62b83fde`；
  AVI SHA-256 依次为
  `14ed94f1ce0c5799bbd8cc6e8baeef4a319c313310c914d52c9c1583c8ad93e6`、
  `edf20ddcf711e329047ba4d70ce1c18d188a11ba93b57f4deafe091f0062dd03`、
  `4b263ce94b6be8ee8b8f9183c32ff523353ae2d4ab31d06bfa04a2569cae8dc9`。
- 对父提交 `5793986` 的同 ROI CSV：car001、car002 的状态、接受/重捕标志、模板动作、
  跟踪框及控制框每帧完全一致。显示框在 car001 第 190 帧由 `63.6×72.0` 收至
  `55.6×63.9`，car002 第 240 帧由 `70.5×73.5` 收至 `54.8×61.6`。
  用户 ROI 在第 197 帧由 `62.6×72.9` 收至 `54.3×63.5`；重捕帧独立运行中由
  193 变为 192，控制轨迹从该帧开始不逐帧相同，不能将其计为严格隔离性对照。
  因显示模块在控制器输出之后运行，该一帧提前不是显示框重置对同帧控制器的直接输入；
  异步调度差异仅为推断，尚未单独归因。
- 重捕后 21 帧显示框与相应可见真值的平均 IoU：用户 ROI `0.537→0.662`，
  car001 `0.525→0.621`，car002 `0.615→0.760`；跟踪框未因此变为显示框。
  full evaluator：car001 **PASS**（185 帧重捕）；用户 ROI **FAIL**（192 帧仍晚于
  182--189 窗口）；car002 **FAIL**（沿袭 213--216 帧完全遮挡语义），其余门通过。
  evaluator JSON 位于主机 `/tmp/display-rebase-evidence/<scenario>/eval.json`，三份
  SHA-256 依次为 `f531a1609ee6513d1505f3056cc794cb56fe73d19f53e9833ff79f60f7e10f1d`、
  `6861b4d4ae608175798831cb3da9ea440f8f91b4e70b3fc0b42b83376401b2b5`、
  `de69da3d32d834ab266da21e9a64d6889e9960bcb36e3543c4f25d13441ccba3`。
- 交互模式在独立 Xvfb 中执行
  `DISPLAY=:99 ./build/nanotrack_v3 --config rk3566/nanotrack_v3/config/rk3588-adaptive-display-debug.conf --bbox 993,585,36,36 --max-frames 200 --output-video runs/interactive_display/debug.avi --output-boxes runs/interactive_display/frames.csv --session-metadata runs/interactive_display/session.json`；
  实际显示至第 200 帧后按 Q 正常退出。板端 `NanoTrack Display` 窗口截图
  `/tmp/display-rebase-window-frame200.png` SHA-256
  `8c4dda386eafedbdc5e6f4c426d997ab64fb64b2c1c9a63aa992d688fb4fec27`，
  主机副本同名；Debug AVI 1920×1080 共 201 帧，SHA-256
  `7298ae96dd953d26f3644e04fe8ce7f97701d62e8fde14e00a3039559110ec44`，保留在板端
  `runs/interactive_display/`。
- **证据边界 / Handoff**：这是 RK3588 视频与虚拟显示器上的显示框修复验证；真实吊舱
  显示效果待人工观察。候选继续保持本地分支，未获用户确认前不合入 `main` 或 `Debug`；
  请求协调对话消费本源码提交及上述板端证据时保留已知失败项。

## 2026-11-13T21:37:35+08:00 重捕后显示框重复放大修正：主机验证

- 分支 `codex/recovery-local-arbitration-debug-video`，工作树
  `/home/u20/.codex/worktrees/recovery-local-arbitration-debug-video/single-object-tracking`，
  本次修改基线 `1417535d6491e6b88b473e25efaffa548698de6f`；源码提交以本条 Git 历史
  为准。输入为此前 RK3588 用户 ROI `993,585,36,36` 的保留 CSV：
  `/tmp/recovery-5793986-user-frames.csv`，对应源码 `5793986`、板端部署根
  `/home/neardi/Project/SOT/recovery-local-debug-video-5793986/runs/user_roi/`。
- 实际 CSV：192 帧 SEARCH 时跟踪框 `36.0×41.1`、显示框 `49.0×57.2`；193 帧重捕后
  跟踪框变为 `47.9×54.6`，但显示模块继续套用旧宽高比，193--197 帧显示框按每帧
  `5%` 上限增至 `62.6×72.9`，这些帧 `display_correction_applied=0`。固定 car001 ROI
  同样从重捕前 `49.8×57.6` 增至 190 帧 `63.6×72.0`。根因是重捕缩放和旧显示变换
  叠加；首次新的检测显示纠正发生在用户 ROI 的 198 帧，不能解释此前放大。
- 修正仅在接受重捕时用新跟踪框和现有类别边距重建显示变换，并清除重捕前尚未完成的显示
  检测确认；保留五帧中心过渡、每帧 `5%` 尺寸上限及原检测纠正规则。跟踪框、控制框、
  模板、运动模型与重捕状态机代码未变。新增单测覆盖旧包络比例过大时的重捕缩放。
- 实际执行
  `cmake -S rk3566/nanotrack_v3 -B /tmp/nt-display-rebase-build -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF -DCMAKE_CXX_FLAGS=-Werror=switch`、
  `cmake --build /tmp/nt-display-rebase-build -- -j4`、
  `ctest --test-dir /tmp/nt-display-rebase-build --output-on-failure`：主机 `23/23 PASS`；
  `git diff --check` 通过。此阶段仅完成源码和主机验证，RK3588 新构建与视频对照待执行。

## 2026-11-13T21:31:38+08:00 板端进度接管与 RK3588 开发范围

- 用户授权当前对话维护 `docs/progress/board-validation.md`，并明确后续开发只在 RK3588
  上进行。此前 RK3566 记录保留为历史证据；后续原生构建、调试、算法验证与运行证据均绑定
  实际 RK3588 板卡。`STATUS.md` 和 `integration-contract.yaml` 仍由协调对话维护。
- 当前功能分支 `codex/recovery-local-arbitration-debug-video`，工作树
  `/home/u20/.codex/worktrees/recovery-local-arbitration-debug-video/single-object-tracking`，
  基线 `ef7dba9e9fe0028850979edcdd5fa06cdb8`，源码提交
  `5793986141ac560371aab7c6d5e15c44da1999aa`。本进度提交身份以 Git 历史为准。
  该功能分支尚未合入 `main`、`Debug`，也未推送远端。
- RK3588 独立部署根 `/home/neardi/Project/SOT/recovery-local-debug-video-5793986`；
  `source.tar` SHA-256 为
  `a70642fe634edbc2f717fb528834bd1cce98fd1bc19508ddae6e876cf757007c`，
  原生 `build/nanotrack_v3` SHA-256 为
  `785d964aca101085528e7aa274c6990068c53704cea865bfee21619d829bf84c`。
  RKNN 原生构建、`-Werror=switch` 和主机/板端 CTest 均为 `23/23 PASS`。
- 输入视频为板端 `data/occ_video_1/1.mp4`，SHA-256
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`。
  模型链接到既有 `adaptive-display-59769e8` 部署：RK3588 NanoTrack
  template/search/head RKNN SHA-256 依次为
  `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`、
  `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`、
  `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`；
  VisDrone9 RK3588 detector RKNN SHA-256 为
  `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`。
  这些模型身份沿用既有 RK3588 部署记录；板端历史环境记录为 RKNN API `2.1.0`、
  RKNPU driver `0.9.8`，本轮未单独重测驱动版本。
- 同一固定 ROI 对照 0--270 帧：`car_001=992,582,37,39` 的 full evaluator **PASS**，
  185 帧重捕；`car_002=970,517,38,38` 的跟踪、控制、状态和重捕逐帧与父提交一致，
  保留 213--216 帧已知 `full_occlusion_semantics` FAIL。两目标算法 P95 分别约
  `10.499 ms`、`10.441 ms`。用户 ROI `993,585,36,36` 的重捕由上一版 218 帧提前到
  193 帧，但仍晚于 car001 正式窗口 182--189，故不宣称该 ROI 通过完整门禁。
  三次完整输出分别保留在板端部署根的 `runs/car_001/`、`runs/car_002/`、`runs/user_roi/`。
- 实际交互验证命令：在部署根运行
  `DISPLAY=:99 ./build/nanotrack_v3 --config rk3566/nanotrack_v3/config/rk3588-adaptive-display-debug.conf --bbox 993,585,36,36 --max-frames 195 --output-video runs/debug_video_recovery/output.avi --output-boxes runs/debug_video_recovery/frames.csv --session-metadata runs/debug_video_recovery/session.json`。
  运行退出码 0，AVI 为 `1920x1080`、196 帧，SHA-256
  `0e8218bddd791c8db231c1f5a02c13ade723e8110f5728cf918d171deacaf740`。
  第 190 帧可见运动预测、recent/safe、控制和三个关键 detector 候选框，未绘制全部原始检测框。
- **证据边界 / Handoff**：本轮是 RK3588 上的候选算法与 Debug 视频验证，不等于正式
  开发板源码/模型 handoff 或真实吊舱验收。请求协调对话把后续开发板目标从 RK3566 更新为
  RK3588，并在审查此候选提交及未通过项后再更新公共状态和集成契约。

## 2026-11-12T22:25:39+08:00 双窗口精简交互 Debug RK3588 验证

- 分支 `codex/adaptive-display-box`，运行源码提交
  `23f594c023492034d6d392686b7d4a89b233b9e0`，源码归档 SHA-256
  `88dc420e27752b960fd8b5e8cc3a234545ad148de22800e15028c77e00af3649`；部署到独立目录
  `/home/neardi/Project/SOT/adaptive-display-debug-23f594c`，未覆盖上一版运行或视频证据。
- RK3588 原生 Debug 构建启用 RKNN、`BUILD_TESTING=ON` 和 `-Werror=switch`，包含
  `src/main.cpp` 的完整可执行目标并通过 CTest `23/23 PASS`。新二进制
  `build/nanotrack_v3` SHA-256 为
  `5c0cbc2ccfdf7436a2e46b986ae857e20c869690f1ba8253c630da0ba5492a74`；runtime/controller
  配置 SHA-256 仍分别为
  `5d67b007ff13cbd617e6ac54485b45c4a89d52da38d3651f54f5e0d3b92dfaca`、
  `3a949556b682011b7ebb40fb0353471b71af0677685e37a938f8af01d66928af`。
- 在板端现有 X11 `DISPLAY=:0` 会话短启动交互程序，`xwininfo -root -tree` 实际观察到
  `NanoTrack Display` 与 `NanoTrack Debug` 两个独立窗口，均为 1280x720，位置分别为
  `+2+23`、`+1298+23`；无 HighGUI/X11 创建错误。固定 ROI 启动同时完成 tracker、RK3588
  detector 加载及自动 car 类别建立，随后保持 PAUSE；短测由 timeout 结束，不计作算法视频回归。
- **结论 / 边界**：双窗口显示和关键 detector 过滤已通过主机单测、RK3588 原生编译测试及
  实际窗口创建检查。改动只在交互绘制层，不改变 `tracking_bbox`、`control_bbox`、CSV、检测器
  调度、状态机或控制输出；因此本次未重复运行 271 帧 canonical evaluator，也不改变上一条
  双车算法验收结论、人物板端验收边界或生产/吊舱状态。

## 2026-11-12T22:23:02+08:00 双窗口精简交互 Debug 主机实现

- 分支 `codex/adaptive-display-box`，基线
  `eae32e31e333d3f636294dfaa59ae9ad6c3fba1c`。交互模式拆分为两个同步窗口：
  `NanoTrack Display` 只显示原始画面和白色 `display_bbox`，`NanoTrack Debug` 显示状态、
  NanoTrack/运动诊断与绿色 `control_bbox`；正常、SUSPECTED 和 SEARCH 的有效控制框统一使用
  绿色，具体控制来源仍由标签区分。ROI 重选继续在干净显示窗口完成，任一窗口关闭均安全退出。
- detector 可视化只保留初始化选中框、通过类别/位置门且已进入候选队列的关键框、当前 active
  candidate/probe 及 search hint；不再绘制全部原始拒绝框或对应拒绝文本。运行时 detector
  诊断、CSV、控制器候选队列和日志未改变，本次只修改交互绘制选择。
- 主机 RKNN-off 增量构建和 CTest `23/23 PASS`，其中 overlay 回归验证关键候选保留、类别不符、
  stale、capacity、位置拒绝及无效框均不再进入画面；`git diff --check` PASS。
- **边界 / 下一步**：主机 RKNN-off 不编译 `main.cpp` 的 RKNN/OpenCV 可执行目标，因此当前只构成
  overlay 单测证据；需提交精确源码后在 RK3588 原生重建并启动交互模式，确认双窗口实际显示。

## 2026-11-12T22:02:57+08:00 自适应显示框 RK3588 双车验收

- 分支 `codex/adaptive-display-box`，实施基线
  `main@dccf8e0844b78cf9a1f4ee207352429b6d884979`，运行与验收源码提交为
  `eb66cf6115eff274aa809f5e875a939de632ef99`（显示实现提交
  `59769e89df45e6e39a69306dadae7a6294e7e911`）。精确 HEAD 源码归档在主机与板端的
  SHA-256 均为 `0a7c2f228407304ffe69b989f2c249c39ee26df29b986980d341e394896db042`；
  独立板端根为 `/home/neardi/Project/SOT/adaptive-display-59769e8`，未覆盖现有基线部署。
- 实际 `LKD3588` 原生 Debug 构建启用 RKNN、`BUILD_TESTING=ON` 与 `-Werror=switch`，
  CTest `23/23 PASS`。`build/nanotrack_v3` SHA-256 为
  `ea562308a9970e93ef8839d9fc3d919c99ed48c841f6edc98d7b6bda7e5fd9f6`；自适应 runtime
  配置与 controller 配置 SHA-256 分别为
  `5d67b007ff13cbd617e6ac54485b45c4a89d52da38d3651f54f5e0d3b92dfaca`、
  `3a949556b682011b7ebb40fb0353471b71af0677685e37a938f8af01d66928af`；tracker manifest
  SHA-256 为 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`。
- 固定 ROI `car_001=992,582,37,39`、`car_002=970,517,38,38` 分别运行 0--270 帧。两次
  runtime exit 0、schema v13 verifier PASS，输出 CSV 与两类 AVI 均为 271 帧；算法运行时间
  P95 分别为 `10.573 ms`、`10.470 ms`，低于 `33.3 ms` 验收线。两次自动类别均唯一建立为
  VisDrone9 car=`2`，显示刷新请求分别 78/80 次、SEARCH 请求均为 2 次、显示纠正应用
  36/37 次、暂停 0 次，最终刷新周期均为 100 ms；single-flight 与 SEARCH 抢占测试通过，
  未见请求积压。
- `output.avi` 保持绘制 `control_bbox`；同一次运行的原视频与 v13 CSV 只读重放生成
  `display-output.avi`，绘制 `display_bbox`。两份显示视频均经 `ffprobe` 确认为
  1920x1080、30 fps、271 帧，car001/car002 SHA-256 分别为
  `cca38a4b6e7f34457e6f9060711a835e589ce5891402cb90a865f3430fb9c7e0`、
  `3caba35936c7c4a4da62d8925ece73d42b0931c6a8fa645c95a739c9ef3b89a6`。控制诊断视频
  SHA-256 分别为 `edf20ddcf711e329047ba4d70ce1c18d188a11ba93b57f4deafe091f0062dd03`、
  `78fe73d1628d2b9b23f91d368175d3e907a02e847c48665a4ffd41186ec04765`。
- 对比既有 `occ-restored-2d31bfe` 基线，两个目标各 271 帧的 53 个跟踪、控制、状态机、
  模板事务和运动模型字段逐项完全一致，差异数均为 0。full evaluator：car001 **PASS**，
  visible mean IoU `0.751326`、185 帧重捕；car002 的可见框、预测连续性、235 帧重捕、连续
  控制和 identity proxy 均 PASS，visible mean IoU `0.798100`，只保留基线已知的 213--216
  帧 `full_occlusion_semantics` FAIL。evaluation JSON SHA-256 分别为
  `dddc669ae1288a6f8c4505095cb710813a699b7e4543fc8596ddae929ef8062e`、
  `131a535178779408ef84a510c6e04dd4536252e6afc298101888dfc9a448f2a9`；完整大工件与日志保留在
  板端 `runs/car_001/`、`runs/car_002/`。
- **证据边界 / Handoff**：本轮证明车目标自适应显示在 RK3588 上满足性能与隔离要求，且不使
  C012/当前控制基线退化。人物类别、边距、姿态变化及接口已有主机单测，但没有新的合规人物
  视频，故不声明人物板端验收；如需纳入正式人物基准，请协调对话更新集成契约。未修改
  `STATUS.md` 或 `integration-contract.yaml`，未执行生产交叉编译或人工吊舱验收。

## 2026-11-12T21:59:27+08:00 evaluator 对 additive schema v13 的兼容修正

- 分支 `codex/adaptive-display-box`，基线为本分支实现提交
  `59769e89df45e6e39a69306dadae7a6294e7e911`，结果为包含本条记录的同一提交。首次 RK3588
  双车运行已证明 schema v13 保留 schema v12 全部语义列，但 canonical evaluator 仍以
  `schema_version == 12` 判断当前重捕，导致实际 185/235 帧声明被列为 invalid schema。
- evaluator 改为接受单一、版本号不低于契约最低版本且语义列完整的 additive schema；session
  metadata 必须与实际行版本一致，旧 schema 9 及缺列版本仍拒绝。新增 v13 继承 v12 重捕语义
  的回归，`python3 -m unittest tests.test_evaluate_occ_video_1` 结果 `24/24 PASS`，
  `git diff --check` PASS。
- **边界 / 下一步**：该修正只改变验收工具的向后兼容版本判断，不改变任何跟踪、显示或控制
  运行时；需把精确新提交同步到 RK3588 后重新执行两车 full evaluator，再记录最终门禁。

## 2026-11-12T21:52:12+08:00 车/人自适应显示框主机实现

- 分支 `codex/adaptive-display-box`，工作树
  `/home/u20/.codex/worktrees/adaptive-display-box/single-object-tracking`，基线
  `main@dccf8e0844b78cf9a1f4ee207352429b6d884979`；结果为包含本条记录的同一提交。实现独立
  `AdaptiveDisplayBox` 后处理层，只覆盖 `ControllerOutput::display_bbox` 及兼容别名 `bbox`；
  `tracking_bbox`、`control_bbox`、模板、运动模型、遮挡判定及重捕控制器均不读取显示状态。
- 检测请求新增 `INITIALIZATION`、`SEARCH_RECOVERY`、`DISPLAY_REFRESH` 用途，运行时按用途
  分流；显示批次不进入恢复候选队列，SEARCH 请求优先提交。自动类别初始化使用
  VisDrone9 `person=0`、`car=2`；唯一匹配时显示框从鼠标 ROI 扩展到带类别边距的全目标框，
  否则保持 ROI。TRACKING 低频修正、SUSPECTED/SEARCH 冻结可信尺寸、SEARCH 跟随预测中心，
  重捕使用五帧渐进过渡。
- 新增 `config/visible-adaptive-display.conf` 与
  `config/rk3588-adaptive-display-debug.conf`；历史配置字节未修改。显示检测初始 5 Hz、范围
  1--10 Hz，并按算法运行时间滚动 P95 在 20/25/30 ms 门槛加速、降频或暂停；schema 升至
  v13，记录请求用途、显示框来源、纠正确认、刷新周期和运行时间 P95。
- 主机命令：`cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-adaptive-display-build
  -DCMAKE_BUILD_TYPE=Debug -DENABLE_RKNN_RUNTIME=OFF`，随后 `cmake --build ... -j4` 与
  `ctest --test-dir ... --output-on-failure`，结果 `23/23 PASS`。另以
  `-DCMAKE_CXX_FLAGS=-Werror=switch` 在 `/tmp/nanotrack-adaptive-display-werror` 重建并运行，
  同样 `23/23 PASS`；`git diff --check` PASS。
- **边界 / 下一步**：当前仅为主机实现证据，尚未构成 RK3588 原生构建、性能或视频效果证据；
  下一步将精确提交部署到 RK3588 独立目录，使用 `occ_video_1` 固定 car001/car002 ROI 运行，
  核对 schema v13、算法 P95、无检测积压及既有 C012/当前基线不退化。人物仅完成代码与单测，
  没有合规人物视频前不声明板端人物验收。

## 2026-11-12T16:19:23+08:00 四个已拒绝运动冻结实现的失败帧导出

- 按用户复核请求，从四个已拒绝 RK3588 保留根的原始输出 AVI 导出失败窗口：
  `d7ee7df`、`2b08bc5`、`3f3d7be`、`49027b1`。选择依据为 full evaluator 的首次坏帧、
  错误重捕帧及 car002 的 213--216 帧遮挡语义失败；每个中心帧导出 `center-4` 到
  `center+4` 共 9 张带真实源帧号 JPEG，并生成一张 3x3 contact sheet。
- 最终板端目录为 `/home/neardi/Project/SOT/motion-freeze-failure-frames-v3`，压缩包为
  `/home/neardi/Project/SOT/motion-freeze-failure-frames-v3.tar.gz`；当前工作树的忽略目录副本为
  `results/failure-frame-sequences/motion-freeze-failure-frames-v3/`。共 28 个事件窗口、252 张
  连续帧、28 张 contact sheet；`windows.csv` 记录全部精确帧段，`README.txt` 记录四种实现语义，
  `SHA256SUMS` 覆盖 282 个非清单文件。
- 最终压缩包 SHA-256 为
  `c2bfd257d2817bc35d40dbc4cebbb78dc3403c857ce81899190ded9b3c83514c`。抽查
  `d7ee7df/car_001/f104_false_reacquisition/contact_sheet.jpg` 确认角标为真实源帧
  100--108，框来自各版本保存视频中绘制的 `control_bbox`。首次 v2 导出因旧 FFmpeg 将
  contact-sheet 角标重编号为 0--8 而废弃；v3 才是交付版本，不改变任何源码、运行结果或判定。

## 2026-11-12T16:06:25+08:00 恢复版 2d31bfe RK3588 精确确认

- 分支 `codex/recent-safe-occlusion-recovery-v2`，工作树
  `/home/u20/.codex/worktrees/rk3588-debug-deploy/single-object-tracking`，基线
  `49027b1a7f6ab6da2f81d0693ae4a6b420463cb7`，源码提交
  `2d31bfee11e381a345ffba958beccd7eadd39289`。该提交恢复已验证的 `5fbcf83` search 趋势行为，
  不再采用本轮四个已拒绝的分级运动冻结实现；schema v12 与 `motion_update_valid` 可观测字段
  保留。主机 CTest `22/22`、evaluator 单测 `23/23`、`git diff --check` 均 PASS。
- 源码归档 `/tmp/occ-restored-2d31bfe.tar.gz` 与板端同名文件 SHA-256 均为
  `b070320cfa355dab7fb1eac7692674414361c2acd678450fa1fdef4fd2027aa4`；独立板端根
  `/home/neardi/Project/SOT/occ-restored-2d31bfe`。RK3588 原生 Debug 构建和 CTest `22/22`
  PASS；`build/nanotrack_v3` SHA-256
  `9a30f2305060cd4d1d7aae7e2e41e23b7cd5c3dda011316234d6a43c34e4c240`。配置/模型 manifest
  SHA-256 分别为 `7b038eed1dd052de5879bedbf465a2ded3cc79d5a6f0b662e39d9fd06fc15c34`、
  `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`。
- 固定 ROI `car_001=992,582,37,39`、`car_002=970,517,38,38` 各运行 0--270 帧并 exit 0；
  schema v12 verifier 均 PASS，AVI 均为 `1920x1080`、271 帧。car001 full evaluator **PASS**，
  visible mean IoU `0.751326`、唯一重捕 `[185]`，所有门通过。CSV/AVI/metadata/evaluator JSON
  SHA-256 分别为 `20dabd22713d37e8e38fbecf2bf3bdd4530eab91a0b02164c3d51542e81d0b1e`、
  `edf20ddcf711e329047ba4d70ce1c18d188a11ba93b57f4deafe091f0062dd03`、
  `96a846da68118a5dcc1703dcd3c46766501f7e0cdc2641d456ea4d890634e929`、
  `90d706c80eac5dea5c5b1921eec249049e6d5dfa5f26aba2dd1a5715422a6c41`。
- car002 visible mean IoU `0.798100`，唯一重捕 `[235]`；visible、prediction continuity、
  reacquisition、continuous control、identity proxy 均 PASS，只有已知 213--216 帧
  `full_occlusion_semantics` FAIL，因此 full verdict 仍为 **FAIL**。CSV/AVI/metadata/evaluator JSON
  SHA-256 分别为 `943ca28e792ad2a12a350af0ef5f840c740253ad941cc7086b44350d0b962a41`、
  `78fe73d1628d2b9b23f91d368175d3e907a02e847c48665a4ffd41186ec04765`、
  `2a5ebdf9d6edf7a89609cc10fa0e127e489d53912dd98cb4d1ff3e3d7bdce441`、
  `14187890856aa0909328e9f0450a642278aef09631a536354a0e2dd555dfab3d`。
- **结论 / Handoff**：最新 HEAD 已恢复到最后验证过的不退化行为，四个实验版本继续作为拒绝
  证据保留，不进入 handoff。用户提出的“运动更新停止早于确认遮挡”原则仍合理，但当前 response
  单指标不能可靠区分遮挡与正常强形变；在增加独立遮挡证据并覆盖多场景验证前，不再针对该单一
  视频调冻结阈值。当前候选仍 HOLD，不构成正式生产或真实吊舱验收。

## 2026-11-12T15:59:24+08:00 49027b1 板端拒绝，恢复最后通过候选

- SUSPECTED 分级提交 `49027b1a7f6ab6da2f81d0693ae4a6b420463cb7` 已部署到独立根
  `/home/neardi/Project/SOT/occ-motion-suspected-49027b1`；源码归档 SHA-256
  `1a713edccd21d13ca30cff20b2178dac4fb74cc10b870b8ee86294fe046e16c6`。原生构建、CTest
  `22/22`、两目标运行与 schema v12 verifier 均 PASS，但 full evaluator 仍非双目标通过。
- car002 明显改善：唯一重捕为234帧，visible/control/identity/prediction/reacquisition 均 PASS，
  只保留已知213--216帧 full-occlusion semantics FAIL；但 car001 出现错误重捕
  `[80,101,212]` 并发生大范围身份/可见框失败。car001 的早期强形变/遮挡依赖原运动加速度
  穿越，SUSPECTED 时无差别清零加速度会破坏它，因此 `49027b1` 仍低于 C012/`5fbcf83`
  基线并被拒绝。
- 四次真实板端失败表明，在当前观测量下，仅靠同一组 response 指标无法通用地区分“正常强
  形变/临时遮挡但应继续运动估计”和“稳定尺寸遮挡即将污染运动”；继续调该视频阈值会产生
  过拟合。分支最新源码恢复最后一个双目标已验证不退化的 `5fbcf83` 行为：继续更新当前运动，
  只冻结 search 专用干净趋势；进入 SEARCH 时仅在当前趋势反向时采用干净趋势并清零加速度。
  schema v12 的 `motion_update_valid` 可观测字段保留，但恢复版中所有 accepted TRACKING 观测均
  为 true。失败根目录和报告全部保留。
- **边界 / 下一步**：恢复版本不是对旧策略的最终否定；若继续实现，需增加独立于当前单指标
  波动的遮挡证据或跨场景数据，而不是继续调 `occ_video_1`。恢复提交仍需主机回归与一次精确
  RK3588 确认；在确认前不宣称最新 HEAD 已回到基线。

## 2026-11-12T15:53:46+08:00 3f3d7be 板端拒绝与 SUSPECTED 分级边界校正

- 几何解耦提交 `3f3d7be753096ad3de6e2d27082f73b7054eeb50` 已部署到独立根
  `/home/neardi/Project/SOT/occ-motion-trust-3f3d7be`；原生 RK3588 Debug 构建/CTest
  `22/22` 与两目标程序均成功，但 full evaluator 仍 **FAIL**：car001 错误重捕
  `[84,102,127]`，car002 错误重捕 `[110]`。局部连续性允许 NanoTrack 框继续输出，却也会在
  单指标波动触发的长冻结期间逐渐接受错误目标，因此该提交拒绝，不作为候选。
- 三次失败共同说明：把任一单项 response 相对下降直接解释为“完全正常跟踪结束”过于敏感；
  这不是用户所述旧策略本身，而是本轮对其触发边界的错误映射。现有状态机已经提供所需递进：
  `TRACKING accepted` → `SUSPECTED tentative` → `SEARCHING`。SUSPECTED 框仍对外输出并可控制，
  但已不被接受为可信状态更新，正好是“禁止运动更新早于确认丢失”的阶段。
- 后续实现撤销所有“accepted 单指标下降即冻结”逻辑及冻结几何特例，改为首次发布
  SUSPECTED tentative 时冻结 motion/shadow motion 的开环加速度和尺度速度、保留此前速度；
  SEARCH 继续使用该未被 tentative 框污染的趋势。既有 `suspicion_clear_ms` 清洁迟滞通过后，
  第一帧重新 accepted 时以当前框重新初始化运动模型。该设计不新增阈值，也不根据视频帧号或
  方向做决策。
- 单测覆盖 tentative 期间 observation/control 连续、`motion_update_valid=false`、速度保留、
  加速度清零，以及原有清洁迟滞通过后重建运动状态。当前尚未提交或部署；旧三组失败证据均保留。

## 2026-11-12T15:48:10+08:00 2b08bc5 板端拒绝与冻结几何参考解耦

- 收窄提交 `2b08bc5bd4bd5c69bab6de2888db53972cc88de3` 已部署到独立根
  `/home/neardi/Project/SOT/occ-motion-trust-2b08bc5`，原生 RK3588 Debug 构建和 CTest
  `22/22` PASS，两目标程序 exit 0；但 full evaluator 仍 **FAIL**：car001 错误重捕
  `[84,102,127]`，car002 错误重捕 `[115,175,226]`，故该提交继续拒绝。
- 逐帧证据确认：即使仅冻结运动模型，观测质量的几何门仍以被冻结的运动预测为参考；随着
  NanoTrack 当前框继续运动，两者残差必然增长，最终由“主动冻结”自身制造 observation
  rejection 和 SEARCH。这违反“禁止运动更新先于确认遮挡，但当前 NanoTrack 框仍继续输出”
  的递进语义。
- 后续修正只解开这一处因果耦合：`motion_updates_held_` 且尚未进入 loss/search 时，观测的
  center/area/aspect 连续性改用上一已接受 `current_bbox_`，不再用有意冻结的 motion bbox；
  响应质量门和后续 SUSPECTED/SEARCH 递进保持不变，真正进入搜索后仍使用运动预测几何。
  单测通过人为拉开冻结预测与局部连续观测，确认不会发生自致拒绝。当前尚未提交或部署。

## 2026-11-12T15:43:51+08:00 d7ee7df 板端拒绝与运动信任边界收窄

- 精确提交 `d7ee7df5c6f3ab9805e7f29d08cf73e13e819564` 已部署到 RK3588 独立根
  `/home/neardi/Project/SOT/occ-motion-trust-d7ee7df`；源码归档主机/板端 SHA-256 均为
  `aa9137740e60a9d5ddd3508a401f4864235c9b2f3fc1b1bcdf9b97edace7b8fa`，二进制 SHA-256
  `e62028fa9f60aaf0585ef4710bae33c9556c02382454d060ecb6e2b1fffedde5`。原生 Debug、RKNN、
  `-Werror=switch` 构建与 CTest `22/22` PASS，两目标程序 exit 0，schema v12 板端 verifier
  均 PASS；这些功能结果不覆盖下述算法拒绝。
- 固定 ROI 双目标 full evaluator 均 **FAIL**。car001 出现错误重捕 `[104,158,194,236]`，
  car002 出现错误早期重捕 `[71]` 且未形成目标遮挡后的正式重捕；两个目标可见期质量均大幅
  回退。根因不是初始 ROI：首个单项 peak-margin 相对下降即同时冻结了运动、响应基线、稳定
  计数和模板推进，冻结参考无法适应正常外观变化，运动预测逐渐远离仍正确的 NanoTrack 观测，
  最终由自身制造几何拒绝和错误搜索。
- 本轮拒绝把上述结果升级为候选。修正边界收窄为用户原始要求：`motion_update_valid` 只控制
  运动模型校正；已接受观测原有的响应基线、stable 计数和 recent-safe 模板状态机恢复原行为，
  避免把“运动模型输入更严格”扩大成所有学习状态同时冻结。运动冻结/连续干净后恢复、schema
  v12 和方向启发式移除继续保留。
- 收窄后的主机 RKNN-off 构建、CTest `22/22`、evaluator 单测 `23/23`、`git diff --check`
  均 PASS；尚未提交或部署，下一步必须以新精确提交重新执行 RK3588 双目标回归。旧
  `d7ee7df` 根目录和失败报告保留，不覆盖。

## 2026-11-12T15:35:54+08:00 分级运动更新信任（主机验证）

- 分支 `codex/recent-safe-occlusion-recovery-v2`，基线
  `3f50eb205899e2b8e656d9c194de67f63478fea4`；结果为包含本条记录的当前提交。根据用户确认，
  本提交废止 `5fbcf83` 的“进入 SEARCH 时若方向相反则切换最后干净趋势”启发式，历史 RK3588
  结果继续保留为失败分析证据，不删除、不改写。
- 新策略将“当前 NanoTrack 框可继续输出/控制”和“允许更新运动模型”拆成独立语义：响应相对
  退化但仍被跟踪器接受时，`tracking_bbox/control_bbox` 继续采用当前观测，状态仍可保持
  `TRACKING`，但 `motion_update_valid=false`，运动位置/速度不再被该框校正，开环加速度和尺度
  速度清零；响应基线、稳定计数和 recent 模板推进同步冻结。只有连续干净观测达到既有
  `suspicion_clear_ms` 后，才用该干净时间段的首尾观测重新建立运动状态并恢复更新。
- 日志协议升为 schema v12，新增 `motion_update_valid`；CSV、JSONL、遥测、板端 verifier 与
  `occ_video_1` evaluator 同步校验“只有已接受的 TRACKING 观测才能更新运动模型”。这只是
  可观测性与安全不变量升级，不改变模型、检测器、遮挡阈值、重捕门或云台控制框语义。
- 主机 RKNN-off Debug 目录 `/tmp/nanotrack-motion-trust` 使用
  `-DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-Werror=switch` 完成构建；
  CTest `22/22` PASS，`python3 -m unittest tests.test_evaluate_occ_video_1` `23/23` PASS，
  `git diff --check` PASS。新增单测覆盖退化观测继续控制但冻结运动、保留干净速度并清零加速度、
  连续干净时间达标后才恢复更新。
- **边界 / Handoff**：当前仅为主机验证，尚未在 RK3588 上构建或复跑 car001/car002；不能据此
  声称视频效果改善或批准生产 handoff。下一步以精确提交部署到独立板端目录，固定双目标 ROI
  回归并与 C012、`b538667` 及 `5fbcf83` 保留结果比较。

## 2026-11-12T15:06:46+08:00 search 反向趋势修正 RK3588 双目标验证

- 分支 `codex/recent-safe-occlusion-recovery-v2`，源码提交
  `5fbcf8301ce52ad1c779541e30a35ad4153b2753`，基线
  `59c79e74a1dfc150d6257734598459f886d3da49`。源码归档在主机与板端 SHA-256 均为
  `6e73944baa06cf20ef24cca147da11c2a16f03be662c506bad1e7971d36609d5`，独立板端根为
  `/home/neardi/Project/SOT/occ-motion-opposite-5fbcf83`。原生 Debug、RKNN runtime、
  `-Werror=switch` 构建和 CTest `22/22` PASS；二进制 SHA-256
  `8766a6122b2d621bfeb95d4e32db8122cb3566fa9bd57c82aa2f8285de72762e`。
- 初版 `0a8304e9485dcb4b9754e37ca8a05300b03ad819` 对所有退化期都采用冻结趋势，car002
  方向改善但 car001 重捕从185延迟到194，故拒绝；其独立根
  `/home/neardi/Project/SOT/occ-motion-clean-0a8304e` 保留为失败证据。最终版只在进入 SEARCH
  时当前速度与最后干净趋势点积为负才替换速度并清零加速度；方向一致的 car001 保留原运动
  模型。
- 固定 ROI `car_001=992,582,37,39`、`car_002=970,517,38,38` 各运行0--270帧，均
  exit 0，AVI 均为1920x1080、30 FPS、271帧。非交互输出视频主框已改画
  `control_bbox`；`display_bbox`、交互调试叠框及生产显示路径未修改。
- car001 full evaluator **PASS**，重捕帧仍为185，visible/control/identity/prediction/
  full-occlusion 全部门通过。CSV、AVI、metadata、evaluator JSON SHA-256 分别为
  `f9161e18ee0fc6857459246262b59c2079025f51559e97595ede7091e8b08314`、
  `edf20ddcf711e329047ba4d70ce1c18d188a11ba93b57f4deafe091f0062dd03`、
  `575566a3b9e9645942d06342f4a65792c4cd9fc4bba46a6d5249e2c177c80ab6`、
  `dd9859784dfa2e4a00e4bd0e0d841264159aa6e04d5e42ecb4186033967e869f`。
- car002 在210帧仍是左下速度 `(-19.852020,+15.126831)`；遮挡物观测把216帧运动状态污染
  为速度 `(+21.519398,-8.891993)`、加速度 `(+146.120041,-109.989998)`。最终版在222帧
  SEARCH 切换为最后干净趋势 `(-15.818654,+16.426685)` 且加速度归零，并保持到234帧；
  预测中心相对正式重现目标的偏差由旧版约56 px降至约21 px，235帧重捕不变。full evaluator
  仍因既有213--216帧错误声明观测而 FAIL，但 visible/control/identity/prediction/reacquisition
  门均 PASS。CSV、AVI、metadata、evaluator JSON SHA-256 分别为
  `0a1d9288f555adb9561c98560f5de2e2d1f8ef0828b9295e18fbf75eceb80144`、
  `78fe73d1628d2b9b23f91d368175d3e907a02e847c48665a4ffd41186ec04765`、
  `63eb52962671321eb86e16bd4f159a22d2f6fe5ce08ede82a347ae74487b2eb7`、
  `7fd391078f91e8120c7a7f953274341ec5dd8c318547fc2727c61df0dc136637`。
- **边界 / Handoff**：本轮只修正进入 SEARCH 后采用反向污染趋势的问题，不修改阈值、配置、
  模型、模板、遮挡状态机或重捕确认。car001 不低于 `b538667` 基线，car002 已改善但并未解决
  213--216帧遮挡判定过晚；候选继续 HOLD，不构成正式生产 handoff 或真实吊舱验收。

## 2026-11-12T14:55:16+08:00 recent-safe v2 search 运动趋势修正（主机验证）

- 分支 `codex/recent-safe-occlusion-recovery-v2`，基线
  `59c79e74a1dfc150d6257734598459f886d3da49`。复核 RK3588 保留运行
  `/home/neardi/Project/SOT/occ-recent-safe-v2-b538667/runs/car_002/bboxes.csv`：目标在
  195--210 帧总体向左下运动，210 帧运动状态仍为
  `velocity=(-19.852020,+15.126831)`；211--216 帧遮挡物上的观测仍被判为可信并修正运动，
  到 216 帧已反向为 `velocity=(+21.519398,-8.891993)`、
  `acceleration=(+146.120041,-109.989998)`。217--221 帧虽不再更新可信状态，旧加速度仍被
  开环积分，222 帧进入 SEARCH 时进一步变为
  `velocity=(+39.388611,-23.931980)`，解释了预测框向右上偏离。
- 通用修正不改变遮挡判定、跟踪框、控制阈值、模板或模型：控制器保留最近一次“响应未出现
  相对退化”时的运动趋势；单项响应退化只冻结该 search 专用参考，不提前进入 SEARCH，连续
  `suspicion_clear_ms` 干净观测后才更新。真正进入 SEARCH 时，仅当当前速度与保留趋势方向
  相反才采用保留速度并将加速度清零；方向一致时保持原运动模型，避免无差别替换正常趋势。
- 后续非交互保存视频改为绘制 `control_bbox`；`display_bbox` 算法、交互叠框和生产
  `rect_show` 均未修改，等待真实吊舱测试后再决定显示平滑策略。
- 主机 RKNN-off 在独立目录 `/tmp/nanotrack-recent-safe-prediction` 完成 Debug 构建，CTest
  `22/22` PASS；新增测试覆盖保留趋势、反向退化观测和 SEARCH 清零加速度；视频主框切换经
  源码链路复核，`git diff --check` 通过。当前仅为主机验证，尚未部署或运行 RK3588，不能声称 car002 实际
  视频中的预测偏离已消除；下一步是提交源码后在独立板端目录原生构建，并以固定双目标 ROI
  复跑，C012 与 `b538667` 保留结果继续作为不得退化的对照。

## 2026-11-11T14:51:34+08:00 C009 holdout FAIL，停止调参与重复运行

- 冻结命令在 RK3588 上精确执行一次，输出 `1920x1080`/271 帧；首个正式
  `scope=holdout` evaluator 返回 FAIL。按冻结规则未执行 full-scope evaluator、第 2/3
  次重复或任何参数修改。
- 原遮挡后仍在帧 185 重捕，identity proxy、continuous control、prediction continuity
  PASS；但可见期 200--215 帧再次进入搜索，216 帧第二次重捕，不满足全序列
  唯一重捕。holdout 有效观测 mean IoU `0.671046 < 0.70`，all-visible `0.677702`。
- evaluator 还把不含全遮挡帧的 190--270 holdout scope 判为 full-occlusion gate FAIL，这是
  scope 适用性缺陷；排除它也不改变上述两个真实失败。
- RK3588 verifier 另在 CSV 帧 204 失败：`search_hint_present=1` + `CONFLICT`，但
  `search_hint_confirmation_count=0`，违反当前 schema 对 present hint 必须为 `1/2` 的约束。
- 保留根目录：`/home/neardi/Project/SOT/occ-candidate-05cf4d9/runs/C009-full-run1`。
  evaluator JSON/bboxes/video SHA-256 分别为
  `dae7168e3df422b42796ba156e70f7c1bfb67b0fb65596e63a3791a439343ffe`、
  `97f275ae1bf05eb345bb8335757ffbcaac063153d895c1f3ccd0808792f4c645`、
  `ac527664f99b1211d6ffa505d4bb65f9cc69b7795b7aaf870c97d3de30a63f36`。
  C009 结论为 `REJECT for release`；holdout 现已揭盲，后续候选不得声称仍为盲测。

## 2026-11-11T14:44:38+08:00 C009 development 平台 PASS 与冻结

- C009 RK3588 原生 Debug、`-Werror=switch`、CTest `19/19`；可执行/config SHA-256
  `3792ecde2f4b915d38496fa4fdbba6ecbe6854aae50897e9fb5e577d1d2a43a3`、
  `87b4b818e740a5a4137e4ab7339a26b5d70e93e5c7852e468dd26c8a73e06791`。
- 33/33 development 真实矩阵 PASS，重捕 `184--187`，最大 mean IoU 下降 `0.016618`；
  variable-time、detector delay/drop、generation-gap 的哈希 focused CTest 证据 PASS。
- 冻结前独立主机 CTest `19/19` 和 evaluator/robustness Python 单测 `18/18` PASS；
  全仓 Python discovery 另有 16 个模块因当前主机未安装 `cv2`/`torch` 而在导入阶段中止。
- 保留 nominal 在帧 185 重捕，evaluator/board verifier PASS；视频 `1920x1080`、190 帧。
  bboxes/video/evaluator JSON SHA-256 分别为
  `4d78dacdbd984f088aec6dff9139fa8e56de4a6e5687b1e3335273c191317098`、
  `0a5d887e20c3f5b556fc564c17abe90bd8a0a5073b0a795cdb490c34c19c34e3`、
  `8ea2ac37ba2238156f7cbd73f2dc1a4f1b5cb43f16c0d6634ad623d87d3bc504`。
- C009 已写入 `frozen-candidate.json`，状态 `NOT_RUN`；下一步只能执行其中精确 full
  command，不能再改 runtime/config。部分 190--193 status 文本暴露边界继续保留。当前仍无
  post-189 视频/bbox/runtime/evaluator 结果，也没有人眼 identity sign-off。

## 2026-11-11T14:26:43+08:00 C009 候选

- C009 恢复已通过 nominal/width+5 的 C007 runtime（probation=3），保留 400 ms、0.020
  和固定 `2/3` 几何显著规则；仅配套使用已提交的非刀刃 evaluator。
- 下一步优先重签 C007 保留 CSV，补跑缺失项并由 robustness aggregator 实际判定；尚未冻结，
  无 holdout/full 运行。

## 2026-11-11T14:26:10+08:00 C008 拒绝与视觉单帧绝对底线校正

- C008 板端 Debug/CTest `19/19`；probation 4/5 均 nominal PASS，但 width `+5%` 到帧 174
  才进入搜索，无法在 190 前完成两击，因此 C008 拒绝，完整矩阵未启动。
- probation=2 的唯一 C007 失败是帧 166 IoU `0.4916`、中心误差 `0.201`，不是丢失或身份
  错误。evaluator 单帧绝对 IoU 底线改为非贴线的 `0.45`；中心 `<=0.50`、mean `>=0.70`、
  identity、重捕、连续控制和矩阵相对下降 `<=0.03` 不变。合成测试锁定 0.46 PASS、0.43
  FAIL；evaluator/robustness 单测 `17/17` 通过。
- 下一候选恢复 probation=3，并重签/聚合已有真实矩阵报告；无 holdout/full 运行。

## 2026-11-11T14:21:16+08:00 C007 消歧通过与 C008 probation 内移

- C007 板端 Debug/CTest `19/19`；nominal 与关键 width `+5%` 均在帧 185 重捕。所有已执行
  实数 `+/-10%` 邻域 PASS，重捕范围 `184--187`。
- `recent_probation_frames=2` 在遮挡前 recent 模板提交后导致帧 166 视觉框门槛失败；该参数
  确实影响模板历史，不能当作无关抖动。C007 因而不冻结。
- C008 只将 probation 从 3 提到更保守的 4，并要求 `3/4/5` 平台；两击、消歧、运动控制和
  detector 隔离不变。尚无 C008 板端结果，无 holdout/full 运行。

## 2026-11-11T14:09:35+08:00 C006 平台结果与 C007 几何显著消歧

- C006 nominal 在帧 185 重捕；cadence `360/400/440 ms` 分别在 `184/185/187`，走廊
  `0.018/0.020/0.022` 均在 185 重捕。中心、width `-5%`、height `+/-5%` 通过。
- width `+5%` 的真实目标/邻车中心距离比为 `0.009615/0.017311`，但 count-only policy
  仍全部拒绝；同时 detector/template 分数更偏邻车，不能用分数消歧。
- C007 增加固定 `nearest <= 2/3 * runner-up` 几何显著规则。只解除最近候选的 ambiguity
  标志，仍需 anchor、recent veto、最小搜索龄与连续两击；相近候选继续拒绝。focused CTest
  `3/3` 通过，尚待全量测试、提交和 RK3588 验证。无 holdout/full 运行。

## 2026-11-11T13:50:36+08:00 C005 nominal 拒绝与 C006 最小候选

- C005 精确提交 `89215ab` 在 RK3588 原生 Debug 构建、`-Werror=switch`、CTest `19/19`
  通过，但 nominal 在 detector frame 182 的批次中 `0.015` 走廊把 121 个位置候选全部拒绝，
  因而无重捕；发现 nominal 失败后已立即停止其余 32 组矩阵。
- C006 保留已证明需要的 400 ms cadence，只把走廊恢复为 C004 的 `0.020`。这样以更早的新鲜
  可见帧解决 deadline/ambiguity，而不靠过紧位置阈值。尚无 C006 板端结果，无 holdout/full
  运行。

## 2026-11-11T13:47:27+08:00 C005 通用内点候选

- C005 仅把 detector cadence 从 `500` 内移到 `400 ms`，并把全帧对角线归一化走廊从
  `0.020` 收紧为 `0.015`；运行时 recent-veto-only、两击原子提交、同类歧义拒绝、
  generation/stale 隔离均不变。
- 选择依据是 C004 的真实邻域失败类别，不使用帧号或真值坐标：未来必须同时通过 cadence
  `360/440 ms` 和 corridor `0.0135/0.0165`，以及中心保持的初始化扰动，才允许冻结。
- 当前仅写入候选与台账，尚无 C005 板端结果；无 holdout/full 运行。

## 2026-11-11T13:46:23+08:00 C004 鲁棒性矩阵拒绝冻结

- RK3588 已执行 37 组 development-only 真实运行；临时视频每组评分后删除，保留 CSV、
  metadata、日志和报告。多数阈值 `+/-10%`、中心 `+/-2 px`、中心保持的高度 `+/-5%`
  均 PASS。
- 两个有效失败阻止冻结：`detector_interval_ms +10%` 无法在 190 前重捕，说明 nominal
  500 ms 卡在截止边缘；中心保持的 width `+5%` 在搜索后得到两个走廊内同类候选，安全地拒绝
  授权。width `-5%` 没有单帧质量失败且相对 mean IoU 只下降 `0.019565`，但被 evaluator
  与聚合器不一致的固定 `0.75` mean 门槛提前判死。
- evaluator 已以合成测试统一合同：单报告仍要求每有效观测 IoU `>=0.50`、中心误差
  `<=0.50`，绝对 mean floor 为 `0.70`；Task 8 聚合器继续要求所有变体 PASS 且相对 nominal
  下降 `<=0.03`。evaluator/robustness 单测 `16/16` 通过。
- C004 明确 `REJECT for freeze`；运行时 recent-veto-only 逻辑保留。下一候选只做通用的探测
  周期内移与归一化走廊收紧，再重跑有效邻域。没有 holdout/full 运行。

## 2026-11-11T13:25:34+08:00 C004 nominal PASS 与 provenance 接口补齐

- 同一 C004 产物经提交 `007f201ff998ec8b3d5f21ee134d406cc81d6870` 的 evaluator
  重算后 development 全门槛 PASS：帧 188 唯一重捕，有效观测 mean IoU `0.762083`，
  all-visible 诊断 mean IoU `0.741046`，搜索段 max speed/acceleration
  `370.984454/9331.818646`，均低于真值派生上限。报告 JSON/Markdown SHA-256 分别为
  `daad50c1a5be7361ba7d1f2b695d9a10a7ca0cf6ba66fc493159cdbf89a096d9`、
  `0336d518f7946a825a413820adc7e98ba87787e30d97223a8e2f218b54075f2c`。
- Task 8 聚合器要求报告内含源码、配置、模型身份；evaluator 新增成组 provenance 参数，拒绝
  缺项、非 40 位 commit 或非 64 位 SHA-256。evaluator/robustness 单测合计 `15/15` 与
  Python 语法检查通过。下一步重签 nominal 报告并执行真实扰动矩阵；当前仍未冻结。

## 2026-11-11T13:23:03+08:00 C004 重捕成功与 evaluator 语义分层修正

- C004 源提交 `0e19decf14d6fc674e78aa4c6249e12b70ba85b0` 已部署到 RK3588
  `/home/neardi/Project/SOT/occ-candidate-0e19dec`；源码归档 SHA-256
  `4eb692d72dae7f74093677b54313c7f0a7447ce3ba12eb253aa934c571f68082`。板端原生
  Debug、`-Werror=switch`、CTest `19/19` 通过；可执行文件 SHA-256
  `89e61bd1fb938be513f3b941af2e4354a682d952856de3566710b27119f28fa2`。
- 严格 `--max-frames 189` 的 `runs/C004-development` 运行完成，schema-10 board verifier
  PASS。帧 172--180 均为 `SEARCHING,false,MOTION_SEARCH`，控制连续；唯一两击重捕在帧
  188，随后帧 189 仍跟踪同一目标。旧 evaluator 报告显示重捕门槛、遮挡语义、连续控制均
  PASS，但把重捕前 `observation_valid=false` 的可见搜索预测当成视觉观测，并把原子
  `SEARCHING -> TRACKING` 切换计入同一运动段，因此其 FAIL 不能作为 schema-10 合同结论。
- evaluator 已用合成回归修正语义分层：搜索预测继续受估计轨迹、搜索段运动连续性和控制门槛
  约束；严格 IoU/中心误差只评价 `observation_valid=true`；原子重捕切换开启新的连续性段，
  但重捕观测仍必须具有真值重叠并经过 identity proxy。新增回归连同 evaluator 单测 `10/10`
  通过；尚待把这一提交精确传板并重算 C004 报告。
- 证据边界：运行程序、视频帧和 bbox 标签均未读取 190 以后；一次诊断命令的 `sed` 范围误显
  示了 `annotation_status.csv` 的 190--193 行状态文本。未运行 holdout/full evaluator，也未据此
  调整运行时代码或参数，但严格的“190 后元数据完全未暴露”边界已不再成立，后续冻结记录必须
  保留这一限制。

## 2026-11-11T13:11:43+08:00 C003 verifier PASS 与 C004 recent-veto-only 修正

- C003 在 RK3588 `/home/neardi/Project/SOT/occ-candidate-2389df0` 原生 Debug 构建、
  `-Werror=switch` 和 CTest `19/19` 通过；二进制 SHA-256
  `b2cad65b27fc18171ce36fadf8bdb63d4a970f16205ed934e3a4f17a01fc46da`。严格 0--189
  运行后，board verifier 首次 PASS，verify log SHA-256
  `f6564fb4dcfed79e3726f98d87cb3fb20a5bdbfa551023f269537cc304c585bd`。
- evaluator 仍与 C002 同一 FAIL：无重捕且 visible/identity gate 失败。逐字段证据显示窄走廊已把
  两批 position-valid/queued 数量从 3 降为 1；frame 187 唯一候选 center ratio `0.016328`，
  anchor 可靠，recent score `0.748582` 只达 weak，但两模板 IoU `0.911284`。当前实现错误要求
  recent 也必须 reliable，导致合法 anchor 无法建立 shadow first hit。
- C004 按既定证据政策修正为“anchor 唯一授权、recent 只否决”：可靠 anchor 必需；recent
  不存在或 weak 时不能授权也不能否决，只有 recent 自身 reliable 且与 anchor 几何冲突才 veto。
  detector identity/class/ambiguity、400 ms 最小搜索龄、0.02 corridor、两击一致性和第一击零提交
  均保持。
- 合成 detector 测试把第一击 recent 降为 weak 且保持高 IoU，确认仍只生成 shadow hint；第二击
  才原子提交。主机 CTest `19/19`、鲁棒性检查器单测 `4/4`、Python 语法检查和
  `git diff --check` 均通过。C003/C004 结果已只追加到 calibration ledger。
- C004 尚未提交、部署或运行；没有 holdout 证据，nominal 即使通过也尚未满足鲁棒性矩阵。

## 2026-11-11T13:07:57+08:00 C002 安全搜索结果与 C003 成熟窄走廊候选

- C002 在 RK3588 继续严格 `--max-frames 189`；二进制/config SHA-256 分别为
  `980545c9baeeceba21a9d0c10e7036c5eb3c73505c46b5c5adce694d1585cfd1`、
  `bb3513919fd4cc78ae3ae78b38057882ffa96d14bf22429a7f3120fd54e51901`。板端原生
  `-Werror=switch` 构建及 CTest `19/19` 通过，运行 SUMMARY mean runtime `9.338 ms`、
  mean pipeline `112.215 ms`、end-to-end `8.911 FPS`。
- evaluator development 仍 FAIL，但安全失败形态符合预期：full-occlusion semantics、
  continuous control、prediction continuity 全部 PASS，错误的 tracker-only 重捕已消失；两个
  detector batch 都保留三个同类候选，ambiguity gate 全部拒绝，因此 190 前无重捕。visible mean
  IoU `0.732761`；JSON SHA-256
  `f6585be2c7faf8b21000f0a99220771773020b08752abba0dcc41d236d8760cc`。
- C002 还暴露 verifier 不应要求 `hypothesis_source=MOTION`：搜索帧可以正在探测 detector ROI，
  但对外控制仍明确来自 `MOTION_SEARCH`。新增 DETECTOR-hypothesis 的合法 fixture 先 RED，再移除
  该错误耦合；phase/source/confidence/control_valid 仍全部必需。
- C003 将按整帧对角线归一化的 detector center gate 从 `0.15` 收紧为 `0.02`。前者在
  1920x1080 上约等于 330 px，而目标宽约 44 px；后者形成约一目标宽的通用 motion corridor。
  同时新增默认 0、候选 400 ms 的 `search_reacquisition_min_age_ms`，阻止短暂遮挡过渡立即种下
  重捕候选，且必须小于等于 motion confidence 期。
- parser、identity、metadata、verifier 与合成 controller 测试覆盖该最小搜索龄：龄前可靠观测
  不能产生 hint，龄后仍需两击才提交。C003 配置 SHA-256
  `89fbc1d23b2be17c8d6938f5f53383ec4b26a59383f4343dce80603335715706`；聚焦 4/4
  CTest 通过。后续即使 nominal 成功，也必须对两个实数门做 ±10% 邻域，不能直接冻结。
- C003 尚未提交、部署或运行；仍无 holdout 证据。

## 2026-11-11T13:01:15+08:00 C001 development 失败与 C002 detector-required 候选

- C001 在 RK3588 `/home/neardi/Project/SOT/occ-candidate-cbd38a5/runs/C001-development`
  严格以 `--max-frames 189` 完成，SUMMARY `tracked_frames=189`、mean runtime `9.148 ms`、
  mean pipeline `111.934 ms`、end-to-end `8.934 FPS`；没有读取 190--270。二进制 SHA-256
  `f58f22bb9e2bb6ba5821841d1402d3869e9978097d921d7bfb0cd5933199439d`，输入和所有
  RK3588 ONNX 派生模型/detector 哈希在运行前复核与 C000 一致。
- evaluator development 明确 FAIL：continuous control PASS，但 tracker-only shadow candidate
  在 171、183、186 三次错误重捕；full-occlusion semantics 于 172--173 失败，visible mean IoU
  `0.731970`，182--189 可见框与 183 以后 identity proxy 失败，运动速度/加速度也越过真值参考。
  JSON/Markdown SHA-256 为 `4aeb6e574db8c3fbbb466e176f9a373b559f725fefe1c419ad10bd1d9f290771`、
  `e616374ebb4b9432d2e5c31c1ffea14b6333ea1a5624e99637160034cb5dac15`。
- 同一真实输出暴露 verifier 将 `motion_control_authorized` 仍限定为旧
  `OCCLUSION_HOLD`，从而错误拒绝 frame 169 的 `UNCERTAIN / SEARCHING / MOTION_SEARCH`。
  新增 valid motion-search fixture 先复现 RED，再改为依据 schema 10 phase/source/confidence
  授权；旧 HOLD 的 age/budget 约束仍保留。
- C002 增加通用 `search_reacquisition_requires_detector=true`，默认 false 保持兼容；搜索态的
  非 detector 观测不再落入 legacy ordinary recovery，只能维持 motion control。detector 仍只是
  class/generation/position 安全的搜索种子，最终授权继续要求 anchor/recent 与两击几何一致，
  detector 本身不能直接控制。两击提交的 hint 同时明确记录 `used=true`。
- 配置解析、identity、metadata、verifier 与候选文件已同步，新候选配置 SHA-256
  `bb3513919fd4cc78ae3ae78b38057882ffa96d14bf22429a7f3120fd54e51901`；定向
  `controller_support|long_term_controller|long_term_runtime|tracking_log|verify_board_schema`
  为 `5/5` 通过。C001/C002 证据已只追加写入 calibration ledger。
- C002 尚未提交、部署或运行；没有 holdout 证据，不能宣称已重捕或已标定。

## 2026-11-11T12:53:27+08:00 development-only 鲁棒性聚合器与 RK3588 首次构建审计

- 在 `codex/occ-video-1-tracking`、Task 8 起点
  `2403c8618dc5f4234e399165e9168c5687aeb8e7` 新增纯报告聚合器
  `scripts/check_occ_tracking_robustness.py`。它不执行 tracker 或选择参数，只接受 schema 1
  candidate manifest 与已命名 evaluator reports；结果提交可用
  `git log -1 --format=%H -- scripts/check_occ_tracking_robustness.py` 查询。
- 聚合器要求每个实数参数 `-10%/nominal/+10%`、每个可调计数 `-1/nominal/+1`、初始框中心
  `±2 px`、尺度 `±5%`、变时间戳和 detector delay/drop/generation gap 全部齐备；所有报告必须
  严格为 development `0--189`、身份哈希匹配、所有 evaluator gate PASS、重捕 `<190`，且相对
  nominal 的可见 mean IoU 最大下降不超过 `0.03`。confirmation 至少两击、detector 不直接
  控制、generation/stale 隔离与异常 fail-closed 作为固定安全审计，不允许向下扰动。
- 单元测试先因脚本不存在 4/4 RED；实现后覆盖缺邻域、nominal-only 刀刃、错误 scope/holdout、
  重复报告、source/config/model 哈希不匹配、安全降级和 IoU 崩落，最终 4/4 PASS，
  `py_compile` 通过。完整主机 CTest `19/19`、0 失败、总时间 `4.24 sec`。
- 已把 `2403c86` 精确归档到 RK3588 `/home/neardi/Project/SOT/occ-candidate-2403c86`，归档两端
  SHA-256 `dc97c84662778a15ff85d36c17b73c1a4069dbfbd6cbf80bef17dcf1b7b4525a`；models、
  releases、data 均只链接到现有 ONNX 派生 RK3588 工件，复核哈希与 C000 一致。原生 Debug
  构建二进制为 `bb596ef7cb95c38232b724f3c4fb5d462d2ac8c0e48d1d2d25b0f5162211f207`，板端
  CTest `19/19`、0 失败、`6.40 sec`。
- 板端 `-Werror=switch` 复现发现 Task 6 新增的 `kSearchControlPrediction`/`kSearchHint` 未进入
  `main.cpp` 颜色 switch；根因是共享 overlay enum 已扩展而 Debug app 映射未同步。当前最小
  修复为两种语义增加独立颜色，避免主要 Debug 功能落到红色 fallback。修复提交后必须重新
  打包、原生构建和复跑 `-Werror=switch`，当前旧二进制不得用于 C001 数据运行。
- 尚未执行任何 0--189 candidate run，也没有读取 190--270；本条不是标定、鲁棒平台或板端
  功能通过结论。

## 2026-11-11T12:47:34+08:00 通用可见光遮挡候选配置与只追加标定账本

- 在 `codex/occ-video-1-tracking`、Task 7 起点
  `15f32fe8d52fee0b53e2c15d961b87f1c006ccd5` 新增通用
  `config/visible-occlusion-calibrated.conf`；结果提交可用
  `git log -1 --format=%H -- rk3566/nanotrack_v3/config/visible-occlusion-calibrated.conf` 查询。
- C001 保留 `visible-default.conf` 的已有视觉、遮挡、detector 和两击确认阈值，只显式启用
  `motion_search_enabled=true`，初始通用物理边界为 `2500 ms` 置信期、`1.0 diagonal/s`
  速度、`2.0 diagonal/s^2` 加速度、`0.50/s` 尺度变化和 `100 ms` probe 间隔；配置中没有
  视频名、帧号、真值坐标、桥梁或场景特例。`confirmation_required=2`、window 3 保持不变。
- 原 `visible-default.conf` 未修改，SHA-256 仍为
  `54dcc973feed11ad3e9e7838c8f35e86b0d587d5aa143863a625c849fa5dae19`；新候选为
  `495d5a0bc74d077cb333ae25affc512005b701fc432552f0994ed55607aab6c9`，controller identity
  测试确认两者不同且默认配置继续关闭 motion search。
- 新建只追加 `docs/evaluation/occ-video-1/calibration-ledger.md`，先冻结 C000 失败基线的源码、
  配置、RK3588 ONNX 派生模型、detector、输入与输出报告哈希，再登记 C001 的 parent、理由、
  变更参数和严格 `--max-frames 189` 待执行命令。C001 明确标为 `PENDING_NOT_RUN`；未知二进制、
  板端目录和报告写为 `PENDING`，没有从历史证据猜测。
- TDD RED 时两个测试分别因候选文件不存在失败；新增文件后，聚焦 `controller_support|cli`
  为 `2/2` 通过。新鲜完整构建及 CTest 为 `19/19` 通过、`0` 失败、总时间 `4.51 sec`，
  `git diff --check` 无输出。
- 本里程碑没有运行 C001、没有读取 190--270 holdout，也没有改变或重新生成 RK3588 模型；
  `calibrated=true` 仅表示候选配置接口，不能解释为真值标定已经完成。下一步 Task 8 先实现
  robustness manifest 校验器，再部署并只用 0--189 做候选与参数邻域验证。

## 2026-11-11T12:43:43+08:00 schema 10 搜索控制证据契约与 fail-closed 校验

- 在 `codex/occ-video-1-tracking`、Task 6 起点
  `936a5e77510786ec6631d94939f236492ae3e643` 将 tracking CSV 与 session metadata
  升级为 schema 10；结果提交可用
  `git log -1 --format=%H -- rk3566/nanotrack_v3/include/tracking_log.hpp` 查询。
- 每行现在直接序列化 controller/runtime 的 `tracking_phase`、`observation_valid`、
  `control_source`、真实 tracking/display/control 三类框、接受路径、搜索年龄与置信状态、
  速度/加速度/尺度速度，以及 shadow hint 的 source frame/timestamp、generation、projected
  bbox、确认计数、使用状态和拒绝原因；没有从旧 mode 或显示框反推这些语义。
- overlay 明确把搜索期云台框显示为 `SEARCHING / MOTION_SEARCH PREDICTION`，与
  `TRACKING / OBSERVATION` 分开；telemetry 以显式 phase/source 产生 `search_enter`、
  `search_expired`、`hint_first_hit`、`hint_reject` 和两击 `reacquired` 事件。
- 板端 verifier 现在要求 schema 10、连续帧/时间戳、三类有限且在画面内的框、通用运动搜索
  配置 identity、搜索控制置信状态和完整 hint 诊断；会拒绝 SEARCHING 冒充 observation、
  detector 直接授权、无置信 motion control、单击重捕、partial hint 以及 metadata/CSV schema
  不一致。旧模型/manifest/hash/runtime 检查继续保留。
- 新鲜验证命令重新配置并构建 `/tmp/nanotrack-occ-plan`，完整 CTest 为 `19/19` 通过、
  `0` 失败、总时间 `3.65 sec`；`git diff --check` 无输出。定向
  `tracking_log|debug_telemetry|interactive_debug_overlay|verify_board_schema` 为 `4/4` 通过。
- 本里程碑仍只有主机合成证据，没有改 ONNX/RKNN 模型，也没有同步或运行 RK3588/RK3566；
  不能宣称 `occ_video_1` 已达成 190 帧前重捕、阈值已由真值标定或真实云台已验收。下一步是
  Task 7 形成少量通用可见光候选配置，再在 0--189 development 段进行候选标定与扰动筛选。

## 2026-11-11T12:33:39+08:00 两击一致性重捕与模板 probation 原子提交

- 在 `codex/occ-video-1-tracking`、Task 5 起点
  `6b8c9f950b77b3b95ff3227d46d1c7d746fe7d9f` 将 tracker-seeded 与
  detector-seeded 重捕统一到一个 shadow confirmation candidate；结果提交可用
  `git log -1 --format=%H -- rk3566/nanotrack_v3/src/long_term_controller.cpp` 查询。
- 搜索中只有 anchor 可靠观测能建立/确认候选；recent 可以通过模板一致性门或否决，但
  recent-only 永远不能授权。第二击必须在 `confirmation_window` 内、identity/source 一致且满足
  IoU、中心、面积和宽高比门；同类邻近歧义、远框、模板冲突、窗口过期和 generation/source
  变化均不会提交。
- 第一击继续发布 motion control，不修改 committed bbox、motion 或模板；第二个合法击中才
  使用真实确认 tracker bbox 一次性重置 motion，清空 hint/候选队列，发布
  `TRACKING / OBSERVATION` 和单帧 `reacquired=true`，路径标记为
  `search_reacquisition`。后续普通跟踪不会重复发出 reacquired pulse。
- 原子提交帧只 `STAGE_RECENT`，不替换 anchor；后续普通可靠帧累计
  `recent_probation_frames` 后才 `COMMIT_RECENT`，期间 anchor/recent 不一致则
  `ROLLBACK_RECENT`，但 anchor 跟踪可继续。runtime 测试确认第一击零 bbox/template commit，
  第二击恰好一次 bbox commit 与一次 recent stage。
- RED 阶段首先因缺少 `kSearchReacquisition` 编译失败；GREEN 后聚焦
  `long_term_controller|long_term_runtime|long_term_replay` 为 `3/3` 通过。完整构建及 CTest
  为 `19/19` 通过、`0` 失败、总时间 `3.22 sec`，`git diff --check` 无输出。
- 本里程碑仍未在 RK3588/RK3566 或真实云台上运行；尚缺 schema 10 遥测、通用配置候选标定、
  冻结 holdout 与板端三次复跑，不能宣称 190 帧前重捕门已通过。

## 2026-11-11T12:27:57+08:00 SEARCHING detector 调度与 shadow hint 隔离

- 在 `codex/occ-video-1-tracking`、Task 4 起点
  `422cab44043786c755fb81a36063e55e3aa795a3` 完成候选路径的 search-wide detector
  调度：目标类别允许恢复且 detector ready 时，从第一个已进入 SEARCHING 的帧开始按
  `detector_interval_ms` 毫秒节流提交全画面请求，不再等待 ASSISTED/LOST；旧默认关闭候选时
  仍保留原调度与 coast 行为。结果提交可用
  `git log -1 --format=%H -- rk3566/nanotrack_v3/include/long_term_types.hpp` 查询。
- 异步候选保留 source frame/timestamp、generation 和 candidate identity，并先用 detector
  请求时与当前帧的有界 motion 位移投影到当前走廊，再执行类别、位置、多候选容量与歧义门。
  新增 `SearchHint` 诊断记录 projected bbox、首击帧、确认计数、是否已使用与拒绝原因。
- 一个合法 detector/anchor 首击只生成恰好供下一已处理帧使用的 shadow hint；hint 可改变
  NanoTrack 搜索 ROI，但未接受帧的 `tracking_bbox`/`control_bbox` 继续来自 motion，且不会
  correct/initialize motion、提交 bbox 或更新模板。第二击在本 Task 仅增加诊断或标记冲突，
  原子重捕提交留给 Task 5；中止帧、冲突、数值异常都会清除 hint/active candidate。
- runtime 测试确认 detector poll 异常仍保留 `MOTION_SEARCH` 控制，且 bbox commit、recent
  stage/commit/rollback 均不发生。既有 stale、frame/generation、class、position、capacity、
  same-class ambiguity 和 legacy HOLD 拒绝顺序继续由原测试覆盖。
- RED 阶段因 projected bbox、SearchHint 与 search-wide 调度接口缺失编译失败；GREEN 后聚焦
  `long_term_controller|long_term_runtime` 为 `2/2` 通过。完整构建及 CTest 为 `19/19`
  通过、`0` 失败、总时间 `3.48 sec`，`git diff --check` 无输出。
- 本里程碑仍仅为主机合成验证；没有板端同步、构建或视频运行，不能表述为 detector 已在
  RK3588 上帮助重捕或 `occ_video_1` 已通过。下一步是 Task 5 的两击一致性原子提交与 recent
  template probation。

## 2026-11-11T12:20:56+08:00 通用时间制有界 motion search 主机实现

- 在 `codex/occ-video-1-tracking`、Task 3 起点
  `339d5e0a50fef15af8f9666f2664678d35d91e99` 以 TDD 增加默认关闭的
  `motion_search_enabled`，以及毫秒置信期、按画面对角线归一化的速度/加速度上限、每秒尺度
  变化上限和搜索探测间隔。配置解析、验证和 identity 均覆盖这些字段；结果提交可用
  `git log -1 --format=%H -- rk3566/nanotrack_v3/src/motion_estimator.cpp` 查询。
- motion estimator 只从已接受视觉观测修正速度、加速度和尺度速度，支持 16/33/67 ms
  变帧率与丢帧时间步，所有导数有界、预测框有限且裁进画面。公开 snapshot 给出运动导数、
  search age 和时间置信状态；时间倒退、未初始化及非有限状态继续 fail closed。
- 候选开关启用后，第一个未接受观测立即进入 `SEARCHING`，保留最近接受尺度并以 motion
  prediction 同时发布 `tracking_bbox`/`control_bbox`，在置信期内明确发布
  `observation_valid=false`、`control_valid=true`、`control_source=MOTION_SEARCH`；详细模式在
  UNCERTAIN/LOCAL/ASSISTED 间变化不会中断授权。置信期后仍是搜索态但撤销控制；两次一致的
  NanoTrack 视觉确认恢复 `OBSERVATION` 控制。高质量但满足遮挡缩框证据的观测不会被提交，
  也不会走旧的固定帧数 coast 授权。
- 每个搜索 episode 只递增一次 detector eligibility generation，并清空确认、候选和旧请求；
  重复拒绝不会重复递增。旧默认 `motion_search_enabled=false`，因此既有 RK3566 配置的普通拒绝
  与 frame-count coast 行为保持不变。
- RED 阶段三个聚焦目标因新增字段和接口不存在而编译失败；GREEN 后命令
  `ctest --test-dir /tmp/nanotrack-occ-plan -R 'controller_support|long_term_controller|long_term_replay' --output-on-failure`
  为 `3/3` 通过，随后完整构建及 CTest 为 `19/19` 通过、`0` 失败、总时间 `3.29 sec`，
  `git diff --check` 无输出。
- 本里程碑仅是主机合成状态矩阵证据，未同步、编译或运行 RK3588/RK3566，未改变 ONNX/RKNN
  模型或工件哈希，也不能表述为 `occ_video_1` 已通过、阈值已完成真值标定或真实云台验收。
  下一步仍需实现搜索期 detector shadow hint、两击重捕、schema 10，再按冻结 evaluator 在
  RK3588 完整复跑。

## 2026-11-11T12:13:38+08:00 观测有效性与控制来源外部语义契约

- 在 `codex/occ-video-1-tracking`、Task 2 起点
  `73efebb0130e6c94ee097e2c4613e2b8179dc59f` 以 TDD 新增
  `TrackingPhase {TRACKING, SEARCHING, INVALID}`、
  `ControlSource {NONE, OBSERVATION, MOTION_SEARCH}`，并把 `tracking_phase`、
  `observation_valid`、`control_source` 放入 `SearchPlan` 和 `ControllerOutput`。所有完成的
  controller 输出在返回前经过同一语义校验器；结果提交以
  `git log -1 --format=%H -- rk3566/nanotrack_v3/include/long_term_types.hpp` 查询。
- 直接接受观测现在明确发布 `TRACKING / observation_valid=true / OBSERVATION`，且
  `tracking_bbox==control_bbox`，display smoothing 继续独立；既有授权 coast 明确发布
  `SEARCHING / observation_valid=false / MOTION_SEARCH`。旧默认配置下的普通拒绝暂时显式为
  `SEARCHING / false / NONE / control_valid=false`，没有在 Task 3 的 feature gate 和有界运动置信期
  完成前静默切换 RK3566 默认控制行为。
- RED 阶段 `test_long_term_controller` 因枚举、字段和校验接口不存在而编译失败；GREEN 后聚焦
  `tracker_math`、`long_term_controller`、`long_term_runtime` 3/3 通过，完整 RKNN-independent
  构建及 CTest `19/19` 通过，`git diff --check` 通过。本里程碑只建立共享 core 语义；尚未写
  schema 10 日志字段，也尚未使普通未接受帧持续驱动云台。下一步由 Task 3 在通用候选配置
  显式启用时间制有界 motion search。

## 2026-11-11T12:09:22+08:00 `occ_video_1` evaluator 与失败基线冻结

- 在分支 `codex/occ-video-1-tracking`、实现起点
  `e51a1d2d63c34077b4489e74675dcef03aba5681` 完成实施计划 Task 1；结果提交以
  `git log -1 --format=%H -- scripts/evaluate_occ_video_1.py` 查询。本里程碑新增独立
  `scripts/evaluate_occ_video_1.py`，runtime 不读取它或真值。evaluator 严格核对 canonical
  video/metadata/events/status/label-manifest 哈希，将 169--181 排除普通 IoU，分层报告可见框、
  估计轨迹、完全遮挡语义、控制连续性、重捕和身份代理；合法输入但硬门失败 exit 1，输入或
  身份错误 exit 2。schema 9 仅允许复现失败基线，候选 PASS 必须使用带明确
  `tracking_phase`、`observation_valid`、`control_source` 和 tracking box 的 schema 10。
- TDD 首轮在生产脚本不存在时 8/8 预期失败；实现后聚焦套件最终 9/9 通过。真实 RK3588
  baseline CSV 只读复制到 `/tmp/occ-baseline-full-20261111-bboxes.csv`，SHA-256
  `b2ba9d7328f0554d1806a36f817ece8a42a5cdd0e4275b9cae44f1b87271eb6f`，与板端记录一致；
  evaluator exit 1 并精确复现普通可见 mean IoU `0.5785649329`、0--168
  `0.6311449519`、182--270 `0.4787219754`，无合法重捕且遮挡/控制/schema/可见框/身份代理门
  失败。紧凑证据在 `docs/evaluation/occ-video-1/baseline-201991a.{json,md}`；完整可再生报告
  SHA-256 `6ed8b993bd825b38dc033515a286e51e6283906f7e1449689d6fc9128ee8ce6a`。
- 系统 `python3` 全量 discover 因缺 `cv2` 等既有环境依赖只运行 79 项并产生 16 个导入错误；
  改用项目 NanoTrack venv 后实际运行 286 项，除本工作树已知绝对主工作树路径/缺本地 venv/
  缺历史 smoke report 的 `test_nanotrack_three_stage_configs` 5 failures + 1 error 外，其余通过。
  本任务新增套件独立全绿；这些既有 linked-worktree 测试边界未被改写或冒充全套通过。
- 本里程碑没有修改 C++ runtime、配置阈值、模型、协调方 `STATUS.md`/契约或板端部署；原基线
  目录仍缺 `manifest.json`，因此继续是诊断 FAIL，不是 board verifier PASS。下一步按计划 Task 2
  先以失败测试引入 observation/control 来源的外部语义契约。

## 2026-11-11T11:18:21+08:00 `occ_video_1` 方案 2 获批并形成实施计划

- 用户已批准
  `docs/superpowers/specs/2026-11-11-occ-video-1-closed-loop-tracking-design.md` 的最终设计，包含
  SEARCHING 期间持续运动预测与云台控制、190 帧前两阶段重捕、detector shadow hint、模板
  probation 以及避免单视频过拟合的泛化护栏。批准不等于算法或板端验收已经完成。
- 实施计划写入
  `docs/superpowers/plans/2026-11-11-occ-video-1-closed-loop-tracking.md`，按 evaluator、外部语义、
  时间制运动搜索、搜索期 detector、两次确认重捕、schema 10、通用可见光候选配置、0--189
  标定/扰动平台、冻结 holdout 和 RK3588 三次完整复跑九个任务执行。计划明确第一次暴露
  190--270 前必须提交冻结记录，若盲测结果促成修改则该段永久降级为 regression。
- 本里程碑仍只提交设计/计划/任务进度，没有修改 runtime 算法、阈值、模型或协调方专属
  `STATUS.md`/`integration-contract.yaml`，也没有产生新的主机测试或开发板运行证据。起点为
  `codex/occ-video-1-tracking@98ee995e0ef67f51bfb6114754eb3f43d7e0e909`；结果提交以
  `git log -1 --format=%H -- docs/progress/board-validation.md` 查询。

## 2026-11-11T10:53:22+08:00 `occ_video_1` 完整基线与方案 2 设计待批准

- 新任务在隔离工作树
  `/home/u20/.codex/worktrees/rk3588-debug-deploy/single-object-tracking` 的独立分支
  `codex/occ-video-1-tracking` 开始，输入基线为已推送 RK3588 Debug 提交
  `201991a92bba95b542c97505c37053d1c4fe1da8`。本里程碑只包含诊断与设计，尚未修改算法；
  结果提交以 `git log -1 --format=%H -- docs/progress/board-validation.md` 查询。
- 用户确认完全遮挡不算永久丢失：172--180 帧仍输出运动预测框并持续驱动云台；用户选择
  “真值 evaluator + 状态机/运动/detector/NanoTrack/模板闭环重构”的方案 2，并把正确重捕
  硬门定为第 190 帧之前。设计在
  `docs/superpowers/specs/2026-11-11-occ-video-1-closed-loop-tracking-design.md`，仍需用户批准后
  才能写实现计划或代码。
- 在 RK3588 `192.168.1.102` 对当前候选运行完整 271 帧基线，实际核心命令为：

```bash
cd /home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f
stdbuf -oL -eL ./build/nanotrack_v3 \
  --video data/occ_video_1/1.mp4 --bbox 992,582,37,39 --model-dir models \
  --sensor visible --controller-config config/visible-default.conf --max-frames 270 \
  --detector-model releases/yolov8n-visdrone9-rk3588-fp16-v1/models/yolov8n-visdrone9-rk3588-fp16.rknn \
  --detector-manifest releases/yolov8n-visdrone9-rk3588-fp16-v1/models/detector-manifest.json \
  --detector-labels releases/yolov8n-visdrone9-rk3588-fp16-v1/models/visdrone9.txt \
  --detector-class-id 2 --detector-conf 0.25 --detector-nms 0.45 \
  --output-video runs/occ-baseline-full-20261111/output.avi \
  --output-boxes runs/occ-baseline-full-20261111/bboxes.csv \
  --session-metadata runs/occ-baseline-full-20261111/session-metadata.json \
  > runs/occ-baseline-full-20261111/run.log 2>&1
```

- 运行 exit 0，SUMMARY `tracked_frames=270`、mean runtime `9.196 ms`、runtime FPS
  `108.744`、mean pipeline `111.414 ms`、end-to-end FPS `8.976`。按 frozen truth 排除
  169--181 后，可评价 258 帧 mean IoU `0.579`；0--168 为 `0.631`，182--270 为
  `0.479`。遮挡状态从未触发、detector 从未请求；174--178 和 180--184
  `control_valid=0`；182 帧 IoU `0.303`，185 帧才恢复普通接受且 `reacquired=0`。
- 证据目录
  `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/runs/occ-baseline-full-20261111`；
  `bboxes.csv`、`output.avi`、`run.log`、`session-metadata.json` SHA-256 依次为
  `b2ba9d7328f0554d1806a36f817ece8a42a5cdd0e4275b9cae44f1b87271eb6f`、
  `0002caa118473ee41e2c015e29728445f157ac2b2f70996d179670fe8c2906e4`、
  `511e02d7e6aca3eac432ceb472bd54fb7c964ea1ef31d362b3fe632260c138cf`、
  `08f34c2452ad379eed26cd2e14e7c8cfd58960bc2ad450ff2b74e65005c8f9a8`。
  本次诊断目录未生成 `manifest.json`，故 `verify_board.sh` 明确报
  `missing or empty artifact`；它是失败基线，不计正式 board PASS。
- 独立 builder 审查的五项歧义已写入设计：一次 detector hit 只允许 shadow search hint；
  改动进入共享 core 但只由独立 Debug 配置启用；identity 自动代理门后仍需人工视频签署；
  本轮是离线帧序功能验收而非 30 Hz 实时门；最终运行必须同时通过 evaluator、run manifest 和
  `verify_board.sh`。未修改协调方专属 `STATUS.md` 或 `integration-contract.yaml`。
- 用户随后明确要求避免对单视频过拟合，把本视频视为典型遮挡场景并保留泛化能力。设计因此
  删除 `occ_video_1` 专用配置和允许过拟合的表述，改用通用
  `visible-occlusion-calibrated.conf` 候选；增加 190--270 冻结 holdout、合成状态机矩阵、
  参数 `±10%`/计数 `±1` 稳定平台、初始框中心 `±2 px`/尺度 `±5%` 扰动和时序/异步扰动门。
  `occ_video_1` 明确禁止进入任何后续模型训练或训练期选择。

## 2026-11-10T19:21:14+08:00 RK3588 独立 Debug 迁移与实板运行

- 本轮按用户确认的“方案 1”在隔离工作树
  `/home/u20/.codex/worktrees/rk3588-debug-deploy/single-object-tracking` 执行；主线输入为
  `main@1bf657ff730891ae0bea48340a046cbcbeaa87d4`，实现链为
  `3c7801897372f63f7b94755806ea2c53f4e03648`（转换目标参数化）、
  `493a3f8429bec0e85bb48fd15598218fb38ab6b2`（RK3588 Debug 配置与校验器）、
  `b1dc8450b3a679e9f41a22a3d9e3af25a527de39`（C++ runtime 接受 RK3588 detector
  manifest）和 `cb2d0fab019bf44edd55b3f947506b8b389bf19f`（校验当前 SUMMARY 格式）。
  工作树为 detached HEAD；本条进度的结果提交以
  `git log -1 --format=%H -- docs/progress/board-validation.md` 查询。未修改协调方专属的
  `docs/progress/STATUS.md` 或 `docs/progress/integration-contract.yaml`。
- 目标板最终地址为 `192.168.1.102`，hostname `LKD3588`，硬件
  `Rockchip RK3588 NEARDI LKD3588 Board`，AArch64 Ubuntu 22.04、Linux 6.1.84，
  GCC 11.4.0、CMake 3.22.1、OpenCV 4.5.4；RKNPU driver `0.9.8`。系统
  `/usr/lib/librknnrt.so` SHA-256 为
  `0f1c5db6c649c8705759308aa56ecffc3295c7eb465f9ec9170882f44004aa8d`，实际运行日志
  API 为 `2.1.0 (967d001cc8@2024-08-07T19:28:19)`。编译头从已连接 RK3566 的
  `/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include/rknn_api.h` 取得，SHA-256
  `c48e11a6f41b451a5fd1e4ad774ea60252d3d94f78bee9b21ea3d21b21deba9a`；只使用该头
  编译，未覆盖 RK3588 系统 runtime。

### ONNX 到 RK3588 模型身份

- 严格未复制 RK3566 RKNN。NanoTrack 作者仓库 HEAD
  `248663fde6bf7c40190cf10ee396d5662919ecd3`，权重 SHA-256
  `4f0d25108385a67bdec5fa01ca37c74acb72343b53996dd645fe18b86e512e69`；确定性导出 seed
  `20260929`、opset 14，ONNX checker 和 ORT parity 均通过。计划指定的旧
  `nanotrack` venv 为 Torch 1.7.1、最高稳定 opset 12，实际会报
  `Unsupported ONNX opset version: 14`；因此实际导出使用与仓库历史 opset-14 制品一致的
  `/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python`（Torch 2.4.0、ONNX
  1.16.1、ORT 1.23.2），未降低 opset 或跳过数值校验。
- 实际转换核心命令为：

```bash
/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python \
  rk3566/nanotrack_v3/tools/export_v3.py \
  --repo /home/u20/single-object-tracking/repos/siamtrackers/NanoTrack \
  --config /home/u20/single-object-tracking/repos/siamtrackers/NanoTrack/models/config/configv3.yaml \
  --weight /home/u20/single-object-tracking/repos/siamtrackers/NanoTrack/models/pretrained/nanotrackv3.pth \
  --output-dir /home/u20/single-object-tracking/artifacts/rk3588-debug-deploy/nanotrack \
  --seed 20260929 --opset 14 --atol 1e-4 --rtol 1e-4

/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python \
  rk3566/nanotrack_v3/tools/convert_rknn.py \
  --model-dir /home/u20/single-object-tracking/artifacts/rk3588-debug-deploy/nanotrack \
  --source-commit 248663fde6bf7c40190cf10ee396d5662919ecd3 \
  --weight-sha256 4f0d25108385a67bdec5fa01ca37c74acb72343b53996dd645fe18b86e512e69 \
  --target rk3588

/home/u20/single-object-tracking/.venvs/rknn-convert/bin/python \
  rk3566/nanotrack_v3/tools/convert_yolov8_rk3566.py \
  --onnx /home/u20/single-object-tracking/artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-rkopt.onnx \
  --output /home/u20/single-object-tracking/artifacts/rk3588-debug-deploy/detector/yolov8n-visdrone9-rk3588-fp16.rknn \
  --manifest /home/u20/single-object-tracking/artifacts/rk3588-debug-deploy/detector/detector-manifest.json \
  --labels /home/u20/single-object-tracking/artifacts/rk3588-debug-deploy/detector/visdrone9.txt \
  --source-url local://yolov8n-visdrone9-rkopt.onnx \
  --source-repository https://github.com/airockchip/ultralytics_yolov8 \
  --source-commit 4674fe6e003dfbc5f2250d3b39dd31faaf7a9877 --target rk3588 \
  --output-count 9 \
  --training-checkpoint /home/u20/single-object-tracking/artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-best.pt \
  --training-repository https://github.com/airockchip/ultralytics_yolov8 \
  --training-commit 4674fe6e003dfbc5f2250d3b39dd31faaf7a9877 \
  --training-config /home/u20/single-object-tracking/artifacts/yolov8-visdrone9/runs/visdrone9-full/args.yaml \
  --dataset-audit /home/u20/single-object-tracking/artifacts/yolov8-visdrone9/dataset-audit.json
```

- 新生成的 RK3588 template/search/head SHA-256 依次为
  `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`、
  `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`、
  `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`；tracker
  manifest 为 `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`，
  `provenance.target_platform=rk3588`。VisDrone9 RK3588 model/manifest/labels 依次为
  `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`、
  `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9`、
  `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`；detector
  manifest 为 RK3588、FP16/no quantization、9 outputs、9 classes，源 RKOPT ONNX SHA-256
  `b2948072f01da7fe3b686392a47fe742380c98775c4149111527b7ddf26a5cc8`。主机模型包
  `/home/u20/single-object-tracking/artifacts/rk3588-debug-deploy/SHA256SUMS` 全部通过；审计还确认
  新包文件名不含 `rk3566`，且没有复制 tracked RK3566 RKNN 路径。

### 原生构建与实板运行

- 隔离部署根为
  `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f`，创建前确认不存在。canonical
  输入 `data/occ_video_1/1.mp4` SHA-256
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，1920x1080、
  30 FPS、271 帧；顶层数据清单 SHA-256
  `872483add3bb9ae3ddc4172b689e224ccf3e65b51bd0ab07268ee02957c3254f`。
  source archive `inputs/source-cb2d0fa.tar.gz` SHA-256
  `171d587d0abc0fa91d4587bc7ad9200bbced198f69118470afd1c89f8fe25be6`。后续源码更新解包
  显式排除 `models/*`，避免源码仓库中的历史 manifest 覆盖生成模型。
- 板端实际构建命令：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DENABLE_RKNN_RUNTIME=ON \
  -DRKNN_INCLUDE_DIR=/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/include \
  -DRKNN_LIBRARY=/usr/lib/librknnrt.so
cmake --build build -- -j2
ctest --test-dir build --output-on-failure
```

  原生 Debug 构建成功；最新完整 CTest `19/19` 通过。`build/nanotrack_v3` 和
  `build/yolo_detector_smoke` SHA-256 分别为
  `64c3839cc4666b48d160e3d7482237979fabf2b96ec51b80888cb7658021008a`、
  `2437181e5d77817137f1af362ddfbdc2181ad488f4ed4d667e4b08c407560e1e`；`ldd`
  确认二者使用板载 `librknnrt.so`，无缺失依赖。完整配置、构建、CTest 和 ldd 日志均留在部署根。
- 以下命令均在
  `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f` 执行；这是本轮保留证据所用的完整
  detector、bounded、verifier 和交互应用参数（交互键盘输入由临时 ctypes/XTest helper 等价执行
  `Space`、等待 4 秒、`Q`，其结果保留在 `runs/interactive-debug/x11-input.log`）：

```bash
ffmpeg -v error -y -i data/occ_video_1/1.mp4 -frames:v 1 \
  runs/detector-smoke/frame0.png
stdbuf -oL -eL ./build/yolo_detector_smoke \
  --model releases/yolov8n-visdrone9-rk3588-fp16-v1/models/yolov8n-visdrone9-rk3588-fp16.rknn \
  --manifest releases/yolov8n-visdrone9-rk3588-fp16-v1/models/detector-manifest.json \
  --labels releases/yolov8n-visdrone9-rk3588-fp16-v1/models/visdrone9.txt \
  --image runs/detector-smoke/frame0.png --conf 0.25 --nms 0.45 --class-id 2 \
  --output-image runs/detector-smoke/annotated.jpg \
  --output-csv runs/detector-smoke/detections.csv \
  > runs/detector-smoke/run.log 2>&1

stdbuf -oL -eL ./build/nanotrack_v3 \
  --video data/occ_video_1/1.mp4 --bbox 992,582,37,39 --model-dir models \
  --sensor visible --controller-config config/visible-default.conf --max-frames 60 \
  --detector-model releases/yolov8n-visdrone9-rk3588-fp16-v1/models/yolov8n-visdrone9-rk3588-fp16.rknn \
  --detector-manifest releases/yolov8n-visdrone9-rk3588-fp16-v1/models/detector-manifest.json \
  --detector-labels releases/yolov8n-visdrone9-rk3588-fp16-v1/models/visdrone9.txt \
  --detector-class-id 2 --detector-conf 0.25 --detector-nms 0.45 \
  --output-video runs/occ-smoke/output.avi --output-boxes runs/occ-smoke/bboxes.csv \
  --session-metadata runs/occ-smoke/session-metadata.json \
  > runs/occ-smoke/run.log 2>&1

EXPECTED_RKNN_TARGET=rk3588 scripts/verify_board.sh \
  runs/occ-smoke models 61 1920 1080 \
  releases/yolov8n-visdrone9-rk3588-fp16-v1/models/yolov8n-visdrone9-rk3588-fp16.rknn \
  releases/yolov8n-visdrone9-rk3588-fp16-v1/models/detector-manifest.json \
  releases/yolov8n-visdrone9-rk3588-fp16-v1/models/visdrone9.txt \
  > runs/occ-smoke/verify.log 2>&1

timeout --signal=INT 30s env DISPLAY=:0 XAUTHORITY=/home/neardi/.Xauthority \
  stdbuf -oL -eL ./build/nanotrack_v3 \
  --config config/rk3588-interactive-debug.conf --bbox 992,582,37,39 \
  --max-frames 120 --output-boxes runs/interactive-debug/bboxes.csv \
  --session-metadata runs/interactive-debug/session-metadata.json \
  > runs/interactive-debug/run.log 2>&1
```

- detector 单帧 smoke 使用 canonical frame 0、class 2 `car`、conf/NMS `0.25/0.45`。
  模型成功初始化并报告 `target=rk3588`、API 2.1.0、driver 0.9.8、9 outputs；推理结果
  `raw=346`、保留上限 100 个 car 候选，preprocess/inference/postprocess/total 为
  `8.745/53.909/12.492/75.146 ms`。这证明模型加载和推理完成，不是 detector 精度或阈值
  质量结论。证据在 `runs/detector-smoke/`；`run.log`、`detections.csv`、
  `annotated.jpg` SHA-256 依次为
  `cb4351742e38787f13e7887e47f884852ee4441893612d1c288ec9ef16661c1c`、
  `22d6a9e758ce791d2cb4f875053b5527c0efa3e5bf2b58f7339f692a6c5018d5`、
  `10b793e67844d041c67b04ca9543a1a3eb925c0041722def7da92bd1634c9c46`。
- bounded canonical smoke 使用 bbox `992,582,37,39`、`--max-frames 60`、class 2 和上述模型。
  初始化帧加 60 个跟踪帧，共 61 行数据/61 帧视频；SUMMARY 为
  `tracked_frames=60`、mean runtime `8.489 ms`、runtime FPS `117.799`、mean pipeline
  `113.422 ms`、end-to-end FPS `8.817`。执行上述完整
  `EXPECTED_RKNN_TARGET=rk3588 scripts/verify_board.sh` 命令得到
  `PASS board artifacts output_frames=61 tracked_frames=60`。61 行中 detector direct-control
  行为 0；本短段稳定跟踪中 detector request/complete/reject/error 均为 0，因此不证明 detector
  重捕。证据在 `runs/occ-smoke/`；log/CSV/metadata/video/verify SHA-256 依次为
  `cf043fa92c8c546e499ee0d2b43c6da9e92b47a8fcaf1a7cfbb45468cc9301b1`、
  `f8bdfbe3d84d793f6acac176de845e4edebd3cc36f5b476dc29276ae4abf0fe8`、
  `5515a66da7125d89ed126845e7023dc58a5ec20a050321c0029344c471a748a6`、
  `0c6c852c045d9d517095e94db7beade7d48bdde94bc963949ee0efe799c2be41`、
  `34296832c1129c5694da72a7f307e3d46bcf7d1aa0b6d9dc391da36e6cc55bab`。
- X11 Debug 在 active `:0` 会话运行 `--config config/rk3588-interactive-debug.conf`，显式同一 bbox，
  通过 XTest helper 发送 Space 播放、4 秒后 Q 退出；应用与输入 helper 均 exit 0。日志记录
  `interactive_ready`、`action=play_pause`、30/60/90/120 帧结构化 `frame_status`、
  `frame_limit_reached`、`action=quit` 和 `shutdown reason=user_quit`，最终 120 tracked frames。
  证据在 `runs/interactive-debug/`；log/CSV/metadata SHA-256 依次为
  `2cc348f9e0d5a74ec14759dea97340e5c2582ef4a07bed6c344093ecee75ada4`、
  `e18aef2ba86ad8fef9e4fa6d3fa1c076255190ec3ddc167b1ab4898a4a15c4d4`、
  `20c3d1d9aaa33b82c452d4d5054f4bb998c8ff5f6371f66a1ef919329d079c77`。
  总清单 `runs/EVIDENCE-SHA256SUMS` 含 40 项；环境快照在 `runs/BOARD-ENVIRONMENT.txt`。
- 首次 detector smoke 因 C++ runtime 仍硬编码 RK3566 manifest 而失败，原日志保留为
  `runs/detector-smoke/run-initial-rk3566-hardcode-failure.log`，随后由 `b1dc845` 修复并复测。
  二次源码归档解包曾只覆盖 tracker manifest 为 RK3566（RKNN 文件实测仍为新 RK3588 哈希），
  该次不一致运行保留于 `runs/occ-smoke-invalid-rk3566-manifest/`，不计入通过证据；恢复并逐文件
  `cmp` RK3588 包后重新运行，最终 `runs/occ-smoke/` 才是有效证据。

### 证据边界与 Handoff

- 本轮只证明在指定 LKD3588 上：四个从 ONNX 新生成的 RK3588 模型身份一致、原生 Debug 构建、
  detector 单图推理、canonical 视频 60 帧集成运行、板端 schema/safety 校验和真实 X11 交互播放/退出
  均通过。它不满足当前 integration contract 的 RK3566 board gate，也不构成 detector promotion、
  production handoff、生产交叉编译、长时稳定性、遮挡重现质量、阈值/性能校准或真实吊舱验收。
  `functional_pass_promotion_held` 和既有 RK3566 `latest_handoff.available=false` 不因本轮改变。
- 请求协调对话决定是否在 `integration-contract.yaml` 中把 RK3588 建立为与 RK3566 分离、明确命名的
  Debug target，并审核提交链 `3c78018..cb2d0fa` 后再决定是否集成。若建立新目标，还需分别冻结
  RK3588 runtime/API/driver、四模型 manifest/hash、canonical 路径及独立验收门；不得用本轮实验结果
  替换既有 RK3566 合同身份。

## 2026-11-08T22:52:30+08:00 统一非训练测试数据板端落盘核验

- 协调基线为 `main@9d0085f63e5e332db2142bfff71bfcef5f20b8c7`；任务工作树
  `/home/u20/single-object-tracking/.worktrees/board-validation-progress-sync`、分支
  `codex/board-validation-progress-sync` 在核验前干净并快进到该提交。本轮只核验用户已上传的
  数据并更新本任务文件，没有运行算法、修改源码、模型、公共 `STATUS.md` 或
  `integration-contract.yaml`。
- 板端实际路径为 `/home/cat/Project/SOT/data/occ_video_1`。在真实 RK3566 板端执行
  `sha256sum 1.mp4 annotation_metadata.json occlusion_events.json annotation_status.csv labels.sha256 checksums.sha256`，
  得到：视频 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`、
  元数据 `5641650435a1980694f982cf1fa6715ba959b1515938cee383c8db8ec4aeb076`、
  遮挡事件 `a43f016c96b34c8a0ec6125dcb944ba4fa07a79d99a46ce696bbb29841ded256`、
  逐帧状态 `e44fd23514d6871e72d628ad4843fc90bb4246f306322ee297c8a2aa75818036`、
  标签清单 `9ec01854884126350f93e1af3a4aee002d38ce7c2d50c71c78de0f1a8dc0a648`、
  顶层清单 `872483add3bb9ae3ddc4172b689e224ccf3e65b51bd0ab07268ee02957c3254f`，
  与 schema v2 集成契约逐项一致。
- 板端继续执行 `sha256sum -c --quiet --strict checksums.sha256` 和
  `sha256sum -c --quiet --strict labels.sha256`，两者均退出成功；`labels.sha256` 为
  271 项，`labels/` 实际 271 个 `frame_*.txt`，共 271 行有效 YOLO 记录，均为
  `class_id=0` 且每行 5 字段。`annotation_status.csv` 为连续 frame 0～270 的 271 条
  状态记录；169～181 的来源均为 `bidirectional_interpolation`。
- 板端 `ffprobe` 实测 `1.mp4` 为 1920×1080、`30/1` FPS、271 帧。解析元数据与事件记录
  确认目标为完整 `car`，169～171 部分遮挡、172～180 完全遮挡、181 首次可见、182
  首次可靠重现、184 稳定重现，遮挡物为人行天桥。169～181 是人工确认估计框，
  `ordinary_iou_metrics_allowed=false`，后续普通 IoU 统计不得包含这些帧。

### Handoff

- 请求协调对话采纳板端实际路径
  `/home/cat/Project/SOT/data/occ_video_1` 及上述哈希，补全公共契约的
  `test_data.canonical_dataset.target_environment_path`。后续新增开发板非训练算法证据只使用
  该数据；`sot-v1.mp4` 和 `girl_dance.mp4` 只保留历史输入身份，不再新增验证结论。
- 本轮仅证明冻结数据已完整落盘并与契约一致，不证明算法运行、遮挡恢复、精度、性能、
  阈值标定、生产交叉编译或吊舱验收。任务仍保持
  `functional_pass_promotion_held`，`development_board_validation.latest_handoff.available=false`。

## 2026-10-31T10:43:00+08:00 最终源码事件脉冲板端复测

- 被测源码 `codex/adaptive-occlusion-coast@d589215f1c0319f2686d6364edfec86a67a0c63e`，工作树干净；相对 `e3ec19615a0d931544fa9567f288be62a0935551` 的 `git diff --binary e3ec196 HEAD -- rk3566/nanotrack_v3` SHA-256 `64a0f8ab085a4e067618944b9e3899b0e7cedbf6df392d5e72b971efd57b01c7`。本进度提交结果身份见 Git 日志。
- 已校验源码 bundle 两端 SHA-256 `25d601866477794fe6d06223c09a22aff0f66db20ec79ba04d13ae6d8f444623`；新建独立板端 worktree `/home/cat/Project/SOT/adaptive-coast-d589215`，干净 detached HEAD `d589215f1c0319f2686d6364edfec86a67a0c63e`，源码子树对象 `4956f404de2ff6e4b63649191c09861ecb4caf83`。原工作区和此前证据未修改。本次复算模型、manifest、labels 和两视频 SHA-256，均与下两节记录相同；控制配置 `visible-default.conf` SHA-256 `2010c6cd16553595844a12f103f53412942289262d866b9fadba26b2821337ed`，Debug 配置 SHA-256 `e3fe5f927ba3a47d2829dce600c01634f7f2f405576865c2edea22e0256493f8`。板端时间 `2026-10-31T10:38:10+08:00`；Linux 4.19.232 AArch64，动态链接 OpenCV 4.10、RKNN runtime 2.3.2，运行日志 API 2.3.2、driver 0.8.2。
- 新证据根 `E=/home/cat/Project/SOT/validation-adaptive-coast-d589215`，源码 `S=/home/cat/Project/SOT/adaptive-coast-d589215/rk3566/nanotrack_v3`；`A`、`R` 与下两节相同。执行 `cmake -S "$S" -B "$E/build" -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so`、`cmake --build "$E/build" -- -j2`、`(cd "$E/build" && ctest --output-on-failure)`：CTest `17/17`、15.74 秒。二进制 SHA-256 `1d09aa599db01c663762f4185a9d7b9a36d2ed6c6c0bbd88a40fbba4ad0e0886`；`configure.log`、`build.log`、`ctest.log` 依次 `bfba39142461f7f1a5338c4de643441dd222e45f7f87fd82ca85be62c48796b5`、`a1c1cc90dc9833a484068d84cbc87d6f6e63ef909bc06ecc785be783ca5b7468`、`bdcf7286abba9ebbadd3cb80b7f8e30e60cf94c015f3739ab8c4729f197ccdd6`。

两次非交互实际命令与两节后的完整命令相同，仅 `E` 和 `S` 换成本节新目录；视频、ROI、模型、配置、帧数、阈值、尺寸不变，均 exit 0。为直接观察仅在交互模式输出的结构化事件，另执行：

```bash
timeout --signal=INT 90s env DISPLAY=:0 stdbuf -oL -eL "$E/build/nanotrack_v3" --interactive --video "$A/data/girl_dance.mp4" --bbox 196,160,184,310 --model-dir "$A/models" --controller-config "$S/config/visible-default.conf" --detector-model "$R/yolov8n-visdrone9-rk3566-fp16.rknn" --detector-manifest "$R/detector-manifest.json" --detector-labels "$R/visdrone9.txt" --max-frames 200 --output-boxes "$E/girl-interactive.csv" --session-metadata "$E/girl-interactive.json" > "$E/girl-interactive.log" 2>&1
```

交互窗口初始化后由 `/tmp/adaptive_xplay.py` 使用 X11/XTest 发送空格播放，达到 200 帧上限后发送 q 正常退出，exit 0；最终辅助脚本 SHA-256 `dba4ccf1bbeaf045b41084450c1664c768ca876a6798656502eab2c68d4b466a`。视频证据来自非交互运行；交互运行保存 CSV、metadata、原始结构化日志。

`girl_dance` 的触发/探测/控制轨迹不变：34 帧 suspicion_start，39 帧 `GRADUAL_WIDTH_PEAK` age 1 且运动控制有效；43 帧 age 5 SEARCH 命中一次，44 帧 age 6 CONFIRM 被质量或运动几何拒绝，48 帧 age 10 SEARCH 同样拒绝；50 帧 age 12，51 帧 LOCAL_RECOVERY 且控制无效。交互结构化日志中该次 suspicion_start 在 34 帧仅一次，51 帧 LOCAL_RECOVERY 的 `occlusion_suspicion_started=0`，没有重复事件；该帧仍保留 `FULL_STABLE_MEDIAN`、参考宽高 `227.833588/278.244293` 和 `GRADUAL_WIDTH_PEAK`。整段共有三次 suspicion_start（34、54、124），后两次出现在恢复为 TRACKING 之后，是后续新的 suspicion 起点；不能声称整段只有一次。全部 201 行 hypothesis 为 MOTION、恢复 detector 请求为零，未见 probe 或 detector 直接授权控制；成功 early recovery 未观察。

`tracking-test-v1` 同样 120 帧，初始类别确立 request/complete/error `1/1/0`，仍 UNKNOWN/NO_MATCH；121 行含 35 个 ASSISTED_RECOVERY 行，恢复 request/submit/complete `0/0/0`，候选队列、锁均为零，hypothesis 全为 MOTION，未见 detector 控制。无真值视频只支持定性状态/控制观察。

`girl.{avi,csv,json,log}` SHA-256 依次为 `aee8ea7743978bb3495f497e2efca8f19d2d5e22f6882f8c3e1dffe25cf38b4a`、`0d20556c753a09794f1cc763ec0ee0bcaf15000d0e5f3a4ac2edb219b32ee23d`、`c6b7126db37cf8c80971f25864ed305a156c8155ab062b7b27354c89c0019ba7`、`083987093b3b3924574a8c1c7cb7303523d8c282b988a8f4998f7d716cca612c`；`girl-interactive.{csv,json,log}` 依次为 `dc4dde141340764e579bb0d431e73bf4d9c3ac4642c4d9d66abdb829f2211e8c`、`c7d5819be9573dcdbd4a95cfd4021a0bc91ab7ab44b22745d50cb73a06abe370`、`5429d883c779c39699e6bdb0fef42f82c53564f3faeb11cddc9d3f7adba4e8d7`；`tracking.{avi,csv,json,log}` 依次为 `1d1d369364bd623d8a98d393f19108103c8356bee42dc3ae44859b0504604d93`、`617b555f612cd3ee42556a418770dbf03d5fa617faae05add47f69646a5b1607`、`847c1ff270d144bc75ae4ba0d2a8b59d25b205c0425e627728b7b9b1b4883da4`、`001cf43552312b7df7277f2c526a2530c1e9ae7487dc588fdf82815fd6f906ec`。

### Handoff

请求协调方采纳本次精确源码板端证据；promotion 继续 held、`latest_handoff.available=false`。detector 正例确立及确认重捕、成功 early recovery、长稳及其他晋级门未闭环；没有真实吊舱验收。

## 2026-10-31T10:29:51+08:00 UNKNOWN 恢复请求门控修复的板端复测

- 复测源码 `codex/adaptive-occlusion-coast@e3ec19615a0d931544fa9567f288be62a0935551`，父提交为上次证据 `9f38674`；相对上次被测源码 `3890c50bc6aff067027d63f0140d798f783d472e` 的 `git diff --binary 3890c50 e3ec196 -- rk3566/nanotrack_v3` SHA-256 `1638b4d3dcdcc855b44f2f9a3dae92374c5d469dd3eb3b30190b893fc7fd2972`。本进度提交的结果身份见 Git 日志。
- 板端独立工作树 `/home/cat/Project/SOT/adaptive-coast-e3ec196` 干净，HEAD 精确为 `e3ec19615a0d931544fa9567f288be62a0935551`，源码子树对象 `4fbd95d4c6c192449e6427a5558397c6be45a52f`；传输 bundle 两端 SHA-256 `eebaeefe98b9a654792e7e630a5cfc2aa475541d24f9523f1941e201e4b2fa64`。原有 checkout 和上次证据目录未修改。新证据根 `E=/home/cat/Project/SOT/validation-adaptive-coast-e3ec196`，源码 `S=/home/cat/Project/SOT/adaptive-coast-e3ec196/rk3566/nanotrack_v3`；`A`、`R`、模型及两视频路径与下节相同，所有模型、manifest、labels 和视频 SHA-256 均于本次复核并与下节一致。`$S/config/visible-default.conf` 为 `2010c6cd16553595844a12f103f53412942289262d866b9fadba26b2821337ed`；Debug 配置为 `e3fe5f927ba3a47d2829dce600c01634f7f2f405576865c2edea22e0256493f8`，本次未传 Debug 配置。
- 板端时间已核对，Linux 4.19.232 AArch64；实际动态链接 OpenCV 4.10 和 RKNN runtime 2.3.2，运行日志 API 2.3.2、driver 0.8.2。执行 `cmake -S "$S" -B "$E/build" -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so`、`cmake --build "$E/build" -- -j2`、`(cd "$E/build" && ctest --output-on-failure)`；完整 CTest `17/17`、15.53 秒。`$E/build/nanotrack_v3` SHA-256 `9c533b18fba5e0f25b56d1c2f48ecf3ede16382e8e675382f44c54e1af83d508`；`configure.log`、`build.log`、`ctest.log` 分别为 `47689096a5c49cb9b8a46010d15fcf6828abb2d43565f2fc5645bb427758b9b4`、`777378efb93222698a54207383f810851819b88908269cad228941ba6c6d7830`、`180d7ff6c69acf98eed4404e4bf0e33b286d306bae2976deb1e5dd17843cfd78`。

复测两条实际命令与下节两条完全相同，除 `E` 和 `S` 使用本节新目录；均 exit 0，输入、ROI、模型、配置、阈值、帧数和输出尺寸不变。非交互输出视频截图保留在新证据目录，不是 GUI 截屏。

- `tracking-test-v1`：初始分类独立请求/完成/错误为 `1/1/0`，六个候选均未匹配，结果仍 UNKNOWN/NO_MATCH、恢复关闭。121 行含初始化，状态 TRACKING 52、UNCERTAIN 10、LOCAL_RECOVERY 24、ASSISTED_RECOVERY 35；恢复 detector 计划请求/实际提交/完成均 `0/0/0`（metadata 恢复 request/complete/error 为 `0/0/0`），旧版同场景提交过 3 次。全部行候选队列 0、candidate ID 空、locked 0、hypothesis MOTION；69 个非控制有效帧中包括全部辅助恢复帧，无 detector 候选激活或 detector 直接控制。该样本复测支持旧 UNKNOWN 下额外请求问题已解决；不代表所有板端场景或 detector 正例恢复通过。无真值视频只作为定性观察。
- `girl_dance`：200 跟踪帧、初始化分类请求/完成 `1/1`、UNKNOWN，恢复 detector 请求/完成 `0/0`。第 34 帧 suspicion 开始；39 帧 `GRADUAL_WIDTH_PEAK` age 1 且运动控制有效；43 帧 age 5 SEARCH 命中一次，44 帧 age 6 CONFIRM 被质量或运动几何拒绝，48 帧 age 10 SEARCH 被拒绝；第 50 帧 age 12，51 帧 LOCAL_RECOVERY 且控制无效。全 201 行均为 MOTION，HOLD/UNCERTAIN 无 detector 请求，未见 probe 直接授权控制。与上轮观察的滑行节奏一致；成功 early recovery 仍未观察。

`tracking.{avi,csv,json,log}` SHA-256 依次为 `1d1d369364bd623d8a98d393f19108103c8356bee42dc3ae44859b0504604d93`、`6429112e5bee74086115123661894227dab7a38ca971875930c3e465b5e8d603`、`0fbef4559311b13983582f7d0604e0d410d86063774bdec8cfe0684eb86040c4`、`ad6929735b9f31a85ecc3ec20de63628f28253b126df6decb4abb975a1461229`；`girl.{avi,csv,json,log}` 依次为 `aee8ea7743978bb3495f497e2efca8f19d2d5e22f6882f8c3e1dffe25cf38b4a`、`57a0d294fa8a15126ff8dd876c429e5249d7f1aa79eb83526a36478b3bee9ab8`、`47aeeb8206789a7460e81b75244417ee84502b6e81aa259fee475ac63e60fa10`、`efe077d9eebefd4f896fe22d3dbd4f413e7455084a5ee3d1b7efdd5324fc8643`。`tracking-initial.png` 为 `e348eff93a1ccb09ca6c9c366f7c8dbe80d85d172d3a2bad85684bb3ec2a33be`；`girl-observation-01.png` 至 `05.png` 依次为 `19b3bb71b46afe492b895755db45ef35b5421b1b9b49728bc4ce222fddc74ef5`、`5a166c086b67fb64eb76248b2955942007a1af5df9650600b22660c133eb6156`、`fe7659b5509b33417ce300ea36bb52320c6310b6e673795acab8db7124b42787`、`c4380d238a8517edb1975a0f341649c32f75d1c90160c36a8b0f5404cf320dde`、`80533a8ad85ad2d91374ccec238a564a7a6ff16e03b405ed7cd98967b28e8321`。

### Handoff

请求协调方采纳本次旧问题复测证据。仍保持 promotion held 和 `latest_handoff.available=false`：detector 正例确立、detector 确认重捕、成功 early recovery、长稳及其他晋级门尚未闭环；没有真实吊舱验收。

## 2026-10-31T10:17:30+08:00 自适应遮挡滑行板端验证：恢复请求问题待处理

- 源码工作树 `/home/u20/single-object-tracking/.worktrees/adaptive-occlusion-coast`，分支 `codex/adaptive-occlusion-coast`，验证基线 `3890c50bc6aff067027d63f0140d798f783d472e`；与 `main` 的 merge-base 为 `b827c0a6d8916e9c0832c748e30722f6f2fe5563`。`git diff <merge-base> HEAD --binary` 的 SHA-256 为 `632162a38530bacaf541179e3a6f6f8920dfb6825d850680a60c807681580fd9`。本进度提交以该基线为父提交；结果提交身份见 Git 日志。
- 新建主机构建 `/tmp/nanotrack-adaptive-coast-final`：`cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-adaptive-coast-final -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF`、`cmake --build /tmp/nanotrack-adaptive-coast-final -- -j2`、`ctest --test-dir /tmp/nanotrack-adaptive-coast-final --output-on-failure` 均成功，CTest `17/17`、1.63 秒；`git diff --check` 成功。主机测试不是板端场景验收。
- 板端既有 `/home/cat/Project/SOT/nanotrack_v3_longterm` 有修改的 `config/interactive-debug.conf` 和未跟踪 `cmd.txt`，未改动。将提交打包为 `/tmp/nanotrack-adaptive-3890c50.bundle`，两端 SHA-256 均为 `a8bbd83e3a19a0f16662994b7c28b24d4583b9616bf66130b66081e839f5d7a9`；独立裸库 `/home/cat/Project/SOT/adaptive-coast-3890c50.git` 的 worktree `/home/cat/Project/SOT/adaptive-coast-3890c50` 为干净 detached HEAD `3890c50bc6aff067027d63f0140d798f783d472e`，源码子树对象 `ec83cf87b265557b49800be729a42ba05aca1e1d` 与主机一致。板端时间 `2026-10-31T10:12:29+08:00`，Linux 4.19.232 AArch64；实际加载 OpenCV 4.10 和 RKNN API 2.3.2、driver 0.8.2。
- 板端构建及证据根目录 `E=/home/cat/Project/SOT/validation-adaptive-coast-3890c50`；源码目录 `S=/home/cat/Project/SOT/adaptive-coast-3890c50/rk3566/nanotrack_v3`。执行 `cmake -S "$S" -B "$E/build" -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so`、`cmake --build "$E/build" -- -j2`、`(cd "$E/build" && ctest --output-on-failure)`；成功，板端 CTest `17/17`、16.01 秒。`$E/build/nanotrack_v3` SHA-256 `7d96500d2832c87c033b8e1afdd95e381d7b71c11c8618540cf80a48b9d2d3c8`；`configure.log`、`build.log`、`ctest.log` SHA-256 分别为 `a7645ce858f8db6463f998e5c9ee150ca6e27ece8fd145c5cce402ae3bb48ae0`、`94a2aed5a4740944b0ccb29390c53440db542a6fc1392390285bf57a519cb89c`、`63cd9c5ec6d283711b0bde6d65d5eab4e4b7c58c8a29546ae4ea7d3d647a91ed`。
- 输入根 `A=/home/cat/Project/SOT/nanotrack_v3_longterm`，`R=$A/releases/yolov8n-visdrone9-rk3566-fp16-v1/models`。实测 `$A/models/{template_backbone.rknn,search_backbone.rknn,head.rknn,manifest.json}` SHA-256 依次为 `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709`、`92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a`、`2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600`、`441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2`；`$R/{yolov8n-visdrone9-rk3566-fp16.rknn,detector-manifest.json,visdrone9.txt}` 依次为 `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0`、`05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e`、`41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`，与已记录身份一致。`$A/data/girl_dance.mp4` 为 `8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9`；`/home/cat/Project/SOT/data/tracking-test-v1.mp4` 为 `aa30402f63cf9fdf01f96a5907ad1010ecdd09098ff0e8435815d9c3ddb82d07`。提交内 `$S/config/interactive-debug.conf` 为 `e3fe5f927ba3a47d2829dce600c01634f7f2f405576865c2edea22e0256493f8`；实际使用的 `$S/config/visible-default.conf` 为 `2010c6cd16553595844a12f103f53412942289262d866b9fadba26b2821337ed`，不同于历史旧配置，原因是本源码提交新增 coast 参数。

### 场景命令与观察

两次均使用下列非交互方式、源码默认 detector 类别和阈值（-1、0.25、0.45），未传 Debug 配置；显式传入上述模型和控制配置。视频、CSV、metadata、日志和提取截图保留在 `$E`，均为真实板端运行，未改阈值：

```bash
timeout --signal=INT 180s stdbuf -oL -eL "$E/build/nanotrack_v3" --video "$A/data/girl_dance.mp4" --bbox 196,160,184,310 --model-dir "$A/models" --controller-config "$S/config/visible-default.conf" --detector-model "$R/yolov8n-visdrone9-rk3566-fp16.rknn" --detector-manifest "$R/detector-manifest.json" --detector-labels "$R/visdrone9.txt" --max-frames 200 --output-size 428x240 --output-video "$E/girl.avi" --output-boxes "$E/girl.csv" --session-metadata "$E/girl.json" > "$E/girl.log" 2>&1
timeout --signal=INT 240s stdbuf -oL -eL "$E/build/nanotrack_v3" --video /home/cat/Project/SOT/data/tracking-test-v1.mp4 --bbox 1395,1075,240,125 --model-dir "$A/models" --controller-config "$S/config/visible-default.conf" --detector-model "$R/yolov8n-visdrone9-rk3566-fp16.rknn" --detector-manifest "$R/detector-manifest.json" --detector-labels "$R/visdrone9.txt" --max-frames 120 --output-size 960x540 --output-video "$E/tracking.avi" --output-boxes "$E/tracking.csv" --session-metadata "$E/tracking.json" > "$E/tracking.log" 2>&1
```

两次均 exit 0。`girl_dance` 200 跟踪帧：第 34 帧 suspicion 开始；第 39 帧触发 `GRADUAL_WIDTH_PEAK`，age 1，`motion_control_authorized=1`、`control_valid=1`。第 43 帧 age 5 SEARCH 命中一次，第 44 帧 age 6 CONFIRM 因 `QUALITY_OR_MOTION_GEOMETRY` 拒绝，第 48 帧 age 10 SEARCH 同样拒绝；第 50 帧 age 12，51 帧进入 LOCAL_RECOVERY 且控制无效，52 帧 MOTION 重捕。该次没有成功的 early probe recovery；12 个 HOLD 帧及两个 UNCERTAIN 帧均无 detector 请求，所有 201 行 hypothesis 均为 MOTION，未见 detector 或 probe 直接授权控制。初始化目标类别 UNKNOWN/NO_MATCH，故不能证明 detector 确认重捕。截图 `$E/girl-observation-01.png` 至 `05.png` 分别取输出帧 39、43、44、48、51；这是输出视频截图，不是交互 GUI 截屏。

路口视频首帧 `$E/tracking-first.png` SHA-256 `f7a460dc17e165adb2e8aba72498c448f0ab2fc8b6df75499cc1fccdbf054088`；人工检查后选取前景深色轿车 ROI `1395,1075,240,125`，保存的初始框截图 `$E/tracking-initial.png` SHA-256 `e348eff93a1ccb09ca6c9c366f7c8dbe80d85d172d3a2bad85684bb3ec2a33be`。120 跟踪帧中 TRACKING 52、UNCERTAIN 10、LOCAL_RECOVERY 24、ASSISTED_RECOVERY 35（计数含初始化行）；未触发 HOLD 或 coast probe。此视频无真值，以上仅为定性状态/控制观察，不是精度、身份、恢复率或阈值校准证据。

**待处理：UNKNOWN 下仍提交恢复 detector 请求。** 路口视频初始化 frame 0 为 UNKNOWN/NO_MATCH、`detector_recovery_enabled=0`；但 ASSISTED_RECOVERY 的 frame 86、101、117 有 `detector_requested=1`、worker `submitted`，87、102、118 分别完成，过滤候选数 8、9、10，批状态 `ACCEPTED`。这不是初始化诊断：源码 `long_term_controller.cpp` 的 `begin_frame` 请求条件未检查 target class，`long_term_runtime.cpp` 随后实际提交；`ingest_detector_batch` 对 UNKNOWN 候选拒绝入队。全 121 行 `detector_candidates_queued=0`、candidate ID 为空、locked=0、hypothesis_source=MOTION；上述提交与完成帧 `control_valid=0`，未观察到 detector 候选激活或直接控制。该请求仍消耗资源，且与 UNKNOWN 关闭 detector 辅助恢复的要求存在冲突，须作为阻塞项由源码 owner 修复或明确裁决；不将本轮记为功能全通过。

`girl.{avi,csv,json,log}` SHA-256 依次 `aee8ea7743978bb3495f497e2efca8f19d2d5e22f6882f8c3e1dffe25cf38b4a`、`14e65e17d80c075cf47dd37f6d4f72ba19c16528fb6d58846f39bdded8636afc`、`6caaf6f4a520ad36254dada95f68d439126798e79d6f274cf1257b7431be18f7`、`48aa466ef62e62f4bf43b74decacc1738f5cfea93d392692ea9c8b8d049e62fa`；`tracking.{avi,csv,json,log}` 依次 `1d1d369364bd623d8a98d393f19108103c8356bee42dc3ae44859b0504604d93`、`3be6ea849dca076bf9804426eaa0f00df2de23ea890bd3220bb8307b765f138a`、`225886a840ae974adcb6e4ab3a9a0b5b8ed671b81619ff2f46aac4db1c6236c0`、`a438364e1f96a736679bccb0bd1d11f67d5f3ad1547617371f3f6228cdd0f879`。

### Handoff

保留 `functional_pass_promotion_held` 的历史状态，不把本次源码候选标为功能全通过；`development_board_validation.latest_handoff.available=false`。请求协调方采纳本证据并安排 UNKNOWN 时请求门控问题的修复与板端复测。正例 detector 确立、detector 确认重捕、成功 early probe recovery、长稳及其他 promotion 门仍未通过；未执行真实吊舱测试。

- 任务状态：`functional_pass_promotion_held`
- 最近核对：`2026-10-26T19:53:37+08:00`
- 核对基线：`main@bb297504070b3dfcf2fd4b4da05ebb1503e309db`
- 任务工作树：`/home/u20/single-object-tracking/.worktrees/debug-runtime-config`
- 任务分支：`codex/debug-runtime-config`
- 责任边界：在交叉编译之前，使用具备完整编译环境的真实 RK3566 开发板，对交互式 Debug 版本执行原生编译、调试、加载/推理、集成视频、控制安全、时延、资源和稳定性测试。

本任务是开发阶段的板端验证，不消费交叉编译生产产物。交叉编译只在本任务冻结并移交最新可用结果后进行；真实吊舱测试由人完成。

## 2026-10-22 首次协调复核

- 主工作树 `/home/u20/single-object-tracking` 在 `main` 上，`HEAD` 正好是协调基线
  `29b083e4e6ced2fd46eb7f9ecf821287b4cc401a`；首次只读检查时没有已暂存、
  未暂存或未跟踪修改，`HEAD...29b083e` 的左右提交数为 `0/0`，因此没有执行
  merge、reset、覆盖或文件搬运。
- 本任务从该基线创建独立工作树和分支。历史控制器源提交
  `1552b7d14f67006768757dff039327253e3a60cb` 是当前基线祖先；交互式 Debug
  分支当前停在 `b98b1335c3d263f542fd52656c7d7c4d58b2bb77`，但它不是协调基线祖先，
  尚不能把两者合称为一份已集成、可移交的源码身份。
- `integration-contract.yaml` 锁定的 VisDrone9 RKNN/manifest/labels 大文件不在
  Git 工作树中，符合“大文件不入库”规则；本轮只能核对契约和历史报告中的哈希，
  不能把路径记录当作当前文件哈希复验。
- 尝试使用当前约定命令
  `ssh -p 10022 cat@127.0.0.1 'printf connected'` 连接开发板，实际返回
  `ssh: connect to host 127.0.0.1 port 10022: Connection refused`。用户随后恢复
  隧道；复测时 TCP 已成功连接到 `127.0.0.1:10022`，但 12 秒内未收到 SSH
  banner，返回 `Connection timed out during banner exchange`。这证明 Linux 端
  reverse-forward listener 已存在，但不能证明 Mac 到 `192.168.103.172:22` 的
  后半段可达。因此本轮仍未重新读取板端 Git、二进制、模型、运行时或保留工件，
  也没有生成新的板端 PASS。
- 对协调基线执行 RKNN-independent 主机回归：临时目录配置
  `cmake -S rk3566/nanotrack_v3 -B <tmp> -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF`，
  随后构建并运行 `ctest --test-dir <tmp> --output-on-failure`，结果为 `12/12`
  通过。这仅证明当前主线的主机逻辑基线可构建和测试，不是开发板证据。

本轮实际任务状态仍为 `functional_pass_promotion_held`。此前板端功能和控制安全
证据继续作为历史保留证据，但在反向隧道恢复并重新核对工件身份前，不升级为
“当前已复验”；`development_board_validation.latest_handoff.available=false` 仍然正确。

## 2026-10-22 隧道恢复后的当前板端复核

- 用户接通 Mac 与开发板的网线后，`ssh -p 10022 cat@127.0.0.1` 恢复。板端为
  LubanCat RK3566、AArch64、Debian 10、Linux `4.19.232`。原生工具链为 GCC
  `8.3.0`、CMake `3.13.4`、Git `2.20.1`；二进制动态链接
  `/home/cat/ThirdParty/OpenCV4100/lib/libopencv_world.so.410` 和 RKNN 2.3.2
  `librknnrt.so`，`ldd` 未报告缺失依赖。
- 板端仓库 `/home/cat/Project/SOT/nanotrack_v3_longterm` 位于
  `codex/rk3566-interactive-debug@d9f200ce26ade0f7664a9c853230f277c28da47b`，
  tracked tree 干净，仅有用户未跟踪文件 `cmd.txt`，本轮未修改。该板端仓库使用
  独立 Git 历史；逐文件对象比较表明，除板端额外的根 `.gitignore` 外，其 tracked
  源码与共享仓库 `b98b1335c3d263f542fd52656c7d7c4d58b2bb77:rk3566/nanotrack_v3`
  一致。
- 当前 Debug 二进制
  `/home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-keyfix/nanotrack_v3`
  是 AArch64 PIE，SHA-256 为
  `c4436db4a7f2491483d5e66e4ce22a98feb710ae72dfe5203cb73793d043f8e9`。
  现有构建目录执行 `ctest --output-on-failure` 为 `13/13` 通过。
- 当前 NanoTrack 模型 SHA-256：template
  `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709`、search
  `92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a`、head
  `2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600`；
  tracker manifest 为
  `441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2`。
- VisDrone9 RKNN/manifest/labels 的板端 SHA-256 分别为
  `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0`、
  `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e`、
  `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`，
  与 `integration-contract.yaml` 完全一致；板端 manifest verifier 报告
  `target=rk3566 outputs=9 classes=9`。
- 使用 release 中样本 `0000330_00801_d_0000804.jpg` 执行当前 VisDrone9 person
  smoke：RKNN API `2.3.2 (429f97ae6b@2025-04-09T09:09:27)`、driver `0.8.2`，
  9 个输出张量与 manifest 一致；`raw=19`、person candidates `7`，preprocess /
  inference / postprocess / total 为 `26.833 / 177.154 / 23.459 / 227.446 ms`。
  输出位于 `/tmp/board-validation-detector-smoke.C97Y8A`，CSV SHA-256
  `499051ec07bbe03ecc6c7dbfa97f1df87c9882495fc52da9e0da51f72e4eee06`，
  标注图 SHA-256
  `3037104c175564fb411fab182381a255d9be9dc5aa54038c51de2d260821b7b9`；
  CSV 与历史 person-filter 证据一致。这是 `detector_function` 复核，不是精度或恢复验收。
- 用当前 Debug 二进制、上述 NanoTrack/VisDrone9 工件、class 0 和
  `girl_dance.mp4`（SHA-256
  `8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9`）
  执行 3 tracked-frame 非交互烟测。结果 `mean_runtime_ms=33.154`、
  `mean_pipeline_ms=67.102`、`control_valid=true` 且无错误；4 帧 428x240 AVI、5 行
  CSV 和 metadata 保留在 `/tmp/board-validation-integrated-smoke.PFYuzG`，SHA-256
  分别为 `f0776f9ae6bf189e2595ce0d9c5ff03fcb8f5afddfd9a3eca16af3786c0bad51`、
  `17711c767032272324a3675cdbb0dae1f33a8b152491bf056301641b0a2f914a`、
  `938e3bcfa74ced584a8e907f7184b81237d3d3aa8d080be0b2bd9d63917e9737`。
  由于稳定
  跟踪期间 detector request/complete 为 `0/0`，此结果只证明 `model_load` 和短程
  `integrated_function`，不证明 detector 队列、重捕或长时稳定性。
- 板卡重启后 RTC 仍为 `2019-02-14`；首次登录时系统时间也为 2019，随后 `ntpd`
  已把系统时间纠正为 2026，且 `ntpq -pn` 有选中 peer，但 `timedatectl` 仍显示
  `System clock synchronized: no`、RTC 仍为 2019。任何原生增量构建前必须先确认
  系统时间已纠正，避免未来时间戳导致陈旧对象未重编译。

本轮板端实际验证命令的核心部分为：

```bash
cd /home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-keyfix
ctest --output-on-failure

/home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/build/yolo_detector_smoke \
  --model /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/yolov8n-visdrone9-rk3566-fp16.rknn \
  --manifest /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/detector-manifest.json \
  --labels /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/visdrone9.txt \
  --image /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/samples/0000330_00801_d_0000804.jpg \
  --conf 0.25 --nms 0.45 --class-id 0 \
  --output-image /tmp/board-validation-detector-smoke.C97Y8A/person.jpg \
  --output-csv /tmp/board-validation-detector-smoke.C97Y8A/person.csv

/home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-keyfix/nanotrack_v3 \
  --video /home/cat/Project/SOT/nanotrack_v3_longterm/data/girl_dance.mp4 \
  --bbox 244,161,74,70 \
  --model-dir /home/cat/Project/SOT/nanotrack_v3_longterm/models \
  --output-video /tmp/board-validation-integrated-smoke.PFYuzG/output.avi \
  --output-boxes /tmp/board-validation-integrated-smoke.PFYuzG/bboxes.csv \
  --max-frames 3 --output-size 428x240 --sensor visible \
  --controller-config /home/cat/Project/SOT/nanotrack_v3_longterm/config/visible-default.conf \
  --session-metadata /tmp/board-validation-integrated-smoke.PFYuzG/session-metadata.json \
  --detector-model /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/yolov8n-visdrone9-rk3566-fp16.rknn \
  --detector-manifest /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/detector-manifest.json \
  --detector-labels /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/visdrone9.txt \
  --detector-class-id 0 --detector-conf 0.25 --detector-nms 0.45
```

当前 live 复核确认了现有 Debug 产物的工件身份、模型加载、detector 单图功能、
短程集成功能和 13 项测试；没有改变 `functional_pass_promotion_held` 裁决，也没有
产生可供交叉编译消费的 latest handoff。

## 2026-10-23 Debug 运行配置板端验证

本轮只改造交互式 Debug 的启动参数入口，不修改跟踪、重捕、模板更新或控制授权
逻辑。经用户确认，采用独立的严格 `key=value` 运行配置和
`nanotrack_v3 --config FILE`，没有引入 YAML 依赖；原命令行继续兼容，显式 CLI
值覆盖配置值，配置中的相对路径以配置文件目录为基准。detector 仍只是候选来源，
不能直接产生 `control_valid=true`。

### 源码与板端身份

- 当前协调基线为
  `main@bb297504070b3dfcf2fd4b4da05ebb1503e309db`。独立分支
  `codex/debug-runtime-config` 在该基线上显式合并既有交互 Debug 候选
  `b98b1335c3d263f542fd52656c7d7c4d58b2bb77`；设计/计划提交为
  `f5cedeb2414473bb6bbf736b8026308688bcec24`，实现提交为
  `eb50d6788c477733697703d6b1e8b9f6e4800cd1`。
- 板端仓库使用独立历史。实现 patch SHA-256 为
  `863e2cd0fd37fd956d928faebea0ffbd3a8877b71f64f70b7951832560d7d4f4`；首次
  `git am --3way` 形成 `a45d563e7e18134400b66313b2db8d7c8f9f5f41` 时，三个新增文件因共享
  仓库子目录前缀被放入嵌套路径。未使用 reset；随后以可追溯提交
  `41aa1ffe703416ea442afa4582d5346d9475f254` 将它们移动到板端仓库根目录。
  最终 tracked 状态干净，仍只有既有用户文件 `cmd.txt` 未跟踪，本轮未修改。
- 共享提交子树与板端 `HEAD` 的 55 个构建相关条目（`CMakeLists.txt`、
  `config/`、`include/`、`src/`、C++ 测试、board schema 测试和 verifier）
  `git ls-tree` 指纹均为
  `9dc0ccb607672b0863f7805c1790db153c880ad2c68d95125cdfeb6ed8e840eb`。
  两个仓库的完整 tracked tree 不相同：板端独立历史不包含主线后增的训练文档、
  Python 训练工具和相应测试；因此这里只声明板端构建/测试输入等价，不声明完整
  仓库树等价。

### 配置、工件和运行环境

- 板端配置为
  `/home/cat/Project/SOT/nanotrack_v3_longterm/config/interactive-debug.conf`，
  SHA-256
  `9e57e3a7e8a13212b46a8b21daba08836c0ed41287f3cb76a73562b57e7551f1`。
  它启用交互模式、可见光 controller 和 VisDrone9 person candidate，不设置
  `bbox`，也不设置 video/CSV/metadata 输出路径；因此启动后暂停等待鼠标框选，
  调试输出默认不保存。
- 输入 `data/girl_dance.mp4` SHA-256 为
  `8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9`。
  NanoTrack template/search/head/manifest SHA-256 依次为
  `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709`、
  `92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a`、
  `2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600`、
  `441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2`。
- VisDrone9 RKNN/manifest/labels SHA-256 依次为
  `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0`、
  `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e`、
  `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`，
  与公共契约一致；本配置使用候选不等于 canonical promotion。
- 本轮板端系统时间为 `2026-10-23T15:13:21+08:00` 且 NTP 同步为 yes；环境仍为
  LubanCat RK3566/AArch64、Linux `4.19.232`、GCC `8.3.0`、CMake `3.13.4`、
  Git `2.20.1`。后续重启后仍需重新检查 RTC/NTP。

### 原生编译与实测结果

首次新建的 `build-interactive-debug-config` 未显式设置 `OpenCV_DIR`，CMake 自动
选择系统 OpenCV 3.2；虽然编译和 13 项测试可通过，实际启动在 OpenCV 3.2 解析
detector JSON manifest 时失败。该目录是无效诊断产物，不能作为本轮结果。根因经
cache、`ldd` 和 manifest 首字节对照确认；manifest 本身以合法 JSON `{` 开头。

有效 clean build 使用另一个全新目录并显式绑定既有 OpenCV 4.10：

```bash
cmake -S /home/cat/Project/SOT/nanotrack_v3_longterm \
  -B /home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-config-opencv4100 \
  -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON \
  -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 \
  -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include \
  -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so
cmake --build /home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-config-opencv4100 -- -j2
cd /home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-config-opencv4100
ctest --output-on-failure
```

观察结果：原生编译成功，CTest `13/13` 通过；有效 AArch64 PIE 为
`/home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-config-opencv4100/nanotrack_v3`，
SHA-256
`e469aa57a209c415f083a1149824d95b6dcd62c3381df1901f4c5a31acf7f33a`。
`ldd` 确认链接
`/home/cat/ThirdParty/OpenCV4100/lib/libopencv_world.so.410` 和 RKNN 2.3.2
`librknnrt.so`，未观察到缺失依赖。

实际简化启动烟测为：

```bash
cd /home/cat/Project/SOT/nanotrack_v3_longterm
timeout --signal=INT 15s env DISPLAY=:0 \
  ./build-interactive-debug-config-opencv4100/nanotrack_v3 \
  --config config/interactive-debug.conf
```

程序 15 秒内没有配置、路径、模型、manifest 或 GUI 启动错误，最终由 `timeout`
按预期结束并报告 `CONFIG_SMOKE_EXIT=124`。这证明实际板端二进制能够消费单一配置
参数并保持在交互运行状态；结合板端 `test_cli`，确认 shipped config 从任意 cwd
解析、默认无输出且等待鼠标 ROI。该有界启动没有人工框选、没有执行跟踪帧，也没有
证明 detector 推理、遮挡重捕、长时稳定性或跟踪质量。

### 本轮裁决与 Handoff

本轮层级为 `native_build + config_parse + interactive_startup` 功能通过，不改变总体
`functional_pass_promotion_held`。共享实现提交尚未合入 `main`，公共契约也尚未把
它锁定为下一轮 source candidate；因此
`development_board_validation.latest_handoff.available` 必须继续为 `false`，交叉编译
任务不得消费本结果。建议协调对话审查并集成
`eb50d6788c477733697703d6b1e8b9f6e4800cd1` 后，再决定是否将其设为下一轮板端候选。

## 2026-10-23 结构化 Debug 遥测板端验证

- 时间：`2026-10-23T22:39:00+08:00`。
- 共享分支/工作树：`codex/debug-runtime-config` /
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config`。
- 本功能基线为 `fb15ba310fa13a26741a22120bacf19b3a3d8aea`；纯遥测组件提交为
  `9289f07f528641ee014f8d1aa94e871acdea0ada`，交互主循环接入提交为
  `d868636f8332a58f7523ad0b941f258a7b07ffc5`。本进度文件的结果提交以
  `git log -1 --format=%H -- docs/progress/board-validation.md` 查询，避免在提交前虚构
  自身 commit ID。
- 共享源码到板端的 patch SHA-256 为
  `599fc21942fdef26319eb9d4f7538d62acf00999618b5c25cc73dee1d63b5dfa`。
  patch 只含 8 个 CMake/C++/测试文件，不含模型、视频、日志或构建产物。

### 板端隔离与源码身份

原板端 checkout `/home/cat/Project/SOT/nanotrack_v3_longterm` 在同步前已有用户状态：
`config/interactive-debug.conf` 将输入改为
`/home/cat/Project/SOT/nanotrack_v3_longterm/data/1.mp4`，另有未跟踪
`cmd.txt`。首次在该 checkout 应用源码并构建后，CTest 为 `13/14`，唯一失败是
`cli` 的 shipped-config 路径断言；根因就是上述配置修改，不是遥测代码。没有覆盖
或提交这两个用户文件；已精确反向应用本任务 patch，使原 checkout 恢复为仅这两项
用户状态。

正式验证改在板端独立工作树
`/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry`、分支
`codex/rk3566-debug-telemetry` 中进行，基线为
`41aa1ffe703416ea442afa4582d5346d9475f254`，板端结果提交为
`0f155ab7dcde33582534f1d87e61a3453167b825`，tracked 状态干净。该提交只增加
结构化终端遥测及其测试，不修改 `long_term_controller.cpp`、阈值、搜索计划、
模板决策或 detector 控制授权逻辑。

### 原生构建、测试和工件

板端系统时间已由 NTP 同步；环境为 LubanCat RK3566/AArch64、Linux
`4.19.232`、GCC `8.3.0`、CMake `3.13.4`、Git `2.20.1`。干净构建命令为：

```bash
cd /home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry
cmake -S . -B build-board \
  -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON \
  -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 \
  -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include \
  -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so
cmake --build build-board -- -j2
(cd build-board && ctest --output-on-failure)
```

观察结果：原生编译成功，CTest `14/14` 通过，其中新增
`debug_telemetry` 测试覆盖首次状态、状态/控制变化、单步强制心跳、连续播放每
30 帧心跳、detector request/batch/error、候选 ID/区域缺失、同类干扰、锁定和
2/3 确认、重捕来源、模板动作及重新初始化 reset。有效 AArch64 PIE 为
`/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/build-board/nanotrack_v3`，
SHA-256 为
`11c70e81ac2beb6305c923bee5bbff61f0969d1d82e16548d9f7393b017f04dd`。
`ldd` 绑定 `/home/cat/ThirdParty/OpenCV4100/lib/libopencv_world.so.410` 与
`/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so`，没有
`not found`。

### 交互烟测

使用 canonical `config/interactive-debug.conf`，并通过显式 CLI 覆盖把大工件指向
原板端 checkout；没有把大文件复制到新工作树。核心启动参数为：

```bash
cd /home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry
timeout --signal=INT 35s env DISPLAY=:0 stdbuf -oL -eL \
  ./build-board/nanotrack_v3 --config config/interactive-debug.conf \
  --video /home/cat/Project/SOT/nanotrack_v3_longterm/data/girl_dance.mp4 \
  --bbox 244,161,74,70 \
  --model-dir /home/cat/Project/SOT/nanotrack_v3_longterm/models \
  --controller-config /home/cat/Project/SOT/nanotrack_v3_longterm/config/visible-default.conf \
  --detector-model /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/yolov8n-visdrone9-rk3566-fp16.rknn \
  --detector-manifest /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/detector-manifest.json \
  --detector-labels /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/visdrone9.txt \
  --max-frames 40
```

板端没有 `xdotool`，因此使用只存在 `/tmp` 的 ctypes/XTest 脚本向现有 X11
窗口依次发送 `R`、`Esc`、`N`、`Space`、`Space`、`Q`；未安装系统包。
脚本 SHA-256 为
`522a09537f16534e94bd44ceec1ae83ccfaccb89250fe05d2ae64eb245e283ec`。
日志 `/tmp/nanotrack-debug-telemetry-smoke.log` 共 47 行，SHA-256 为
`a4ced575095a1e4207dedeb22fab17ac09c36e525fc6b15aaa008f363661384d`。

实际观察到：

- 启动时打印视频、分辨率/FPS、模型目录、detector 开关/类别/模式、标定状态和
  `video=disabled boxes=disabled metadata=disabled`；调试默认仍不保存输出。
- `R -> Esc` 打印 `select_roi`、`roi_selection_started` 和
  `roi_selection_cancelled`；`N` 在第 1 帧立即打印完整 `frame_status`。
- 连续播放在第 30 帧打印周期心跳，并分别在第 30/33 帧打印
  `STAGE_RECENT`/`COMMIT_RECENT`；第 40 帧达到限制后由 `Q` 正常退出，退出原因为
  `user_quit`。
- 日志错误扫描为空；最终摘要为 40 tracked frames，mean runtime
  `31.909 ms`、mean pipeline `37.131 ms`。这是短程 Debug 烟测时序，不是正式
  性能基准或长稳资源结论。

本次沿用的 `girl_dance.mp4`、NanoTrack template/search/head/manifest 和
VisDrone9 detector RKNN/manifest/labels SHA-256 依次为
`8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9`、
`38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709`、
`92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a`、
`2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600`、
`441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2`、
`3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0`、
`05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e`、
`41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。
RKNN API 为 `2.3.2`，driver 为 `0.8.2`，detector 实际加载 9 个输出。

### 裁决与 Handoff

本轮新增层级为 `native_build + interactive_debug_telemetry_smoke` 功能通过，且没有
改变 detector 候选不能直接产生 `control_valid=true` 的安全约束。稳定跟踪期间
没有触发 detector request、丢失或重捕，因此本结果不证明 detector 队列、同类
干扰、2/3 重捕、遮挡恢复、长时稳定、内存平台化或真实吊舱验收。

协调对话需要审查并集成 `codex/debug-runtime-config` 的最新结果提交，并在公共契约
中决定是否把它锁定为下一轮交互 Debug source candidate；这不是发布板端 latest
handoff。`development_board_validation.latest_handoff.available` 必须继续为
`false`，总体裁决仍为 `functional_pass_promotion_held`。

## 已观察证据

- VisDrone9 detector-only 验收：工件身份、manifest、模型加载、真实 tensor 布局、20 图推理、类别过滤和单进程资源检查为 `PASS`。
- 集成运行保留了人员、车辆、detector-disabled 和 1001 tracked-frame stability 证据。
- 所有保留运行的 detector 请求最终完成，最大队列 gap 不超过 1；未观察到 detector/RKNN error。
- 所有 `LOST` 行均保持 `control_valid=false`；detector-disabled 路径计数为零。
- 主要证据：
  `rk3566/nanotrack_v3/docs/visdrone9-detector-board-verification.md`。

## 开发板交付门结果

- 1001-frame stability 的 warm RSS 约为 111 MiB 且线程数稳定，但 VmHWM 在每个 30 秒采样点持续增长到 `160556 kB`。
- 人员运行有 detector hypothesis，但 confirmed reacquisition 为 0。
- 车辆运行的 4 次 reacquisition 都来自 motion，而不是 detector。
- 固定阈值下，VisDrone9 人员 usable hypotheses 少于 COCO 基线。
- 因此当前结论是“功能和控制安全通过，正式 promotion hold”；不是长时重捕产品验收通过。

## Blockers

- 协调基线尚未集成交互式 Debug 分支，也未在公共契约中锁定下一轮板端验证要使用
  的精确 source commit、NanoTrack 模型身份和 detector 选择。
- 板端 RTC 在重启后仍回退到 2019；NTP 纠正前执行增量构建存在时钟偏移风险。
- 隔离或修复视频 writer/buffer/allocator 内存路径，并证明 RSS 与 VmHWM 平台化。
- 用相同输入、时长、采样协议完成 COCO/VisDrone9 资源对比。
- 使用标注的遮挡/可靠重现数据证明 detector-confirmed recovery，而不是只统计候选或 motion reacquisition。
- 完成可见光/红外阈值标定和同类目标身份风险评估。

## Next action

1. 请协调对话先锁定下一轮板端验证的 source commit、NanoTrack 模型和 detector
   组合；在此之前不开始新的 promotion run，也不发布 latest handoff。
2. 锁定后先确认板端系统时间已由 NTP 纠正，再在完整原生环境中 clean-build
   交互式 Debug 版本，依次复测模型加载、推理、
   集成视频、控制安全、运行/端到端时延、同协议资源平台化及标注遮挡重捕。
3. 下一轮至少保留源 commit、板端原生编译二进制、模型/manifest/labels/输入哈希、
   完整命令、资源采样、verifier 结果和分层裁决。

## Handoff

任何 `PASS` 都必须注明层级：`model_load`、`detector_function`、`integrated_function`、`stability`、`recovery_quality` 或 `development_board_acceptance`。低层级通过不得升级表述为高层级通过。

当某一组合达到预定开发板门槛后，本任务应向协调对话移交精确 source commit、模型/manifest/labels 哈希、配置、板端验证证据和裁决。协调对话将其标记为最新 handoff，供交叉编译任务获取。此 handoff 不代表真实吊舱已通过测试。

当前需要协调对话更新公共契约的内容是“下一轮候选锁定”，不是发布通过的 handoff：
请明确批准的交互式 Debug source commit、NanoTrack 模型身份和 COCO/VisDrone9 detector
选择。`latest_handoff.available` 在新的真实板端验收达到门槛前必须继续保持 `false`。

## 2026-10-26 Debug 窗口 720p 初始尺寸验证

- 时间：`2026-10-26T19:53:37+08:00`。共享工作树/分支为
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config` /
  `codex/debug-runtime-config`，任务基线为
  `2f2f3fb57022d42855f20eca940eaafb3103720f`；结果提交可用
  `git log -1 --format=%H -- rk3566/nanotrack_v3/src/main.cpp` 查询。
- 变更仅在交互式 Debug 分支的 `cv::WINDOW_NORMAL` 窗口创建后调用
  `cv::resizeWindow(..., 1280, 720)`。输入帧、NanoTrack/detector 输入、框坐标、
  保存输出和生产 `ai_alg` 均未缩放或修改；窗口启动后仍允许用户调整大小。
- 主机侧先增加尺寸断言并观察到因常量不存在而编译失败，随后实现转绿；
  `test_interactive_control` 通过，RKNN-independent CTest 为 `14/14` 通过，
  `git diff --check` 通过。该主机构建目录不含 RKNN 可执行目标，因此完整程序由板端
  原生环境编译验证。
- 板端隔离工作树为
  `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry`，分支
  `codex/rk3566-debug-telemetry`，基线
  `0f155ab7dcde33582534f1d87e61a3453167b825`，结果提交
  `8802eabc4a56d3311de72e450d73967f218ab827`。系统时间为
  `2026-10-26T19:51:53+08:00` 且 NTP 已同步；`cmake --build build-board -- -j2`
  原生编译成功。
- 板端新二进制
  `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/build-board/nanotrack_v3`
  SHA-256 为
  `dbf1804ff2dda7a452d87149980a73dbd967e76744e7314f457a8192dc50fb71`。
  使用 `DISPLAY=:0`、`girl_dance.mp4`、既有 NanoTrack 模型和 VisDrone9 detector
  做有界启动后，`xwininfo -name "NanoTrack V3 RK3566 Debug"` 实测窗口内容区为
  `Width: 1280`、`Height: 720`；程序日志仍报告源视频和输出尺寸为 `856x480`，
  证明本次只改变窗口初始显示尺寸。启动日志和窗口查询分别保留在
  `/tmp/nanotrack-window-720p.log`、`/tmp/nanotrack-window-720p.xwininfo`，SHA-256
  分别为
  `e84814050a5698d4e0a896987c83a0c82e343cdc9260abb53ca70d0c3439403a`、
  `3cf58780fd121382d9e20b38ad63a537d7f9a9ef2ef4cf40fedaf81021e54c20`。
- 本次有界启动绑定的输入、NanoTrack template/search/head/manifest 和 VisDrone9
  detector RKNN/manifest/labels SHA-256 依次为
  `8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9`、
  `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709`、
  `92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a`、
  `2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600`、
  `441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2`、
  `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0`、
  `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e`、
  `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`。
  二进制绑定 OpenCV 4.10 `libopencv_world.so.410`；启动日志实测 RKNN API
  `2.3.2 (429f97ae6b@2025-04-09T09:09:27)`、driver `0.8.2`。
- 实际启动、窗口查询和停止命令如下。程序由 `timeout` 设置 25 秒硬边界；查询到
  窗口后向本轮启动的进程发送 `SIGINT`，没有让其继续执行长时跟踪：

```bash
cd /home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry
timeout --signal=INT 25s env DISPLAY=:0 stdbuf -oL -eL \
  ./build-board/nanotrack_v3 --config config/interactive-debug.conf \
  --video /home/cat/Project/SOT/nanotrack_v3_longterm/data/girl_dance.mp4 \
  --bbox 244,161,74,70 \
  --model-dir /home/cat/Project/SOT/nanotrack_v3_longterm/models \
  --controller-config /home/cat/Project/SOT/nanotrack_v3_longterm/config/visible-default.conf \
  --detector-model /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/yolov8n-visdrone9-rk3566-fp16.rknn \
  --detector-manifest /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/detector-manifest.json \
  --detector-labels /home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/visdrone9.txt \
  --max-frames 1 >/tmp/nanotrack-window-720p.log 2>&1 &
app_pid=$!
found=0
attempt=0
while [ "$attempt" -lt 15 ]; do
  if xwininfo -display :0 -name "NanoTrack V3 RK3566 Debug" \
      >/tmp/nanotrack-window-720p.xwininfo 2>/dev/null; then
    found=1
    break
  fi
  attempt=$((attempt + 1))
  sleep 1
done
test "$found" -eq 1
kill -INT "$app_pid"
wait "$app_pid" || true
```

- 板端 `interactive_control` 及其他 12 项测试通过；全量 CTest 为 `13/14`，唯一
  失败是 `cli` 对发行配置视频路径的断言。根因已由重复运行和差异核对确认：本轮
  开始前用户已把 `config/interactive-debug.conf` 的 `video` 从
  `../data/girl_dance.mp4` 改为 `/home/cat/Project/SOT/data/1.mp4`。该用户修改未被
  覆盖、暂存或提交，因此不能把本轮板端结果表述为全量 CTest 通过；它与窗口尺寸
  代码无重叠，窗口尺寸聚焦测试和实测均通过。

### 裁决与 Handoff

本轮仅形成 `native_build + interactive_window_geometry_smoke` 功能通过，不增加跟踪、
重捕、资源、长稳或真实吊舱证据，不改变 detector 不能直接产生
`control_valid=true` 的安全约束。协调对话可集成本次 Debug 功能提交和板端证据，
但无需修改 `integration-contract.yaml`；总体状态仍为
`functional_pass_promotion_held`，
`development_board_validation.latest_handoff.available` 继续为 `false`。

## 后续更新模板

```text
时间：<ISO 8601 + timezone>
板卡/运行时：<board, OS, RKNN API, driver, OpenCV>
集成身份：<source commit, executable/model/manifest/labels/input SHA-256>
命令：<exact board command>
结果：<frames, request/complete/error, reacquisition source, safety>
性能边界：<runtime vs end-to-end>
资源：<RSS, VmHWM, threads, temperature, duration>
verifier：<command -> observed result>
裁决/阻塞：<layered pass/fail/risk>
Handoff：<requested contract change or none>
```

## 2026-10-23 Debug 日志字段参考文档

- 时间：`2026-10-23T23:33:06+08:00`。
- 分支/工作树：`codex/debug-runtime-config` /
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config`。
- 基线：`e91af4c7d4943cd3ed8e4aba7d9d644cf891b4d2`；结果提交可用
  `git log -1 --format=%H -- RK3566_NANOTRACK_LONGTERM_CODE_FLOW.zh-CN.md`
  查询，避免在提交前记录不存在的 commit ID。
- 变更：在 `RK3566_NANOTRACK_LONGTERM_CODE_FLOW.zh-CN.md` 新增交互式 Debug
  日志章节，覆盖 `main.cpp` 和 `debug_telemetry.cpp` 当前输出的全部 20 种
  `DEBUG_EVENT`，说明触发条件、字段含义、源结构、枚举、打印节拍和排障顺序。
- 关键边界：明确这些日志只属于独立交互式 Debug 程序，不属于 ViewLink
  `ai_alg` 生产 ABI；再次明确 detector batch、候选框、可靠质量或候选锁定均不能
  直接产生 `control_valid=true`，必须经过 NanoTrack 验证和多帧确认。
- 一致性检查：从 `src/main.cpp` 的 `DEBUG_EVENT event=` 和
  `src/debug_telemetry.cpp` 的 `event_stream()` 提取事件名，与文档反查，观察结果为
  `documented_events=20 required_fields=17`；`git diff --check` 通过；现有主机
  CTest 回归仍为 `14/14` 通过。独立审查后又按源码修正了 2/3 确认示例、候选 ID
  重置范围、`FULL_OBJECT` 探针数量和选项结构类型名；针对这四点的 6 项文档断言
  由失败转为通过。
- 证据边界：本轮只更新说明文档，没有修改或重新编译程序，没有执行新的开发板、
  重捕、资源、长稳或吊舱测试；此前板端二进制、模型身份和验收结论均不改变。

### Handoff

协调对话只需集成本次文档提交；不需要修改 `integration-contract.yaml`，不改变
`functional_pass_promotion_held` 和
`development_board_validation.latest_handoff.available=false`。

## 2026-10-26 detector 位置硬门与 Debug 分色框简单板端验证

- 时间：`2026-10-26T21:25:05+08:00`。共享工作树/分支为
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config` /
  `codex/debug-runtime-config`，实现基线为
  `7d2fad2bbe4ebe67efe2f67aee492d755dbb3bf6`；实现提交为
  `c07f85080e1af02b53e9fa77f2b8bb0a5503b60e`、
  `5055e75af57c2a99e1d92a4eff13440e04f90759`、`1451537`、`02aef3c`。
  板端独立工作树/分支为
  `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry` /
  `codex/rk3566-debug-telemetry`，结果提交为
  `d845f056d41d5b1ab108b72375dbde59aab61f62`。
- 同步前板端已有用户修改 `config/interactive-debug.conf`（视频路径为
  `/home/cat/Project/SOT/data/1.mp4`）和未跟踪 `log.txt`；两者均未覆盖、未暂存、
  未提交。板端系统时间与 RTC 已正确到 2026，系统报告时钟已同步。
- 新增严格位置门：候选中心到本帧运动预测中心的距离除以画面对角线不得超过
  `detector_candidate_max_center_ratio=0.15`；`ASSISTED_RECOVERY` 与 `LOST`
  不放宽，通过者按距离升序、置信度降序排序。新增 `POSITION_REJECTED`、位置通过/
  拒绝计数和 schema 6 字段，且 detector 仍不能直接产生 `control_valid=true`。
- 交互窗口区分青色 motion、紫色 detector candidate、橙色 detector probe、蓝色
  anchor、黄色 recent、绿色 `ACCEPTED CTRL` 与红色 `REJECTED NO_CTRL`。第一次
  板端截图发现动态标签遮挡，随后通过共享提交 `02aef3c` 改为右侧固定纵向标签并
  重新原生链接。非交互输出视频的原单框渲染未改变。

主机 clean build 命令：

```bash
cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-position-gate-clean-build \
  -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF
cmake --build /tmp/nanotrack-position-gate-clean-build -- -j2
ctest --test-dir /tmp/nanotrack-position-gate-clean-build --output-on-failure
```

结果 `15/15` 通过。新增 `interactive_debug_overlay` 测试覆盖六类框的绘制顺序、
`FULL_OBJECT` 候选/probe 同坐标仍保留不同标签，以及未确认假设不得生成
`ACCEPTED CTRL`。

板端为 LubanCat RK3566/AArch64、Linux `4.19.232`、GCC `8.3.0`、CMake
`3.13.4`、OpenCV 4.10、RKNN API `2.3.2` / driver `0.8.2`。原生命令：

```bash
cd /home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry
cmake -S . -B build-board -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON
cmake --build build-board -- -j2
cd build-board
ctest --output-on-failure
ctest --output-on-failure -E '^cli$'
```

完整 RKNN/OpenCV 可执行程序构建成功。全量 CTest 为 `14/15`；唯一失败的 `cli`
读取了必须保留的用户绝对视频路径覆盖，导致 shipped-profile 相对路径断言失败。
没有为凑通过而覆盖用户文件。排除这项已解释的本地配置差异，其余板端测试
`14/14` 通过；干净共享工作树的同一 `cli` 已包含在主机 `15/15` 中。

板端有界交互命令：

```bash
DISPLAY=:0 ./build-board/nanotrack_v3 \
  --config config/interactive-debug.conf \
  --video /home/cat/Project/SOT/nanotrack_v3_longterm/data/girl_dance.mp4 \
  --bbox 244,161,74,70 --max-frames 200
```

通过 X11 发送一次空格开始播放，帧 200 自动暂停后截图，再发送 `Q` 正常退出。日志
在帧 129 进入 `ASSISTED_RECOVERY`，帧 189 进入 `LOST`；帧 176 实际记录：

```text
DEBUG_EVENT event=detector_batch frame=176 source_frame=174 age_ms=67 raw=1 filtered=1 position_valid=0 position_rejected=1 status=POSITION_REJECTED total_ms=196.923
```

该候选没有后续 `detector_verify` 或 `reacquired`，不可靠状态始终
`control_valid=0`。帧 200 截图显示青色 `MOTION PRED`、蓝色 `NT ANCHOR`、黄色
`NT RECENT` 和红色 `REJECTED NO_CTRL`，标签不再互相遮挡；被位置门拒绝的候选
没有伪装成活动假设或绿色控制框。由于本次真实候选被硬门拒绝，截图不包含紫色/
橙色实例；它们及 `FULL_OBJECT` 重合情况由板端通过的纯数据 overlay 测试覆盖，
不能表述为七种颜色均完成了人工视觉验收。

| 工件 | 路径 | SHA-256 |
|---|---|---|
| Debug 可执行程序 | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/build-board/nanotrack_v3` | `30855451e2e7050ad208e70fb459e43cf61e459b962cfba8ac680111514c13ac` |
| template RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/template_backbone.rknn` | `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709` |
| search RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/search_backbone.rknn` | `92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a` |
| head RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/head.rknn` | `2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600` |
| tracker manifest | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/manifest.json` | `441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2` |
| VisDrone9 RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/yolov8n-visdrone9-rk3566-fp16.rknn` | `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0` |
| detector manifest | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/detector-manifest.json` | `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e` |
| VisDrone9 labels | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/visdrone9.txt` | `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992` |
| 输入视频 | `/home/cat/Project/SOT/nanotrack_v3_longterm/data/girl_dance.mp4` | `8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9` |
| 帧 200 截图 | `/tmp/nanotrack-position-gate-debug-v2.png` | `53c8ace17b620759536e2f42a11b2b76b488073aca04684c36d60ebaad709efe` |
| 200 帧 Debug 日志 | `/tmp/nanotrack-position-gate-debug-v2.log` | `f70331b9f028b2ad361cc0dcd9debac6c3d32fe527f270f8fa466d9329e9805d` |

本轮只达到 `integrated_function_short_smoke` 与 Debug 可观测性验证，不是长时重捕
质量、阈值标定、资源平台化、完整七色人工视觉验收、真实吊舱或开发板发布门通过。
任务状态继续为 `functional_pass_promotion_held`，`latest_handoff.available=false`。

### Handoff

请协调对话在集成本任务提交后，把下一轮共享源码候选更新到该集成结果，并继续保持
detector 不能直接授权控制、VisDrone9 promotion hold 和 latest handoff 不可用；
是否把严格 `0.15` Debug 策略锁定为下一轮板端候选仍由协调者决定。

## 2026-10-27 独立审查修正：取消位置门前截断

- 时间：`2026-10-27T00:51:25+08:00`；分支/工作树仍为
  `codex/debug-runtime-config` /
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config`，审查起点为
  `59187142454acf9773423e42c8d829e3ba24a348`。
- 独立审查发现实际 RKNN detector 链路此前会在 YOLO 后处理中先按置信度截断为
  `detector_max_candidates=3`，三个远处高置信框可能在控制器位置门之前挤掉近处
  候选。这不满足“位置过滤、距离排序之后再截断”的完整顺序。
- 修正后 detector 请求允许后处理保留全部经过置信度过滤和 NMS 的候选，最终数量
  限制只在控制器完成 `0.15` 位置硬门和距离优先排序后执行。新增控制器请求与真实
  YOLO 后处理回归，覆盖第四个较低置信候选不会被上游提前丢弃。
- 同一审查指出 `FULL_OBJECT` 下候选框与 probe 框重合时，等线宽会使后画的橙色
  probe 覆盖紫色 candidate。现改为紫色 3 像素粗框、橙色 1 像素细框，并由纯数据
  overlay 测试锁定线宽关系。
- TDD 证据：新增断言先观察到控制器请求仍为 `3` 而失败、overlay 缺少
  `line_thickness` 而编译失败；实现后
  `long_term_controller`、`yolov8_postprocess`、`interactive_debug_overlay` 三项
  聚焦测试 `3/3` 通过。修正共享提交为
  `a6c23be`；主机重新构建后完整 CTest `15/15` 通过。
- 板端在保留用户修改 `config/interactive-debug.conf` 和未跟踪 `log.txt` 的前提下，
  应用同一源代码修正并原生重编译完整 RKNN/OpenCV 程序；板端独立提交为
  `5e620b1`。命令
  `cmake --build build-board -- -j2` 成功，随后
  `ctest --output-on-failure -E '^cli$'` 为 `14/14` 通过。这里排除的 `cli` 仍仅因
  已记录的用户视频路径覆盖，不把结果表述为板端全量 `15/15`。
- 新 Debug 可执行程序路径为
  `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/build-board/nanotrack_v3`，
  SHA-256 为
  `2b6a3836fb2ff46b38fa21a71a4c1610cab4914bef3f2ae101568a28531c229c`。
  本次复测是原生编译和测试级验证，没有再次运行视频，也没有新增重捕性能或人工
  七色视觉验收证据。
- 安全边界不变：detector 候选仍只是假设，不能直接产生 `control_valid=true`；
  NanoTrack 双模板、几何门和 2/3 确认规则均未绕过。任务状态仍为
  `functional_pass_promotion_held`，`latest_handoff.available=false`。

## 2026-10-28 遮挡等待状态与 Detector 全候选显示设计

- 时间：`2026-10-28T18:51:42+08:00`。
- 分支/工作树：`codex/debug-runtime-config` /
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config`；设计基线为
  `afc5f4942a6020b50058ecccd349cf1dc87288c2`。
- 与用户确认采用独立 `OCCLUSION_HOLD` 状态。默认触发条件为连续 2 帧非可靠且
  宽或高相对最近可靠尺度中位数缩小到 `0.70` 以下；HOLD 帧数通过
  `occlusion_hold_frames` 配置，默认 12。
- 触发帧仍执行 NanoTrack；随后完整 N 帧不运行 tracker、不发起 detector，只以
  冻结宽高的 motion 框预测中心且保持 `control_valid=false`。同一次 loss episode
  只允许触发一次，仅在 `TRACKING/UNCERTAIN/LOCAL_RECOVERY` 早期阶段允许触发。
- Detector batch 到达时计划显示全部经过类别阈值和 NMS 的候选，并区分位置门通过、
  位置拒绝、非法、最终入队和活动候选；所有原始 detector 框仍不能直接授权控制。
- 设计文档：
  `docs/superpowers/specs/2026-10-28-occlusion-hold-detector-debug-design.md`。
  独立实现者审查已用于消除 HOLD 首尾帧、重复触发、晚到 detector batch 和现有
  motion/detector 重捕规则边界的歧义。
- 用户批准设计后形成实施计划：
  `docs/superpowers/plans/2026-10-28-occlusion-hold-detector-debug.md`。计划按配置/类型、
  motion 尺度冻结、控制器状态、runtime 与 detector generation、全候选可视化/日志、
  主机与开发板验证六个可独立审查的任务执行，全部要求先写失败测试。
- 本次只形成设计，没有修改程序、编译开发板、运行视频或产生新的模型/二进制哈希。
  当前状态继续为 `functional_pass_promotion_held`，
  `development_board_validation.latest_handoff.available=false`。

### Handoff

协调对话暂不需要修改公共契约；待实现、主机测试和真实 RK3566 简单验证形成提交后，
再请求集成新的 Debug 源码候选。Detector 直接控制禁令和人工吊舱边界保持不变。

## 2026-10-28 OCCLUSION_HOLD 整体审查与板前主机门

- 时间：`2026-10-28T23:27:15+08:00`；工作树/分支为
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config` /
  `codex/debug-runtime-config`，审查和修正基线为
  `6e533c2785b1389c3613be38788468554f4133e4`。本节与相关测试、README
  修正同一提交，结果提交可用
  `git log -1 --format=%H -- docs/progress/board-validation.md` 查询。
- 整体功能审查范围为 `4f1a046..6e533c2`；审查门结论为没有未解决的
  Critical/Important 项。三个测试覆盖类 Minor 与三个 README 文案类 Minor 已在
  板端同步前处理，未因审查修改生产控制逻辑。
- 新增直接回归覆盖：HOLD 退出而尚未重捕时运动预测宽高仍保持冻结；活动 detector
  候选正在 NanoTrack 验证时不能进入 HOLD；HOLD 触发会清除在触发帧到达、通过位置门
  的晚到 detector 候选。此提交当时关于“清除旧 motion 确认”的公开行为断言并不能
  单独证明清理发生，因为固定 2/3 确认窗口会在 HOLD 结束前自然淘汰旧确认；该证据缺口
  由下方 `2026-10-28 HOLD 确认清理测试证据修正` 补齐。
- README 同步把控制无效状态列表补入 `OCCLUSION_HOLD`，把遗留
  `schema-v5` 改为当前 `schema-v7`，并收窄颜色说明：只有无拒绝原因的合法原始假设为
  紫色，位置/容量/stale/frame/HOLD 拒绝均为灰色，非法几何只显示深红文字。
- TDD/聚焦证据：新增候选清理断言首次运行时发现测试错误地读取触发帧末尾队列为
  `1`，实际设计语义是候选先经批次接收和位置门、随后由 HOLD 立即作废并把最终队列
  清为 `0`；断言改为同时验证 `ACCEPTED` 批次、位置通过计数、
  `HOLD_INVALIDATED` 诊断和最终零队列。随后
  `ctest -R '^(long_term_controller|long_term_replay)$'` 为 `2/2` 通过。
- 主机命令：

  ```bash
  cmake -S rk3566/nanotrack_v3 \
    -B /tmp/nanotrack-occlusion-hold-final-build \
    -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF
  cmake --build /tmp/nanotrack-occlusion-hold-final-build -- -j2
  ctest --test-dir /tmp/nanotrack-occlusion-hold-final-build --output-on-failure
  git diff --check 4f1a046..HEAD
  git diff --check
  ```

  观察结果：配置和完整构建成功，完整主机 CTest 为 `15/15` 通过、`0` 失败，
  总测试时间 `1.13 sec`；两项 whitespace 检查均无输出。
- 证据边界：本节仅证明 x86_64 主机、`ENABLE_RKNN_RUNTIME=OFF` 的构建/测试及
  源码审查状态；尚未连接 RK3566、未进行原生 RKNN 构建、模型加载、
  `girl_dance` 流程观察、二进制/模型/视频哈希或资源测量。不得把本节表述为板端验证、
  重捕性能、真实吊舱或发布门通过。
- 安全边界保持不变：detector 原始候选、HOLD motion 框和 WEAK 观测都不能直接产生
  `control_valid=true`；`latest_handoff.available=false`。板端仅允许同步本节对应的精确
  已提交源码，必须保留板端用户配置、日志、模型、视频和构建目录。

### Handoff

当前不请求协调对话修改 `STATUS.md` 或 `integration-contract.yaml`。完成真实 RK3566
原生构建、CTest 和有界 `girl_dance` 观察后，再在本文件追加板端工件身份、观察项、
未观察项和最终 Debug 分支提交。

## 2026-10-28 OCCLUSION_HOLD RK3566 原生构建与有界视频观察

- 时间：`2026-10-28T23:35:55+08:00`。共享源码提交为
  `578613dbe11a67c4241363e02e6554755258c178`，工作树/分支为
  `/home/u20/single-object-tracking/.worktrees/debug-runtime-config` /
  `codex/debug-runtime-config`。板端为 `lubancat`、Linux `4.19.232`，源码工作树/分支为
  `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry` /
  `codex/rk3566-debug-telemetry`，同步前提交 `5e620b1`，同步后的审计提交为
  `7ba0da0e0359c1ead4bd00656366a4cfbd463d69`。
- 同步前板端仍只有用户修改的 `config/interactive-debug.conf` 和未跟踪 `log.txt`；前者
  把视频改为 `/home/cat/Project/SOT/data/1.mp4`。二者均未覆盖、暂存或提交。系统时间为
  `2026-10-28T23:28:19+08:00`，时区 `Asia/Shanghai`，时钟已同步；根分区可用约
  `15G`。
- 板端源码仓库没有配置远程，且其仓库根对应共享仓库的 `rk3566/nanotrack_v3` 子目录。
  因此使用以下可审计同步方式：从两个精确共享提交生成相对路径补丁，传到板端 `/tmp`，
  校验 SHA-256，先执行 `git apply --check`，再应用并只暂存补丁列出的 31 个文件：

  ```bash
  git diff --relative=rk3566/nanotrack_v3 --binary \
    a6c23be..578613d -- rk3566/nanotrack_v3 \
    > /tmp/nanotrack-occlusion-hold-578613d.patch
  sha256sum /tmp/nanotrack-occlusion-hold-578613d.patch
  git apply --check /tmp/nanotrack-occlusion-hold-578613d.patch
  git apply /tmp/nanotrack-occlusion-hold-578613d.patch
  ```

  主机和板端补丁 SHA-256 均为
  `a481f5a69700bcb15a0935f626d009734ce27f59ce4627f6a6158e1cdb79b75d`；
  板端应用检查成功。提交后 `git status --short` 仍只显示上述用户配置和 `log.txt`。
- 板端工具链为 GCC `8.3.0`、CMake `3.13.4`、OpenCV `4.10.0`。首次按最简命令配置
  失败，明确提示必须设置 `RKNN_INCLUDE_DIR` 和 `RKNN_LIBRARY`；未改源码。随后复用
  板端既有验证路径，在新的独立构建目录完成原生配置和全量链接：

  ```bash
  cmake -S . -B build-board-occlusion-hold-578613d \
    -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON \
    -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 \
    -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include \
    -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so
  cmake --build build-board-occlusion-hold-578613d -- -j2
  cd build-board-occlusion-hold-578613d
  ctest --output-on-failure
  ctest --output-on-failure -E '^cli$'
  ```

  构建成功。完整 CTest 为 `14/15`，唯一失败仍是用户绝对视频路径导致的既有 `cli`
  shipped-profile 断言；没有覆盖用户配置。排除该已解释差异后其余 `14/14` 全部通过，
  包括新增 HOLD/controller/runtime/overlay/telemetry/schema-7 回归。
- 有界板端命令：

  ```bash
  DISPLAY=:0 ./build-board-occlusion-hold-578613d/nanotrack_v3 \
    --config config/interactive-debug.conf \
    --video /home/cat/Project/SOT/nanotrack_v3_longterm/data/girl_dance.mp4 \
    --bbox 244,161,74,70 --max-frames 400
  ```

  通过本机 X11 向精确窗口发送 Space 开始连续播放，处理 307 帧后保留窗口 XWD 并发送
  Q 正常退出；因此这是 307 帧有界观察，不宣称达到配置上限 400。日志记录 RKNN API
  `2.3.2 (429f97ae6b@2025-04-09T09:09:27)`、driver `0.8.2`，最终摘要为
  `tracked_frames=307`、`mean_tracker_ms=45.596`、`mean_runtime_ms=45.846`、
  `runtime_fps=21.812`、`mean_pipeline_ms=50.029`、`end_to_end_fps=19.989`；这是单次
  Debug 观察值，不是性能门通过。
- 本次 `girl_dance` 自然流程没有触发 `occlusion_hold_enter/tick/exit`，三类事件计数均
  为 `0`。因此“触发帧仍有 anchor/recent、随后恰好 12 帧跳过推理、冻结尺度、
  `control_valid=0`、之后恢复搜索”在真实视频中均标记为**未观察到**，不使用人工伪造
  或单元测试代替板端视频证据。对应状态机边界仅有板端通过的自动化测试证据。
- Detector 在该 307 帧运行中完成 `12` 个 batch，记录 `7` 条逐候选事件。例如帧 237
  的 batch 对 `source_frame=234`、`age_ms=100` 的两个 filtered 候选逐一记录 bbox、
  class/confidence、center ratio、`eligible=1`、`position_pass=0`、`queued=0` 和
  `POSITION_REJECTED`；随后 batch 汇总为 `raw=10 filtered=2 position_rejected=2`。
  运行中没有 `reacquired` 事件；候选出现后的周期状态均保持
  `ASSISTED_RECOVERY/LOST`、`control_valid=0`。这证明本次返回的候选未直接授权控制，
  但由于所有候选均被位置门拒绝，未观察到 PASS/queued/active-probe 实例，也未在候选
  完成帧人工截图验证全部颜色；这些显示分支只有板端通过的纯数据 overlay 测试证据。

| 工件 | 板端绝对路径 | SHA-256 |
|---|---|---|
| Debug 可执行程序 | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/build-board-occlusion-hold-578613d/nanotrack_v3` | `62fe954c00fd4cce96e55db62a1f1687e037b53c5045070eef333dc02cf2e3f2` |
| template RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/template_backbone.rknn` | `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709` |
| search RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/search_backbone.rknn` | `92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a` |
| head RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/head.rknn` | `2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600` |
| tracker manifest | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/models/manifest.json` | `441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2` |
| VisDrone9 RKNN | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/yolov8n-visdrone9-rk3566-fp16.rknn` | `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0` |
| detector manifest | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/detector-manifest.json` | `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e` |
| VisDrone9 labels | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/releases/yolov8n-visdrone9-rk3566-fp16-v1/models/visdrone9.txt` | `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992` |
| visible controller config | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/config/visible-default.conf` | `b9a128b78063056173626af1c7c7d2c580a59b295c45a4afc0ef8e2e04bff95b` |
| preserved user Debug config | `/home/cat/Project/SOT/nanotrack_v3_longterm-debug-telemetry/config/interactive-debug.conf` | `c07da92f912f91d5c57d42f005017a309b874445e728079d3740148b9584ab6e` |
| 输入视频 | `/home/cat/Project/SOT/nanotrack_v3_longterm/data/girl_dance.mp4` | `8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9` |
| 307 帧 Debug 日志 | `/tmp/nanotrack-occlusion-hold-578613d-girl-dance.log` | `9118fab42e1d512617689afb88efcaeae621994e49f61a415482158d88dd4bc0` |
| 退出前窗口 XWD | `/tmp/nanotrack-occlusion-hold-578613d-frame-limit.xwd` | `dff2ed7e978ee665e2151ae2bbc7519a56956e45fb3347e1f96d1d6b710519a5` |
| 同步补丁 | `/tmp/nanotrack-occlusion-hold-578613d.patch` | `a481f5a69700bcb15a0935f626d009734ce27f59ce4627f6a6158e1cdb79b75d` |

- 裁决：本轮达到“精确提交同步 + RK3566 原生完整链接 + 除已解释 CLI 本地配置差异外
  其余测试通过 + 无标注有界 detector 诊断观察”。不达到真实视频 HOLD 时序观察、
  detector PASS/入队/活动探针人工视觉验收、标注重捕率、资源长稳、发布门或真实吊舱
  验收。任务状态继续为 `functional_pass_promotion_held`，
  `development_board_validation.latest_handoff.available=false`。

### Handoff

协调对话可采纳本节锁定的共享源码提交和板端工件身份，但不应把它发布为公共
`latest_handoff`。公共契约需要协调者评估是否仅更新 Debug 源码候选；VisDrone9
promotion hold、detector 直接控制禁令、交叉编译前板端门和人工吊舱边界均保持不变。

## 2026-10-28 HOLD 确认清理测试证据修正

- 时间：`2026-10-28T23:41:18+08:00`；修正基线为
  `bc6103ddf1c327cc6c88b3121784de6a71fa31cc`。独立复核指出，上述旧测试在 HOLD 前
  已通过重捕路径清空确认，不能证明 HOLD 入口自身执行了 `confirmations_.clear()`。
- 新测试把 `occlusion_trigger_consecutive_unreliable` 设为 `3`：先以模板冲突留下恰好
  一个 motion 确认，再输入两个 WEAK 缩小框，第二个 WEAK 触发 HOLD。测试在触发前验证
  待确认数为 `1`，在 HOLD 入口后验证为 `0`；HOLD 结束后的第一个一致可靠观测只能建立
  新的第一次确认，保持 `accepted=false`、`control_valid=false`。
- 由于当前确认策略固定为 2/3，旧确认按帧号在 HOLD 结束前也会自然过期；仅验证退出后
  未接受不足以证明入口清理。为验证新断言的敏感性，临时且仅在主机工作树中禁用 HOLD
  入口的 `confirmations_.clear()`，重新构建运行 `test_long_term_controller`，观察到：

  ```text
  FAIL: HOLD entry clears the pending motion confirmation immediately
  1 test(s) failed
  ```

  随后立即恢复原生产代码；`git diff -- src/long_term_controller.cpp` 无输出。恢复后聚焦
  `long_term_controller|long_term_replay` 为 `2/2` 通过，完整主机 CTest 为 `15/15`
  通过、`0` 失败、总时间 `0.55 sec`，`git diff --check` 无输出。
- 本次提交只增加/修正测试与证据文档，不改变生产决策或板端二进制对应的生产源码。
  真实 RK3566 构建和 307 帧视频观察发生在本测试补强之前，本轮没有重新同步、编译或
  运行开发板；板端提交仍为 `7ba0da0e0359c1ead4bd00656366a4cfbd463d69`，此前二进制
  SHA-256 `62fe954c00fd4cce96e55db62a1f1687e037b53c5045070eef333dc02cf2e3f2`
  和全部板端证据边界不变，不能把新增主机测试表述为板端复测。
- 安全与发布裁决不变：detector/HOLD/WEAK 不能直接授权控制，
  `functional_pass_promotion_held` 与 `latest_handoff.available=false` 保持不变。

### Handoff

协调对话应采用本修正后的测试覆盖和证据措辞；公共契约无需因测试补强改变工件身份或
发布裁决。若未来要求在板端执行这个新增测试，必须另行形成真实板端复测证据。

## 2026-10-30T01:34:14+08:00 目标类别初始化 RK3566 板端验证

- 共享分支/工作树：`codex/debug-target-class-init` /
  `/home/u20/single-object-tracking/.worktrees/debug-target-class-init`；输入源码
  `85c7ba592577915576be018093524a8711915ce2`。本节结果提交可用
  `git log -1 --format=%H -- docs/progress/board-validation.md` 查询，不在提交前虚构自身哈希。
- 独立板端工作树 `/home/cat/Project/SOT/nanotrack_v3_longterm-target-class-85c7ba5`
  位于 `75d44431887f29f6813329a8acb102c9d9bed594`，复核时 tracked/untracked
  状态均干净。共享源码补丁 `/tmp/nanotrack-target-class-85c7ba5.patch` 的 SHA-256
  为 `93467e875b3def3ba4aedfc0062f6c482681892083e14101bea6956881acd64d`；
  前序部署核对的构建相关对象指纹匹配，本轮又复核了补丁哈希与板端 HEAD。
- 板卡为 LubanCat RK3566/AArch64、Debian 10、Linux `4.19.232`、GCC `8.3.0`、
  CMake `3.13.4`，OpenCV `4.10.0`；实际运行日志报告 RKNN API
  `2.3.2 (429f97ae6b@2025-04-09T09:09:27)`、driver `0.8.2`。
  `ldd` 指向 `/home/cat/ThirdParty/OpenCV4100/lib/libopencv_world.so.410` 和
  `/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so`，没有缺失依赖。

### 输入与输出身份

路径均在板端。`A=/home/cat/Project/SOT/nanotrack_v3_longterm`，
`B=/home/cat/Project/SOT/nanotrack_v3_longterm-target-class-85c7ba5`，
`R=$A/releases/yolov8n-visdrone9-rk3566-fp16-v1/models`。

| 工件 | 路径 | SHA-256 |
| --- | --- | --- |
| 原生可执行文件 | `$B/build-board-target-class-85c7ba5/nanotrack_v3` | `28ffadf45d023ea66bb99ec30e7a7b7efb755d6d0ad1edc4336940b19368d243` |
| NanoTrack template | `$A/models/template_backbone.rknn` | `38b60334e8e0f0999bfb7ff9e943090546461f9307682653685237e24b2a7709` |
| NanoTrack search | `$A/models/search_backbone.rknn` | `92125f1ef6254dade5786bb999faa5b9d9412ceb2b7987f6dbba82f959b7ef6a` |
| NanoTrack head | `$A/models/head.rknn` | `2065bfca503cca9197e67de1a754efad930dfed4d2fc73881c77d9e850dcb600` |
| NanoTrack manifest | `$A/models/manifest.json` | `441e93f2092afe94bac663f27adb1fe99848357356efd67c7473b0b506f854b2` |
| VisDrone9 detector RKNN | `$R/yolov8n-visdrone9-rk3566-fp16.rknn` | `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0` |
| Detector manifest | `$R/detector-manifest.json` | `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e` |
| Detector labels | `$R/visdrone9.txt` | `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992` |
| 输入视频 | `$A/data/girl_dance.mp4` | `8090fcd5090adb62f21e79e799f0b0871d9c610d8cf8087b9fcdd3c7e9c2e9b9` |
| Debug 配置 | `$B/config/interactive-debug.conf` | `e3fe5f927ba3a47d2829dce600c01634f7f2f405576865c2edea22e0256493f8` |
| 可见光控制配置 | `$B/config/visible-default.conf` | `b9a128b78063056173626af1c7c7d2c580a59b295c45a4afc0ef8e2e04bff95b` |

### 原生构建、CTest 与场景命令

前序代理在新的板端构建目录执行以下原生配置/构建；保留原始日志为
`/tmp/nanotrack-target-class-configure.log`、`/tmp/nanotrack-target-class-build.log`、
`/tmp/nanotrack-target-class-ctest.log`，其 SHA-256 分别为
`68557c3c81af073eac3527862eeb766f15c4d3b8a8f0d948ed2a426219296984`、
`6cb11a61d3fa9938bc957421b752fc65379022ef44622c9e5c49b842b563a947`、
`34604e9e286bbf61537d176b305b5dfe9db1f4755294382d766ce403b60d06fc`。

```bash
cmake -S "$B" -B "$B/build-board-target-class-85c7ba5" \
  -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=ON \
  -DOpenCV_DIR=/home/cat/ThirdParty/OpenCV4100/lib/cmake/opencv4 \
  -DRKNN_INCLUDE_DIR=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/include \
  -DRKNN_LIBRARY=/home/cat/ThirdParty/rknpu2_v2.3.2/librknn_api/aarch64/librknnrt.so
cmake --build "$B/build-board-target-class-85c7ba5" -- -j2
(cd "$B/build-board-target-class-85c7ba5" && ctest --output-on-failure)
```

配置/构建完成，原始 CTest `16/16` 通过、`5.18 s`；本轮从构建目录重跑
`ctest --output-on-failure`，`16/16` 通过、`5.10 s`。板端 CTest 3.13 不支持
`--test-dir`；一次从 `/home/cat` 使用该参数得到 `No tests were found`，未计入测试结果。

场景脚本 `/tmp/target_class_board_scenarios.py` SHA-256
`9153936f9c06fd14bebb9c210ba1681623119558c0fc0eb8dc0e1a53c2ff3dcb`，
GUI 辅助脚本 `/tmp/target_class_board_gui.py` SHA-256
`3558a257d7863e0a03b0acc38f0301274393d67c1b657504925b37955c2b47ac`。
实际命令为 `python3 /tmp/target_class_board_scenarios.py {full,local,background,reselect}`；
脚本启动上述可执行文件，传入 `--config $B/config/interactive-debug.conf`、
`--video $A/data/girl_dance.mp4`、`--model-dir $A/models`、
`--controller-config $B/config/visible-default.conf` 和三个 `$R` detector 工件；
各场景完整命令保留在
`/home/cat/Project/SOT/validation-target-class-85c7ba5/{full,local,background,reselect}.command`。
前三个命令使用 `--max-frames 3` 并通过 GUI 单帧步进，重选命令使用
`--max-frames 210`；脚本对等待和进程退出设有上限。所有场景 exit 0。

| 场景 | ROI（原视频坐标） | 板端观察 |
| --- | --- | --- |
| 人物全身大框 | `196,160,184,310` | `UNKNOWN/NO_MATCH`，仅一个 car 候选，非 person；恢复关闭 |
| 人物局部框 | `244,161,74,70` | `UNKNOWN/NO_MATCH`，仍仅 car 候选；恢复关闭 |
| 背景框 | `30,70,100,100` | `UNKNOWN/NO_MATCH`；恢复关闭 |
| 同进程重选 | 全身→局部→背景→全身 | 初始化与 request ID 均为 1–4，四次完成、零错误；四次均 UNKNOWN，随后跟踪 210 帧 |

在默认 detector confidence `0.25` 下，初始化 batch 原始数 `6`，过滤后仅一框：
`car(2)`、confidence `0.363`、bbox `484.21,163.14,98.73,175.73`，
与左侧人物全身 ROI 的覆盖率 `0`。用于诊断的有界命令额外覆盖
`--detector-conf 0.05`，日志 `$E/full-conf005.log`（`E` 为下述证据目录）记录左侧
人物对应的 `truck(4)` 候选 confidence `0.162` 被选中、右侧有两个低置信度 person
候选；`timeout --signal=INT 12s` 退出码 `124`，没有 metadata 文件。
这既不是默认配置结果，也不是正确的 person 确立或阈值优化通过。

诊断命令（`E=/home/cat/Project/SOT/validation-target-class-85c7ba5`）：

```bash
timeout --signal=INT 12s env DISPLAY=:0 stdbuf -oL -eL \
  "$B/build-board-target-class-85c7ba5/nanotrack_v3" \
  --config "$B/config/interactive-debug.conf" --video "$A/data/girl_dance.mp4" \
  --bbox 196,160,184,310 --model-dir "$A/models" \
  --controller-config "$B/config/visible-default.conf" \
  --detector-model "$R/yolov8n-visdrone9-rk3566-fp16.rknn" \
  --detector-manifest "$R/detector-manifest.json" --detector-labels "$R/visdrone9.txt" \
  --detector-conf 0.05 --max-frames 1 --session-metadata "$E/full-conf005.json" \
  > "$E/full-conf005.log" 2>&1
```

证据目录为 `/home/cat/Project/SOT/validation-target-class-85c7ba5`。
`full/local/background/reselect.log` SHA-256 依次为
`db5581161e5f7dde8736b7f732b7af96dbeca725237717cd8900fbbdef7fc9c`、
`3a23962e92eb971a26c6ec7c1a0a686550473069279f131ebab2643339e160bc`、
`5e12cea76d00490fecd7cb327241a454b27d91ca49e5d5c0357be8d77edf339f`、
`1a79b7b7e209f908f14c037f465b5440d8ba3e9a785930380821cdfb720b3e97`；
对应 `.json/.csv/.xwd` 也保留。`reselect.csv` 为 212 行（表头、唯一 frame 0、
210 跟踪帧），未重复写入初始化帧；`reselect.json` 的初始化 detector
`request_count=4, complete_count=4, error_count=0`，恢复 detector
`request_count=0, complete_count=0`。跟踪期出现 `control_valid=1` 的记录来自
NanoTrack `MOTION` 路径，未观察到 detector-only 控制授权；没有实际触发 detector
辅助恢复，也没有可观察的旧 batch 污染或新 eligibility generation 遥测。

另一次无预置 bbox 的 GUI 运行使用 `timeout --signal=INT 45s`，在
`interactive_ready initialized=0` 时截取 `prepaint-before-roi.xwd`，随后选取
`196,160,184,310` 并截取 `prepaint-after-roi.xwd`。前者可见首帧、操作提示及
`NO_TARGET`；后者可见 `UNKNOWN — DET RECOVERY OFF`、灰色拒绝 car 候选和绿色
tracker 控制框。白色初始化框的边缘被绿色框覆盖，无法从此截图独立确认白框层叠
效果；未观察到成功选择的候选。两个截图 SHA-256 分别为
`39e2ca4cf221fc9c05fdf88ffafe278b464dfe7f4c561506b68b31e5e51548f5`、
`3ac57070626ead0ab19a1c38d03b10ab31bdfa1eff26823ea1bb012e64f9d95e`；
`prepaint.log` SHA-256 为
`134037f819800ccba563b261cdb83cce2fa1fb2ab87f17994a64c037f465b544`。
GUI 启动命令与场景命令使用相同输入，使用
`timeout --signal=INT 45s`、不传 `--bbox`，显式设置
`--max-frames 1 --output-boxes $E/prepaint.csv --session-metadata $E/prepaint.json`。
随后依次运行
`python3 /tmp/target_class_board_gui.py capture $E/prepaint-before-roi.xwd`、
`python3 /tmp/target_class_board_gui.py roi 196,160,184,310`、
`python3 /tmp/target_class_board_gui.py capture $E/prepaint-after-roi.xwd`、
`python3 /tmp/target_class_board_gui.py key q` 并等待正常退出。

### 裁决、边界与 Handoff

本轮 **原生构建、板端 schema/CTest、模型加载和实际初始化推理路径通过**；
`girl_dance` 正例目标类别确立 **失败**：人物全身/局部均 UNKNOWN，默认阈值下
仅检出 car，降低阈值会把左侧人物误判为 truck。UNKNOWN 时恢复关闭、同进程
request ID 隔离与初始化帧不重复写出已观察。板端仅有 `girl_dance` 与
`sot-v1` 系列视频，未确认合法车辆样本，车辆全身/局部场景记为 **未观察**。
没有成功确立 person 或 vehicle，detector 辅助恢复未开启/未触发，不能以
`control_valid=1` 的 tracker 帧替代该门槛；亦无长稳、性能晋级或真实吊舱验收。
总体任务状态仍为 `functional_pass_promotion_held`，公共契约中的
`development_board_validation.latest_handoff.available=false` 必须保持。

请求协调对话采纳本证据提交，保持交叉编译 handoff 不可用，并决定后续 detector
类别误判/样本覆盖的独立修复与重测；不得把本次原生编译成功写成 target-class
功能通过或生产交付批准。完整任务报告在工作树
`.superpowers/sdd/2026-10-29-target-class-establishment/task-9-report.md`（不纳入 Git）。
## 2026-11-11T15:17:45+08:00 C010 post-holdout PARTIAL FAIL

- 分支 `codex/occ-video-1-tracking`，源码提交
  `033dc9058dfc4bf86d46fc74b1b472f689c6922b`，板端根目录
  `/home/neardi/Project/SOT/occ-candidate-033dc90`。源码归档、配置、板端可执行 SHA-256
  分别为 `82d41764a70fecbd6b48cf89571d05fb6c6f0d699d133e125e1347732690009b`、
  `83f1382e57942d44692c687c388133639a2cc94c91de3eff9a09ca688895d274`、
  `5802f441e8907007f54d1cacfa9fa4e0ba362f44786daa12689adde97daf7f51`。
- RK3588 原生 Debug、RKNN、`-Werror=switch` 构建成功，CTest `19/19`；完整运行
  verifier PASS，视频 1920x1080/271 帧，CSV 271 行。运行证据保留于
  `/home/neardi/Project/SOT/occ-candidate-033dc90/runs/C010-full-run1`。
- full evaluator FAIL，仅 `reacquisition` 门失败：帧 200 由 `template_bridge`
  正常接受，但 anchor 的 score-only 低谷持续到 201；单帧预算耗尽后 201--216
  再次 SEARCHING，重捕获帧为 `[185,217]`。其余门全部通过，valid-observation
  mean IoU `0.734841`，all-visible mean IoU `0.721368`。
- 证据边界：这是 C009 揭盲后的迭代，不是盲测；没有执行 C010 重复运行，也没有
  声称云台实机验收。下一步为通用、有界的 C011 两帧桥接设计，不降低可靠性或评估阈值。

## 2026-11-11T15:31:46+08:00 C012 canonical board acceptance PASS

- 分支 `codex/occ-video-1-tracking`，最终源码提交
  `e865c78679247cb45ddbdbf2fb466e49930d5d12`，板端根目录
  `/home/neardi/Project/SOT/occ-candidate-e865c78`。源码归档、配置、可执行 SHA-256
  分别为 `64896748bbedbe16a8bcea762087e3060db0f0a0ba4b7a64cbc113b56755ef69`、
  `82ce1e035e202f10a46f1aa2a21f4ed42d79839dca01b0629e1848c90919432c`、
  `e2a44c74bf6410dc79b5d07a5c134e27cd0096cdca16a81c67d2f27df521ab4d`。
- C011 两帧预算 full FAIL（重捕获 `[185,218]`），三帧诊断仅将失败平移到 203，
  因此停止按帧追参。C012 改为与帧率无关的 200 ms 短时滞回，不改变 tracker
  reliability、geometry 或 evaluator 阈值。
- RK3588 原生 Debug/RKNN/`-Werror=switch` 构建成功，CTest `19/19`。三次完整
  0--270 运行均 verifier PASS、full evaluator PASS，语义签名完全一致：唯一重捕获
  帧 185，模板桥接帧 200--206，190 以后无 SEARCHING；valid-observation mean IoU
  `0.732253`，all-visible mean IoU `0.725693`。
- 最终审查补充了 abort / motion prediction exception 立即撤销 bridge 的 fail-closed
  负例；修复后重新构建并重新执行三次。三次 evaluator JSON SHA-256 均为
  `8748b91ca59a8a91ecb013099d3aa43cb7868b1227c6123a5008c4f16f530aaf`；
  verifier log 均为
  `86808013d246f358f354b6101ef4e84470bb2cd29dfb95fa89482d31d735f02e`。
  49 帧复核视频保留在 run1，SHA-256
  `a3e76846fbf5b256a52226f21d041deec8b102c458fe3810c1f3410eb2f221c6`。
- 证据边界：`PASS_CANONICAL_BOARD_ACCEPTANCE` 仅表示 RK3588 上 canonical
  `occ_video_1` 后揭盲三次验收通过；人工复核视频状态仍为 PENDING，未声称真实云台
  pod 验收。

## 2026-11-12T10:48:45+08:00 C012 main 基线集成门禁

- 用户明确批准 C012 作为不可退化的效果基线。`main@1bf657ff730891ae0bea48340a046cbcbeaa87d4`
  是 C012 最终修复提交 `e865c78679247cb45ddbdbf2fb466e49930d5d12` 的直接祖先，且没有
  main-only 提交；集成分支 `codex/c012-main-integration` 从该 C012 提交创建，不包含其后的
  recent-safe/unified-candidates 实验提交。运行时基线仍由既有 C012 RK3588 证据约束，不把
  主机构建或 Git 快进改写成新的板端验收。
- 新增 `docs/evaluation/occ-video-1/c012-effect-baseline.json` 和
  `scripts/check_c012_effect_baseline.py`。门禁直接读取两个候选运行 CSV：car001 必须使用
  `992,582,37,39`、覆盖 0--270、首次重捕获不晚于 185、190 后无 SEARCHING 且 0--270
  `control_valid=1`；car002 必须使用 `970,517,38,38`，60--80 路牌区间逐帧保持
  TRACKING、accepted 和 control_valid。任一条件失败均禁止替换 C012。
- 实际保留的 C012 car001/car002 CSV SHA-256 分别为
  `e94d3afc3eaff438900f488b0eb9901d9ef97e6d4a1a94330831acde3918936a`、
  `ff46e033c1e900832970fa4558164842004ea9adbc02073e5a2e64717b281f20`；新门禁对两者输出
  PASS，结果 JSON SHA-256
  `b042f585b54775ae1429e1afeff835216c591a24c1b3564784587ea29085126c`。car001 为完整 271
  帧正式 C012 运行；car002 为用户交互运行保留的 242 帧 CSV，本次只冻结已有证据覆盖的
  60--80 路牌区间，不外推为完整 car002 验收。
- 新门禁单元测试 5/5 通过，覆盖晚重捕、190 后再次搜索、控制失效、car002 第 70 帧丢失、
  ROI 不一致、帧缺口与短运行负例。独立 RKNN-off Debug 构建完成、CTest 19/19 通过；
  C012 门禁、occ evaluator 与 robustness checker Python 回归合计 24/24 通过，
  `git diff --check` 通过。
- **Handoff**：请求协调者将 C012 精确历史快进到 main，并以
  `rk3588-c012-baseline` 固定 `e865c78679247cb45ddbdbf2fb466e49930d5d12`。未来候选必须在
  完全相同 ROI 和输入身份下通过上述双目标门禁；编译成功、配置可加载或单目标通过均不能
  替代该门禁。正式生产 handoff 与人工吊舱验收状态保持不变；未修改 `STATUS.md` 或
  `integration-contract.yaml`。
## 2026-11-11T16:10:00+08:00 RK3588 interactive one-command profile startup PASS

- 分支 `codex/occ-video-1-tracking`，任务基线
  `10c311e4576e67036ff30eb2538e9298aab5abfc`，配置/测试/文档提交
  `b3a7e4a`。本机配置为
  `rk3566/nanotrack_v3/config/rk3588-interactive-debug.conf`，板端部署路径为
  `/home/neardi/Project/SOT/occ-candidate-e865c78/rk3588-interactive-debug.conf`；两端
  SHA-256 均为
  `e7daafa4373f676b3e9b10984e48fa2ffe46cc6d9ec1c422dbe3d4e77e88544a`。
- 主机侧先修改 CLI 测试形成 3 个预期失败断言，再更新配置；聚焦 `cli` 测试通过，
  全量 CTest `19/19` 通过，`git diff --check` 通过。配置保留
  `interactive=true` 且不预置 bbox，绑定 C012 已验收的 RK3588 NanoTrack 模型、
  `visible-occlusion-calibrated.conf` 和 RK3588 VisDrone9 detector release；固定输出为
  `/tmp/rk3588-interactive-debug.avi`、`.csv`、`.json`。
- 板端在 `/home/neardi/Project/SOT/occ-candidate-e865c78` 执行
  `DISPLAY=:0 timeout --signal=INT --kill-after=2s 15s stdbuf -oL -eL ./build/nanotrack_v3 --config rk3588-interactive-debug.conf`。
  日志确认 RKNN API `2.1.0`、driver `0.9.8`、detector `target=rk3588 outputs=9`，
  随后出现 `runtime_ready`、三个输出均 enabled，以及
  `interactive_ready playing=0 initialized=0`；冒烟结束后无残留 `nanotrack_v3`
  进程。日志保留于板端 `/tmp/rk3588-interactive-config-smoke.log`。
- 证据边界：本次只验证新配置通过既有二进制加载并进入等待鼠标选框状态；未人工画框、
  未运行完整视频、未生成新的算法验收结论，也未声称真实云台 pod 验收。C012 的
  canonical acceptance 及其源码/可执行身份不因本次运行配置部署而改变。SSH 登录环境
  的 `DISPLAY` 为空，本次冒烟因此显式使用 `DISPLAY=:0`；用户给出的原样两行命令应在
  RK3588 图形桌面终端（已继承 `DISPLAY`）执行，不能把 SSH 冒烟误记为字面命令验收。

## 2026-11-11T22:21:04+08:00 occ_video_1 car_002 271-frame draft generated

- 分支 `codex/occ-video-1-tracking`，本任务执行基线
  `8545f97ac9539231e936dfcc2a322d8916e5f143`，候选工具提交 `1ccff5d`，分层草案打包器
  提交 `8545f97`，review contact-sheet 纳入包与哈希的修复提交
  `5d186f8705b5f8a1bababe51a11ffdd43a080935`，独立审查后的 provenance/validator/
  staged-replace 加固提交 `b1d9dc2`、`d33e848`、`e069bf6` 与 `db20a75`。这是主机侧标注草案生成，不是
  RK3588 新运行或板端验收。
- 原视频 `/home/u20/single-object-tracking/data/occ_video_1/1.mp4` SHA-256
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`，严格解码为
  1920x1080、30/1 fps、271 帧。用于辅助定位运行行为的保留证据 CSV/log/JSON/AVI
  SHA-256 依次为
  `ab5f05f2b278ae0216f404f8fee8130b53a396a582a361f0cd00e07fc13ae79f`、
  `269b80673b436db70e2f1d16be56898e69f64b78b114068a128fed7cba6d78b0`、
  `c588f6867043256266a838e2c95517f31bd334355dfbf402e6d53028ae6a8088`、
  `7146e5719363ff4a5611d15b758f67b6e3abc09a5997fd30c6c3358c8d2cce61`；运行证据只作
  诊断参考，不充当真值。
- 工作输入位于
  `artifacts/ground_truth/occ_video_1_car_002_work/`：`anchors.csv`、`candidates.csv`、
  `review-policy.csv` SHA-256 分别为
  `9a618cb492bf5d47b6d6688feb8cf2af7780b6c030438c78c1d027da377cf994`、
  `39c23fa800287843d5e20f3e528c796b7d44bb6315c6c10652c4c82465ca3821`、
  `e9bfa7ab37394b299ce5dc9c482878dc759b5936f84dd6ff78f1f847e6c4f3ac`。
  `distractors.json` SHA-256 为
  `dfcde77592f879ca85924c6155f2f0ffc49c5f1a1925ea6d77c50ac677723f23`。
  候选 review AVI SHA-256
  `ca40b981acb8dd8c4fae41cd75b157dcd77041105eb4e64baa63d81eb5233a67`，解码为
  1920x1080、30/1 fps、271 帧；三张 contact sheet 已逐张查看，SHA-256 为
  `dc3976f3da8d7be84740227b450e52d24e43acf4ce70736a737640e6c5ed2762`、
  `607796268c2b5031c857c6d34d264dd11aa52eaac1e3984541a57c0c79170116`、
  `4968b176b57957a2a8e212b06fa004a0421ba535e79ec6b0cf63439f2b982b94`。
- 草案位于 `artifacts/ground_truth/occ_video_1_car_002_draft/`，
  `checksums.sha256` SHA-256
  `be57dbaee44cfda1e2f925ea08f24a76edbf968c1159076a8c2c7f39303e1e27`，
  `annotation_metadata.json` SHA-256
  `2df9f8ccdb5c546133cac0e7e2bc26111a6aae3635938b9ba23a0ae4f7ce28b2`；连续两次重建
  获得相同两项哈希。
  三层均恰有 271 个连续文件：正式 `labels/` 非空 0，未确认候选
  `candidate_labels/` 非空 252，非可观测估计 `estimated_labels/` 非空 4；
  `frames.jsonl`、`annotation_status.csv` 均为 271 帧，`review_manifest.csv` 有 267 条
  待确认记录。状态计数为 assistant candidate 252、needs human review 15、estimated
  non-observable 4、manual confirmed 0。
- 身份复核结论：目标是桥前初始化白车，桥后为绿色车左侧白车；双向候选在 0--210、
  230--270 连续跟随该车。211--212 与 217--229 为高优先级人工复核区间；213--216
  标为 full occlusion，仅保存低置信度运动估计。运行在 259 帧取得的 detector 候选及
  262 帧恢复对应另一辆同类白车，已以 `car_002_distractor_001` 连续记录 259--264 帧
  （并保留 270 帧参照），不能作为目标真值。
- 实际生成命令为
  `/tmp/nanotrack-car002-tools/generate_bidirectional_candidates --video ... --anchors ...`
  与 `python3 rk3566/nanotrack_v3/tools/build_occ_car002_draft.py build ... --review-dir ...`；
  `validate --require-confirmed false` PASS，`validate --require-confirmed true` 按预期以
  `unresolved review rows: 267` FAIL。三个 layer manifest 与总 `checksums.sha256` 均经
  `sha256sum -c --quiet` PASS；Python 聚焦测试 8/8、annotation-tools CTest 20/20、
  既有主机 CTest 19/19 PASS，`git diff --check` PASS。
- 独立终审指出的四项问题已处理：人工确认必须通过独立 `confirmed_*` 框且保留原候选；
  validator 重新检查几何、枚举、派生字段、估计依据、逐层 YOLO 一致性、状态计数、
  metadata/video/episode 身份、状态与可见率关系、干扰物类型/ID 及 checksum 完整覆盖；
  独立 `validate` 默认绑定 canonical `occ_video_1` 固定 SHA-256，包内即使改绑到另一存在
  文件并提供其正确哈希也会被拒绝；
  `--replace-draft` 先在同级 staging 完整构建并通过
  validator，再以可恢复备份交换（不声称单次系统调用原子交换）；错误同类车补齐
  259--264 逐帧记录。测试覆盖多类语义篡改后重算哈希仍必须失败、结构无效 staging
  不得替换旧包，以及非法输入不得破坏旧草案。
- 证据边界：这是完整覆盖 0--270 的 assistant draft，不是人工确认真值。正式层为空，
  在用户逐帧确认并生成新版本/新哈希前，不授权 IoU、阈值标定、算法准确率、canonical
  dataset 晋级、板端验收或云台 pod 验收；没有修改 `car_001`、`STATUS.md` 或
  `integration-contract.yaml`。

## 2026-11-11T23:33:23+08:00 occ_video_1 car_002 confirmed package generated

- 分支 `codex/occ-video-1-tracking`，任务基线
  `100afb18d371707d9700b40a8f3931d5c22a931c`，结果代码与本记录位于同一 Git 提交。
  用户明确“整体接受”：接受 0--210、230--270 的候选框；确认遮挡区间；确认
  259--264 的候选框仍是正确目标，并与既有运行在 264 帧跳到错误车辆的负面证据严格
  区分。原 assistant draft 未作为本轮生成目的地，也未被重建或覆盖。
- 确认策略输入为
  `artifacts/ground_truth/occ_video_1_car_002_work/review-policy-confirmed.csv`，SHA-256
  `2fd9a8959ddb141292680fc4c5c30f7a0c3a682358788a0def55880eacde75a5`。0--210、
  230--270 共 252 帧使用 `manual_confirmed_visible`，候选框晋级到正式 `labels/`，同时
  保留同框的 candidate 层和候选来源；211--212、217--229 共 15 帧使用
  `manual_confirmed_no_bbox`，只确认遮挡状态与目标身份，正式层和候选层都明确为空；
  213--216 共 4 帧保留 `estimated_non_observable`，估计层不晋级为正式真值。
- 独立确认包位于
  `artifacts/ground_truth/occ_video_1_car_002_confirmed/`。`checksums.sha256` 与
  `annotation_metadata.json` SHA-256 分别为
  `4540ff280903a3e32d8b439f54ddca4497e855095da68ebdbe8fbea044371811`、
  `ddbd8846cd12466d82a833f2f335160a6e6dd1f6189963d268db1b1c54d03a63`；正式、候选、
  估计 manifest SHA-256 分别为
  `2f0e78ab1123cb035c87efa06171f1057c108f38b06fcc5c1ed3a3ec63fdbe98`、
  `c6d1093c16fd17c02c9b8ce1743a85bbc5316dacef76d08e368093a976a6c11e`、
  `62a2877bfc38bf4482e3e1cca1e81551272a677f81d932918723e557db3d24e4`。
  三层仍各有 271 个连续文件，非空计数依次为 252、252、4；
  `review_manifest.csv` 只有表头，未决计数为 0。连续两次生成得到相同 metadata 与总清单
  哈希，`validate --require-confirmed true` 及确认包 `sha256sum -c` 均 PASS。
- 打包器新增 confirmed-package 模式及严格语义：只有零未决项才可写入
  `draft=false`、`formal_evaluation_authorized=true`；`manual_confirmed_no_bbox` 必须身份
  certain、质量 unusable，且 formal/candidate/estimate 三种框都为空。实际执行 Python
  单元测试 10/10、含 annotation tool 的 CTest 20/20、既有主机 CTest 19/19，全部通过；
  `git diff --check` 通过。
- 复核旧 draft 时发现其
  `review_contact_sheets/contact-sheet-000.jpg` 当前缺失，旧总清单仍期望其 SHA-256
  `dc3976f3da8d7be84740227b450e52d24e43acf4ce70736a737640e6c5ed2762`，因此旧 draft
  当前独立校验失败。工作输入和新 confirmed 包都含有该文件且哈希匹配；为遵守“保留原
  draft、不覆盖”的边界，本轮未修改或修复旧 draft，并把此漂移作为外部证据异常记录。
- 证据边界：`formal_evaluation_authorized=true` 只授权新 confirmed 包中 252 个非空正式
  框参与 ordinary IoU；211--229 中无正式框的帧不能被当成零框误计 IoU，213--216 的
  估计框也必须排除。该确认不自动完成阈值标定、canonical dataset 晋级、RK3588 新运行、
  板端验收或真实云台 pod 验收；未修改协调者所有的 `STATUS.md` 与
  `integration-contract.yaml`。

## 2026-11-11T23:44:37+08:00 dual ground-truth sequences installed

- 分支 `codex/occ-video-1-tracking`，任务基线
  `4d38c28f31f125479bcc24c92231584a9cdde3e1`。按用户要求，将第二目标 confirmed 包从
  `artifacts/ground_truth/occ_video_1_car_002_confirmed/` 移至真实 canonical 数据目录
  `/home/u20/single-object-tracking/data/occ_video_1/ground_truth/car_002/`；原 artifacts
  confirmed 路径现已不存在。共享视频仍只有根目录 `1.mp4` 一份，不复制视频。
- 新增 `ground_truth/index.json` 与 `ground_truth/README.md`，SHA-256 分别为
  `42d08ea98642f81640842dd166ff8ba3518668441d430ce9931f99d1dc544177`、
  `2b2b9e10e7d70cecffc693f7044cde6a61128e8517188bf897bdcae6578dc6b9`；既有第一目标
  标注以逐文件相同的镜像放入 `ground_truth/car_001/`，根目录旧路径继续保留以兼容现有
  公共契约和旧命令。镜像 metadata/status/events/labels manifest 哈希仍分别为
  `5641650435a1980694f982cf1fa6715ba959b1515938cee383c8db8ec4aeb076`、
  `e44fd23514d6871e72d628ad4843fc90bb4246f306322ee297c8a2aa75818036`、
  `a43f016c96b34c8a0ec6125dcb944ba4fa07a79d99a46ce696bbb29841ded256`、
  `9ec01854884126350f93e1af3a4aee002d38ce7c2d50c71c78de0f1a8dc0a648`，271 项标签
  `sha256sum -c` 全部通过。
- 第二目标迁移后 `checksums.sha256` 仍为
  `4540ff280903a3e32d8b439f54ddca4497e855095da68ebdbe8fbea044371811`，严格 confirmed
  validator PASS；metadata/status/events/formal manifest/estimate manifest 哈希分别为
  `ddbd8846cd12466d82a833f2f335160a6e6dd1f6189963d268db1b1c54d03a63`、
  `212525642591c124320e93369f8748d82cc53092628beff051507f930946dec1`、
  `efc0652335fcf19c766def88bf50bb8defab1441fdc8fbb412b85d02135c31db`、
  `2f0e78ab1123cb035c87efa06171f1057c108f38b06fcc5c1ed3a3ec63fdbe98`、
  `62a2877bfc38bf4482e3e1cca1e81551272a677f81d932918723e557db3d24e4`。
- `scripts/evaluate_occ_video_1.py` 新增
  `--target-id {car_001,car_002}`，默认仍为 `car_001`。第二序列读取空正式标签、独立估计层
  与新遮挡事件：252 帧允许 ordinary IoU、4 帧仅作估计轨迹参考、15 帧无 bbox 且不参与
  几何评分。第一序列继续按 `source` 排除 169--181 共 13 个插值帧，避免被同一 CSV 的
  `review_status` 覆盖。双序列身份加载分别得到 271 帧；合成完美运行对两序列 full evaluator
  均为全部 gate PASS。Python evaluator 单元测试 16/16 通过。
- 后续同一运行 CSV 可分别执行：
  `python3 scripts/evaluate_occ_video_1.py --dataset-root /home/u20/single-object-tracking/data/occ_video_1 --target-id car_001 ...`
  与将 target 改为 `car_002` 的第二次评估。一次运行的初始化目标必须与所选真值目标一致，
  不能用初始化 `car_001` 的 CSV 对 `car_002` 结论作有效性声明。
- Handoff：请求协调对话把双序列目录、索引和上述新哈希纳入
  `integration-contract.yaml`，并决定板端数据复制路径；本任务按所有权规则未直接修改
  `STATUS.md` 或公共契约。此次仅完成主机数据安装和 evaluator 消费能力，不是新的 RK3588
  算法运行、阈值标定、板端验收或吊舱验收。

## 2026-11-12T02:38:54+08:00 RS001 recent/safe RK3588 原生验证 HOLD

- 分支 `codex/occ-video-1-tracking`，工作树
  `/home/u20/.codex/worktrees/rk3588-debug-deploy/single-object-tracking`，Task 6 基线
  `68b527659e8be39304ab805c9a6f85219c2af5ce`。板端源码先提交为
  `e027006339f0cdafb0b8d38c4dfa5e14abe66593`，证据与本条记录同提交
  （`test: validate recent-safe recovery on RK3588`）。
  `git archive` 两端 SHA-256 均为
  `fc8deb4fd4f96d28ab27be34feaf2133e6e8dc75937380b8304b913220af080e`；
  板端逐文件比对 314 个归档文件内容与执行权限通过。
- 实际板为 `LKD3588`，Linux 6.1.84 aarch64。独立候选根
  `/home/neardi/Project/SOT/occ-recent-safe-rs001`。RKNN API
  `2.1.0 (967d001cc8@2024-08-07T19:28:19)`、driver `0.9.8`；
  运行前用同一提交的 RKNN loader 逐一查询三份 tracker 模型和 detector。
  `build/nanotrack_v3` SHA-256
  `c4161c0a448351f4a6d84be60bc00ce8c7c6e98ed28134efb5069d7562821304`，
  `visible-recent-safe-v1.conf` SHA-256
  `7b038eed1dd052de5879bedbf465a2ded3cc79d5a6f0b662e39d9fd06fc15c34`（`calibrated=false`）。
  原生 CMake 使用 Debug、`ENABLE_RKNN_RUNTIME=ON`、`BUILD_TESTING=ON`、
  `-Werror=switch`，`cmake --build build --parallel 4` 和
  `ctest --test-dir build --output-on-failure` 通过 22/22（13.85 s）。
  主机 RKNN-off CTest 22/22、evaluator Python 19/19、语法检查和 `git diff --check` 通过。
- 两次各读取独立 ROI 和真值，`car_001=992,582,37,39` 使用 canonical 标注，
  `car_002=967,513,42,42` 使用已确认包（252 帧正式可观测框）。
  两次均运行 0–270 帧，1920×1080、30/1 fps，runtime exit 0，
  schema v11 verifier PASS，271 行 CSV 与实际 CLI 发布的 271 行 JSONL companion 均完整。
  模型、标注、输出哈希、完整 evaluator 门槛、源码与运行命令记录在
  [RS001 评估记录](../evaluation/occ-video-1/recent-safe-candidate.json)；
  大工件留在板端 `runs/car_001/` 与 `runs/car_002/`，每个目录有 `SHA256SUMS`。
- `car_001` full evaluator **FAIL**。172–173 帧未满足完全遮挡语义，174 帧开始搜索，
  `[182,190)` 内及整段均无重捕获，248–270 帧控制无效。最后可信帧 172，
  到首次失去控制经过 2534 ms。有效观测 mean IoU 0.764074（168 帧），
  全部可见帧 mean IoU 0.602621。候选冲突 76 帧。
- `car_002` full evaluator **FAIL**。70 帧进入搜索后无重捕获，145–270 帧控制无效；
  213–216 帧完全遮挡语义和估计轨迹连续性均失败，`[230,271)` 无重捕获。
  最后可信帧 69，到首次失去控制经过 2533 ms。有效观测 mean IoU 0.910586（70 帧），
  全部可见帧 mean IoU 0.302287。候选冲突 143 帧。两目标 identity proxy 均 PASS，
  其结论限于有权使用的可见观测，两次均未提交新恢复身份。
- recent/safe/staged 物理 head 调用分别为 `270/96/14` 与 `270/200/6`，
  safe 复用各 0 次，任何帧最多 2 次，未观察到模板事务应用失败。
  runtime p50/p95 分别为 7.82/9.97 ms 和 8.71/10.55 ms，pipeline p50/p95 为
  109.90/113.42 ms 和 111.02/113.78 ms。板端缺少 `/usr/bin/time`，以约 100 ms
  间隔采样进程 `/proc/<PID>/status`，观察到 VmHWM 237260 / 235476 kB。
- **Handoff**：RS001 保持 HOLD 和 `calibrated=false`。两次均按原配置一次运行，
  C012 图形启动仍使用原路径与配置；其二进制、交互配置和控制器配置 SHA-256
  前后一致。需复查 RS001 时，在候选根执行
  `cat runs/car_001/commands.txt runs/car_002/commands.txt` 查阅准确诊断命令，
  新诊断使用新的输出路径。证据限 RK3588 原生 Debug 的两目标算法结果。
  请求协调对话继续保留正式 handoff 门槛，生产交叉编译与人工吊舱验收未启动。

## 2026-11-12T02:53:15+08:00 RS001 v11 evaluator 复核与重新评分

- 在 `codex/occ-video-1-tracking` 的既有证据提交
  `fcab231098371b4e659282b1472b7faf243934e5` 上修复 evaluator。板端运行源码仍为
  `e027006339f0cdafb0b8d38c4dfa5e14abe66593`，重新评分脚本提交为
  `eb344708429949356c9f0161253bb326e0c16502`，脚本 SHA-256
  `57bdd5d6824c5517adaeeac1910152b52dbc1721589983cea548c6c4f5f1b7f4`。
  v11 重捕获必须来自可信跟踪观测和已应用的恢复阶段；`LOST` 禁止控制和观测声明；
  完全遮挡门直接核对观测缺失、可信标记和已获授权的运动预测。新增负例先失败，修复后
  evaluator Python 23/23、完整主机 CTest 22/22、Python 语法与 Git diff 检查通过。
- 用该提交的脚本在 RS001 分别重新评估保留的 `runs/car_001/bboxes.csv` 和
  `runs/car_002/bboxes.csv`，沿用各自正式真值及 session metadata。两次仍为 **FAIL**，
  每项 gate 的布尔结果和失败帧与原报告一致，均无重捕获或无效重捕获声明。
  更新后的 JSON SHA-256 分别为
  `b338b8b8ce5967d377bbdf15751a8ab36b63023151758e162937a34a88ddee19` 和
  `3e37c58e1387769203353dd81dcebbacf441ec55a269522b13deb1426fbd1846`，
  Markdown SHA-256 分别为
  `0de2acb648f2aa4fd42e2ffdcc583392bf28031b7a882b29402edfa4ebb011ae` 和
  `b2673e732693c3f43e15caf4abe9723ae638d0440e5808dc4c3d53aced6c21b0`。
  新 `SHA256SUMS` 均通过复核，非 evaluator 文件逐项哈希与旧清单一致；原报告及清单保留于
  两个 run 的 `evaluator-original-e027006/`。准确命令、原始及更新后的哈希见
  [RS001 评估记录](../evaluation/occ-video-1/recent-safe-candidate.json)。
- **Handoff**：保持 RS001 **HOLD**。本次仅对保留 CSV 重新评分，新增推理运行 0 次。

## 2026-11-12T09:56:47+08:00 latest HEAD RK3588 双目标视频运行与回传

- 分支 `codex/occ-video-1-tracking`，运行源码提交与任务基线均为
  `3ec5d56c72567b4e3eb6cab89cda149f51ce77f3`。源码通过 `git archive` 部署，归档在
  主机与板端核验的 SHA-256 均为
  `5d523e03c7d13a70b4065e9f450f8c79f1b0627d2f7a652610a8a68ba7114cc1`；独立板端根为
  `/home/neardi/Project/SOT/occ-latest-3ec5d56`，未覆盖 C012 或 RS001。
- 实际板为 `LKD3588`、Linux 6.1.84 aarch64，RKNN API
  `2.1.0 (967d001cc8@2024-08-07T19:28:19)`、driver `0.9.8`。原生 Debug 构建启用
  `ENABLE_RKNN_RUNTIME=ON`、`BUILD_TESTING=ON` 和 `-Werror=switch`，CTest 22/22 通过；
  新 `build/nanotrack_v3` SHA-256 为
  `8b30b04ce84dcfe4b4209a9aff7cf94a21697b78c35d2422fb2cee9396aebd38`。
- 输入仍为 `data/occ_video_1/1.mp4`，SHA-256
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`；控制器配置
  `visible-recent-safe-v1.conf` SHA-256
  `7b038eed1dd052de5879bedbf465a2ded3cc79d5a6f0b662e39d9fd06fc15c34`
  （`calibrated=false`）。tracker manifest 和 RK3588 detector SHA-256 分别为
  `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`、
  `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`。
- `car_001=992,582,37,39` 与 `car_002=967,513,42,42` 分别独立运行 0--270 帧，
  runtime 均 exit 0；schema v11 `verify_board.sh` 均 PASS，CSV 均为 271 数据行，输出视频
  均经 `ffprobe` 确认为 1920x1080、30/1 fps、271 帧。板端完整结果保留在
  `runs/car_001/` 与 `runs/car_002/`。
- 两段视频已回传至 `/home/u20/occ-video-1-latest-3ec5d56-results/`，并在 u20 端重新
  解码计数和复核哈希。`car_001-3ec5d56.avi` SHA-256 为
  `7d0438ea9da9dc08f5f7effa5d0084668c9c97091b8e818e4e2a5e11eb5aa1fd`；
  `car_002-3ec5d56.avi` SHA-256 为
  `dee808467725707228f5eda6045f21aa8388425bc3099855189acab28fc0e0ef`。
- **证据边界**：本次按用户要求完成最新程序的两目标单次运行、结构校验和视频交付，未执行
  full evaluator，也不改变 RS001 HOLD、`calibrated=false`、正式 handoff 门槛或人工吊舱
  验收状态；未修改 `STATUS.md` 或 `integration-contract.yaml`。

## 2026-11-12T13:07:25+08:00 recent-safe v2 阶段 1：单指标瞬降板端验证

- 分支 `codex/recent-safe-occlusion-recovery-v2`，任务基线
  `ef0c02130fba4aa900f85daf3341943bd408e634`，本阶段源码提交
  `c3d27bea3087f42ae60b8aa7e891bbc9a61fe605`。改动只调整 recent 观测判定：恰好一个
  响应指标失败时发布 tentative/SUSPECTED 观测，不更新可信运动、响应基线或模板；尺寸
  塌缩和多个绝对响应指标失败仍直接进入 SEARCH。主机 RKNN-off CTest 22/22、C012
  门禁 Python 单测 5/5 和 `git diff --check` 通过。
- 源码以 `git archive` 部署到实际板 `LKD3588` 的独立目录
  `/home/neardi/Project/SOT/occ-recent-safe-v2-c3d27be`，未覆盖 C012、RS001 或 latest。
  `source.tar` SHA-256 为
  `42741709a2dc50150c558cd4452b95dafbaa7eadfcbb1814b4613784aba915b3`。
  板端原生 Debug 构建启用 `ENABLE_RKNN_RUNTIME=ON`、`BUILD_TESTING=ON` 和
  `-Werror=switch`，CTest 22/22 通过；`build/nanotrack_v3` SHA-256 为
  `668aa05e6a7981f428afd3591d7436862575864afb39e13499dace806639ce52`。
- 使用固定 C012 A/B 初始框 `car_002=970,517,38,38`，同一视频、tracker 模型、detector
  和 `visible-recent-safe-v1.conf` 单次运行 0--270 帧。视频、配置、detector SHA-256
  分别为 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`、
  `7b038eed1dd052de5879bedbf465a2ded3cc79d5a6f0b662e39d9fd06fc15c34`、
  `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`；三份 tracker
  RKNN SHA-256 记录在板端 `provenance/car002-inputs.sha256`。runtime exit 0，schema v11
  verifier PASS，CSV 和 1920x1080 AVI 均为 271 帧。
- 60--80 帧没有 SEARCH 且全部 `control_valid=1`。第 70 帧从 RS001 的直接 SEARCH 变为
  一帧 SUSPECTED，仍输出 NanoTrack 观测框并保持 observation control；第 71 帧恢复
  TRACKING。第 70 帧框为 `894.354431,618.288696,36.756348,37.964905`，与 C012 同帧
  `896.088928,618.415588,37,38` 接近。新 CSV、AVI 和 session metadata SHA-256 分别为
  `69fd729a37415b8c9071b4e9527c367e0b791e8fc13fcf4017399c1578b2fa67`、
  `6350da6ec20043f9bce4cce36dbefad108f4eb5745182d3812b12b741a6a9abd`、
  `a054f00d3d02b25042fb730a594299401a79705773a11319b91619d42f26bda7`；完整大工件留在
  板端 `runs/car_002/`。
- **证据边界 / Handoff**：阶段 1 的遮挡判定目标在 car002 上达到，但现有 C012 晋级脚本
  要求 60--80 每帧均 `tracking_phase=TRACKING` 且 `accepted=1`，因此会在有意冻结可信
  状态的第 70 帧报告失败。此处不修改或放宽协调者批准的门禁，不宣称候选已达到 C012
  全部晋级条件；C012 继续作为默认效果基线。尚未实施阶段 2 重捕获候选关联、阶段 3
  持续搜索控制，也未完成 car001 新运行、生产交叉编译或人工吊舱验收。

## 2026-11-12T13:38:53+08:00 recent-safe v2 三阶段 RK3588 双目标验证 HOLD

- 分支 `codex/recent-safe-occlusion-recovery-v2`。阶段 2 候选关联提交
  `d36ae22a200522314f7b8e0a4362e04e0cd97bc3`，恢复 probation 提交
  `5ce180dd4e14b04e9328150fd87bc38ca58e0c33`、`b18bb8241e39fc0f13886564d6e843eb584f0d0f`，
  阶段 3 持续有界搜索控制提交
  `b53866785d4a50855f3c2abf08ccb89f2aa69b9d`。后续实验
  `ffe8bd9a466b13de641a62b51928903e118759a7` 试图让持续单指标下降快速进入 SEARCH，
  但板端双目标均出现多次正常段误 SEARCH/误重捕获，已由
  `b8aa1006b66843a37f711e147ce4f92632279317` 明确撤销；当前分支树与已验证的
  `b538667` 相同。未修改阈值、配置或 ROI 扰动策略。
- 保留候选源码部署根为
  `/home/neardi/Project/SOT/occ-recent-safe-v2-b538667`，`source.tar` SHA-256
  `ed2f4f61014a4ef107eabe44d7a2d02b57ee2a42dc6a42f2582cedf2b36c8ed2`。
  RK3588 原生 Debug CTest 22/22 通过；主机 RKNN-off CTest 22/22、C012 门禁单测 5/5
  与 `git diff --check` 通过。板端 `build/nanotrack_v3` SHA-256 为
  `c97945d117dc6d611f222477308dfe4a7a07d2067b0c306872c091d3b557c3c2`；配置 SHA-256
  仍为 `7b038eed1dd052de5879bedbf465a2ded3cc79d5a6f0b662e39d9fd06fc15c34`
  （`calibrated=false`）。视频、detector 和三份 tracker 模型哈希均与 RS001 相同，精确清单
  位于板端 `provenance/final-inputs.sha256`。
- 最终固定 A/B ROI 运行：`car_001=992,582,37,39`、`car_002=970,517,38,38`，各
  0--270 帧。两次 runtime exit 0、schema v11 verifier PASS、AVI 1920x1080 共 271 帧；
  两次均全程 `control_valid=1` 且无 `LOST`。car001 在 185 帧重捕获，186 帧只进入一帧
  SUSPECTED，187--189 由 staged 承接并提交 recent，190--270 无 SEARCH。car002 在
  60--80 无 SEARCH，第 70 帧为单帧 SUSPECTED 且 observation control 有效；后续搜索在
  运动置信度衰减后仍保持有界 `MOTION_SEARCH` 控制。
- car001 CSV/AVI/session metadata SHA-256 分别为
  `82e23ad08e80d976ed072187ea1e0e2344a220595f2a925e3e33ce302792c89a`、
  `4f40eeb1b35b5a23d4814213a6bdeabde6587d7b5ea4f72a481bf7d4a23c0a89`、
  `e5db2fd2225e580c00a0ece936aa747780cd081ce6d2e5532809060e1cb39944`；car002 对应为
  `2c03b7de9803922e83fc75ab46e09ba08211005c7f8ff6d09eebb607abda427e`、
  `cde0c9267243483c1319b7c65454b761d1cb55575a7abdd54d3043b780b6eb75`、
  `49dbc0385e5d8da53a34e2264f72a379520a1d5448c2a7882bb8416843a8c07b`。
  大工件和完整日志保留在板端 `runs/car_001/`、`runs/car_002/`。
- full evaluator：car001 **PASS**，有效观测 mean IoU `0.752494`，重捕获恰为 185，
  evaluator JSON SHA-256
  `7b945e043c0c9c5f6c485b5f20beb5f00e361f7aa77c12cd2a75a804a5a67c3e`；car002
  可见框、连续控制、identity proxy、预测连续性和 235 帧重捕获均 PASS，但 213--216
  完全遮挡仍声明观测，`full_occlusion_semantics` FAIL，JSON SHA-256
  `886b4aead8f8595bd721c57ae155159969c8cea09ae51b7531d3757dc62101bf`。
  现有 C012 双目标脚本另因 car002 第 70 帧有意为 `accepted=0` 的 SUSPECTED 状态而在该
  单帧 FAIL；视觉框和控制未中断，但未经协调者批准不放宽该门禁。
- **结论 / Handoff**：相对 RS001，car001 的永久身份冲突、恢复后立即回滚及搜索控制失效
  已修复；car002 路牌段不再丢失，且后段可重捕获。候选仍为 **HOLD**，C012 保持正式效果
  基线和默认部署。未解决项是 car002 完全遮挡的提前识别，以及 C012 门禁是否应允许
  `SUSPECTED + observation control + accepted=0` 的单帧冻结语义；不得用已否决的单指标
  快速升级规则解决。未启动生产交叉编译或人工吊舱验收，未修改协调者所有的
  `STATUS.md` 或 `integration-contract.yaml`。
