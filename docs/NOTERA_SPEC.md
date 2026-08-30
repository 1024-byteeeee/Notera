# Notera 开发任务说明

项目名称：Notera

项目定位：
Notera 是一款面向乐器演奏者的现代化桌面乐谱阅读器，首发平台为 macOS 与 Windows。

Notera 第一阶段专注于本地乐谱管理、PDF / 图片阅读、演奏模式与基础标注，不开发账号系统、云同步、AI、在线曲库和 MusicXML 编辑功能。

# 1. 技术栈

前端：

- React
- TypeScript
- Vite
- Tailwind CSS

桌面框架：

- Tauri 2

后端 / Native：

- Rust

PDF：

- PDF.js

数据库：

- SQLite

状态管理：

- Zustand

图标：

- Lucide React

项目必须同时支持：

- macOS Apple Silicon
- macOS Intel
- Windows x64

代码必须尽可能保持跨平台，不允许在业务代码中大量散落 macOS / Windows 判断。

# 2. 产品设计原则

Notera 的核心原则：

1. 乐谱本身是阅读界面的核心。
2. UI 尽可能简洁。
3. 阅读过程中减少无关元素。
4. 所有用户数据默认保存在本地。
5. 不依赖服务器即可完整使用。
6. 原始 PDF / 图片不得被直接修改。
7. 标注必须作为独立数据层保存。
8. Windows 与 macOS 必须保持功能一致。
9. 项目结构必须可维护，不允许将大量逻辑写进单个 React Component。
10. 为未来移动端、云同步和 MusicXML 留出扩展空间，但当前不实现。

# 3. 项目目录

推荐使用以下目录结构：

```
notera/
│
├── src/
│   ├── app/
│   │   ├── App.tsx
│   │   ├── router.tsx
│   │   └── providers/
│   │
│   ├── components/
│   │   ├── common/
│   │   ├── layout/
│   │   └── ui/
│   │
│   ├── pages/
│   │   ├── Library/
│   │   ├── Reader/
│   │   └── Settings/
│   │
│   ├── features/
│   │   ├── library/
│   │   ├── reader/
│   │   ├── annotation/
│   │   ├── performance/
│   │   └── settings/
│   │
│   ├── services/
│   │   ├── PlatformService.ts
│   │   ├── DatabaseService.ts
│   │   ├── FileService.ts
│   │   └── ShortcutService.ts
│   │
│   ├── stores/
│   │   ├── libraryStore.ts
│   │   ├── readerStore.ts
│   │   ├── settingsStore.ts
│   │   └── annotationStore.ts
│   │
│   ├── types/
│   │
│   ├── hooks/
│   │
│   └── utils/
│
├── src-tauri/
│   ├── src/
│   │   ├── commands/
│   │   ├── database/
│   │   ├── filesystem/
│   │   ├── platform/
│   │   └── lib.rs
│   │
│   ├── capabilities/
│   ├── Cargo.toml
│   └── tauri.conf.json
│
├── public/
├── package.json
└── README.md
```

不要过度设计。

如果某些目录在当前阶段没有必要，可以暂时不创建。

# 4. 应用主要页面

Notera 第一版包含：

- Library
- Reader
- Settings

应用打开后默认进入 Library。

# 5. Library 页面

Library 是用户的乐谱库。

左侧 Sidebar：

```
Notera

Library
Recent
Favorites

Folders
Tags

Settings
```

右侧显示乐谱。

乐谱采用 Grid 卡片布局。

每个 ScoreCard 显示：

- 缩略图
- 标题
- 作曲家
- 页数
- 收藏状态

支持：

- 双击打开
- 右键菜单
- 收藏
- 删除
- 重命名
- 修改作曲家
- 在文件管理器中显示
- 添加标签

# 6. 乐谱导入

支持文件：

```
.pdf
.jpg
.jpeg
.png
```

支持以下导入方式：

### 方式一

Import 按钮。

打开系统文件选择器。

### 方式二

Drag & Drop。

用户可以直接将 PDF / 图片拖入 Library。

### 导入流程

```
选择文件
↓
校验文件类型
↓
生成 Score ID
↓
复制文件到 Notera Library
↓
读取 PDF 页数
↓
生成第一页 Thumbnail
↓
写入 SQLite
↓
刷新 Library
```

默认采用：

COPY INTO LIBRARY

即导入以后，将源文件复制进入 Notera 自己的数据目录。

不要只保存源路径。

避免用户移动原始文件后乐谱失效。

# 7. 文件存储

不要硬编码路径。

必须使用 Tauri / 系统 API 获取 App Data Directory。

逻辑目录：

```
Notera/
│
├── database/
│   └── notera.db
│
├── library/
│   └── scores/
│
├── thumbnails/
│
├── annotations/
│
├── cache/
│
└── settings/
```

macOS 应位于类似：

```
~/Library/Application Support/Notera/
```

Windows 应位于类似：

```
%APPDATA%\Notera\
```

实际目录必须通过 API 获取，而不是手工拼接。

# 8. SQLite 数据库

创建 scores 表。

```
CREATE TABLE scores (
    id TEXT PRIMARY KEY,
    title TEXT NOT NULL,
    composer TEXT,
    file_name TEXT NOT NULL,
    file_path TEXT NOT NULL,
    file_type TEXT NOT NULL,
    page_count INTEGER NOT NULL DEFAULT 1,
    thumbnail_path TEXT,
    favorite INTEGER NOT NULL DEFAULT 0,
    last_page INTEGER NOT NULL DEFAULT 1,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL,
    last_opened_at INTEGER
);
```

tags：

```
CREATE TABLE tags (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL UNIQUE
);
```

score_tags：

```
CREATE TABLE score_tags (
    score_id TEXT NOT NULL,
    tag_id TEXT NOT NULL,

    PRIMARY KEY (score_id, tag_id)
);
```

annotations：

```
CREATE TABLE annotations (
    id TEXT PRIMARY KEY,
    score_id TEXT NOT NULL,
    page INTEGER NOT NULL,
    type TEXT NOT NULL,
    data TEXT NOT NULL,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL
);
```

允许后续通过 migration 升级数据库。

不要在每次启动时直接 DROP / CREATE。

# 9. Reader

Reader 是整个 Notera 最重要的模块。

必须独立设计。

核心组件：

```
Reader
│
├── ReaderToolbar
├── ReaderViewport
├── PageRenderer
├── PdfRenderer
├── ImageRenderer
├── PageController
└── ZoomController
```

支持以下阅读模式：

### Single Page

单页模式。

### Double Page

左右两页。

适合钢琴谱。

### Continuous

纵向连续滚动。

### Horizontal

横向翻页。

### Half Page

半页翻页模式。

# 10. PDF Renderer

使用 PDF.js。

不要一次性渲染整个 PDF。

必须实现：

Lazy Rendering。

例如当前页面是：

```
Page 10
```

只需要优先渲染：

```
Page 9
Page 10
Page 11
```

根据需要预加载：

```
Page 8
Page 12
```

远离当前页面的 Canvas 可以释放。

目的：

防止 100+ 页 PDF 内存占用过高。

# 11. Zoom

支持：

```
Zoom In
Zoom Out
Fit Page
Fit Width
Actual Size
```

快捷键：

macOS：

```
⌘ +
⌘ -
⌘ 0
```

Windows：

```
Ctrl +
Ctrl -
Ctrl 0
```

不要在业务组件中直接判断 Command / Ctrl。

创建统一 ShortcutService。

# 12. PlatformService

创建：

```
interface PlatformService {
    isMacOS(): boolean;
    isWindows(): boolean;

    getPrimaryModifier(): "Meta" | "Control";

    openFile(path: string): Promise<void>;

    revealInFileManager(path: string): Promise<void>;

    enterFullscreen(): Promise<void>;

    exitFullscreen(): Promise<void>;
}
```

平台相关逻辑统一通过该 Service 调用。

# 13. Reader 页面布局

普通模式：

```
┌─────────────────────────────────────────┐
│ ← Library      Score Name      Controls │
├─────────────────────────────────────────┤
│                                         │
│                                         │
│                SCORE                    │
│                                         │
│                                         │
├─────────────────────────────────────────┤
│   ←      4 / 15        →      100%     │
└─────────────────────────────────────────┘
```

Reader 背景使用中性深灰。

乐谱页面使用白色。

# 14. Performance Mode

Performance Mode 是 Notera 的重要功能。

进入后隐藏：

- Sidebar
- Toolbar
- Window UI 中不必要元素
- Library controls

只保留：

- Score
- Page number
- 必要的翻页反馈

支持：

```
F11
```

或自定义快捷键进入演奏模式。

鼠标静止数秒后隐藏控制 UI。

移动鼠标以后恢复。

# 15. 翻页快捷键

默认：

```
→
PageDown
Space
```

下一页。

```
←
PageUp
Shift + Space
```

上一页。

允许用户后期自定义。

# 16. Bluetooth Pedal

第一版不要开发专用 Bluetooth Stack。

大量蓝牙翻页踏板实际上会模拟：

```
Keyboard Arrow
PageUp
PageDown
Space
Enter
```

因此第一版：

只实现键盘映射系统。

用户可以在 Settings 中配置：

```
Next Page Key
Previous Page Key
```

这样即可兼容大量蓝牙踏板。

# 17. Half Page Turn

必须预留 Half Page 模式。

这是 Notera 比普通 PDF Reader 更重要的音乐场景功能。

例如当前：

```
Page 1 Bottom
+
Page 2 Top
```

下一次翻页：

```
Page 2 Top
+
Page 2 Bottom
```

继续：

```
Page 2 Bottom
+
Page 3 Top
```

实现时不要真正修改 PDF。

采用 viewport crop / render region。

HalfPageController 建议独立实现：

```
HalfPageController
│
├── currentPage
├── currentHalf
├── next()
├── previous()
└── getVisibleRegions()
```

# 18. Annotation

标注不允许写入源 PDF。

使用独立 Annotation Layer。

架构：

```
PDF Layer

      ↓

Annotation Canvas

      ↓

Interaction Layer
```

支持：

- Pen
- Highlighter
- Eraser
- Text
- Undo
- Redo

暂时不实现复杂音乐符号。

第一阶段先保证自由书写体验。

# 19. Annotation 数据结构

TypeScript：

```
interface Annotation {
    id: string;

    scoreId: string;

    page: number;

    type:
        | "pen"
        | "highlighter"
        | "text";

    data: unknown;

    createdAt: number;

    updatedAt: number;
}
```

Pen：

```
interface PenAnnotationData {
    points: {
        x: number;
        y: number;
        pressure?: number;
    }[];

    width: number;

    color: string;
}
```

坐标不要保存 Canvas 实际像素坐标。

必须使用 normalized coordinate。

例如：

```
x = 0 ~ 1
y = 0 ~ 1
```

这样无论：

- Zoom
- Retina
- Windows DPI
- 窗口大小

标注位置都不会改变。

# 20. Undo / Redo

Annotation Store 必须实现：

```
past
present
future
```

或 Command Pattern。

必须支持：

```
Undo
Redo
```

macOS：

```
⌘ Z
⌘ Shift Z
```

Windows：

```
Ctrl Z
Ctrl Shift Z
```

# 21. 自动保存

以下数据需要自动保存：

- 当前页
- Zoom
- Reader Mode
- 标注
- Favorite
- Last Opened Time

用户重新打开一个乐谱时：

自动恢复：

```
Last Page
```

未来可以再考虑恢复 Zoom / 阅读模式。

# 22. Thumbnail

导入 PDF 后：

使用第一页生成 Thumbnail。

建议：

```
width ≈ 300px
```

不需要保存原始大小。

格式可以采用：

```
.webp
```

或：

```
.png
```

Thumbnail 使用后台异步生成。

不要阻塞 Library UI。

# 23. 搜索

Library 搜索支持：

```
Title
Composer
Tag
```

搜索实时更新。

第一阶段数据量不大，可以先使用 SQLite LIKE。

未来再考虑 FTS。

# 24. Settings

Settings 第一版包含：

### General

```
Theme
Language
```

Theme：

```
System
Light
Dark
```

### Reader

```
Default Reader Mode

Single
Double
Continuous
Horizontal
```

### Performance

```
Next Page Shortcut

Previous Page Shortcut
```

### Library

显示 Library 数据存储路径。

# 25. Theme

使用 CSS variables。

例如：

```
:root {
    --background: ...;
    --foreground: ...;
    --surface: ...;
    --border: ...;
}
```

不要在大量 React Component 中硬编码颜色。

支持：

```
System
Light
Dark
```

# 26. UI 风格

Notera UI 风格：

- 极简
- 专业
- 克制
- 现代
- 不要大量渐变
- 不要玻璃拟态堆叠
- 不要过度圆角
- 不要像管理后台
- 不要像网页 SaaS

设计参考方向：

```
Apple Books
Apple Music
Arc
Notion
Linear
Preview
```

但不要复制任何具体产品 UI。

# 27. macOS

需要注意：

- Retina Display
- Command shortcuts
- Fullscreen
- App Menu
- Window controls
- DMG build
- Apple Silicon
- Intel

未来需要支持签名：

```
Developer ID
Notarization
```

当前开发阶段不要求配置真实证书。

# 28. Windows

需要注意：

- High DPI
- Ctrl shortcuts
- File picker
- AppData
- MSI / NSIS
- Windows file path

不要假设：

```
/
```

永远是路径分隔符。

Rust 端使用 Path / PathBuf。

# 29. 错误处理

Rust command 不允许随意：

```
unwrap()
```

需要正常返回：

```
Result<T, Error>
```

前端调用 Native command 必须处理异常。

错误统一显示 Toast。

例如：

```
Failed to import score.
```

开发环境 Console 可以打印详细 Error。

# 30. Logging

Rust：

使用 tracing。

前端：

封装 logger。

禁止大量：

```
console.log(...)
```

散落在代码中。

# 31. 性能要求

Reader 必须重点优化。

避免：

```
100 页 PDF
=
100 Canvas 同时存在
```

目标：

当前页面附近优先渲染。

页面切换过程不应该明显卡顿。

Thumbnail 不重复生成。

PDF document 不重复 load。

# 32. React 代码规范

必须：

- Functional Component
- Hooks
- TypeScript strict
- 避免 any
- Component 单一职责
- Hooks 独立
- State 与 UI 分离
- Service 与 Component 分离

禁止出现类似：

```
Reader.tsx

2000 lines
```

如果组件超过约 300 行，应判断是否需要拆分。

# 33. Rust 代码规范

Native 功能按照模块拆分。

例如：

```
commands/
    library.rs
    database.rs
    filesystem.rs
```

不要把所有 Tauri command 写在：

```
lib.rs
```

# 34. Phase 1

先完成项目基础架构。

需要完成：

- Tauri 2
- React
- TypeScript
- Vite
- Tailwind
- React Router
- Zustand
- SQLite
- 基础主题
- Library 页面骨架
- Reader 页面骨架
- Settings 页面骨架

必须保证：

```
npm run tauri dev
```

可以正常运行。

# 35. Phase 2

实现 Library。

包括：

- 文件选择
- Drag & Drop
- PDF
- PNG
- JPG
- JPEG
- 数据复制
- SQLite 写入
- Thumbnail
- Library Grid
- 删除
- 重命名
- Favorite
- Search
- Recent

# 36. Phase 3

Reader。

包括：

- PDF.js
- Image renderer
- Single Page
- Double Page
- Continuous
- Horizontal
- Zoom
- Fit Width
- Fit Page
- Keyboard Page Turn
- Current Page persistence

# 37. Phase 4

Performance。

包括：

- Fullscreen
- Auto-hide controls
- Shortcut system
- Bluetooth keyboard pedal compatibility
- Half-page turning

# 38. Phase 5

Annotation。

包括：

- Pen
- Highlighter
- Eraser
- Text
- Undo
- Redo
- Auto Save
- Normalized coordinates

# 39. Phase 6

Polish。

包括：

- Settings
- Dark Mode
- Empty states
- Loading states
- Error UI
- Performance optimization
- Keyboard navigation
- DPI testing
- macOS testing
- Windows testing

# 40. Phase 7

Release。

构建：

macOS：

```
.app
.dmg
```

Windows：

```
.exe
.msi
```

检查：

- App name
- Version
- App icon
- Bundle ID
- Installer
- Auto updater architecture

第一版暂时可以不正式开启自动更新服务器。

但架构不能阻碍未来加入 Tauri Updater。

# 41. 当前禁止开发的功能

不要实现：

```
AI
OCR
MusicXML Editor
MIDI Editor
Account
Login
Server
Cloud Sync
Online Score Store
Social
Payment
Subscription
Collaboration
```

这些属于未来版本。

# 42. Codex 工作规则

不要一次性完成整个项目。

必须按照 Phase 工作。

每个 Phase：

1. 分析当前代码。
2. 给出该阶段计划。
3. 实现。
4. 运行 TypeScript Check。
5. 运行 ESLint。
6. 运行 Rust cargo check。
7. 修复 Error。
8. 检查 Windows / macOS 跨平台问题。
9. 总结修改内容。
10. 再进入下一阶段。

不得为了绕过错误：

- 删除功能
- 使用 any
- 添加大量 eslint-disable
- 添加 ts-ignore
- 捕获错误但不处理
- 使用 unwrap 隐藏 Rust 错误

# 43. 第一条 Codex 指令

现在先执行 Phase 1。

目标：

建立 Notera 基础工程。

完成：

```
Tauri 2
React
TypeScript
Vite
Tailwind CSS
React Router
Zustand

Library
Reader
Settings

基础 Layout
Sidebar
Theme
Routing
```

创建合理的项目结构。

暂时不要实现：

```
PDF.js
SQLite 具体业务
Annotation
Half Page
Bluetooth Pedal
```

但是要提前保证架构可以支持这些功能。

完成后：

运行所有可用检查。

确保项目能够启动。

最后输出：

1. 创建了哪些文件
2. 修改了哪些文件
3. 当前架构说明
4. 如何运行项目
5. 下一阶段建议
6. 当前存在的问题或技术债务

不要直接开始 Phase 2。