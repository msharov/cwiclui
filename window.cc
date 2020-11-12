// This file is part of the cwiclui project
//
// Copyright (c) 2019 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the ISC License.

#include "window.h"
#include "cwidgets.h"

namespace cwiclui {

//{{{ Widget creation --------------------------------------------------

Window::Window (const Msg::Link& l)
: Msger (l)
,_widgets()
,_widgets_area()
,_scr (l.dest)
,_size_hints()
,_focused (wid_None)
,_info()
,_scrinfo()
{
    _scr.get_info();
}

void Window::close (void)
{
    _scr.close();
    set_unused();
}

void Window::on_msger_destroyed (mrid_t mid)
{
    if (mid == _scr.dest())
	set_unused();
    Msger::on_msger_destroyed (mid);
}

void Window::create_widgets (const Widget::Layout* f, const Widget::Layout* l)
{
    if (f >= l)
	return;
    _widgets = Widget::create (this, *f);
    f = _widgets->add_widgets (next(f), l);
    assert (f == l && "Your layout array must have a single root widget containing all the others");
}

//}}}-------------------------------------------------------------------
//{{{ Focus

void Window::focus_widget (widgetid_t id)
{
    auto newf = widget_by_id (id);
    if (!newf || !newf->flag (Widget::f_CanFocus))
	return;
    _focused = id;
    _widgets->focus (id);
    draw();
}

void Window::focus_next (void)
{
    if (_widgets)
	focus_widget (_widgets->next_focus (focused_widget_id()));
}

void Window::focus_prev (void)
{
    if (_widgets)
	focus_widget (_widgets->prev_focus (focused_widget_id()));
}

// Change enabled page in a Stack widget
void Window::set_stack_selection (widgetid_t id, dim_t s)
{
    if (auto w = widget_by_id (id); w) {
	w->set_stack_selection (s);
	if (w->focused())
	    focus_next();
    }
}

//}}}-------------------------------------------------------------------
//{{{ Layout

void Window::compute_size_hints (void)
{
    Size sh (_scrinfo.size());
    if (_widgets) {
	_widgets->compute_size_hints();
	sh = _widgets->size_hints();
	// Stretch, if requested
	if (_widgets->expandables().x)
	    sh.w = _scrinfo.size().w;
	if (_widgets->expandables().y)
	    sh.h = _scrinfo.size().h;
    }
    set_size_hints (sh);
}

void Window::layout (void)
{
    compute_size_hints();
    auto oinfo (window_info());
    oinfo.set_area (Rect (size_hints()));
    _scr.open (oinfo);
}

void Window::ScreenR_resize (const Info& wi)
{
    _info = wi;
    on_resize();
    if (_widgets) {
	// Size hints must now be recomputed as they may depend on
	// screen info or on forced hints set in on_resize
	_widgets->compute_size_hints();
	_widgets->resize (widgets_area());
	// Initialize focus if needed
	if (!focused_widget_id())
	    focus_next();
    }
    draw();
}

//}}}-------------------------------------------------------------------
//{{{ Drawing

void Window::draw (void)
{
    if (flag (f_DrawInProgress)) {
	set_flag (f_DrawPending);
	if (!_scr.has_outgoing_draw())
	    return;
    }
    set_flag (f_DrawPending, false);
    set_flag (f_DrawInProgress);
    auto dl = _scr.begin_draw();
    on_draw (dl);
    if (_widgets)
	_widgets->draw (dl);
    _scr.end_draw (move(dl));
}

//}}}-------------------------------------------------------------------
//{{{ Event handling

void Window::on_event (const Event& ev)
{
    // KeyDown events go only to the focused widget
    if (ev.type() == Event::Type::KeyDown && (focused_widget_id() == wid_None || ev.src() != wid_None))
	on_key (ev.key());	// key events unused by widgets are processed in the window handler
    else if (ev.type() == Event::Type::Close)
	close();
    else if (ev.type() == Event::Type::VSync) {
	set_flag (f_DrawInProgress, false);
	if (flag (f_DrawPending))
	    draw();
    } else if (_widgets)
	_widgets->on_event (ev);
}

void Window::on_key (key_t k)
{
    if (k == Key::Tab)
	focus_next();
    else if (k == KMod::Shift+Key::Tab)
	focus_prev();
}

bool Window::dispatch (Msg& msg)
{
    return PScreenR::dispatch (this, msg)
	|| PWidgetR::dispatch (this, msg)
	|| Msger::dispatch (msg);
}

//}}}-------------------------------------------------------------------

} // namespace cwiclui
