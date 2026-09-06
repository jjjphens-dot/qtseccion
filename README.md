# 外卖订单管理系统

C++17 / Qt 6.11.2 Widgets / CMake。当前版本 **0.3.0 W02 规则版（AI 辅助）**，不是完整业务系统，也不是课程纯手写版本。

## 当前可运行范围

- 统一的实体、UUID ID、整数分金额、七个订单状态和 Result<T>/Result<void>。
- AppContext 组合根；Repository → DataStore → Session → Service 的明确生命周期。
- DataStore 候选快照事务，只有 Repository 保存成功后才替换内存并发出信号。
- JsonRepository 使用 QFile/QSaveFile，仅支持合法空库读写。非空数据、损坏数据和待恢复备份均明确报错，不静默初始化或覆盖。
- AppPaths、数据目录 QLockFile、NeedsAdminBootstrap 启动状态；不创建初始管理员或内置账号。
- 四角色架构预览页面、只读 OrderTableModel、筛选排序 Proxy、金额和状态 Delegate。导航不执行登录，不包含演示业务数据。
- Auth/Catalog/Order/Query/Admin/Statistics 接口与拒绝未授权调用的基础入口。未实现命令返回 NotImplemented，不能用返回成功的占位逻辑代替业务。
- 集中 Validation 覆盖账号、密码、Unicode 文本、价格、数量、UUID 等边界；OrderPolicy 可按历史重放校验金额、引用、角色、七状态、支付组合、关键时间、取消/退款与骑手收入。
- 应用级 Palette 和完整 QSS 明确指定文字、背景、表头、输入、选中、禁用、菜单和状态栏颜色，避免系统深色主题造成白字白底。

## 尚未实现

登录/PBKDF2/bootstrap、商家建店、菜品 CRUD、购物车命令、订单状态变更命令、角色授权查询与详细 DTO、统计、全实体 JSON 编解码及校验接入、备份恢复体验和完整业务测试。报告不在本轮范围。

实现顺序和关键约束见 [agent.md](agent.md)，详细规划见 [CODING_PLAN.md](CODING_PLAN.md)。计划描述最终目标，不代表所有模块完成。

## 构建与运行

在 `TakeoutOrderManagementSystem` 目录执行，或在 Qt Creator 打开该目录的 CMakeLists.txt，选择 Desktop Qt 6.11.2 MinGW 64-bit Kit。

```powershell
$env:PATH = 'C:/Qt/6.11.2/mingw_64/bin;C:/Qt/Tools/mingw1310_64/bin;' + $env:PATH
& 'C:/Qt/Tools/CMake_64/bin/cmake.exe' -S . -B build/architecture-debug -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/mingw_64 -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/mingw1310_64/bin/mingw32-make.exe -DBUILD_TESTING=ON
& 'C:/Qt/Tools/CMake_64/bin/cmake.exe' --build build/architecture-debug --parallel 4
& 'C:/Qt/Tools/CMake_64/bin/ctest.exe' --test-dir build/architecture-debug --output-on-failure
& './build/architecture-debug/TakeoutOrderManagementSystem.exe'
```

每个构建命令检查退出码，失败立即停止。Release 将构建目录换为 `build/architecture-release`，并使用 `-DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF`。目前验证的是 Windows/Qt 6.11.2/MinGW 13.1，未声称其他 Kit 已验证。

默认数据目录由 QStandardPaths::AppDataLocation 解析（组织名 QtTraining，应用名 TakeoutOrderManagementSystem），状态栏显示实际路径。`--data-dir <目录>` 可指定独立目录。`--smoke-test` 自动使用临时目录，短暂打开架构窗口并退出，适合冒烟测试。普通启动不会写业务文件或创建账号，只持有目录锁。

## 目录

- `core/`：实体、枚举、请求与 Result。
- `data/`：持久化接口、空库 JSON 实现、DataStore 和路径。
- `services/`：Session、权限入口与分阶段业务接口。
- `models/`、`delegates/`：Qt MVD。
- `app/`：依赖组装；`mainwindow.*`：窗口和角色模块导航。
- `tests/`：架构契约测试及窗口冒烟。
- `resources/`：可提交的 .qrc 和样式。

## 仓库

开发仓库：https://github.com/jjjphens-dot/qtseccion 。原东大 GitLab remote 保留，课程提交要求仍需按教师要求执行。本次架构提交不包含工作区中已有的 Sports2026 删除操作；Git 历史可能仍含旧课程示例，当前构建入口只有 TakeoutOrderManagementSystem。

源码仓库不提交 build、EXE/DLL、账号数据、环境配置或报告。可运行包通过匹配 Kit 的 windeployqt 生成；架构版不能用于处理真实订单。
