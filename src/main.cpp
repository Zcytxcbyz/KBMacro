#include <wx/wx.h>
#include <wx/notebook.h>

class MainFrame : public wxFrame
{
public:
    MainFrame() : wxFrame(nullptr, wxID_ANY, wxString::FromUTF8("KBMacro"), wxDefaultPosition, wxSize(900, 700))
    {
        wxNotebook* notebook = new wxNotebook(this, wxID_ANY);

        // ---- 标签页1：鼠标连点器 ----
        wxPanel* page1 = new wxPanel(notebook, wxID_ANY);
        wxStaticText* text1 = new wxStaticText(page1, wxID_ANY,
            wxString::FromUTF8("鼠标连点器功能将在此实现"),
            wxPoint(20, 20));
        notebook->AddPage(page1, wxString::FromUTF8("鼠标连点器"));

        // ---- 标签页2：键盘模拟器 ----
        wxPanel* page2 = new wxPanel(notebook, wxID_ANY);
        wxStaticText* text2 = new wxStaticText(page2, wxID_ANY,
            wxString::FromUTF8("键盘模拟器功能将在此实现"),
            wxPoint(20, 20));
        notebook->AddPage(page2, wxString::FromUTF8("键盘模拟器"));

        // ---- 标签页3：录制与编排 ----
        wxPanel* page3 = new wxPanel(notebook, wxID_ANY);
        wxStaticText* text3 = new wxStaticText(page3, wxID_ANY,
            wxString::FromUTF8("录制、编排、回放功能将在此实现"),
            wxPoint(20, 20));
        notebook->AddPage(page3, wxString::FromUTF8("录制与编排"));

        // ---- 标签页4：设置 ----
        wxPanel* page4 = new wxPanel(notebook, wxID_ANY);
        wxStaticText* text4 = new wxStaticText(page4, wxID_ANY,
            wxString::FromUTF8("语言切换、热键配置、配置文件等设置将在此实现"),
            wxPoint(20, 20));
        notebook->AddPage(page4, wxString::FromUTF8("设置"));

        this->SetBackgroundColour(wxColour(240, 240, 240));
    }
};

class KBMacroApp : public wxApp
{
public:
    virtual bool OnInit() override
    {
        MainFrame* frame = new MainFrame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(KBMacroApp);
