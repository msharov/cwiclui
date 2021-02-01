// This file is part of the cwiclui project
//
// Copyright (c) 2018 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the ISC License.

#include "../window.h"
#include "../termscr.h"
using namespace cwiclui;

class UITWindow : public Window {
public:
    UITWindow (Msg::Link l);
private:
    DECLARE_WIDGET_WRITE_DRAWLIST (Drawlist);
    void on_key (key_t k) override;
    void on_resize (void) override;
    void on_selection (widgetid_t id, unsigned, unsigned) override;
private:
    enum : widgetid_t {
	wid_TabBar = wid_First,
	wid_TabStack,
	wid_Radio1,
	wid_Radio2,
	wid_Label,
	wid_Edit,
	wid_OK,
	wid_Cancel,
	wid_List,
	wid_LabelOnTab2,
	wid_Selbox2,
	wid_Checkbox2,
	wid_Progress2,
	wid_Status
    };
    static constexpr const Layout c_layout[] = {
	WL_(VBox),
	WL___(HSplitter),
	WL___(HBox),
	WL_____(Radiobox,	wid_Radio1),
	WL_____(Radiobox,	wid_Radio2),
	WL___(HSplitter),
	WL___(Stack,		wid_TabStack),
	WL_____(HBox),
	WL_______(VBox),
	WL_________(Label,	wid_Label),
	WL_________(Editbox,	wid_Edit),
	WL_________(HBox, HAlign::Center),
	WL___________(Button,	wid_OK),
	WL___________(Button,	wid_Cancel),
	WL_______(VSplitter),
	WL_______(Listbox,	wid_List),
	WL_____(VBox),
	WL_______(GroupFrame),
	WL_________(VBox),
	WL___________(HBox),
	WL_____________(Label,	wid_LabelOnTab2),
	WL_____________(Selbox,	wid_Selbox2),
	WL___________(Checkbox,	wid_Checkbox2),
	WL___________(ProgressBar, wid_Progress2),
	WL___(StatusLine,	wid_Status),
    };
    static constexpr const widgetid_t c_tab_rbox[]
	= { wid_Radio1, wid_Radio2 };
};

UITWindow::UITWindow (Msg::Link l)
: Window(l)
{
    create_widgets (c_layout);
    set_widget_text (wid_Label, "Test label above edit box");
    set_widget_text (wid_OK, "OK");
    set_widget_text (wid_Cancel, "Cancel");
    set_widget_text (wid_List, ARRAY_BLOCK ("Line one\0Line two\0 Three\0Long line four and ffff gggg dddd aaaa\0Seventy five"));
    set_widget_text (wid_Radio1, "Page 1");
    set_widget_text (wid_Radio2, "Page 2");
    set_widget_text (wid_LabelOnTab2, "Testing selections:");
    set_widget_text (wid_Selbox2, ARRAY_BLOCK("Selone\0Seltwo\0Selthree\0Selfour"));
    set_widget_text (wid_Checkbox2, "An option to enable");
    set_widget_text (wid_Status, "Status line text\0Indicators");
    set_widget_selection (wid_Radio1, true);
}

void UITWindow::on_resize (void)
{
    Window::on_resize();
    set_widgets_area (Rect (0, area().h-12, area().w, 12));
}

DEFINE_WIDGET_WRITE_DRAWLIST (UITWindow, Drawlist, dlw)
{
    dlw.move_to (area().w/2, area().h/2);
    dlw.text ("Hello world!", HAlign::Center, VAlign::Center);
}

void UITWindow::on_key (key_t k)
{
    if (k == Key::Escape)
	return close();
    else if (k == '1' || k == '2') {
	set_radiobox_selection (c_tab_rbox[k-'1'], c_tab_rbox);
	set_stack_selection (wid_TabStack, k-'1');
    } else if (k == '[' || k == ']') {
	auto pw = widget_by_id (wid_Progress2);
	if (pw) {
	    if (k == '[' && pw->selection_start() > 0)
		pw->set_selection (pw->selection_start()-1);
	    else if (k == ']' && pw->selection_start()+1 < Widget::ProgressMax)
		pw->set_selection (pw->selection_start()+1);
	}
    } else
	return Window::on_key (k);
    draw();
}

void UITWindow::on_selection (widgetid_t id, unsigned f, unsigned l)
{
    // Switch tabs on radio boxes
    auto rsel = find (c_tab_rbox, id);
    if (rsel) {
	set_radiobox_selection (id, c_tab_rbox);
	set_stack_selection (wid_TabStack, rsel-begin(c_tab_rbox));
    }
    Window::on_selection (id, f, l);
}

//{{{ TestApp ----------------------------------------------------------

class TestApp : public AppL {
public:
    static auto& instance (void) { static TestApp s_app; return s_app; }
    int run (void) { _uitwp.create_dest_as<UITWindow>(); return AppL::run(); }
private:
    TestApp (void) : AppL(),_uitwp (mrid_App) {}
private:
    Interface	_uitwp;
};

CWICLO_APP_L (TestApp, (App::Timer)(TerminalScreenWindow))
SET_WIDGET_FACTORY (Widget::default_factory)

//}}}-------------------------------------------------------------------
