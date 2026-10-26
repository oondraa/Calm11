#include "gui.h"

#include "app.h"
#include "backup.h"
#include "system.h"
#include "tweaks.h"

#include <commctrl.h>
#include <uxtheme.h>

#include <algorithm>
#include <string>
#include <vector>

namespace {

enum : int { IDC_MAC = 100, IDC_GNOME, IDC_NONE, IDC_LIST, IDC_REVERT, IDC_APPLY };

struct Ui {
    HWND wnd{}, title{}, subtitle{}, mac{}, gnome{}, none{}, list{}, status{}, revert{}, apply{};
    HFONT font{}, titleFont{};
    UINT dpi = 96;
    bool elevated = false;
    Backup backup;
};
Ui* ui;

int S(int v) { return MulDiv(v, static_cast<int>(ui->dpi), 96); }

std::wstring NoteFor(const Tweak& t) {
    std::wstring note;
    auto add = [&](const wchar_t* s) {
        if (!note.empty()) note += L" · ";
        note += s;
    };
    if (NeedsAdmin(t)) add(L"správce");
    if (t.needsExplorerRestart) add(L"restart Průzkumníka");
    if (t.note) add(t.note);
    return note;
}

void SetStatus(const std::wstring& text) { SetWindowTextW(ui->status, text.c_str()); }

void CreateFonts() {
    if (ui->font) DeleteObject(ui->font);
    if (ui->titleFont) DeleteObject(ui->titleFont);
    NONCLIENTMETRICSW ncm{sizeof(ncm)};
    SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0, ui->dpi);
    LOGFONTW lf = ncm.lfMessageFont;
    ui->font = CreateFontIndirectW(&lf);
    lf.lfHeight *= 2;
    lf.lfWeight = FW_SEMIBOLD;
    ui->titleFont = CreateFontIndirectW(&lf);

    for (HWND h : {ui->subtitle, ui->mac, ui->gnome, ui->none, ui->list, ui->status, ui->revert, ui->apply})
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(ui->font), TRUE);
    SendMessageW(ui->title, WM_SETFONT, reinterpret_cast<WPARAM>(ui->titleFont), TRUE);
}

void Layout() {
    RECT rc;
    GetClientRect(ui->wnd, &rc);
    const int m = S(20), w = rc.right - 2 * m, bh = S(32), bw = S(150), gap = S(8);
    int y = S(14);

    MoveWindow(ui->title, m, y, w, S(40), TRUE);
    y += S(40);
    MoveWindow(ui->subtitle, m, y, w, S(22), TRUE);
    y += S(22) + S(14);
    MoveWindow(ui->mac, m, y, bw, bh, TRUE);
    MoveWindow(ui->gnome, m + bw + gap, y, bw, bh, TRUE);
    MoveWindow(ui->none, m + 2 * (bw + gap), y, bw, bh, TRUE);
    y += bh + S(12);

    const int bottom = rc.bottom - S(16) - bh;
    MoveWindow(ui->list, m, y, w, std::max(0, bottom - S(12) - y), TRUE);
    MoveWindow(ui->status, m, bottom + S(7), std::max(0, w - 2 * (bw + gap)), S(22), TRUE);
    MoveWindow(ui->revert, rc.right - m - 2 * bw - gap, bottom, bw, bh, TRUE);
    MoveWindow(ui->apply, rc.right - m - bw, bottom, bw, bh, TRUE);

    const int lw = w - GetSystemMetricsForDpi(SM_CXVSCROLL, ui->dpi) - S(6);
    ListView_SetColumnWidth(ui->list, 0, lw * 52 / 100);
    ListView_SetColumnWidth(ui->list, 1, lw * 13 / 100);
    ListView_SetColumnWidth(ui->list, 2, lw - lw * 52 / 100 - lw * 13 / 100);
}

void Populate() {
    ListView_EnableGroupView(ui->list, TRUE);
    std::vector<std::wstring> groups;
    const auto& tweaks = AllTweaks();
    for (size_t i = 0; i < tweaks.size(); ++i) {
        const Tweak& t = tweaks[i];
        auto it = std::find(groups.begin(), groups.end(), t.group);
        int groupId = static_cast<int>(it - groups.begin()) + 1;
        if (it == groups.end()) {
            groups.push_back(t.group);
            LVGROUP g{sizeof(g)};
            g.mask = LVGF_HEADER | LVGF_GROUPID | LVGF_STATE;
            g.pszHeader = const_cast<wchar_t*>(t.group);
            g.iGroupId = groupId;
            g.stateMask = g.state = LVGS_COLLAPSIBLE;
            ListView_InsertGroup(ui->list, -1, &g);
        }
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_GROUPID;
        item.iItem = static_cast<int>(i);
        item.pszText = const_cast<wchar_t*>(t.title);
        item.iGroupId = groupId;
        int index = ListView_InsertItem(ui->list, &item);
        std::wstring note = NoteFor(t);
        ListView_SetItemText(ui->list, index, 2, note.data());
    }
}

// Shows what the system currently has and resets the checkboxes to match it.
void Refresh() {
    const auto& tweaks = AllTweaks();
    for (size_t i = 0; i < tweaks.size(); ++i) {
        State s = GetState(tweaks[i]);
        const wchar_t* text = s == State::On ? L"zapnuto" : s == State::Partial ? L"částečně" : L"–";
        int index = static_cast<int>(i);
        ListView_SetItemText(ui->list, index, 1, const_cast<wchar_t*>(text));
        ListView_SetCheckState(ui->list, index, s == State::On);
    }
}

void AskExplorerRestart() {
    if (MessageBoxW(ui->wnd,
                    L"Některé změny se projeví až po restartu Průzkumníka.\n"
                    L"Taskbar na pár vteřin zmizí a otevřená okna složek se zavřou.\n\nRestartovat teď?",
                    APP_NAME, MB_YESNO | MB_ICONQUESTION) == IDYES) {
        HCURSOR old = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
        RestartExplorer();
        SetCursor(old);
    }
}

void OnApply() {
    std::vector<bool> want;
    for (int i = 0; i < ListView_GetItemCount(ui->list); ++i) want.push_back(ListView_GetCheckState(ui->list, i) != 0);

    HCURSOR old = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    ApplyReport r = ApplyDesired(want, ui->backup);
    NotifySettingsChanged();
    Refresh();
    SetCursor(old);

    if (r.changed == 0 && r.failed.empty()) {
        SetStatus(L"Není co měnit – systém už odpovídá výběru.");
        return;
    }
    std::wstring status = L"Hotovo, změněno úprav: " + std::to_wstring(r.changed) + L".";
    if (!r.failed.empty()) status += L" Nepovedlo se: " + std::to_wstring(r.failed.size()) + L".";
    SetStatus(status);

    if (!r.failed.empty()) {
        std::wstring msg = L"Tyhle úpravy se nepovedly:\n\n";
        for (const auto& f : r.failed) msg += L"• " + f + L"\n";
        if (!ui->elevated) msg += L"\nČást úprav potřebuje práva správce – spusť " APP_NAME L" znovu a potvrď UAC.";
        MessageBoxW(ui->wnd, msg.c_str(), APP_NAME, MB_OK | MB_ICONWARNING);
    }
    if (r.needExplorerRestart) AskExplorerRestart();
}

void OnRevertAll() {
    if (ui->backup.Empty()) {
        SetStatus(L"Není co vracet – " APP_NAME L" zatím nic nezměnil.");
        return;
    }
    if (MessageBoxW(ui->wnd, L"Vrátit všechno, co " APP_NAME L" změnil, do původního stavu?", APP_NAME,
                    MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    Backup::Result r = ui->backup.RestoreAll();
    ui->backup.Save();
    NotifySettingsChanged();
    Refresh();
    SetStatus(L"Vráceno hodnot: " + std::to_wstring(r.restored) + L".");
    if (!r.failed.empty()) {
        std::wstring msg = L"Tohle se vrátit nepovedlo (zůstává v záloze):\n\n";
        for (const auto& f : r.failed) msg += L"• " + f + L"\n";
        MessageBoxW(ui->wnd, msg.c_str(), APP_NAME, MB_OK | MB_ICONWARNING);
    }
    if (r.restored > 0) AskExplorerRestart();
}

void SelectPreset(unsigned preset) {
    const auto& tweaks = AllTweaks();
    for (size_t i = 0; i < tweaks.size(); ++i) {
        int index = static_cast<int>(i);
        if (preset == kNoPreset) {
            ListView_SetCheckState(ui->list, index, FALSE);
        } else if (tweaks[i].presets & preset) {
            ListView_SetCheckState(ui->list, index, TRUE);
        }
    }
    SetStatus(preset == kNoPreset ? L"Vše odškrtnuto – Použít vrátí Windows do původního stavu."
                                  : L"Preset vybrán – zkontroluj a dej Použít.");
}

HWND Child(const wchar_t* cls, const wchar_t* text, DWORD style, int id = 0) {
    return CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, ui->wnd,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
}

void OnCreate(HWND wnd) {
    ui->wnd = wnd;
    ui->dpi = GetDpiForWindow(wnd);

    ui->title = Child(L"STATIC", APP_NAME, SS_LEFT | SS_NOPREFIX);
    ui->subtitle = Child(L"STATIC", L"Uklidí Windows 11 bez balastu. Zaškrtni, co chceš, a dej Použít. Vše jde vrátit.",
                         SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS);
    ui->mac = Child(L"BUTTON", L"Preset macOS", WS_TABSTOP | BS_PUSHBUTTON, IDC_MAC);
    ui->gnome = Child(L"BUTTON", L"Preset GNOME", WS_TABSTOP | BS_PUSHBUTTON, IDC_GNOME);
    ui->none = Child(L"BUTTON", L"Odškrtnout vše", WS_TABSTOP | BS_PUSHBUTTON, IDC_NONE);
    ui->list = Child(WC_LISTVIEWW, L"",
                     WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER, IDC_LIST);
    ui->status = Child(L"STATIC", L"", SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS);
    ui->revert = Child(L"BUTTON", L"Vrátit vše", WS_TABSTOP | BS_PUSHBUTTON, IDC_REVERT);
    ui->apply = Child(L"BUTTON", L"Použít", WS_TABSTOP | BS_DEFPUSHBUTTON, IDC_APPLY);

    SetWindowTheme(ui->list, L"Explorer", nullptr);
    ListView_SetExtendedListViewStyle(ui->list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    const wchar_t* columns[] = {L"Úprava", L"Teď", L"Poznámka"};
    for (int i = 0; i < 3; ++i) {
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT;
        col.pszText = const_cast<wchar_t*>(columns[i]);
        ListView_InsertColumn(ui->list, i, &col);
    }

    CreateFonts();
    Populate();
    Refresh();
    Layout();
    SetStatus(ui->elevated ? L"Připraveno." : L"Bez práv správce – úpravy označené „správce“ se nepovedou.");
}

LRESULT CALLBACK WndProc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        OnCreate(wnd);
        return 0;
    case WM_SIZE:
        Layout();
        return 0;
    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
        UINT dpi = GetDpiForWindow(wnd);
        mmi->ptMinTrackSize = {MulDiv(560, dpi, 96), MulDiv(420, dpi, 96)};
        return 0;
    }
    case WM_DPICHANGED: {
        ui->dpi = HIWORD(wp);
        CreateFonts();
        auto* r = reinterpret_cast<RECT*>(lp);
        SetWindowPos(wnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        Layout();
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wp);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, GetSysColor(reinterpret_cast<HWND>(lp) == ui->title ? COLOR_WINDOWTEXT : COLOR_GRAYTEXT));
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    }
    case WM_NOTIFY: {
        auto* hdr = reinterpret_cast<NMHDR*>(lp);
        // Clicking anywhere on a row toggles it, not just the tiny checkbox.
        if (hdr->hwndFrom == ui->list && hdr->code == NM_CLICK) {
            auto* ia = reinterpret_cast<NMITEMACTIVATE*>(lp);
            LVHITTESTINFO hit{};
            hit.pt = ia->ptAction;
            ListView_SubItemHitTest(ui->list, &hit);
            if (hit.iItem >= 0 && !(hit.flags & LVHT_ONITEMSTATEICON)) {
                ListView_SetCheckState(ui->list, hit.iItem, !ListView_GetCheckState(ui->list, hit.iItem));
            }
        }
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_MAC: SelectPreset(kMac); break;
        case IDC_GNOME: SelectPreset(kGnome); break;
        case IDC_NONE: SelectPreset(kNoPreset); break;
        case IDC_REVERT: OnRevertAll(); break;
        case IDOK:
        case IDC_APPLY: OnApply(); break;
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(wnd, msg, wp, lp);
}

}  // namespace

int RunGui(HINSTANCE instance, int show) {
    Ui state;
    state.elevated = IsElevated();
    ui = &state;

    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName = APP_NAME L"Window";
    RegisterClassExW(&wc);

    UINT dpi = GetDpiForSystem();
    HWND wnd = CreateWindowExW(0, wc.lpszClassName, APP_NAME, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                               MulDiv(780, dpi, 96), MulDiv(680, dpi, 96), nullptr, nullptr, instance, nullptr);
    if (!wnd) return 1;
    ShowWindow(wnd, show);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (IsDialogMessageW(wnd, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(state.font);
    DeleteObject(state.titleFont);
    ui = nullptr;
    return static_cast<int>(msg.wParam);
}
