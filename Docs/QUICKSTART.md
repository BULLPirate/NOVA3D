# NOVA3D — быстрый старт

## Сборка

```bash
cmake -S . -B build
cmake --build build --target NovaEditor Nova3D NovaTests
```

## Запуск

| Что | Команда |
|-----|---------|
| **Редактор** | `open build/bin/NovaEditor.app` или `./Tools/run.sh editor` |
| **Runtime** | `open build/bin/Nova3D.app` или `./Tools/run.sh game` |
| **Тесты** | `build/bin/NovaTests` |

## Редактор — управление

- **Hierarchy** — выбор объекта; Add Cube / Delete.
- **Viewport** (наведи мышь на серую область под подсказками):
  - **ЛКМ** — вращение выбранного меша (куба).
  - **Shift + ЛКМ** — сдвиг по X/Y.
  - **ПКМ** — орбита камеры.
  - **Колёсико** — приближение/отдаление камеры.
- **File → Save Scene** — в `Assets/Scenes/demo.scene.json` (Ctrl+S в меню).

Лог: `~/Library/Logs/NOVA3D.log`
