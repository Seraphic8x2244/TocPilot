#include "package_folder.h"

int main() {
    namespace fs = std::filesystem;
    using tp::ui::IsPathWithinRoot;
    if (!IsPathWithinRoot(fs::path(L"C:/WoW"), fs::path(L"c:/wow/Interface/AddOns"))) return 1;
    if (IsPathWithinRoot(fs::path(L"C:/WoW"), fs::path(L"C:/WoWBackup"))) return 2;
    if (IsPathWithinRoot(fs::path(L"C:/WoW"), fs::path(L"D:/WoW"))) return 3;
    if (IsPathWithinRoot(fs::path(L"C:/WoW"), fs::path(L"C:/"))) return 4;
    if (tp::ui::ResolvePackageFolder(fs::path(L"C:/WoW"), fs::path(L"../Outside"))) return 5;
    if (tp::ui::ResolvePackageFolder(fs::path(L"C:/WoW"), fs::path(L"C:/Outside"))) return 6;
    return 0;
}
