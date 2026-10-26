# Calm11

Uklidí Windows 11 bez balastu: jedno malé `.exe` (~220 KB), žádná služba, nic na pozadí.
Všechno jde vrátit – původní stav se ukládá do `%LOCALAPPDATA%\Calm11\backup.txt`.

## Build

```
cmake -S . -B build -A x64
cmake --build build --config Release
```

Výsledek: `build\Release\Calm11.exe`. Název appky se mění v `CMakeLists.txt` (`APP_NAME`).

## Použití

- bez parametrů – otevře okno (požádá o práva správce kvůli úpravám v HKLM)
- `--status` – jen vypíše stav, nic nemění
- `--preset mac|gnome` – zapne úpravy z presetu
- `--revert` – vrátí vše, co appka změnila

Nová úprava = jeden záznam v `src/tweaks.cpp`.

## Licence

Copyright (c) 2026 Ondra (oondraa). Všechna práva vyhrazena.
Zdarma jen pro osobní, nekomerční použití – podmínky v [LICENSE](LICENSE).
