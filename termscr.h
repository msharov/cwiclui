// This file is part of the cwiclui project
//
// Copyright (c) 2019 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the ISC License.

#pragma once
#include "draw.h"
#include <cwiclo/app.h>

namespace cwiclui {

class TerminalScreenWindow;

//----------------------------------------------------------------------

class TerminalScreen : public Msger {
    IMPLEMENT_INTERFACES_I (Msger,,(ITimer)(ISignal))
public:
    enum { f_UIMode = Msger::f_Last, f_CaretOn, f_InputEOF, f_Last };
    using windowid_t = WindowInfo::windowid_t;
    //{{{ Surface
    class Surface {
    public:
	struct Attr {
	    enum EAttr {
		Bold,
		Italic,
		Underline,
		Blink,
		Reverse,
		Altcharset,
		Last
	    };
	};
	struct alignas(8) Cell {
	    union Char {
		char		c[4];
		uint32_t	u;
	    public:
		constexpr void	operator= (char v)		{ c[0] = v; c[1] = 0; }
		constexpr void	operator= (char32_t v) {
		    if (v >= char32_t(Drawlist::GChar::Last)) {
			Cell r = {}; *utf8::out(&r.c.c[0]) = v; u = r.c.u;
		    } else
			operator= (char(v));
		}
		constexpr void	operator= (const Char& v)	{ u = v.u; }
		constexpr bool	operator== (const Char& v)const	{ return u == v.u; }
		constexpr bool	is_ascii (void) const		{ return c[0] >= ' ' && c[0] <= '~'; }
	    };
	public:
	    Char	c;
	    uint8_t	z;
	    uint8_t	attr;
	    icolor_t	fg,bg;
	public:
	    constexpr bool	operator== (const Cell& v) const { return *pointer_cast<uint64_t>(this) == *pointer_cast<uint64_t>(&v); }
	    constexpr bool	operator!= (const Cell& v) const { return !operator==(v); }
	};
	using value_type	= Cell;
	using cellvec_t		= vector<Cell>;
	using iterator		= cellvec_t::iterator;
	using const_iterator	= cellvec_t::const_iterator;
    public:
			Surface (void)		:_sz(),_cells() {}
	auto		begin (void)		{ return _cells.begin(); }
	auto		begin (void) const	{ return _cells.begin(); }
	auto		end (void)		{ return _cells.end(); }
	auto		end (void) const	{ return _cells.end(); }
	auto&		size (void) const	{ return _sz; }
	void		resize (const Size& sz)	{ _sz = sz; _cells.resize (_sz.w*_sz.h); }
	auto		iat (dim_t x, dim_t y)		{ return _cells.iat (y*_sz.w+x); }
	auto		iat (dim_t x, dim_t y) const	{ return _cells.iat (y*_sz.w+x); }
	auto		iat (const Point& p)		{ return iat(p.x,p.y); }
	auto		iat (const Point& p) const	{ return iat(p.x,p.y); }
	auto		iat (const Offset& o)		{ return iat(o.dx,o.dy); }
	auto		iat (const Offset& o) const	{ return iat(o.dx,o.dy); }
	static constexpr auto default_cell (void) { return Cell {{" "}, 0, 0, IColor::Default, IColor::Default }; }
	void		clear (void)		{ fill (_cells, default_cell()); }
    private:
	Size		_sz;
	vector<Cell>	_cells;
    };
    //}}}
public:
    static auto& instance (void) { static TerminalScreen s_scr; return s_scr; }
    void	reset (void);
    void	register_window (TerminalScreenWindow* w);
    void	unregister_window (const TerminalScreenWindow* w);
    Rect	position_window (const WindowInfo& winfo) const;
    void	draw_window (const TerminalScreenWindow* w);
    inline void	Signal_signal (const ISignal::Info& s);
    void	Timer_timer (fd_t fd);
    auto&	screen_info (void) const { return _scrinfo; }
protected:
		TerminalScreen (void);
		~TerminalScreen (void) override;
private:
    inline void	ui_mode (void);
    void	tt_mode (void);
    void	update_screen_size (void);
    inline void	parse_keycodes (void);
    void	caret_state (bool on);
private:
    vector<TerminalScreenWindow*> _windows;
    string	_tout;
    memblaz	_tin;
    Surface	_surface;
    ScreenInfo	_scrinfo;
    Surface::Cell _lastcell;
    Point	_curwpos;
    ITimer	_ptermi;
    ITimer	_ptermo;
};

//----------------------------------------------------------------------

class TerminalScreenWindow : public Msger {
    IMPLEMENT_INTERFACES_I (Msger, (IScreen),)
public:
    using Surface	= TerminalScreen::Surface;
    using Cell		= Surface::Cell;
    using PanelType	= Drawlist::PanelType;
    using windowid_t	= WindowInfo::windowid_t;
    enum { f_DrawInProgress = Msger::f_Last, f_DrawPending, f_Last };
public:
		TerminalScreenWindow (Msg::Link l);
		~TerminalScreenWindow (void) override;
    auto&	screen_info (void) const	{ return TerminalScreen::instance().screen_info(); }
    auto&	window_info (void) const	{ return _winfo; }
    auto	window_id (void) const		{ return msger_id(); }
    auto&	area (void) const		{ return window_info().area(); }
    auto&	viewport (void) const		{ return _viewport; }
    auto&	surface (void) const		{ return _surface; }
    auto&	caret (void) const		{ return _caret; }
    void	on_event (const Event& ev);
    void	draw (void);
    void	reset (void);
    void	on_resize (const Rect& warea);
    void	on_new_screen_info (void);
    bool	is_mapped (void) const		{ return area().w; }
private:
		friend class IScreen;
    inline void	Screen_open (const WindowInfo& wi);
    inline void	Screen_draw (const cmemlink& dl);
    void	Screen_get_info (void)		{ IScreen::Reply (creator_link()).screen_info (screen_info()); }
    void	Screen_close (void)		{ set_unused (true); }
		friend class Drawlist;
    Rect	interior_area (void) const	{ return Rect (area().size()); }
    Rect	clip_to_screen (void) const	{ return TerminalScreen::instance().position_window (window_info()); }
    icolor_t	clip_color (icolor_t c, Surface::Attr::EAttr fattr);
    auto	cell_from_char (char32_t c) const { Cell cc (_attr); cc.c = c; return cc; }
    inline void	Draw_reset (void);
    void	Draw_clear (void);
    inline void	Draw_enable (uint8_t feature);
    inline void	Draw_disable (uint8_t feature);
    inline void	Draw_move_to (const Point& p);
    inline void	Draw_move_by (const Offset& o);
    inline void	Draw_viewport (const Rect& vp);
    void	Draw_line (const Offset& o);
    inline void	Draw_draw_color (icolor_t c);
    inline void	Draw_fill_color (icolor_t c);
    inline void	Draw_char (char32_t c, HAlign ha = HAlign::Left, VAlign va = VAlign::Top);
    inline void	Draw_text (const string& t, HAlign ha = HAlign::Left, VAlign va = VAlign::Top);
    void	Draw_box (const Size& wh);
    inline void	Draw_bar (const Size& wh);
    void	Draw_char_bar (const Size& wh, char32_t c);
    void	Draw_panel (const Size& wh, PanelType t);
    void	Draw_edit_text (const string& t, uint32_t cp, HAlign ha, VAlign va);
private:
    Surface	_surface;
    Rect	_viewport;
    Point	_pos,_caret;
    Cell	_attr;
    WindowInfo	_winfo;
};

} // namespace cwiclui
