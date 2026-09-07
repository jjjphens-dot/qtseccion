# 后续开发上下文

## 本轮授权与版本性质

用户先要求依据 Coding Plan 实现程序架构，随后确认 W02、W04 人工检验通过并授权继续开发。当前 0.6.0 是 AI 辅助 W05 目录购物车版；不是完整 P0，也不能标记为学生纯手写版本。报告仍不处理。学生手写版、学号 D01 仍需本人核实；只向 GitHub 功能分支推送，不操作 GitLab。

阅读顺序：本文件 → README.md → CODING_PLAN.md → 当前源码。仓库内计划为工作区根目录计划的同步副本；开始下轮修改时核对是否出现更新，避免不同副本各自演进。计划中的工作包完成条件不能当作当前实现事实。

## 已落地架构

- 固定 C++17、Qt 6.11.2 Widgets、CMake；Qt5 模板分支已去除。
- `takeout_core`（Core/Network）、`takeout_models`（Core/Gui）、`takeout_ui`（Widgets）、主 EXE、可选 Qt Test。核心库不得依赖 QWidget。
- `AppContext` 组装 Repository、DataStore、SessionContext 与所有 Service，拥有单数据目录 QLockFile；依赖按声明次序构造、反向销毁。
- `core/entities.h` 是值对象模型，使用完整 UUID 字符串、qint64 分、订单快照、可空 claimedAt 等字段；保留完整7状态。`validation.*` 集中实现输入边界，`orderpolicy.*` 实现状态迁移许可、金额/关系/唯一性以及按 history 重放的确定性快照校验。
- `Result<T>` 使用值/错误二择一，`Result<void>` 支持无值结果，均 nodiscard；失败含 code/message/field。只有先检查 ok 才能读取 value/error。
- DataStore 不暴露可修改集合；snapshot() 返回值副本。commitCandidate 仅通过 ServiceBase 的保护入口访问；先调用 Repository 保存，成功才替换快照、递增 revision 并发 committed。拒绝过期 revision；GUI 线程执行提交与 Model 刷新。
- `JsonCodec` 完整覆盖 Account/Shop/Dish/Cart/Order/history 及所有可空字段。schemaVersion=1 采用严格字段集、稳定英文枚举、标准 Base64 和规范 UTC ISO 8601 毫秒时间；未知/缺失字段、错误类型、非精确整数、非规范时间均整库拒绝。
- JsonRepository 限制 100 MiB，加载与保存均调用 OrderPolicy 全局校验。QSaveFile 禁用 direct-write fallback；已有主文件保存前必须有效，先将上一快照原子写入 `.bak`，再替换主文件。主文件缺失/损坏而备份有效时返回 RecoveryAvailable，不自动覆盖；`loadBackup()` 只返回通过完整校验的备份供 W08 恢复流程使用。
- 当前空库启动不保存数据、不创建管理员。NeedsAdminBootstrap 是数据层启动结果，不是认证。非空 JSON 已可读取；DataStore 仍要求其至少含一个有效 Admin，否则启动失败。
- AuthService 是唯一可建立/清除 Session 的类。Credentials 使用随机16字节盐、PBKDF2-HMAC-SHA256、600,000次迭代和32字节派生值；当前同步执行，W08再移至工作线程并强化比较细节。bootstrap只允许合法空库且不自动登录；普通注册拒绝Admin/Merchant。CatalogService复用AuthService账号准备能力，一次提交Merchant Account与唯一Shop。
- ServiceBase 校验初始化、Session、存储账号存在/未删除/角色匹配；CatalogService 的资料、店铺、菜品 CRUD 与 OrderService 的购物车命令进一步校验实体归属和状态，均通过候选快照提交，不产生越权数据。
- OrderTableModel 只接收 OrderRow DTO 投影；OrderQueryService 尚未实现，当前页面永远为空。Proxy 用真实金额/时间排序、完整 IdRole 定位，半开日期范围筛选。MoneyDelegate/OrderStatusDelegate 已安装在表格。
- LoginDialog/RegisterDialog已接入真实服务。未登录隐藏角色内容；登录后只按存储账号角色定位页面，注销清空Session。W05 Customer 页面浏览营业店铺/在售菜品并提交购物车，Merchant 页面维护店铺、营业状态和本店菜品；当前仍无支付、配送按钮或假订单。
- W05 的 `ShopModel`、`DishModel`、`CartModel` 提供稳定 `IdRole`/`ShopIdRole` 投影，并由 Qt Model Tester 验证；资料提交成功后同步更新 Session 显示名。
- UI 主题必须经 `applyApplicationTheme()` 同时设置 QPalette 和 `style.qss`。QSS 已显式覆盖深色正文、白底输入/表格、深蓝选中白字以及禁用状态，不得依赖系统主题的隐含前景色。

## 不可改变的规则

1. Service 是权限边界；UI 隐藏按钮、切换预览页不能赋予权限。未完成的操作禁止假成功。
2. 所有持久化变更先构造候选、验证、保存成功再发布内存；不能每次 textChanged/valueChanged 都保存。纯过滤、选择不提交事务。
3. 7个 OrderStatus 不增加取餐状态：claim 写 riderId/riderNameSnapshot/claimedAt；送达仍 Delivering；只有本人用户确认才 Completed。
4. Cancelled 只允许 Unpaid/Refunded；PendingPayment 只允许 Unpaid；其余5状态 Paid。完整时间/history规则按计划第5节，后续实现不能只校验本轮已有的组合函数。
5. 价格与总额是整数分，历史快照不受当前商品价格、地址或店铺名称变化影响；逻辑删除保留引用。
6. 数据库/Web服务器/真实支付/联网业务不在计划中。QSaveFile、QLockFile、单JSON不替换。
7. W02 经用户人工检验通过，W03数据存储与W04认证子集已自动验证；不宣称P0已验收。Qt Test不能代替计划T01–T23的最终业务验收。

## 下一轮实施顺序

| 工作包 | 应继续做的内容 |
|---|---|
| W02 | 已完成独立 Validation、OrderPolicy、规则测试和 UI 对比度修复；后续发现规则缺口时在不改变7状态与既定不变量的前提下补测试 |
| W03 | 已完成全实体 JSON、全局校验、原子主文件、上一有效 `.bak`、RecoveryAvailable 与 100 MiB 超限测试；10,000 单性能样本及恢复 UI 按计划留在 W08 |
| W04 | 已完成Auth/PBKDF2/bootstrap、用户/骑手注册、商家账号+店铺单事务、登录注销表单与四角色Session路由 |
| W05 | 已完成：CatalogService资料/营业/菜品CRUD；同一OrderService实现updateCart；Shop/Dish/Cart Models与真实顾客/商家页面；目录、购物车重启恢复及模型协议测试通过 |
| W06 | 扩展同一OrderService全部订单动作；OrderQueryService授权过滤与详情DTO、角色范围实际数据展示、骑手页与订单详情 |
| W07 | 管理员账号逻辑删除、统计、Core闭环与Release验收 |
| W08 | Core通过后完善恢复交互、第二实例UX、损坏文件/commit错误注入、密码工作线程/比较细节、性能 |

`ServiceBase::pending` 是本轮明确失败的临时接口支撑。将具体方法实现时，应替换该方法内的 pending 调用，不要把 pending 改成 success：若统一改成功会同时破坏多个入口及 Result::error() 前置条件。所有临时接口需按计划返回真实业务结果。

`OrderTableModel::replaceProjection` 仅供已授权DTO投影和测试fixture使用，生产调用数据来源必须经过Query Service；不能把Store全集转换成DTO后交给Proxy“做权限”。订单详情DTO刻意留到W06，不能直接把Order（含完整地址等）公开给全部角色。

## 构建和验证

主工程目录 `TakeoutOrderManagementSystem`；Windows Kit 为 `C:/Qt/6.11.2/mingw_64` + `C:/Qt/Tools/mingw1310_64`，CMake位于 `C:/Qt/Tools/CMake_64/bin`。具体命令见README。测试时 Qt/MinGW bin 必须在当前进程 PATH。

当前 W05 Debug 验收目录为 `build-w05-debug`，CTest 的 `architecture`、`rules`、`persistence`、`auth`、`catalog`、`shell_smoke` 共 6 项已全部通过；测试用 QTemporaryDir，不读写正常用户数据。Release 验收使用独立的 `build-w05-release`，测试关闭，仅验证可执行文件构建。`--smoke-test` 用独立临时目录启动并退出。测试可选环境变量 TAKEOUT_SCREENSHOT 输出窗口截图，仅用于 QA；离屏平台缺少中文字形时只核验布局/颜色，Windows 平台再核验实际中文。

架构测试覆盖 Result、状态矩阵、事务、认证表单角色约束、Model协议、主题及实例互斥。规则测试覆盖输入、UUID、金额、七状态、时间/history、引用和唯一性。持久化测试覆盖全实体往返、备份恢复报告、严格schema及文件上限。认证测试覆盖bootstrap保存失败重试、派生凭据、重复账号、普通入口角色限制、商家+店铺单提交、错误密码/角色/删除状态、四角色Session生命周期及重启登录。目录测试覆盖店铺/菜品 CRUD、同店重名、购物车边界、下架拒绝、重启恢复及三个业务 Model 的稳定 ID。

## Git 与发布

开发上传目标 GitHub `jjjphens-dot/qtseccion`；保留原 `origin`（东大GitLab），GitHub使用单独 `github` remote。功能分支 `feat/w02-validation-ui-contrast` 从 GitHub main 的 `59e66d1` 创建，0.6.0 W05 在该分支接续提交；不合并 main、不操作 GitLab。不得强推、重置或重建历史。

本轮开始已有 Sports2026 大量工作区删除及外卖模板 intent-to-add。仅暂存本轮架构相关路径，**不暂存或恢复 Sports2026 删除**。旧示例仍可能出现在提交树和历史中，当前唯一构建入口为外卖工程。

网络曾直连失败，系统配置代理是 `127.0.0.1:7897`。需要时仅在单次 git 命令使用 `-c http.proxy=http://127.0.0.1:7897`；不改全局代理，不存账号/Token。本文件不含凭据。

`.qrc` 必须提交；build、发行运行库、环境文件、真实账号JSON不可入库。没有生成课程报告。后续学生手写版单独保留真实作者记录，不得将本轮架构打为 handwritten 标签。
