#include "ui/panels/AboutPanel.hpp"

#include <string>

#include <windows.h>

#include <wx/statbox.h>

#include "managers/LanguageManager.hpp"

// ============================================================
// 版本 / 主页由 CMake 注入
// ============================================================
#ifndef KBMACRO_VERSION
#define KBMACRO_VERSION "0.0.0"
#endif
#ifndef KBMACRO_HOMEPAGE
#define KBMACRO_HOMEPAGE "https://example.com/kbmacro"
#endif

namespace {

    // 第三方库信息与对应的许可资源 ID。
    // 顺序即列表顺序，也是许可全文拼接顺序。
    // 所有字段均为 ASCII 语义（库名 / 版本号 / 许可名），用窄字符存储。
    struct LibInfo {
        const char *name;
        const char *version;
        const char *license;
        int licenseResId;
    };

    constexpr LibInfo kLibs[] = {
        {.name = "KBMacro", .version = KBMACRO_VERSION, .license = "GPL-3.0", .licenseResId = IDR_LICENSE_KBMACRO},
        {.name = "wxWidgets", .version = "3.2.11", .license = "wxWindows Library Licence", .licenseResId = IDR_LICENSE_WXWIDGETS},
        {.name = "spdlog", .version = "1.17.0", .license = "MIT", .licenseResId = IDR_LICENSE_SPDLOG},
        {.name = "nlohmann-json", .version = "3.12.0", .license = "MIT", .licenseResId = IDR_LICENSE_NLOHMANN_JSON},
        {.name = "moodycamel::ConcurrentQueue", .version = "1.0.5", .license = "BSD 2-Clause",
            .licenseResId = IDR_LICENSE_CONCURRENTQUEUE
        },
        {.name = "WIL", .version = "1.0.260126.7", .license = "MIT", .licenseResId = IDR_LICENSE_WIL},
        {.name = "GoogleTest", .version = "1.18.0", .license = "BSD 3-Clause", .licenseResId = IDR_LICENSE_GTEST},
    };

    constexpr auto kSeparator =
            L"\n\n==================================================\n\n";

    // 从 RC 的 RCDATA 资源读出原始字节，当作 UTF-8 文本返回
    std::string LoadRcText(const int resId) {
        const auto hInst = ::GetModuleHandleW(nullptr);
        const auto hRes = ::FindResourceW(hInst, MAKEINTRESOURCEW(resId), RT_RCDATA);
        if (!hRes) return {};
        const auto hGlobal = ::LoadResource(hInst, hRes);
        if (!hGlobal) return {};
        const DWORD size = ::SizeofResource(hInst, hRes);
        const auto data = static_cast<const char *>(::LockResource(hGlobal));
        if (!data || size == 0) return {};
        return {data, size};
    }

    // 拼接：库名 + 版本 + 许可名 + 分隔线 + 许可全文
    wxString BuildLicenseText() {
        wxString text;
        for (const auto &[name, version, license, licenseResId]: kLibs) {
            text += wxString::FromAscii(name);
            text += L"\n";
            text += wxString::FromAscii(version);
            text += L"  |  ";
            text += wxString::FromAscii(license);
            text += kSeparator;

            const std::string raw = LoadRcText(licenseResId);
            text += wxString::FromUTF8(raw.c_str(), raw.size());
            text += kSeparator;
        }
        return text;
    }

} // namespace

AboutPanel::AboutPanel(wxWindow *parent)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       wxVSCROLL | wxHSCROLL) {
    buildUi();

    // 页面级双向滚动：正常尺寸都看不到，窗口太窄时出横向滚条
    SetScrollRate(FromDIP(10), FromDIP(10));
    wxWindowBase::FitInside();
}

void AboutPanel::buildUi() {
    auto *root = new wxBoxSizer(wxVERTICAL);

    // ---- 分组一：程序信息 ----
    {
        auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_ABOUT_GROUP_APP));
        auto *box = new wxStaticBoxSizer(staticBox, wxVERTICAL);

        auto *form = new wxFlexGridSizer(2, FromDIP(wxSize(8, 8)));
        form->AddGrowableCol(1, 1);

        form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_ABOUT_LABEL_NAME)),
                  0, wxALIGN_CENTER_VERTICAL);
        form->Add(new wxStaticText(staticBox, wxID_ANY, L"KBMacro"),
                  0, wxALIGN_CENTER_VERTICAL);

        form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_ABOUT_LABEL_VERSION)),
                  0, wxALIGN_CENTER_VERTICAL);
        form->Add(new wxStaticText(staticBox, wxID_ANY, wxString::FromAscii(KBMACRO_VERSION)),
                  0, wxALIGN_CENTER_VERTICAL);

        form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_ABOUT_LABEL_HOMEPAGE)),
                  0, wxALIGN_CENTER_VERTICAL);
        form->Add(new wxHyperlinkCtrl(staticBox, wxID_ANY,
                                      wxString::FromAscii(KBMACRO_HOMEPAGE),
                                      wxString::FromAscii(KBMACRO_HOMEPAGE)),
                  0, wxALIGN_CENTER_VERTICAL);

        box->Add(form, 0, wxEXPAND | wxALL, 8);

        root->Add(box, 0, wxEXPAND | wxALL, 8);
    }

    // ---- 分组二：第三方库列表 ----
    {
        auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_ABOUT_GROUP_LIBS));
        auto *box = new wxStaticBoxSizer(staticBox, wxVERTICAL);

        m_libList = new wxListCtrl(staticBox, wxID_ANY,
                                   wxDefaultPosition, FromDIP(wxSize(-1, 170)),
                                   wxLC_REPORT | wxLC_SINGLE_SEL);
        m_libList->AppendColumn(TR(IDS_ABOUT_LIST_COL_NAME), wxLIST_FORMAT_LEFT, FromDIP(220));
        m_libList->AppendColumn(TR(IDS_ABOUT_LIST_COL_VERSION), wxLIST_FORMAT_LEFT, FromDIP(120));
        m_libList->AppendColumn(TR(IDS_ABOUT_LIST_COL_LICENSE), wxLIST_FORMAT_LEFT, FromDIP(180));

        long row = 0;
        for (const auto &lib: kLibs) {
            m_libList->InsertItem(row, wxString::FromAscii(lib.name));
            m_libList->SetItem(row, 1, wxString::FromAscii(lib.version));
            m_libList->SetItem(row, 2, wxString::FromAscii(lib.license));
            ++row;
        }

        box->Add(m_libList, 0, wxEXPAND | wxALL, 8);

        // 内容固定 7 行：按实际行高把列表撑到刚好放下，列表内部不出滚动条（滚动统一交给页面）
        wxRect lastRow;
        if (const long last = m_libList->GetItemCount() - 1;
            last >= 0 && m_libList->GetItemRect(last, lastRow)) {
            m_libList->SetMinSize(wxSize(-1, lastRow.GetBottom() + FromDIP(6)));
            }

        // 许可证全文不在这里展开，单独开对话框查看
        auto *btnRow = new wxBoxSizer(wxHORIZONTAL);
        m_btnLicense = new wxButton(staticBox, wxID_ANY, TR(IDS_ABOUT_BTN_LICENSE));
        m_btnLicense->Bind(wxEVT_BUTTON, &AboutPanel::onShowLicense, this);
        btnRow->AddStretchSpacer(1);
        btnRow->Add(m_btnLicense, 0, wxALL, 4);
        box->Add(btnRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);

        root->Add(box, 0, wxEXPAND | wxALL, 8);
    }

    SetSizer(root);
}

// 许可证全文放进模态对话框看：内容就是一段长文本 + 确定按钮，
// 不值得为它单开一个类。文本上千行，所以用 wxTE_RICH2 保证滚动流畅。
void AboutPanel::onShowLicense(wxCommandEvent &event) {
    (void)event;

    wxDialog dlg(this, wxID_ANY, TR(IDS_ABOUT_GROUP_LIBS), wxDefaultPosition,
                 FromDIP(wxSize(720, 600)), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

    auto *root = new wxBoxSizer(wxVERTICAL);

    auto *text = new wxTextCtrl(&dlg, wxID_ANY, BuildLicenseText(), wxDefaultPosition,
                                wxDefaultSize,
                                wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    text->SetInsertionPoint(0);   // 打开时停在开头，而不是末尾

    root->Add(text, 1, wxEXPAND | wxALL, 8);
    root->Add(dlg.CreateButtonSizer(wxOK), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    dlg.SetSizer(root);
    dlg.CentreOnParent();
    dlg.ShowModal();
}
