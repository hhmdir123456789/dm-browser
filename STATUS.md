# 画板状态

## 当前卡点
双击画布 → 浮动放大：浮层出来了，菜单/工具栏过去了，
但画布区域是白的，没铺满。

## 已确认
- 文件菜单 ✅ 新建/打开/保存PNG/JPG/另存为/导出SVG
- 工具栏 ✅ 选择/撤销/重做/复制/粘贴/删除/清空
- 状态栏 ✅ 缩放/坐标/尺寸/对象数
- 浮动层 ✅ 菜单+工具栏过去，❌ 画布没铺满

## 下一次只做这一件事
改 sidebar.html 里 .float-panel .float-body 的 CSS，
让它变成 display:block + 子 .db-canvas-wrap 撑满 100%。

## 关键文件
- src/ui/sidebar.html（前端全部逻辑）
- src/ui/BrowserWindow.cpp（expandSidebar 消息）
- build/Release/dm_ui.exe（跑这个）

## 编译
Copy-Item src\ui\sidebar.html build\Release\sidebar.html -Force
Stop-Process -Name dm_ui -Force -ErrorAction SilentlyContinue
cd build\Release; .\dm_ui.exe