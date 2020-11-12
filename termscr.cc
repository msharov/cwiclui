// This file is part of the cwiclui project
//
// Copyright (c) 2019 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the ISC License.

#include "termscr.h"
#include <signal.h>
#if __has_include(<termio.h>)
    #include <termio.h>
#else
    #include <termios.h>
#endif

namespace cwiclui {

//{{{ Statics ----------------------------------------------------------

// termios settings before TerminalScreen activation
static struct termios s_old_termios = {};

//}}}-------------------------------------------------------------------
//{{{ T_ terminal code strings

#define T_ESC			"\033"
#define T_CSI			T_ESC "["
#define T_ALTSCREEN_ON		T_CSI "?1049h"
#define T_ALTSCREEN_OFF		T_CSI "?1049l"
#define T_ALTCHARSET_ENABLE	T_ESC "(B" T_ESC ")0"
#define T_ALTCHARSET_DISABLE	T_ESC "(B" T_ESC ")B"
#define T_SET_DEFAULT_ATTRS	"\017" T_CSI "0;39;49m"
#define T_CARET_ON		T_CSI "?25h"
#define T_CARET_OFF		T_CSI "?25l"
#define T_SKIP_N		T_CSI "%uC"
#define T_MOVE_TO_ORIGIN	T_CSI "H"
#define T_MOVE_TO		T_CSI "%u;%uH"
#define T_CLEAR_TO_BOTTOM	T_CSI "J"
#define T_CLEAR_SCREEN		T_MOVE_TO_ORIGIN T_CLEAR_TO_BOTTOM

//}}}-------------------------------------------------------------------
//{{{ TerminalScreen

TerminalScreen::TerminalScreen (void)
: Msger()
,_windows()
,_tout()
,_tin()
,_surface()
,_scrinfo()
,_lastcell (Surface::default_cell())
,_curwpos()
,_ptermi (msger_id())
,_ptermo (msger_id())
{
    _tin.reserve (256);
    if (auto term = getenv("TERM"); term) {
	if (!strncmp (term, "linux", strlen("linux")))
	    _scrinfo.set_depth (3);
	else if (!strstr (term, "256"))
	    _scrinfo.set_depth (4);
    }
}

TerminalScreen::~TerminalScreen (void)
{
    _windows.clear();
    tt_mode();
}

bool TerminalScreen::dispatch (Msg& msg)
{
    return PTimerR::dispatch (this, msg)
	|| PSignal::dispatch (this, msg)
	|| Msger::dispatch (msg);
}

//}}}-------------------------------------------------------------------
//{{{ TerminalScreen state management

void TerminalScreen::ui_mode (void)
{
    if (flag (f_UIMode))
	return;
    signal (SIGTSTP, SIG_IGN);		// Disable suspend signal in addition to the key.
    if (0 == tcgetattr (STDIN_FILENO, &s_old_termios)) {
	auto tios = s_old_termios;
	tios.c_lflag &= ~(ICANON| ECHO);// No by-line buffering, no echo.
	tios.c_iflag &= ~(IXON| IXOFF);	// No ^s scroll lock
	tios.c_cc[VMIN] = 1;		// Read at least 1 character on each read().
	tios.c_cc[VTIME] = 0;		// Disable time-based preprocessing (Esc sequences)
	tios.c_cc[VQUIT] = 0xff;	// Disable ^\. Root window will handle.
	tios.c_cc[VSUSP] = 0xff;	// Disable ^z. Suspends in UI mode result in garbage.
	tcsetattr (STDIN_FILENO, TCSAFLUSH, &tios);
    }
    _tout =
	T_ALTSCREEN_ON
	T_ALTCHARSET_ENABLE
	T_CARET_ON;
    for (int fd = STDIN_FILENO; fd <= STDOUT_FILENO; ++fd)
	PTimer::make_nonblocking (fd);
    set_flag (f_CaretOn);
    set_flag (f_UIMode);
    update_screen_size();
}

void TerminalScreen::tt_mode (void)
{
    if (!flag (f_UIMode))
	return;
    _ptermi.stop();
    if (!_tout.empty())
	_ptermo.stop();
    reset();
    caret_state (true);
    for (int fd = STDIN_FILENO; fd <= STDOUT_FILENO; ++fd)
	PTimer::make_blocking (fd);
    _tout +=
	T_ALTCHARSET_DISABLE
	T_ALTSCREEN_OFF;
    printf ("%s", _tout.c_str());
    _tout.clear();
    if (s_old_termios.c_lflag)	// don't set termios if it wasn't read successfully in ui_mode
	tcsetattr (STDIN_FILENO, TCSAFLUSH, &s_old_termios);
    signal (SIGTSTP, SIG_DFL);	// reenable Ctrl-Z
    set_flag (f_UIMode, false);
}

void TerminalScreen::reset (void)
{
    _curwpos = Point();
    _lastcell = Surface::default_cell();
    _surface.clear();
    _tout +=
	T_SET_DEFAULT_ATTRS
	T_CLEAR_SCREEN;
}

void TerminalScreen::caret_state (bool on)
{
    if (flag (f_CaretOn) != on) {
	set_flag (f_CaretOn, on);
	_tout += on ? T_CARET_ON : T_CARET_OFF;
    }
}

void TerminalScreen::update_screen_size (void)
{
    Size nsz (80, 24);
    if (struct winsize ws; !ioctl (STDIN_FILENO, TIOCGWINSZ, &ws)) {
	nsz.w = ws.ws_col;
	nsz.h = ws.ws_row;
    } else {
	if (auto e = getenv("COLUMNS"); e)
	    if (auto w = atoi(e); w > 0)
		nsz.w = w;
	if (auto e = getenv("LINES"); e)
	    if (auto h = atoi(e); h > 0)
		nsz.h = h;
    }
    if (_scrinfo.size() != nsz) {
	_scrinfo.set_size (nsz);
	_surface.resize (nsz);
	for (auto& w : _windows)
	    w->on_new_screen_info();
    }
    reset();
}

void TerminalScreen::Signal_signal (const PSignal::Info& si)
{
    if (si.sig == SIGWINCH)
	update_screen_size();
}

//}}}-------------------------------------------------------------------
//{{{ TerminalScreen window management

void TerminalScreen::register_window (TerminalScreenWindow* w)
{
    assert (w);
    assert (!find (_windows, w));
    ui_mode();
    _windows.push_back (w);
}

void TerminalScreen::unregister_window (const TerminalScreenWindow* w)
{
    remove (_windows, w);
    if (_windows.empty())
	tt_mode();
    else {
	// Redraw all windows to erase the one that was destroyed
	reset();	// need to reset in case there was no window underneath
	for (auto& bw : _windows)
	    draw_window (bw);
    }
}

Rect TerminalScreen::position_window (const WindowInfo& winfo) const
{
    // Find parent window to position in; if none, use screen
    auto parwin = find_if (_windows, [&](auto& w){ return w->window_id() == winfo.parent(); });
    Rect scrarea (screen_info().size());
    auto pararea = parwin ? (*parwin)->area() : scrarea;

    // Window area is specified in parent coordinates
    auto warea = winfo.area();
    warea.move_by (pararea.pos().as_offset());

    // Maximize window if empty
    if (!warea.w)
	warea.w = pararea.w;
    if (!warea.h)
	warea.h = pararea.h;

    // Center dialogs and toplevel windows
    if (!parwin || winfo.type() == WindowInfo::Type::Dialog) {
	warea.x = pararea.x + (pararea.w - warea.w)/2;
	warea.y = pararea.y + pararea.h - warea.h;
    }

    // Clip the window to the screen area
    return scrarea.clip (warea);
}

//}}}-------------------------------------------------------------------
//{{{ TerminalScreen draw_window

void TerminalScreen::draw_window (const TerminalScreenWindow* w)
{
    assert (flag (f_UIMode));
    assert (Rect(screen_info().size()).clip (w->area()) == w->area() && "you must use position_window to set window area");
    if (w->area().empty())
	return;
    auto wpos (w->area().pos());
    auto oci = _surface.iat (wpos);
    size_t oskip = screen_info().size().w - w->area().w;

    foreach (ici, w->surface()) {
	assert (oci < _surface.end() && "position_window must clip each window to screen area");
	// Skip writing if nothing changed
	if (*oci != *ici) {
	    auto curcell = *ici;

	    // Convert GChars to ACS chars
	    static constexpr const char c_acs_sym[] = "+,-.0`afghijklmnopqrstuvwxyz{|}~";
	    static_assert (size(c_acs_sym)-1 == uint8_t(Drawlist::GChar::N), "c_acs_sym must parallel Drawlist::GChar");
	    if (unsigned acsi = uint8_t(curcell.c.c[0]) - uint8_t(Drawlist::GChar::First); acsi < size(c_acs_sym)) {
		curcell.c = c_acs_sym [acsi];	// ACS char, substitute
		set_bit (curcell.attr, Surface::Attr::Altcharset);
	    } else if (uint8_t(curcell.c.c[0]) < ' ') {
		curcell.c = '?';		// unprintable character
		set_bit (curcell.attr, Surface::Attr::Blink);
		curcell.fg = IColor::Gray;
		curcell.bg = IColor::Red;
	    }

	    // Collect numbers for the term sgr command
	    uint8_t sgr[11], nsgr = 0;

	    // {Off,On} sequences for attributes
	    static const uint8_t c_attr_tseq[][2] = {
		{22,1},	// Bold
		{23,3},	// Italic
		{24,4},	// Underline
		{25,5},	// Blink
		{27,7}	// Reverse
	    };
	    auto chattr = _lastcell.attr ^ curcell.attr;
	    if (chattr)
		for (auto a = 0u; a < size(c_attr_tseq); ++a)
		    if (get_bit (chattr, a))
			sgr[nsgr++] = c_attr_tseq[a][get_bit(curcell.attr,a)];

	    // Set colors
	    if (curcell.bg != _lastcell.bg) {
		if (curcell.bg < 8)
		    sgr[nsgr] = 40+curcell.bg;
		else if (curcell.bg < 16)
		    sgr[nsgr] = 100+curcell.bg;
		else if (curcell.bg == IColor::Default)
		    sgr[nsgr] = 49;
		else {
		    sgr[nsgr++] = 48;
		    sgr[nsgr++] = 5;
		    sgr[nsgr] = curcell.bg;
		}
		++nsgr;
	    }
	    if (curcell.fg != _lastcell.fg) {
		if (curcell.fg < 8)
		    sgr[nsgr] = 30+curcell.fg;
		else if (curcell.fg < 16)
		    sgr[nsgr] = 90+curcell.fg;
		else if (curcell.fg == IColor::Default)
		    sgr[nsgr] = 39;
		else {
		    sgr[nsgr++] = 38;
		    sgr[nsgr++] = 5;
		    sgr[nsgr] = curcell.fg;
		}
		++nsgr;
	    }

	    // Skip over unchanged content on the screen
	    if (wpos != _curwpos) {
		auto dpos = wpos - _curwpos;
		if (!dpos.dy && dpos.dx > 0) {
		    if (dpos.dx < 5 && ici-dpos.dx >= w->surface().begin()) {
			// Rewrite unchanged chars if only a few and no attributes changed
			for (; dpos.dx; --dpos.dx) {
			    auto uici = ici-dpos.dx;
			    if (uici->attr != _lastcell.attr || !uici->c.is_ascii())
				break;
			    _tout += uici->c.c[0];
			}
		    }
		    if (dpos.dx > 0)
			_tout.appendf (T_SKIP_N, dpos.dx);
		} else
		    _tout.appendf (T_MOVE_TO, wpos.y+1, wpos.x+1);
		_curwpos = wpos;
	    }

	    // Write the sgr sequence
	    if (nsgr > 0) {
		_tout += T_CSI;
		for (auto i = 0u; i < nsgr; ++i)
		    _tout.appendf ("%hhu;", sgr[i]);
		_tout.back() = 'm';
	    }

	    // Enable (14) or disable (15) altcharset if changed
	    if (get_bit (chattr, Surface::Attr::Altcharset))
		_tout += char(15-get_bit (curcell.attr, Surface::Attr::Altcharset));

	    // Write the character
	    _tout += curcell.c.c;

	    // Adjust tracking variables
	    if (++_curwpos.x > _scrinfo.size().w) {
		_curwpos.x = 0;
		if (_curwpos.y < _scrinfo.size().h)
		    ++_curwpos.y;
	    }
	    _lastcell = curcell;
	    *oci = *ici;
	}

	// Advance output and position
	++oci;
	if (++wpos.x >= w->area().x + w->area().w) {
	    wpos.x = w->area().x;
	    ++wpos.y;
	    oci += oskip;
	}
    }
    // Turn on the caret, if set in window
    auto caretpos = w->caret() + w->area().pos();
    bool careton = w->area().contains (caretpos);
    caret_state (careton);
    if (careton && _curwpos != caretpos) {
	_curwpos = caretpos;
	_tout.appendf (T_MOVE_TO, caretpos.y+1, caretpos.x+1);
    }

    // Write the buffer
    _ptermo.wait_write (STDOUT_FILENO);
}

//}}}-------------------------------------------------------------------
//{{{ TerminalScreen input processing

void TerminalScreen::TimerR_timer (PTimerR::fd_t)
{
    static constexpr const Event c_vsync_event (Event::Type::VSync, 0, 60);
    static constexpr const Event c_close_event (Event::Type::Close);

    if (!flag (f_UIMode))
	return;

    while (!_tout.empty()) {
	auto bw = write (STDOUT_FILENO, _tout.data(), _tout.size());
	if (bw == 0)
	    return error ("terminal closed");
	else if (bw < 0) {
	    if (errno == EINTR)
		continue;
	    if (errno == EAGAIN) {
		_ptermo.wait_write (STDOUT_FILENO);
		break;
	    }
	    return error_libc ("write");
	}
	_tout.erase (_tout.begin(), bw);
    }
    if (_tout.empty())
	for (auto& w : _windows)
	    if (w->flag (TerminalScreenWindow::f_DrawInProgress) || w->flag (TerminalScreenWindow::f_DrawPending))
		_windows.back()->on_event (c_vsync_event);

    while (_tin.capacity() > _tin.size()) {
	auto br = read (STDIN_FILENO, _tin.end(), _tin.capacity()-_tin.size());
	if (br == 0) {
	    if (!flag (f_InputEOF)) {
		set_flag (f_InputEOF);
		eachfor (w, _windows) // close all windows
		    (*w)->on_event (c_close_event);
	    }
	    break;
	} else if (br < 0) {
	    if (errno == EINTR)
		continue;
	    if (errno == EAGAIN)
		break;
	    return error_libc ("read");
	}
	_tin.shrink (_tin.size()+br);
    }
    parse_keycodes();
}

struct EscSeq { char s[5]; unsigned char k; };

static const EscSeq* match_escape_sequence (const char* s, size_t n)
{
    //{{{2 c_seq - key escape sequence mapping
    static constexpr const EscSeq c_seq[] = {
	{ "O2P",	Key::Print	},
	{ "O2Q",	Key::Break	},
	{ "OA",		Key::Up		},
	{ "OB",		Key::Down	},
	{ "OC",		Key::Right	},
	{ "OD",		Key::Left	},
	{ "OE",		Key::Center	},
	{ "OF",		Key::End	},
	{ "OH",		Key::Home	},
	{ "OP",		Key::F1		},
	{ "OQ",		Key::F2		},
	{ "OR",		Key::F3		},
	{ "OS",		Key::F4		},
	{ "Ou",		Key::Center	},
	{ "[11~",	Key::F1		},
	{ "[12~",	Key::F2		},
	{ "[13~",	Key::F3		},
	{ "[14~",	Key::F4		},
	{ "[15~",	Key::F5		},
	{ "[17~",	Key::F6		},
	{ "[18~",	Key::F7		},
	{ "[19~",	Key::F8		},
	{ "[1~",	Key::Home	},
	{ "[20~",	Key::F9		},
	{ "[21~",	Key::F10	},
	{ "[23~",	Key::F11	},
	{ "[24~",	Key::F12	},
	{ "[2~",	Key::Insert	},
	{ "[3~",	Key::Delete	},
	{ "[4~",	Key::End	},
	{ "[5~",	Key::PageUp	},
	{ "[6~",	Key::PageDown	},
	{ "[7~",	Key::Home	},
	{ "[8~",	Key::End	},
	{ "[<",		Key::Wheel	},
	{ "[A",		Key::Up		},
	{ "[B",		Key::Down	},
	{ "[C",		Key::Right	},
	{ "[D",		Key::Left	},
	{ "[E",		Key::Center	},
	{ "[F",		Key::End	},
	{ "[G",		Key::Center	},
	{ "[H",		Key::Home	},
	{ "[M",		Key::Wheel	},
	{ "[P",		Key::Break	},
	{ "[[A",	Key::F1		},
	{ "[[B",	Key::F2		},
	{ "[[C",	Key::F3		},
	{ "[[D",	Key::F4		},
	{ "[[E",	Key::F5		}
    };
    //}}}2
    const EscSeq* match = nullptr;
    for (auto& sm : c_seq) {
	match = &sm;
	for (auto i = 0u; i < size(sm.s) && sm.s[i]; ++i) {
	    if (i >= n || sm.s[i] != s[i]) {
		match = nullptr;
		break;
	    }
	}
	if (match)
	    break;
    }
    return match;
}

void TerminalScreen::parse_keycodes (void)
{
    if (_windows.empty() || !_windows.back()->is_mapped())
	return;
    istream is (_tin.data(), _tin.size());
    while (is.remaining()) {
	auto ic = is.ptr<char>();

	auto cb = utf8::ibytes (*ic);
	if (is.remaining() < cb)
	    break;	// incomplete multibyte char
	Event::key_t c = *utf8::in(ic);
	is.skip (cb);
	ic = is.ptr<char>();

	if (c == 8)
	    c = Key::Backspace;
	else if (c == '\t')
	    c = Key::Tab;
	else if (c == '\n')
	    c = Key::Enter;
	else if (c < 27)	// 1-26 is ctrl+a - ctrl+z with exceptions of backspace, tab, and newline above
	    c = KMod::Ctrl+('a'-1+c);
	else if (c == 27) {	// Esc key or a compound key sequence
	    auto ematch = match_escape_sequence (ic, is.end()-ic);
	    if (ematch) {	// compound sequence
		c = ematch->k;
		is.skip (4-(ematch->s[3]?0:(ematch->s[2]?1:2))); // sequences are 2-4 bytes
	    } else if (_tin.capacity() <= _tin.size())
		break;		// possible incomplete sequence, try again later
	    else if (ic < is.end() && *ic >= ' ' && *ic <= '~') {
		// Alt+key for printable characters is Esc+key
		c = KMod::Alt+*ic;
		is.skip(1);
	    } else		// fallback to plain Esc
		c = Key::Escape;
	} else if (c == 28)
	    c = Key::Print;
	else if (c == 127)
	    c = Key::Backspace;
	_windows.back()->on_event (Event (Event::Type::KeyDown, c));
    }
    _tin.erase (_tin.begin(), is.begin()-_tin.begin());
    if (_tin.capacity() > _tin.size() && !flag (f_InputEOF))
	_ptermi.wait_read (STDIN_FILENO);
}

//}}}-------------------------------------------------------------------
//{{{ TerminalScreenWindow

TerminalScreenWindow::TerminalScreenWindow (const Msg::Link& l)
: Msger (l)
,_surface()
,_viewport()
,_pos()
,_caret (-1,-1)
,_attr (Surface::default_cell())
,_reply (l)
,_winfo()
{
    TerminalScreen::instance().register_window (this);
}

TerminalScreenWindow::~TerminalScreenWindow (void)
{
    TerminalScreen::instance().unregister_window (this);
}

bool TerminalScreenWindow::dispatch (Msg& msg)
{
    return PScreen::dispatch (this, msg)
	|| Msger::dispatch (msg);
}

void TerminalScreenWindow::on_event (const Event& ev)
{
    if (flag (f_Unused))
	return;
    if (ev.type() == Event::Type::VSync && (flag (f_DrawInProgress) || flag (f_DrawPending))) {
	set_flag (f_DrawInProgress, false);
	if (flag (f_DrawPending))
	    return draw();	// for multiple draws per frame, only send vsync for the last one
    }
    _reply.event (ev);
}

//}}}-------------------------------------------------------------------
//{{{ TerminalScreenWindow sizing and layout

void TerminalScreenWindow::Screen_open (const WindowInfo& wi)
{
    _winfo = wi;
    on_resize (clip_to_screen());
}

void TerminalScreenWindow::reset (void)
{
    _viewport = interior_area();
    _attr = Surface::default_cell();
    _pos = Point();
    _caret = Point(-1,-1);
    _surface.clear();
}

void TerminalScreenWindow::on_resize (const Rect& warea)
{
    _winfo.set_area (warea);
    _surface.resize (_winfo.area().size());
    _reply.resize (_winfo);
    reset();
}

void TerminalScreenWindow::on_new_screen_info (void)
{
    if (auto newarea = clip_to_screen(); newarea != area())
	on_resize (newarea);
    _reply.screen_info (screen_info());
}

//}}}-------------------------------------------------------------------
//{{{ TerminalScreenWindow drawing operations

void TerminalScreenWindow::Draw_reset (void)
{
    reset();
    TerminalScreen::instance().reset();
}

void TerminalScreenWindow::Draw_enable (uint8_t f)
{
    if (f < Drawlist::Feature::Last)
	set_bit (_attr.attr, f);
}

void TerminalScreenWindow::Draw_disable (uint8_t f)
{
    if (f < Drawlist::Feature::Last)
	set_bit (_attr.attr, f, false);
}

void TerminalScreenWindow::Draw_move_to (const Point& p)
    { _pos = _viewport.pos() + p; }
void TerminalScreenWindow::Draw_move_by (const Offset& o)
    { _pos += o; }
void TerminalScreenWindow::Draw_viewport (const Rect& vp)
{
    // Viewport must always be inside the window rect
    _viewport = interior_area().clip (vp);
    _pos = _viewport.pos();
}

icolor_t TerminalScreenWindow::clip_color (icolor_t c, Surface::Attr::EAttr fattr)
{
    if (screen_info().depth() < 4)
	set_bit (_attr.attr, fattr, c >= 8);
    if (c != IColor::Default)
	c &= (1u<<screen_info().depth())-1;
    return c;
}

void TerminalScreenWindow::Draw_draw_color (icolor_t c)
    { _attr.fg = clip_color (c, Surface::Attr::Bold); }
void TerminalScreenWindow::Draw_fill_color (icolor_t c)
    { _attr.bg = clip_color (c, Surface::Attr::Blink); }

void TerminalScreenWindow::Draw_char (char32_t c, HAlign, VAlign)
{
    if (_viewport.contains (_pos))
	*_surface.iat(_pos) = cell_from_char (c);
    ++_pos.x;
}

void TerminalScreenWindow::Draw_char_bar (const Size& wh, char32_t c)
{
    auto orect = _viewport.clip (Rect (_pos, wh));
    if (orect.empty())
	return;
    auto o = _surface.iat (orect.pos());
    auto rowskip = _surface.size().w - orect.w;
    auto oc = cell_from_char (c);
    for (auto r = orect.h; r--; o += rowskip) {
	for (auto x = orect.w; x--; ++o) {
	    o->c = oc.c;
	    if (oc.c.c[0] == ' ') {
		o->bg = oc.bg;
		o->attr = oc.attr;
	    } else {
		o->fg = oc.fg;
		o->attr |= oc.attr;
	    }
	}
    }
}

void TerminalScreenWindow::Draw_edit_text (const string& t, uint32_t cp, HAlign ha, VAlign va)
{
    auto nlines = 1u + count (t,'\n');
    if (va == VAlign::Center)
	_pos.y -= nlines/2;
    else if (va == VAlign::Bottom)
	_pos.y -= nlines;

    auto lsz = 0u;
    auto tx = _pos.x, ly = _pos.y;

    vector<char32_t> wt (t.length());
    copy (t.wbegin(), t.wend(), wt.begin());

    // Caret position iterator
    auto cpi = wt.begin();
    if (cp <= wt.size()) {
	cpi += cp;
	// Special case for empty string that draws nothing
	if (wt.empty())
	    _caret = _pos;
    }

    for (auto l = wt.begin(), tend = wt.end(); l < tend; ++ly) {
	auto lend = find (l, tend, char32_t('\n'));
	if (!lend)
	    lend = tend;
	lsz = lend-l;

	// Clip lines above and below the viewport
	if (dim_t(ly-_viewport.y) < _viewport.h) {
	    auto lx = tx;
	    if (ha == HAlign::Center)
		lx -= lsz/2;
	    else if (ha == HAlign::Right)
		lx -= lsz;

	    // Clip to left edge of viewport
	    auto vlx = lx - _viewport.x;
	    if (vlx < 0) {
		l -= vlx;
		lx -= vlx;
	    }

	    // l is now the first visible char, line invisible if l is past lend
	    if (l < lend) {
		// Clip against the right side of the viewport
		auto nvis = min (lend-l, (_viewport.x + _viewport.w) - lx);
		auto vislend = l + nvis;

		// Set the caret position if it is on this line and visible
		if (vislend >= cpi && l <= cpi) {
		    _caret.x = lx + (cpi-l);
		    _caret.y = ly;
		}

		// Draw the actual characters
		for (auto o = _surface.iat (lx, ly); l < vislend; ++o, ++l) {
		    auto cc = cell_from_char (*l);
		    o->c = cc.c;
		    o->fg = cc.fg;
		    o->attr |= cc.attr;
		}
	    }
	}
	l = lend+1;	// go to next line
    }
    // Set end _pos.
    // End position for Center and Right alignments is the left edge
    // taking advantage of text measurement needed to draw the text.
    _pos.y = ly-1;
    _pos.x = tx;
    if (ha == HAlign::Center)
	_pos.x -= lsz/2;
    else if (ha == HAlign::Right)
	_pos.x -= lsz;
    else
	_pos.x += lsz;
}

void TerminalScreenWindow::Draw_text (const string& t, HAlign ha, VAlign va)
{
    auto oldcaret = _caret;
    Draw_edit_text (t, 0, ha, va);
    _caret = oldcaret;
}

void TerminalScreenWindow::Draw_clear (void)
{
    Draw_move_to (Point());
    Draw_bar (_viewport.size());
}

void TerminalScreenWindow::Draw_line (const Offset& o)
{
    // The resulting position is at the end of the line
    auto newpos = _pos + o;

    // Horizontal and vertical lines are drawn with different chars
    Drawlist::GChar lc;
    // Normalize line direction and bar thickness
    Size lsz;
    if (!o.dx) { // vertical line
	lsz.w = 1; // vertical lines are 1 char wide
	lc = Drawlist::GChar::VLine;
	// Reverse line direction if offset is negative
	if (o.dy < 0) {
	    _pos.y += o.dy;
	    lsz.h = -o.dy;
	} else
	    lsz.h = o.dy;
    } else { // horizontal line
	lsz.h = 1;
	lc = Drawlist::GChar::HLine;
	if (o.dx < 0) {
	    _pos.x += o.dx;
	    lsz.w = -o.dx;
	} else
	    lsz.w = o.dx;
    }
    Draw_char_bar (lsz, char32_t(lc));
    _pos = newpos;
}

void TerminalScreenWindow::Draw_box (const Size& wh)
{
    const Offset sides[4] = {
	{coord_t(wh.w-1),0},
	{0,coord_t(wh.h-2)},
	{coord_t(-(wh.w-1)),0},
	{0,coord_t(-(wh.h-1))}
    };
    static constexpr const Drawlist::GChar cornchar[4] = {
	Drawlist::GChar::URCorner,
	Drawlist::GChar::LRCorner,
	Drawlist::GChar::LLCorner,
	Drawlist::GChar::ULCorner
    };
    for (auto i = 0u; i < size(sides); --_pos.x,_pos.y+=!i++) {
	Draw_line (sides[i]);
	Draw_char (char32_t(cornchar[i]));
    }
}

void TerminalScreenWindow::Draw_bar (const Size& wh)
    { Draw_char_bar (wh, ' '); }

void TerminalScreenWindow::Draw_panel (const Size& wh, PanelType t)
{
    auto oldattr = _attr.attr;
    if (t == PanelType::Raised || t == PanelType::Button || t == PanelType::ButtonOn) {
	Draw_bar (wh);
	Draw_char ('[');
	_pos.x += wh.w-2;
	Draw_char (']');
	_pos.x -= wh.w-2;
    } else if (t == PanelType::Sunken || t == PanelType::Editbox || t == PanelType::FocusedEditbox) {
	set_bit (_attr.attr, Surface::Attr::Underline);
	if (t == PanelType::FocusedEditbox)
	    set_bit (_attr.attr, Surface::Attr::Reverse);
	Draw_bar (wh);
    } else if (t == PanelType::Selection || t == PanelType::Statusbar) {
	set_bit (_attr.attr, Surface::Attr::Reverse);
	Draw_bar (wh);
    } else if (t == PanelType::Checkbox)
	Draw_text ("[ ] ");
    else if (t == PanelType::CheckboxOn)
	Draw_text ("[x] ");
    else if (t == PanelType::Radio)
	Draw_text ("( ) ");
    else if (t == PanelType::RadioOn)
	Draw_text ("(*) ");
    else if (t == PanelType::MoreLeft)
	Draw_char ('<');
    else if (t == PanelType::MoreRight)
	Draw_char ('>');
    else if (t == PanelType::ProgressOn) {
	set_bit (_attr.attr, Surface::Attr::Reverse);
	Draw_char_bar (wh, ' ');
    } else if (t == PanelType::Progress)
	Draw_char_bar (wh, char32_t(Drawlist::GChar::Checkerboard));
    _attr.attr = oldattr;
}

void TerminalScreenWindow::Screen_draw (const cmemlink& dl)
{
    reset();
    Drawlist::dispatch (this, dl);
    draw();
}

void TerminalScreenWindow::draw (void)
{
    if (flag (f_DrawInProgress) || flag (f_Unused))
	set_flag (f_DrawPending);
    else {
	set_flag (f_DrawPending, false);
	set_flag (f_DrawInProgress);
	TerminalScreen::instance().draw_window (this);
    }
}

//}}}-------------------------------------------------------------------

} // namespace cwiclui
