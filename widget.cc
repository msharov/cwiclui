// This file is part of the cwiclui project
//
// Copyright (c) 2019 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the ISC License.

#include "widget.h"
#include "window.h"

namespace cwiclui {

//{{{ Widget creation --------------------------------------------------

Widget::Widget (Window* w, const Layout& lay)
:_text()
,_widgets()
,_widget_areas()
,_win (w)
,_area()
,_widgets_area()
,_size_hints()
,_selection()
,_flags()
,_nexp{}
,_layinfo(lay)
{
}

const Widget* Widget::widget_by_id (widgetid_t id) const
{
    if (id == wid_None)
	return nullptr;
    if (widget_id() == id)
	return this;
    for (auto& w : _widgets)
	if (auto sw = w->widget_by_id(id); sw)
	    return sw;
    return nullptr;
}

const Widget::Layout* Widget::add_widgets (const Layout* f, const Layout* l)
{
    while (f < l && f->level() > layinfo().level()) {
	auto& w = _widgets.emplace_back (create (_win, *f));
	auto nf = next(f);
	if (nf < l && nf->level() > f->level())
	    nf = w->add_widgets (nf, l);
	f = nf;
    }
    _widget_areas.resize (_widgets.size());
    return f;
}

Widget* Widget::replace_widget (unique_ptr<Widget>&& nw)
{
    assert (nw->widget_id() && "can only replace a widget with an assigned id");
    if (!nw->widget_id())
	return nullptr;
    for (auto& w : _widgets) {
	if (w->widget_id() == nw->widget_id())
	    return (w = move (nw)).get();
	else if (auto nwp = w->replace_widget (move (nw)); nwp)
	    return nwp;
    }
    return nullptr;
}

//}}}-------------------------------------------------------------------
//{{{ Sizing and layout

auto Widget::measure_text (const string_view& text) -> Size
{
    Size sz;
    for (auto l = text.begin(), textend = text.end(); l < textend;) {
	auto lend = text.find ('\n', l);
	if (!lend)
	    lend = textend;
	sz.w = max (sz.w, lend-l);
	++sz.h;
	l = lend+1;
    }
    return sz;
}

void Widget::compute_size_hints (void)
{
    if (_widgets.empty())
	return;
    // Iterate over all widgets on the same level
    _nexp.x = 0;
    _nexp.y = 0;
    for (auto& w : _widgets) {
	// Update the subwidget size hints
	w->compute_size_hints();

	// Count expandables
	if (w->expandables().x || !w->size_hints().w)
	    ++_nexp.x;
	if (w->expandables().y || !w->size_hints().h)
	    ++_nexp.y;
    }
    // The default layout is Stack, making all widgets the same size
    Size sh;
    for (auto& w : _widgets) {
	sh.w = max (sh.w, w->size_hints().w);
	sh.h = max (sh.h, w->size_hints().h);
    }
    set_size_hints (sh);
}

void Widget::resize (const Rect& inarea)
{
    set_area (inarea);
    set_widgets_area (inarea);
    on_resize();
    for (auto wi = 0u; wi < _widgets.size(); ++wi)
	_widgets[wi]->resize (widget_area (wi));
}

void Widget::on_resize (void)
{
    fill (_widget_areas, widgets_area());
}

//}}}-------------------------------------------------------------------
//{{{ Content

PWidgetR Widget::widget_reply (void) const
    { return PWidgetR (parent_window()->msger_id(), parent_window()->msger_id()); }
void Widget::report_selection (void) const
    { widget_reply().selection (widget_id(), selection()); }
void Widget::report_modified (void) const
    { widget_reply().modified (widget_id(), text()); }

void Widget::draw (drawlist_t& dl) const
{
    on_draw (dl);
    for (auto i = 0u; i < _widgets.size(); ++i) {
	if (unlikely (layinfo().type() == Type::Stack && selection_start() != i))
	    continue; // Stack only enables one child
	_widgets[i]->draw (dl);
    }
}

//}}}-------------------------------------------------------------------
//{{{ Focus

void Widget::get_focus_neighbors_for (widgetid_t wid, FocusNeighbors& n) const
{
    if (widget_id() != wid_None && flag (f_CanFocus)) {
	n.first = min (n.first, widget_id());
	n.last = max (n.last, widget_id());
	// next is after wid, but closer than n.next
	if (widget_id() > wid && widget_id() < n.next)
	    n.next = widget_id();
	if (widget_id() < wid && widget_id() > n.prev)
	    n.prev = widget_id();
    }
    for (auto i = 0u; i < _widgets.size(); ++i) {
	if (unlikely (layinfo().type() == Type::Stack && selection_start() != i))
	    continue;
	_widgets[i]->get_focus_neighbors_for (wid, n);
    }
}

auto Widget::get_focus_neighbors_for (widgetid_t wid) const -> FocusNeighbors
{
    FocusNeighbors n = { wid_Last, wid_None, wid_Last, wid_None };
    get_focus_neighbors_for (wid, n);
    // No widgets - n.first should be wid_None
    if (n.first == wid_Last)
	n.first = wid_None;
    // Wrap around next/prev when at ends
    if (n.prev == wid_None)
	n.prev = n.last;
    if (n.next == wid_Last)
	n.next = n.first;
    return n;
}

void Widget::focus (widgetid_t id)
{
    // Focus is given to widget id and all its parent containers.
    bool f = (widget_id() == id && flag (f_CanFocus));
    for (auto i = 0u; i < _widgets.size(); ++i) {
	_widgets[i]->focus (id);
	if (_widgets[i]->flag (f_Focused))
	    f = true;
    }
    set_flag (f_Focused, f);
}

//}}}-------------------------------------------------------------------
//{{{ Event handling

void Widget::on_event (const Event& ev)
{
    // Key events go only to the focused widget
    if (ev.type() == Event::Type::KeyDown || ev.type() == Event::Type::KeyUp) {
	// on_key handlers are called in leaves and in focusable containers
	if (flag (f_CanFocus))	// only call if the widget is able to handle it
	    on_key (ev.key());
	// Key events that the focused widget does not use are forwarded
	// back here with the source widget id set to that widget.
	auto focusw = find_if (_widgets, [](auto& w) { return w->focused(); });
	if (focusw)
	    (*focusw)->on_event (ev);
    } else for (auto& w : _widgets)
	w->on_event (ev);
}

void Widget::on_key (key_t k)
{
    // At the end of the focus path, return unused key events to parent window.
    // Retuning directly to on_event to avoid sequencing problems with returned
    // and directly handled key events.
    if (widget_id() && _widgets.empty() && flag (f_CanFocus))
	_win->on_event (Event (Event::Type::KeyDown, k, 0, widget_id()));
}

//}}}-------------------------------------------------------------------

} // namespace cwiclui
