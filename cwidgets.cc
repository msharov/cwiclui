// This file is part of the cwiclui project
//
// Copyright (c) 2019 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the ISC License.

#include "cwidgets.h"
#include <ctype.h>

namespace cwiclui {

//{{{ Label ------------------------------------------------------------

void Label::on_set_text (void)
{
    Widget::on_set_text();
    set_size_hints (measure_text (text()));
}

DEFINE_WIDGET_WRITE_DRAWLIST (Label, Drawlist, drw)
    { drw.text (text()); }

//}}}-------------------------------------------------------------------
//{{{ Button

void Button::on_set_text (void)
{
    Widget::on_set_text();
    auto th = measure_text (text());
    set_size_hints (strlen("[ ") + th.w + strlen (" ]"), th.h);
}

DEFINE_WIDGET_WRITE_DRAWLIST (Button, Drawlist, drw)
{
    if (focused())
	drw.enable (Drawlist::Feature::ReverseColors);
    drw.panel (area().size(), PanelType::Button);
    if (!text().empty()) {
	drw.enable (Drawlist::Feature::BoldText);
	drw.text (text()[0]);
	drw.disable (Drawlist::Feature::BoldText);
	drw.text (text().iat(1));
    }
    if (focused())
	drw.disable (Drawlist::Feature::ReverseColors);
}

//}}}-------------------------------------------------------------------
//{{{ Checkbox

void Checkbox::on_set_text (void)
{
    Widget::on_set_text();
    auto th = measure_text (text());
    set_size_hints (strlen("[x]  ") + th.w, th.h);
}

void Checkbox::on_key (key_t k)
{
    if (k != Key::Enter && k != Key::Space)
	return Widget::on_key (k);
    if (bool on = !selection_start() || layinfo().type() == Type::Radiobox; selection_start() != on) {
	set_selection (on);
	report_selection();
    }
}

DEFINE_WIDGET_WRITE_DRAWLIST (Checkbox, Drawlist, drw)
{
    if (focused())
	drw.panel (area().size(), PanelType::Selection);
    static const PanelType c_pt[2][2] = {
	{ PanelType::CheckboxOn, PanelType::Checkbox },
	{ PanelType::RadioOn, PanelType::Radio }
    };
    drw.panel (area().size(), c_pt[layinfo().type() == Type::Radiobox][!selection_start()]);
    drw.text (text(), HAlign::Left, VAlign::Center);
}

//}}}-------------------------------------------------------------------
//{{{ Editbox

Editbox::Editbox (Window* w, const Layout& lay)
: Widget(w,lay)
,_cpos()
,_fc()
{
    set_flag (f_CanFocus);
    // An edit box must be one line only
    set_size_hints (0, 1);
}

void Editbox::on_resize (void)
{
    Widget::on_resize();
    posclip();
}

void Editbox::posclip (void)
{
    if (_cpos < _fc) {	// cursor past left edge
	_fc = _cpos;
	if (_fc > 0)
	    --_fc;	// adjust for clip indicator
    }
    if (area().w) {
	if (_fc+area().w < _cpos+2)	// cursor past right edge +2 for > indicator
	    _fc = _cpos+2-area().w;
	while (_fc && _fc-1u+area().w > text().size())	// if text fits in box, no scroll
	    --_fc;
    }
}

void Editbox::on_set_text (void)
{
    Widget::on_set_text();
    _cpos = text().size();
    _fc = 0;
    posclip();
}

void Editbox::on_key (key_t k)
{
    auto oldcpos = _cpos;
    if (k == Key::Left && _cpos)
	--_cpos;
    else if (k == Key::Right && _cpos < coord_t (text().size()))
	++_cpos;
    else if (k == Key::Home)
	_cpos = 0;
    else if (k == Key::End)
	_cpos = text().size();
    else {
	if (k == Key::Backspace && _cpos)
	    textw().erase (text().iat(--_cpos));
	else if (k == Key::Delete && _cpos < coord_t (text().size()))
	    textw().erase (text().iat(_cpos));
	else if (k >= ' ' && k <= '~')
	    textw().insert (text().iat(_cpos++), char(k));
	else
	    return Widget::on_key (k);
	report_modified();
    }
    posclip();
    if (oldcpos != _cpos) {
	set_selection (_cpos);
	report_selection();
    }
}

DEFINE_WIDGET_WRITE_DRAWLIST (Editbox, Drawlist, drw)
{
    drw.panel (area().size(), PanelType::Editbox);
    if (focused())
	drw.edit_text (text().iat(_fc), text().size()-_fc, _cpos-_fc);
    else
	drw.text (text().iat(_fc), text().size()-_fc);
    if (_fc) {
	drw.move_to (0, 0);
	drw.text ('<');
    }
    if (_fc+area().w <= text().size()) {
	drw.move_to (area().w-1, 0);
	drw.text ('>');
    }
    drw.move_to (_cpos-_fc, 0);
}

//}}}-------------------------------------------------------------------
//{{{ Selbox

void Selbox::on_set_text (void)
{
    Widget::on_set_text();
    Size szh;
    for (zstr::cii li (text().c_str(), text().size()); li; ++szh.h) {
	auto lt = *li;
	auto tlen = *++li - lt - 1;
	szh.w = max (szh.w, tlen);
    }
    set_n (szh.h);
    set_size_hints (strlen("< ") + szh.w + strlen(" >"), 1);
}

void Selbox::on_key (key_t k)
{
    if ((k == 'h' || k == Key::Left) && selection_start())
	set_selection (selection_start()-1);
    else if ((k == 'l' || k == Key::Right) && selection_start()+1 < _n)
	set_selection (selection_start()+1);
    else
	return Widget::on_key (k);
    report_selection();
}

DEFINE_WIDGET_WRITE_DRAWLIST (Selbox, Drawlist, drw)
{
    if (focused())
	drw.panel (area().size(), PanelType::Selection);
    drw.panel (area().size(), PanelType::Selbox);
    if (selection_start() < _n) {
	auto stxt = zstr::at (selection_start(), text());
	drw.move_to (area().w/2, area().h/2);
	drw.text (stxt, HAlign::Center, VAlign::Center);
    }
    if (selection_start() > 0) {
	drw.move_to (0, area().h/2);
	drw.text ('<', HAlign::Left, VAlign::Center);
    }
    if (selection_start()+1 < _n) {
	drw.move_to (area().w, area().h/2);
	drw.text ('>', HAlign::Right, VAlign::Center);
    }
}

//}}}-------------------------------------------------------------------
//{{{ Listbox

void Listbox::on_set_text (void)
{
    Widget::on_set_text();
    Size szh;
    for (zstr::cii li (text().c_str(), text().size()); li; ++szh.h) {
	auto lt = *li;
	auto tlen = *++li - lt - 1;
	szh.w = max (szh.w, tlen);
    }
    set_n (szh.h);
    set_size_hints (szh);
}

void Listbox::on_key (key_t k)
{
    if ((k == 'k' || k == Key::Up) && selection_start())
	set_selection (selection_start()-1);
    else if ((k == 'j' || k == Key::Down) && selection_start()+1 < _n)
	set_selection (selection_start()+1);
    else
	return Widget::on_key (k);
    report_selection();
}

DEFINE_WIDGET_WRITE_DRAWLIST (Listbox, Drawlist, drw)
{
    if (area().w < 1)
	return;
    drw.panel (area().size(), PanelType::Listbox);
    coord_t y = 0;
    for (zstr::cii li (text().c_str(), text().size()); li; ++y) {
	auto lt = *li;
	auto tlen = *++li - lt - 1;
	if (y < _top)
	    continue;
	drw.move_to (0, y-_top);
	if (y == selection_start() && focused())
	    drw.panel (area().w, 1, PanelType::Selection);
	auto vislen = tlen;
	if (tlen > area().w)
	    vislen = area().w-1;
	drw.text (lt, vislen);
	if (tlen > area().w)
	    drw.text ('>');
    }
}

//}}}-------------------------------------------------------------------
//{{{ HSplitter

DEFINE_WIDGET_WRITE_DRAWLIST (HSplitter, Drawlist, drw)
    { drw.hline (area().w); }

//}}}-------------------------------------------------------------------
//{{{ VSplitter

DEFINE_WIDGET_WRITE_DRAWLIST (VSplitter, Drawlist, drw)
    { drw.vline (area().h); }

//}}}-------------------------------------------------------------------
//{{{ GroupFrame

DEFINE_WIDGET_WRITE_DRAWLIST (GroupFrame, Drawlist, drw)
{
    drw.box (area().size());
    auto tsz = min (text().size(), area().w-2);
    if (tsz > 0) {
	drw.move_to ((area().w-tsz)/2u-1, 0);
	drw.bar (tsz+2, 1);
	drw.move_by (1, 0);
	drw.text (text().c_str(), tsz);
    }
}

//}}}-------------------------------------------------------------------
//{{{ StatusLine

DEFINE_WIDGET_WRITE_DRAWLIST (StatusLine, Drawlist, drw)
{
    drw.panel (area().size(), PanelType::Statusbar);
    drw.move_by (1, 0);
    drw.text (text());
    if (is_modified()) {
	drw.move_to (area().w-strlen(" *"), 0);
	drw.text (" *");
    }
}

//}}}-------------------------------------------------------------------
//{{{ ProgressBar

DEFINE_WIDGET_WRITE_DRAWLIST (ProgressBar, Drawlist, drw)
{
    drw.panel (area().size(), PanelType::Progress);
    drw.panel (area().w*selection_start()/Widget::ProgressMax, area().h, PanelType::ProgressOn);
}

//}}}-------------------------------------------------------------------
//{{{ Default widget factory

BEGIN_WIDGET_FACTORY (Widget::default_factory)
    WIDGET_TYPE_IMPLEMENT (GroupFrame, GroupFrame)
    WIDGET_TYPE_IMPLEMENT (Label, Label)
    WIDGET_TYPE_IMPLEMENT (Button, Button)
    WIDGET_TYPE_IMPLEMENT (Checkbox, Checkbox)
    WIDGET_TYPE_IMPLEMENT (Radiobox, Checkbox)
    WIDGET_TYPE_IMPLEMENT (Editbox, Editbox)
    WIDGET_TYPE_IMPLEMENT (Selbox, Selbox)
    WIDGET_TYPE_IMPLEMENT (Listbox, Listbox)
    WIDGET_TYPE_IMPLEMENT (HSplitter, HSplitter)
    WIDGET_TYPE_IMPLEMENT (VSplitter, VSplitter)
    WIDGET_TYPE_IMPLEMENT (StatusLine, StatusLine)
    WIDGET_TYPE_IMPLEMENT (ProgressBar, ProgressBar)
END_WIDGET_FACTORY

//}}}-------------------------------------------------------------------

} // namespace cwiclui
