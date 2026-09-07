# 外卖订单管理系统

C++17 / Qt 6.11.2 Widgets / CMake。当前版本 **0.7.0 W06 订单闭环版（AI 辅助）**，不是完整业务系统，也不是课程纯手写版本。

## 当前可运行范围

- 统一的实体、UUID ID、整数分金额、七个订单状态和 Result<T>/Result<void>。
- AppContext 组合根；Repository → DataStore → Session → Service 的明确生命周期。
- DataStore 候选快照事务，只有 Repository 保存成功后才替换内存并发出信号。
- JsonRepository 完整读写五类业务集合，使用严格 schema、UTC 时间和稳定英文枚举；加载与保存均执行全局不变量校验。
- QSaveFile 原子替换主文件；第二次及以后保存先验证主文件，并把上一有效版本原子写入 `.bak`。主文件缺失或损坏但备份有效时报告 `RecoveryAvailable`，不自动覆盖。
- AppPaths、数据目录 QLockFile、NeedsAdminBootstrap 启动状态；不创建初始管理员或内置账号。
- 首次启动通过专用入口原子创建管理员；普通入口只注册用户、商家和骑手，管理员不能自助注册。
- PBKDF2-HMAC-SHA256 使用每账号随机盐和 600,000 次迭代；密码原文不持久化。登录同时核对账号、密码、角色和删除状态，注销立即清除 Session。
- 商家账号与唯一店铺由 CatalogService 在一个候选快照中保存，任一步失败均不产生半成品账号。
- LoginDialog、RegisterDialog 和四角色真实路由已接入。未登录时隐藏角色业务区，登录后定位存储账号对应角色。
- W05 已接入资料、店铺营业状态和菜品 CRUD；同店有效菜品名称规范化后唯一，删除采用逻辑删除。
- Customer 页面展示营业店铺与在售菜品，购物车按“一用户一车、一车一店”提交并落盘；Merchant 页面可维护店铺和本店菜品。
- ShopModel、DishModel、CartModel 使用稳定 ID 角色并通过 Qt Model Tester 验证；提交失败时不发布候选状态。
- W06 已接入订单创建、模拟支付、取消、商家接单/拒单/出餐、骑手认领/送达、顾客确认收货；每个动作都由 OrderService 校验角色、归属、状态并原子持久化。
- OrderQueryService 只返回角色授权的 OrderRow/OrderDetail；骑手未认领前不返回顾客姓名和完整地址，认领后才开放履约所需地址。
- 订单表使用 OrderTableModel、FilterProxyModel、MoneyDelegate 和 OrderStatusDelegate；顾客、商家、骑手页面分别提供对应动作入口。
- 集中 Validation 覆盖账号、密码、Unicode 文本、价格、数量、UUID 等边界；OrderPolicy 可按历史重放校验金额、引用、角色、七状态、支付组合、关键时间、取消/退款与骑手收入。
- 应用级 Palette 和完整 QSS 明确指定文字、背景、表头、输入、选中、禁用、菜单和状态栏颜色，避免系统深色主题造成白字白底。

## 尚未实现

管理员管理、统计、备份恢复 UI、完整异常注入和性能测试。报告不在本轮范围。

实现顺序和关键约束见 [agent.md](agent.md)，详细规划见 [CODING_PLAN.md](CODING_PLAN.md)。计划描述最终目标，不代表所有模块完成。

## 构建与运行

在 `TakeoutOrderManagementSystem` 目录执行，或在 Qt Creator 打开该目录的 CMakeLists.txt，选择 Desktop Qt 6.11.2 MinGW 64-bit Kit。

```powershell
$env:PATH = 'C:/Qt/6.11.2/mingw_64/bin;C:/Qt/Tools/mingw1310_64/bin;' + $env:PATH
& 'C:/Qt/Tools/CMake_64/bin/cmake.exe' -S . -B build-w05-debug -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/mingw_64 -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/mingw1310_64/bin/mingw32-make.exe -DBUILD_TESTING=ON
& 'C:/Qt/Tools/CMake_64/bin/cmake.exe' --build build-w05-debug --parallel 4
& 'C:/Qt/Tools/CMake_64/bin/ctest.exe' --test-dir build-w05-debug --output-on-failure
& './build-w05-debug/TakeoutOrderManagementSystem.exe'
```

每个构建命令检查退出码，失败立即停止。Release 将构建目录换为 `build-w06-release`，使用 `-DCMAKE_BUILD_TYPE=Release`；W06 验证在 Debug 和 Release 各运行 7 个 CTest 用例。当前验证的是 Windows/Qt 6.11.2/MinGW 13.1，未声称其他 Kit 已验证。

默认数据目录由 QStandardPaths::AppDataLocation 解析（组织名 QtTraining，应用名 TakeoutOrderManagementSystem），状态栏显示实际路径。`--data-dir <目录>` 可指定独立目录。`--smoke-test` 自动使用临时目录，短暂打开架构窗口并退出，适合冒烟测试。普通启动不会写业务文件或创建账号，只持有目录锁。

## 目录

- `core/`：实体、枚举、请求与 Result。
- `data/`：严格 JSON 编解码、原子存储与备份、DataStore 和路径。
- `services/`：Session、权限入口与分阶段业务接口。
- `dialogs/`：登录、首次管理员初始化及三类普通注册表单。
- `models/`、`delegates/`：Qt MVD。
- `app/`：依赖组装；`mainwindow.*`：窗口和角色模块导航。
- `tests/`：架构、规则、持久化、认证、目录购物车、订单闭环契约测试及窗口冒烟。
- `resources/`：可提交的 .qrc 和样式。

## 仓库

开发仓库：https://github.com/jjjphens-dot/qtseccion 。原东大 GitLab remote 保留，课程提交要求仍需按教师要求执行。本次架构提交不包含工作区中已有的 Sports2026 删除操作；Git 历史可能仍含旧课程示例，当前构建入口只有 TakeoutOrderManagementSystem。

源码仓库不提交 build、EXE/DLL、账号数据、环境配置或报告。可运行包通过匹配 Kit 的 windeployqt 生成；架构版不能用于处理真实订单。
