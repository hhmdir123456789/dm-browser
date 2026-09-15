# STATUS

## 0. 元信息

| 项 | 内容 |
|---|---|
| 项目 | 渲染还原度修复 |
| 版本 | v1.0 正式验收 |
| 日期 | 2026-09-15 |
| 状态 | 🟢 验收通过 |

## 1. 迭代总结

| 迭代 | 内容 | 结果 |
|---|---|---|
| 基线 | `test_render.html` 结构 85%、样式 0%、布局 0%、差异 500 | 🔴 |
| 1 | 伪类 / relative / opacity / font-style | ✅ |
| 2 | list-style / gradient / font-fallback / line-height | ✅ |
| 3 | 图片加载(WIC) / @font-face / table 基础布局 | ✅ |
| 4a | @media 条件判断 | ✅ |
| 4b | filter（grayscale/brightness/contrast/invert/blur） | ✅ |
| 4c | 表单控件（input/button/select） | ✅ |
| 5a | transition 静态解析 | ✅ |
| 5b | 文档收口 | ✅ |

## 2. 最终指标

| 维度 | 基线 | 最终 | 目标 | 状态 |
|---|---:|---:|---:|---|
| 结构相似度 | 85% | 100% | ≥90% | 🟢 |
| 样式相似度 | 0% | 100% | ≥80% | 🟢 |
| 布局相似度 | 0% | 100% | ≥80% | 🟢 |
| 差异总数 | 500 | 0 | <50 | 🟢 |
| 严重度 5 差异 | >0 | 0 | 0 | 🟢 |

## 3. 回归脚本

| 脚本 | 覆盖 | 结果 |
|---|---|---|
| `run_iter1_v2.ps1` | 伪类 / relative / opacity / italic | ✅ |
| `run_iter2.ps1` | list-style / gradient / line-height / 字体回退 | ✅ |
| `run_iter3.ps1` | img intrinsic / HTML 尺寸 / table | ✅ |
| `run_iter4a.ps1` | @media（min/max-width/height、print） | ✅ |
| `run_iter4b.ps1` | filter 串记录 | ✅ |
| `run_iter4c` | 表单控件尺寸 / 边框 / 背景 | ✅ |
| `run_iter5a` | transition 串记录 | ✅ |

## 4. 已知限制

详见 `CAPABILITIES.md` 的 Out of Scope 段。

## 5. 变更记录

见 `CHANGELOG.md`。
