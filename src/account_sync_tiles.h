#pragma once

#include "account_sync.h"
#include "account_sync_tile_layout.h"

#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <array>
#include <memory>
#include <new>
#include <string>
#include <utility>

namespace tp {

namespace account_sync_tiles_detail {

inline constexpr wchar_t kClassName[] = L"TocPilotAccountSyncTileViewport";

struct View {
    AccountSyncTileProjection projection;
    std::wstring error;
    int offset = 0;
    HFONT font = nullptr;
};

inline View* Get(HWND hwnd) {
    return reinterpret_cast<View*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

inline int AccountIndex(
    const AccountSyncTileProjection& projection,
    std::wstring_view account) {
    for (std::size_t i = 0; i < projection.accounts.size(); ++i) {
        if (projection.accounts[i].account == account) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

inline unsigned Dpi(HWND hwnd) {
    return GetDpiForWindow(hwnd);
}

inline void UpdateScrollbar(HWND hwnd, View& view) {
    RECT client{};
    GetClientRect(hwnd, &client);
    const int width = std::max(0, static_cast<int>(client.right - client.left));
    const int content = SyncTileContentWidth(
        view.projection.accounts.size(), Dpi(hwnd));
    view.offset = ClampSyncTileScroll(view.offset, content, width);

    SCROLLINFO scroll{};
    scroll.cbSize = sizeof(scroll);
    scroll.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    scroll.nMin = 0;
    scroll.nMax = std::max(0, content - 1);
    scroll.nPage = static_cast<UINT>(width);
    scroll.nPos = view.offset;
    SetScrollInfo(hwnd, SB_HORZ, &scroll, TRUE);
}

inline void DrawTextIn(HDC dc, const std::wstring& text,
                       RECT rect, COLORREF color, UINT flags) {
    SetTextColor(dc, color);
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()),
              &rect, flags | DT_SINGLELINE | DT_VCENTER |
              DT_END_ELLIPSIS | DT_NOPREFIX);
}

inline COLORREF StateColor(AccountSyncTileState state) {
    switch (state) {
    case AccountSyncTileState::RuleSatisfied: return RGB(17, 112, 51);
    case AccountSyncTileState::Newer: return RGB(24, 91, 169);
    case AccountSyncTileState::Older: return RGB(150, 91, 0);
    default: return RGB(24, 24, 24);
    }
}

inline int VisualRow(AccountSyncTileRow row) {
    switch (row) {
    case AccountSyncTileRow::Keybindings: return 0;
    case AccountSyncTileRow::Macros: return 1;
    case AccountSyncTileRow::PfUiSettings: return 3;
    case AccountSyncTileRow::PfUiCache: return 4;
    }
    return 0;
}

inline const wchar_t* RowLabel(AccountSyncTileRow row) {
    switch (row) {
    case AccountSyncTileRow::Keybindings: return L"Keybinds";
    case AccountSyncTileRow::Macros: return L"Macros";
    case AccountSyncTileRow::PfUiSettings: return L"Settings";
    case AccountSyncTileRow::PfUiCache: return L"Cache";
    }
    return L"";
}

inline const AccountSyncTileOperation* TargetOperation(
    const AccountSyncTileProjection& projection,
    const AccountSyncTileAccount& account,
    AccountSyncTileRow row) {
    for (const auto& op : projection.operations) {
        if (op.row == row && op.targetAccount == account.account) {
            return &op;
        }
    }
    return nullptr;
}

inline std::wstring DetailText(
    const AccountSyncTileProjection& projection,
    const AccountSyncTileCell& cell,
    const AccountSyncTileOperation* operation) {
    if (!cell.enabled) return L"Off";
    if (operation) {
        if (operation->kind == AccountSyncTileOperationKind::MergeSupportedCache) {
            return L"Merge " +
                std::to_wstring(operation->sourceAccounts.size()) +
                L" input(s)";
        }
        std::wstring text = L"From " + operation->selectedSourceAccount;
        if (operation->sourceAccounts.size() > 1) {
            text += L" +" +
                std::to_wstring(operation->sourceAccounts.size() - 1);
        }
        return text;
    }
    if (!cell.exists) return L"Missing";
    if (cell.matchingGroup != 0 &&
        cell.matchingGroup <= projection.matchingGroups.size()) {
        const auto& group =
            projection.matchingGroups[cell.matchingGroup - 1];
        if (group.row == cell.row && group.accounts.size() > 1) {
            return L"Match " + std::to_wstring(group.accounts.size());
        }
    }
    return L"";
}

inline void PaintTiles(HWND hwnd, HDC dc, const View& view) {
    RECT client{};
    GetClientRect(hwnd, &client);
    FillRect(dc, &client, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
    SetBkMode(dc, TRANSPARENT);
    const HFONT selectedFont = view.font
        ? view.font
        : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    const HGDIOBJ oldFont = SelectObject(dc, selectedFont);

    const unsigned dpi = Dpi(hwnd);
    const int margin = ScaleSyncTile(kSyncTileMargin, dpi);
    const int tileWidth = ScaleSyncTile(kSyncTileWidth, dpi);
    const int tileHeight = ScaleSyncTile(kSyncTileHeight, dpi);
    const int inside = ScaleSyncTile(11, dpi);
    const int labelWidth = ScaleSyncTile(94, dpi);
    const int rowHeight = ScaleSyncTile(19, dpi);

    if (!view.error.empty()) {
        RECT notice{margin, margin, client.right - margin,
                    client.bottom - margin};
        DrawTextIn(dc, view.error, notice, RGB(150, 91, 0),
                   DT_LEFT | DT_TOP | DT_WORDBREAK);
    } else if (view.projection.accounts.empty()) {
        RECT notice{margin, margin, client.right - margin,
                    client.bottom - margin};
        DrawTextIn(dc, L"Select two or more accounts and choose the sync items.",
                   notice, RGB(24, 24, 24), DT_LEFT);
    }

    constexpr std::array<AccountSyncTileRow, kAccountSyncTileRowCount> rows{
        AccountSyncTileRow::Keybindings, AccountSyncTileRow::Macros,
        AccountSyncTileRow::PfUiSettings, AccountSyncTileRow::PfUiCache
    };
    for (std::size_t i = 0; i < view.projection.accounts.size(); ++i) {
        const int x = SyncTileX(i, dpi) - view.offset;
        if (x + tileWidth < 0) continue;
        if (x >= client.right) break;

        const auto& tile = view.projection.accounts[i];
        RECT bounds{x, ScaleSyncTile(3, dpi), x + tileWidth,
                    ScaleSyncTile(3, dpi) + tileHeight};
        HBRUSH face = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(dc, &bounds, face);
        DeleteObject(face);
        HPEN border = CreatePen(PS_SOLID, 1, RGB(208, 211, 214));
        const auto oldPen = SelectObject(dc, border);
        const auto oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(border);

        RECT header{x + inside, ScaleSyncTile(kSyncTileHeaderY, dpi),
                    x + tileWidth - inside,
                    ScaleSyncTile(kSyncTileFirstRowY - 7, dpi)};
        DrawTextIn(dc, tile.account, header, RGB(24, 24, 24), DT_LEFT);

        RECT section{x + inside, SyncTileRowY(2, dpi),
                     x + tileWidth - inside, SyncTileRowY(2, dpi) + rowHeight};
        DrawTextIn(dc, L"pfUI", section, RGB(24, 24, 24), DT_LEFT);

        for (const auto row : rows) {
            const auto& cell = tile.rows[static_cast<std::size_t>(row)];
            const int y = SyncTileRowY(VisualRow(row), dpi);
            const int indent =
                row == AccountSyncTileRow::PfUiSettings ||
                row == AccountSyncTileRow::PfUiCache
                    ? ScaleSyncTile(11, dpi) : 0;
            const COLORREF color = cell.enabled
                ? StateColor(cell.state) : RGB(24, 24, 24);
            RECT label{x + inside + indent, y,
                       x + inside + labelWidth, y + rowHeight};
            DrawTextIn(dc, RowLabel(row), label, color, DT_LEFT);

            const auto* op = TargetOperation(view.projection, tile, row);
            RECT status{x + inside + labelWidth, y,
                        x + tileWidth - ScaleSyncTile(6, dpi),
                        y + rowHeight};
            DrawTextIn(dc, DetailText(view.projection, cell, op),
                       status, color, DT_RIGHT);
        }
    }

    // One connector per planned target operation. For non-adjacent accounts
    // the marker sits next to the TARGET, and its row names the true source;
    // no intermediate tile is shown as an additional source or target.
    for (const auto& op : view.projection.operations) {
        const int target = AccountIndex(view.projection, op.targetAccount);
        if (target < 0) continue;

        int source = AccountIndex(view.projection, op.selectedSourceAccount);
        if (op.kind == AccountSyncTileOperationKind::MergeSupportedCache) {
            source = -1;
            for (const auto& contributor : op.sourceAccounts) {
                const int candidate = AccountIndex(view.projection, contributor);
                if (candidate >= 0 && candidate != target &&
                    (source < 0 ||
                     std::abs(candidate - target) < std::abs(source - target))) {
                    source = candidate;
                }
            }
        }
        if (source < 0 || source == target) continue;

        const auto gap = SyncTileTargetGap(
            static_cast<std::size_t>(source),
            static_cast<std::size_t>(target));
        const int left = SyncTileX(gap, dpi) + tileWidth - view.offset;
        RECT marker{left + ScaleSyncTile(2, dpi),
                    SyncTileRowY(VisualRow(op.row), dpi),
                    left + ScaleSyncTile(kSyncTileGap - 2, dpi),
                    SyncTileRowY(VisualRow(op.row), dpi) + rowHeight};
        if (marker.right < 0 || marker.left >= client.right) continue;
        const bool forward = SyncTileArrowPointsRight(
            static_cast<std::size_t>(source),
            static_cast<std::size_t>(target));
        const wchar_t* arrow = forward ? L"\u2192" : L"\u2190";
        std::wstring markerText(arrow);
        if (op.requiresConfirmation) markerText += L" !";
        DrawTextIn(dc, markerText, marker, RGB(24, 24, 24), DT_CENTER);
    }
    SelectObject(dc, oldFont);
}

inline LRESULT CALLBACK WindowProc(HWND hwnd, UINT message,
                                    WPARAM wParam, LPARAM lParam) {
    auto* view = Get(hwnd);
    switch (message) {
    case WM_NCCREATE: {
        auto state = std::unique_ptr<View>(new (std::nothrow) View());
        if (!state) return FALSE;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(state.release()));
        return TRUE;
    }
    case WM_SETFONT:
        if (view) {
            view->font = reinterpret_cast<HFONT>(wParam);
            if (LOWORD(lParam)) InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    case WM_GETFONT:
        return view ? reinterpret_cast<LRESULT>(view->font) : 0;
    case WM_SIZE:
        if (view) {
            UpdateScrollbar(hwnd, *view);
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    case WM_HSCROLL:
        if (view) {
            SCROLLINFO scroll{};
            scroll.cbSize = sizeof(scroll);
            scroll.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_HORZ, &scroll);
            int position = view->offset;
            switch (LOWORD(wParam)) {
            case SB_LINELEFT: position -= ScaleSyncTile(32, Dpi(hwnd)); break;
            case SB_LINERIGHT: position += ScaleSyncTile(32, Dpi(hwnd)); break;
            case SB_PAGELEFT: position -= static_cast<int>(scroll.nPage); break;
            case SB_PAGERIGHT: position += static_cast<int>(scroll.nPage); break;
            case SB_THUMBTRACK:
            case SB_THUMBPOSITION: position = scroll.nTrackPos; break;
            case SB_LEFT: position = 0; break;
            case SB_RIGHT: position = scroll.nMax; break;
            default: return 0;
            }
            RECT client{};
            GetClientRect(hwnd, &client);
            const int maxWidth = SyncTileContentWidth(
                view->projection.accounts.size(), Dpi(hwnd));
            const int clamped = ClampSyncTileScroll(
                position, maxWidth, client.right - client.left);
            if (clamped != view->offset) {
                view->offset = clamped;
                UpdateScrollbar(hwnd, *view);
                InvalidateRect(hwnd, nullptr, TRUE);
            }
        }
        return 0;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        if (view) {
            const int direction =
                GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? -1 : 1;
            SendMessageW(hwnd, WM_HSCROLL,
                MAKEWPARAM(direction < 0 ? SB_LINELEFT : SB_LINERIGHT, 0), 0);
            return 0;
        }
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        if (view) {
            RECT client{};
            GetClientRect(hwnd, &client);
            const int width = client.right - client.left;
            const int height = client.bottom - client.top;
            HDC memory = width > 0 && height > 0 ? CreateCompatibleDC(dc) : nullptr;
            HBITMAP bitmap = memory ? CreateCompatibleBitmap(dc, width, height) : nullptr;
            if (memory && bitmap) {
                const auto previous = SelectObject(memory, bitmap);
                PaintTiles(hwnd, memory, *view);
                BitBlt(dc, 0, 0, width, height, memory, 0, 0, SRCCOPY);
                SelectObject(memory, previous);
            } else {
                PaintTiles(hwnd, dc, *view);
            }
            if (bitmap) DeleteObject(bitmap);
            if (memory) DeleteDC(memory);
        }
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_NCDESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        delete view;
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace account_sync_tiles_detail

inline HWND CreateAccountSyncTileViewport(HWND parent, int id) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpfnWndProc = account_sync_tiles_detail::WindowProc;
    wc.lpszClassName = account_sync_tiles_detail::kClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassExW(&wc) &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return nullptr;
    }
    return CreateWindowExW(WS_EX_CLIENTEDGE,
        account_sync_tiles_detail::kClassName, L"",
        WS_CHILD | WS_HSCROLL | WS_CLIPSIBLINGS,
        0, 0, 0, 0, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        wc.hInstance, nullptr);
}

inline void SetAccountSyncTilePreview(
    HWND hwnd, AccountSyncTileProjection projection, std::wstring error) {
    if (!hwnd) return;
    auto* view = account_sync_tiles_detail::Get(hwnd);
    if (!view) return;
    view->projection = std::move(projection);
    view->error = std::move(error);
    account_sync_tiles_detail::UpdateScrollbar(hwnd, *view);
    InvalidateRect(hwnd, nullptr, TRUE);
}

} // namespace tp
