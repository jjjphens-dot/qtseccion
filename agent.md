# 后续开发上下文

## 本轮授权与版本性质

用户先要求依据 Coding Plan 实现程序架构；本轮又要求修复白字白底并继续按计划开发。当前 0.3.0 是 AI 辅助 W02 规则版；不是完整 P0，也不能标记为学生纯手写版本。报告仍不处理。学生手写版、学号 D01 仍需本人核实；本轮只向 GitHub 新分支推送，不操作 GitLab。

阅读顺序：本文件 → README.md → CODING_PLAN.md → 当前源码。仓库内计划为工作区根目录计划的同步副本；开始下轮修改时核对是否出现更新，避免不同副本各自演进。计划中的工作包完成条件不能当作当前实现事实。

## 已落地架构

- 固定 C++17、Qt 6.11.2 Widgets、CMake；Qt5 模板分支已去除。
- `takeout_core`（Core/Network）、`takeout_models`（Core/Gui）、`takeout_ui`（Widgets）、主 EXE、可选 Qt Test。核心库不得依赖 QWidget。
- `AppContext` 组装 Repository、DataStore、SessionContext 与所有 Service，拥有单数据目录 QLockFile；依赖按声明次序构造、反向销毁。
- `core/entities.h` 是值对象模型，使用完整 UUID 字符串、qint64 分、订单快照、可空 claimedAt 等字段；保留完整7状态。`validation.*` 集中实现输入边界，`orderpolicy.*` 实现状态迁移许可、金额/关系/唯一性以及按 history 重放的确定性快照校验。
- `Result<T>` 使用值/错误二择一，`Result<void>` 支持无值结果，均 nodiscard；失败含 code/message/field。只有先检查 ok 才能读取 value/error。
- DataStore 不暴露可修改集合；snapshot() 返回值副本。commitCandidate 仅通过 ServiceBase 的保护入口访问；先调用 Repository 保存，成功才替换快照、递增 revision 并发 committed。拒绝过期 revision；GUI 线程执行提交与 Model 刷新。
- JsonRepository **只支持 schemaVersion=1 的合法空结构**，100 MiB 上限。非空业务集合返回 NotImplemented；坏文件/未知字段拒绝；主文件缺失但有备份不当新库。save 先检查原文件可读且受支持，再用 QSaveFile 原子替换，direct-write fallback 关闭。
- 当前空库启动不保存数据、不创建管理员。NeedsAdminBootstrap 是数据层启动结果，不是认证。Ready 识别契约通过注入测试 Repository 验证；生产 JSON 非空读取仍未完成。
- AuthService 是唯一可建立/清除 Session 的类；当前 login/bootstrap 未实现，绝不建立伪登录。普通注册入口拒绝 Admin/Merchant，商家必须走 CatalogService 原子建店入口。
- ServiceBase 校验初始化、Session、存储账号存在/未删除/角色匹配；所有后续业务入口需再校验实体归属与状态。当前命令是显式 Forbidden/NotImplemented，不产生业务数据。
- OrderTableModel 只接收 OrderRow DTO 投影；OrderQueryService 尚未实现，当前页面永远为空。Proxy 用真实金额/时间排序、完整 IdRole 定位，半开日期范围筛选。MoneyDelegate/OrderStatusDelegate 已安装在表格。
- 四角色导航仅是架构预览，不改变 Session。切 Session/数据提交时先清空旧投影再查询；目前无注册、支付、配送按钮或假数据。
- UI 主题必须经 `applyApplicationTheme()` 同时设置 QPalette 和 `style.qss`。QSS 已显式覆盖深色正文、白底输入/表格、深蓝选中白字以及禁用状态，不得依赖系统主题的隐含前景色。

## 不可改变的规则

1. Service 是权限边界；UI 隐藏按钮、切换预览页不能赋予权限。未完成的操作禁止假成功。
2. 所有持久化变更先构造候选、验证、保存成功再发布内存；不能每次 textChanged/valueChanged 都保存。纯过滤、选择不提交事务。
3. 7个 OrderStatus 不增加取餐状态：claim 写 riderId/riderNameSnapshot/claimedAt；送达仍 Delivering；只有本人用户确认才 Completed。
4. Cancelled 只允许 Unpaid/Refunded；PendingPayment 只允许 Unpaid；其余5状态 Paid。完整时间/history规则按计划第5节，后续实现不能只校验本轮已有的组合函数。
5. 价格与总额是整数分，历史快照不受当前商品价格、地址或店铺名称变化影响；逻辑删除保留引用。
6. 数据库/Web服务器/真实支付/联网业务不在计划中。QSaveFile、QLockFile、单JSON不替换。
7. 本轮只是架构搭建，不宣称 W02/W03/P0 已验收。Qt Test 是架构测试，不能代替计划 T01–T23 的业务验收。

## 下一轮实施顺序

| 工作包 | 应继续做的内容 |
|---|---|
| W02 | 已完成独立 Validation、OrderPolicy、规则测试和 UI 对比度修复；后续发现规则缺口时在不改变7状态与既定不变量的前提下补测试 |
| W03 | 替换空库限定实现，完成全实体JSON编码/解析、候选全局校验、备份底层、错误分类/恢复状态；完善10,000单与100 MiB边界；数据层不得处理密码 |
| W04 | Auth/PBKDF2/bootstrap及真实登录UI；同一CatalogService的createMerchantWithShop最小子集，同事务账号+店铺，不能先单独落账号 |
| W05 | 扩展CatalogService资料/营业/菜品CRUD；同一OrderService实现updateCart，不新增CartService；Shop/Dish/Cart Models与真实顾客/商家页面 |
| W06 | 扩展同一OrderService全部订单动作；OrderQueryService授权过滤与详情DTO、角色范围实际数据展示、骑手页与订单详情 |
| W07 | 管理员账号逻辑删除、统计、Core闭环与Release验收 |
| W08 | Core通过后完善恢复交互、第二实例UX、损坏文件/commit错误注入、密码工作线程/比较细节、性能 |

`ServiceBase::pending` 是本轮明确失败的临时接口支撑。将具体方法实现时，应替换该方法内的 pending 调用，不要把 pending 改成 success：若统一改成功会同时破坏多个入口及 Result::error() 前置条件。所有临时接口需按计划返回真实业务结果。

`OrderTableModel::replaceProjection` 仅供已授权DTO投影和测试fixture使用，生产调用数据来源必须经过Query Service；不能把Store全集转换成DTO后交给Proxy“做权限”。订单详情DTO刻意留到W06，不能直接把Order（含完整地址等）公开给全部角色。

## 构建和验证

主工程目录 `TakeoutOrderManagementSystem`；Windows Kit 为 `C:/Qt/6.11.2/mingw_64` + `C:/Qt/Tools/mingw1310_64`，CMake位于 `C:/Qt/Tools/CMake_64/bin`。具体命令见README。测试时 Qt/MinGW bin 必须在当前进程 PATH。

历史架构目录可复用，当前验收目录为 `build/w02-debug` 和 `build/w02-release`。CTest 包含 `architecture`、`rules`、`shell_smoke`；测试用 QTemporaryDir，不读写正常用户数据。`--smoke-test` 用独立临时目录启动并退出。测试可选环境变量 TAKEOUT_SCREENSHOT 输出窗口截图，仅用于QA；离屏平台缺少中文字形时只核验布局/颜色，Windows 平台再核验实际中文。

架构测试覆盖 Result、全部21种支付组合、保存失败/过期revision、bootstrap职责分离、未认证Service拒绝、空库JSON保护、Model协议、数值排序/ID、金额极值、导航不认证、主题 Palette/QSS 和第二实例互斥。规则测试覆盖输入边界、Unicode/UUID、金额、七状态迁移、完成与两类取消路径、时间/history篡改、历史快照不受当前名称/上架状态影响、引用和唯一性。

## Git 与发布

开发上传目标 GitHub `jjjphens-dot/qtseccion`；保留原 `origin`（东大GitLab），GitHub使用单独 `github` remote。0.3.0 从 GitHub main 的 `59e66d1` 创建 `feat/w02-validation-ui-contrast`；检验通过后推送该分支，不合并 main、不操作 GitLab。不得强推、重置或重建历史。

本轮开始已有 Sports2026 大量工作区删除及外卖模板 intent-to-add。仅暂存本轮架构相关路径，**不暂存或恢复 Sports2026 删除**。旧示例仍可能出现在提交树和历史中，当前唯一构建入口为外卖工程。

网络曾直连失败，系统配置代理是 `127.0.0.1:7897`。需要时仅在单次 git 命令使用 `-c http.proxy=http://127.0.0.1:7897`；不改全局代理，不存账号/Token。本文件不含凭据。

`.qrc` 必须提交；build、发行运行库、环境文件、真实账号JSON不可入库。没有生成课程报告。后续学生手写版单独保留真实作者记录，不得将本轮架构打为 handwritten 标签。
