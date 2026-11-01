#include "package_details.h"

#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace {
int failures = 0;
void Check(bool condition, const char* why) {
    if (!condition) { std::cerr << why << "\n"; ++failures; }
}
struct FakeClipboard {
    bool openWorks=true, emptyWorks=true, allocWorks=true;
    bool lockWorks=true, setWorks=true;
    int opens=0, empties=0, allocations=0, frees=0, closes=0, sets=0;
    std::wstring copied;
    bool Open(HWND) { ++opens; return openWorks; }
    bool Empty() { ++empties; return emptyWorks; }
    HGLOBAL Allocate(std::size_t count) {
        ++allocations;
        return allocWorks ? GlobalAlloc(GMEM_MOVEABLE, count) : nullptr;
    }
    void* Lock(HGLOBAL h) { return lockWorks ? GlobalLock(h) : nullptr; }
    void Unlock(HGLOBAL h) { GlobalUnlock(h); }
    HANDLE Set(HGLOBAL h) {
        ++sets;
        if (!setWorks) return nullptr;
        const auto* value=static_cast<const wchar_t*>(GlobalLock(h));
        if (value) copied=value;
        GlobalUnlock(h);
        GlobalFree(h); // simulate clipboard ownership after Set succeeds
        return reinterpret_cast<HANDLE>(1);
    }
    void Free(HGLOBAL h) { ++frees; GlobalFree(h); }
    void Close() { ++closes; }
};
void TestClipboard() {
    const std::wstring unicode=L"https://github.com/\u00C5ctor/\u65E5\u672C\u8A9E";
    FakeClipboard good;
    Check(tp::ui::CopyUnicodeClipboard(good, nullptr, unicode),
          "Valid Unicode clipboard write succeeds");
    Check(good.copied == unicode, "Clipboard preserves Unicode exact URL");
    Check(good.opens==1 && good.closes==1 && good.frees==0 && good.sets==1,
          "Successful Set transfers ownership and closes clipboard");
    for (int failure=0; failure<5; ++failure) {
        FakeClipboard bad;
        if (failure==0) bad.openWorks=false;
        if (failure==1) bad.emptyWorks=false;
        if (failure==2) bad.allocWorks=false;
        if (failure==3) bad.lockWorks=false;
        if (failure==4) bad.setWorks=false;
        Check(!tp::ui::CopyUnicodeClipboard(bad,nullptr,L"hello"),
              "Failed clipboard operation is reported");
        Check(bad.closes==(failure==0 ? 0 : 1),
              "Opened clipboard always closes on failure");
        Check(bad.frees==(failure>=3 ? 1 : 0),
              "Allocated memory freed if not transferred");
        Check(bad.sets==(failure==4 ? 1 : 0),
              "Clipboard only receives payload after a successful lock");
    }
    FakeClipboard empty;
    Check(!tp::ui::CopyUnicodeClipboard(empty,nullptr,L""),
          "Empty URL cannot overwrite clipboard");
    Check(empty.opens==0,"Empty URL does not open clipboard");
}
void TestDetails() {
    tp::PackageRecord p;
    p.name=L"Custom UI"; p.provider=L"github"; p.repository=L"alice/addon";
    p.target=L"addons"; p.mode=L"branch"; p.ref=L"feature/long-branch";
    p.installedRevision=L"0123456789abcdef"; p.latestRevision=L"fedcba9876543210";
    auto text=tp::ui::PackageDetailsText(p,L"GitHub",p.ref,L"Update Available");
    Check(text.find(L"Source: GitHub / alice/addon")!=std::wstring::npos,
          "Provider and repository are separate from owner");
    Check(text.find(L"Author: alice")!=std::wstring::npos,
          "Author means repository owner");
    Check(text.find(L"Branch: feature/long-branch")!=std::wstring::npos,
          "Branch rendered verbatim");
    Check(text.find(p.installedRevision)!=std::wstring::npos &&
          text.find(p.latestRevision)!=std::wstring::npos,
          "Both exact revisions remain accessible");
    Check(text.find(L"Status: Update Available")!=std::wstring::npos,
          "Status retains full readable wording");
    p.repository=L"group/subgroup/project"; p.provider=L"gitlab";
    p.target=L"dll"; p.mode=L"release"; p.ref.clear();
    text=tp::ui::PackageDetailsText(p,L"GitLab",L"Latest stable / module.dll",L"Up To Date");
    Check(text.find(L"Author: group/subgroup")!=std::wstring::npos,
          "Nested GitLab namespace is repository owner");
    Check(text.find(L"Type: DLL")!=std::wstring::npos &&
          text.find(L"Branch: Latest stable / module.dll")!=std::wstring::npos,
          "Release DLL has accurate type and source mode");
    p.target=L"mpq"; p.repository.clear();
    p.installedRevision.clear();p.latestRevision.clear();
    text=tp::ui::PackageDetailsText(p,L"GitHub",L"",L"Not Installed");
    Check(text.find(L"Type: MPQ")!=std::wstring::npos,
          "MPQ package type");
    Check(text.find(L"Author: \u2014")!=std::wstring::npos &&
          text.find(L"Installed: \u2014")!=std::wstring::npos &&
          text.find(L"Latest: \u2014")!=std::wstring::npos &&
          text.find(L"Branch: \u2014")!=std::wstring::npos,
          "Missing metadata is shown explicitly");
    const std::wstring longPart(700,L'x');
    p.repository=L"owner/"+longPart; p.ref=longPart;
    text=tp::ui::PackageDetailsText(p,L"GitHub",p.ref,L"Needs Attention");
    Check(text.find(longPart)!=std::wstring::npos && text.size()>longPart.size()*2,
          "Long names and branches remain complete in accessible text");
    Check(tp::ui::PackageDetailsText(tp::PackageRecord{},L"",L"",L"").find(
          L"Status: \u2014")!=std::wstring::npos,
          "Default/null-like metadata has safe placeholders");
}
void TestActions() {
    using A=tp::ui::PackageDetailAction;
    const auto& actions=tp::ui::kPackageDetailActions;
    Check(actions.size()==6,"Exactly six contextual actions");
    Check(tp::ui::PackageDetailActionAt(0,0)==A::Install &&
          tp::ui::PackageDetailActionAt(0,1)==A::Remove &&
          tp::ui::PackageDetailActionAt(1,0)==A::OpenRepo &&
          tp::ui::PackageDetailActionAt(1,1)==A::CopyLink &&
          tp::ui::PackageDetailActionAt(2,0)==A::OpenFolder &&
          tp::ui::PackageDetailActionAt(2,1)==A::ChangeBranch,
          "Exact two-column three-row action order");
    std::set<int> ids;
    for(auto action: actions) ids.insert(static_cast<int>(action));
    Check(ids.size()==6,"Every action has a unique command identifier");
    Check(static_cast<int>(A::Install)==1013 &&
          static_cast<int>(A::Remove)==1014 &&
          static_cast<int>(A::OpenRepo)==1050 &&
          static_cast<int>(A::CopyLink)==1053 &&
          static_cast<int>(A::OpenFolder)==1049 &&
          static_cast<int>(A::ChangeBranch)==1051,
          "Existing handlers retain command routing; only Copy Link is new");
}
} // namespace
int main() {
    TestClipboard();
    TestDetails();
    TestActions();
    return failures ? 1 : 0;
}
