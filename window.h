// This file is part of the cwiclui project
//
// Copyright (c) 2019 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the ISC License.

#pragma once
#include "widget.h"

namespace cwiclui {

class Widget;

class Window : public Msger {
    IMPLEMENT_INTERFACES_I (Msger,,(IWidget)(IScreen))
public:
    using key_t		= Event::key_t;
    using Layout	= Widget::Layout;
    using Info		= WindowInfo;
    using drawlist_t	= IScreen::drawlist_t;
    using windowid_t	= WindowInfo::windowid_t;
    enum { f_DrawInProgress = Msger::f_Last, f_DrawPending, f_Last };
public:
    explicit		Window (Msg::Link l);
    void		draw (void);
    virtual void	on_event (const Event& ev);
    virtual void	on_modified (widgetid_t, const string_view&) { draw(); }
    virtual void	on_selection (widgetid_t, unsigned, unsigned) { draw(); }
    virtual void	on_key (key_t key);
    void		close (void);

    void		Widget_event (const Event& ev)	{ on_event (ev); }
    void		Widget_modified (widgetid_t wid, const string_view& t)
			    { on_modified (wid, t); }
    void		Widget_selection (widgetid_t wid, const Size& sel)
			    { on_selection (wid, sel.w, sel.h); }
    void		Screen_event (const Event& ev)	{ on_event (ev); }
    void		Screen_expose (void)		{ draw(); }
    void		Screen_resize (const Info& wi);
    void		Screen_screen_info (const ScreenInfo& scrinfo)
			    { _scrinfo = scrinfo; layout(); }

    auto		window_id (void) const		{ return _scr.dest(); }
    auto&		window_info (void)		{ return _info; }
    auto&		window_info (void) const	{ return _info; }
    auto&		screen_info (void) const	{ return _scrinfo; }
    auto&		area (void) const		{ return window_info().area(); }
    auto&		widgets_area (void) const	{ return _widgets_area; }
    auto&		size_hints (void) const		{ return _size_hints; }
protected:
    void		on_msger_destroyed (mrid_t mid) override;
    virtual void	compute_size_hints (void);
    virtual void	on_resize (void)		{ set_widgets_area (Rect (area().size())); }
    void		layout (void);
    void		create_widgets (const Layout* f, const Layout* l);
    template <unsigned N>
    void		create_widgets (const Layout (&l)[N])
			    { create_widgets (begin(l), end(l)); }
    auto		replace_widget (unique_ptr<Widget>&& w)	{ return _widgets->replace_widget (move(w)); }
    void		destroy_widgets (void)			{ _widgets.reset(); }
    auto		widget_by_id (widgetid_t id) const	{ return _widgets->widget_by_id(id); }
    auto		widget_by_id (widgetid_t id)		{ return _widgets->widget_by_id(id); }
    void		set_widgets_area (const Rect& wa)	{ _widgets_area = wa; }
    void		set_widget_text (widgetid_t id, const char* t)			{ if (auto w = widget_by_id (id); w) w->set_text (t); }
    void		set_widget_text (widgetid_t id, const string& t)		{ if (auto w = widget_by_id (id); w) w->set_text (t); }
    void		set_widget_text (widgetid_t id, const char* t, unsigned n)	{ if (auto w = widget_by_id (id); w) w->set_text (t,n); }
    void		set_widget_size_hints (widgetid_t id, const Size& s)		{ if (auto w = widget_by_id (id); w) w->set_forced_size_hints (s); }
    void		set_widget_size_hints (widgetid_t id, dim_t ww, dim_t hh)	{ if (auto w = widget_by_id (id); w) w->set_forced_size_hints (ww,hh); }
    void		set_widget_selection (widgetid_t id, const Size& s)		{ if (auto w = widget_by_id (id); w) w->set_selection (s); }
    void		set_widget_selection (widgetid_t id, dim_t f, dim_t t)		{ if (auto w = widget_by_id (id); w) w->set_selection (f,t); }
    void		set_widget_selection (widgetid_t id, dim_t f)			{ if (auto w = widget_by_id (id); w) w->set_selection (f); }
    void		set_radiobox_selection (widgetid_t id, const widgetid_t* rb, unsigned nrb)
			    { for (auto i = 0u; i < nrb; ++i) set_widget_selection (rb[i], rb[i] == id); }
    template <unsigned N>
    inline void		set_radiobox_selection (widgetid_t id, const widgetid_t (&rb)[N])
			    { set_radiobox_selection (id, ARRAY_BLOCK(rb)); }
    void		set_stack_selection (widgetid_t id, dim_t s);
    void		set_size_hints (const Size& sh)		{ _size_hints = sh; }
    void		set_size_hints (dim_t w, dim_t h)	{ set_size_hints (Size(w,h)); }
    void		focus_widget (widgetid_t id);
    auto		focused_widget_id (void) const		{ return _focused; }
    const Widget*	focused_widget (void) const
			    { return widget_by_id (focused_widget_id()); }
    Widget*		focused_widget (void)
			    { return UNCONST_MEMBER_FN (focused_widget); }
    void		focus_next (void);
    void		focus_prev (void);
private:
    virtual void	on_draw (drawlist_t&) const {}
private:
    unique_ptr<Widget>	_widgets;
    Rect		_widgets_area;
    IScreen		_scr;
    Size		_size_hints;
    widgetid_t		_focused;
    Info		_info;
    ScreenInfo		_scrinfo;
};

} // namespace cwiclui
