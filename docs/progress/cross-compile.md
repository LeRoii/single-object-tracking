# 生产交叉编译任务进度

- 任务状态：`needs_owner_refresh`
- 最近核对：`2026-10-22T21:18:30+08:00`
- 核对基线：`main@352018bd588a999d03f2685dc7b71266f396b39d`
- 责任边界：确认能够取得协调对话指定的最新开发板验证结果，并使用现有交叉编译流程成功生成生产产物。

## 前置条件

开发板验证在本任务之前完成。开发板具备完整原生编译环境；交互式 Debug 版本用于板端开发、调试和测试。只有开发板任务形成明确 handoff 后，交叉编译任务才消费该结果。

“最新结果”不以文件时间戳判断，而是由协调对话锁定的以下身份共同确定：

- 已通过相应开发板门槛的源代码 commit；
- 对应模型、manifest 和 labels 路径及 SHA-256；
- 必需配置和接口版本；
- 开发板验证记录路径及裁决。

## 本任务只需确认

1. 能取得上述精确 commit 和工件，而不是旧工作树、旧板端目录或未提交副本。
2. 交叉编译命令针对这份最新结果执行并返回成功。
3. 记录生产产物路径；如流程方便，同时记录产物 SHA-256，便于人工部署时核对。

## 不属于本任务

- 不负责算法、模型或交互式 Debug 功能开发。
- 不负责开发板原生编译、调试或板端验收。
- 不要求重新建立完整工具链/sysroot provenance，只使用现有交叉编译环境完成编译确认。
- 不负责把产物部署到真实吊舱。
- 不负责真实吊舱测试，也不得声明吊舱测试通过。

## 当前待确认

- 协调对话尚未在 `integration-contract.yaml` 中填入可供本任务消费的最新开发板 handoff。
- 负责交叉编译的对话尚未记录能否取得该 handoff 以及实际编译结果。

## 完成条件

- `development_board_validation.latest_handoff.available=true`；
- 本地取得的 commit 和工件哈希与 handoff 完全一致；
- 对该结果执行交叉编译并观察到成功退出；
- 将命令、结果和生产产物路径写入本文件。

达到以上条件只能表述为“最新板端结果已成功交叉编译”，不能表述为“已部署”或“吊舱测试通过”。

## Next action

等待开发板验证任务和协调对话发布最新 handoff，然后由本任务对话核对输入并执行现有交叉编译命令。

## Handoff

交叉编译完成后，把生产产物路径和可选 SHA-256 交给用户。真实吊舱部署和测试由人完成；人工结果可由协调对话记录，但不是本任务的自动验收项。

## 后续更新模板

```text
时间：<ISO 8601 + timezone>
分支/工作树：<branch> / <absolute path>
最新板端 handoff：<source commit, model/manifest/labels SHA-256>
取得结果：<path or checkout, matched yes/no>
交叉编译命令：<exact command>
编译结果：<exit code and concise observed result>
生产产物：<path, optional SHA-256>
下一步：<hand to human for pod deployment/test>
```
