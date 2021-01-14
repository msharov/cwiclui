// This file is part of the cwiclui project
//
// Copyright (c) 2019 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the ISC License.

#pragma once
#include "config.h"

//{{{ SettingsKey ------------------------------------------------------
namespace cwiclui {
using namespace cwiclo;

// A key is a set of entry=value pairs stored in consolidated form
// that is more space-efficient than the obvious vector of strings.
//
class SettingsKey {
public:
    //{{{2 value_type --------------------------------------------------
    struct value_type {
	const char*		name;
	const char*		value;
	const char*		description;
	const char*		next;
    public:
	constexpr auto	compare (const char* n) const		{ return __builtin_strcmp (name, n); }
	constexpr auto	compare (const value_type& v) const	{ return compare (v.name); }
	constexpr bool	operator== (const char* n) const	{ return 0 == compare (n); }
	constexpr bool	operator== (const value_type& v) const	{ return 0 == compare (v); }
	constexpr bool	operator!= (const char* n) const	{ return 0 != compare (n); }
	constexpr bool	operator!= (const value_type& v) const	{ return 0 != compare (v); }
	constexpr bool	operator< (const char* n) const		{ return 0 > compare (n); }
	constexpr bool	operator< (const value_type& v) const	{ return 0 > compare (v); }
	constexpr bool	operator> (const char* n) const		{ return 0 < compare (n); }
	constexpr bool	operator> (const value_type& v) const	{ return 0 < compare (v); }
	constexpr bool	operator<= (const char* n) const	{ return 0 >= compare (n); }
	constexpr bool	operator<= (const value_type& v) const	{ return 0 >= compare (v); }
	constexpr bool	operator>= (const char* n) const	{ return 0 <= compare (n); }
	constexpr bool	operator>= (const value_type& v) const	{ return 0 <= compare (v); }
	constexpr auto	str_value (void) const			{ return string_view (value, description-1); }
	constexpr bool	bool_value (void) const			{ return value && value[0] == 't'; }
	constexpr long	int_value (void) const			{ return value ? atol(value) : 0; }
	constexpr double float_value (void) const		{ return value ? atof(value) : 0; }
	auto		enum_value (const char* evn, size_t evnsz, unsigned nfv = 0) const
			    { return value ? zstr::index (value, evn, evnsz, nfv) : nfv; }
	template <typename E, unsigned N>
	auto		enum_value (const char (&evn)[N], E nfv = E()) const
			    { return E (enum_value (ARRAY_BLOCK(evn), unsigned(nfv))); }
    };
    //}}}2--------------------------------------------------------------
    using pointer		= value_type*;
    using reference		= value_type&;
    using const_pointer		= const value_type*;
    using const_reference	= const value_type&;
    using size_type		= string::size_type;
    using difference_type	= string::difference_type;
    //{{{2 const_iterator ----------------------------------------------
    class const_iterator {
	void scan_value (void) {
	    zstr::cii ei (_p, _n);
	    _v.name = *ei;
	    _v.value = *++ei;
	    _v.description = *++ei;
	    _v.next = *++ei;
	    _n = ei.remaining();
	}
    public:
	constexpr	const_iterator (const char* p)			:_p(p),_n(),_v{} {}
			const_iterator (const char* p, size_type n)	:_p(p),_n(n),_v{} { scan_value(); }
			const_iterator (const zstr::cii& i)		: const_iterator (*i,i.remaining()) {}
	constexpr	operator bool (void) const			{ return _p != nullptr; }
	auto&		operator++ (void)				{ _p = _v.next; scan_value(); return *this; }
	auto		operator++ (int)				{ auto r (*this); ++*this; return r; }
	auto&		operator+= (unsigned n)				{ while (n--) ++*this; return *this; }
	auto		operator+ (unsigned n)				{ auto r (*this); r += n; return r; }
	constexpr auto	base (void) const				{ return _p; }
	constexpr auto	remaining (void) const				{ return _n; }
	constexpr auto&	operator* (void) const				{ return _v; }
	constexpr auto	operator-> (void) const				{ return &_v; }
	constexpr bool	operator== (const const_iterator& o) const	{ return base() == o.base(); }
	constexpr bool	operator!= (const const_iterator& o) const	{ return base() != o.base(); }
	constexpr bool	operator< (const const_iterator& o) const	{ return base() < o.base(); }
	constexpr bool	operator> (const const_iterator& o) const	{ return base() > o.base(); }
	constexpr bool	operator<= (const const_iterator& o) const	{ return base() <= o.base(); }
	constexpr bool	operator>= (const const_iterator& o) const	{ return base() >= o.base(); }
    private:
	const char*	_p;
	size_type	_n;
	value_type	_v;
    };
    //}}}2--------------------------------------------------------------
    using iterator	= const_iterator;
    static constexpr const char c_Bool_names[] = "false\0true";
private:
    constexpr auto	ebegin (void) const	{ return zstr::cii (_entries.data(), _entries.size()); }
    constexpr auto	ebegin (void)		{ return zstr::ii (_entries.data(), _entries.size()); }
public:
    explicit		SettingsKey (const char* path = "", const char* filename = "", time_t modified = 0) NONNULL();
    explicit		SettingsKey (const string_view& path, const string_view& filename = "", time_t modified = 0)
								: SettingsKey (path.c_str(), filename.c_str(), modified) {}
    explicit		SettingsKey (const SettingsKey& k)	: _entries(k._entries), _modified(k._modified) {}
    constexpr		SettingsKey (SettingsKey&& k)		: _entries(move(k._entries)), _modified(k._modified) {}
    constexpr auto&	operator= (SettingsKey&& k)		{ _entries = move(k._entries); _modified = k._modified; return *this; }
    auto&		operator= (const SettingsKey& k)	{ _entries = k._entries; _modified = k._modified; return *this; }
    constexpr auto	path (void) const	{ return _entries.data(); }
    constexpr auto	name (void) const	{ auto n = __builtin_strrchr (path(), '/'); return n ? n+1 : path(); }
    auto		filename (void) const	{ return zstr::next (path()); }
    void		set_filename (const string_view& filename);
    constexpr auto	modified (void) const	{ return _modified; }
    constexpr void	set_modified (time_t t)	{ _modified = t; }
    constexpr auto	compare (const char* cpath) const	{ return strcmp (path(), cpath); }
    constexpr auto	compare (const string_view& cpath)const	{ return strncmp (path(), cpath.c_str(), cpath.size()); }
    constexpr auto	compare (const SettingsKey& k) const	{ return compare (k.path()); }
    #define SETTINGS_KEY_CMP_OPERATOR(fn,op)\
    constexpr bool	fn (const char* path) const		{ return compare (path) op 0; }\
    constexpr bool	fn (const string_view& path) const	{ return compare (path) op 0; }\
    constexpr bool	fn (const SettingsKey& k) const		{ return compare (k) op 0; }
			SETTINGS_KEY_CMP_OPERATOR (operator==, ==)
			SETTINGS_KEY_CMP_OPERATOR (operator!=, !=)
			SETTINGS_KEY_CMP_OPERATOR ( operator<,  <)
			SETTINGS_KEY_CMP_OPERATOR ( operator>,  >)
			SETTINGS_KEY_CMP_OPERATOR (operator<=, <=)
			SETTINGS_KEY_CMP_OPERATOR (operator>=, >=)
    #undef SETTINGS_KEY_CMP_OPERATOR
    auto		begin (void) const	{ return const_iterator (ebegin()+2u); }
    iterator		begin (void)		{ return as_const(*this).begin(); }
    auto		cbegin (void) const	{ return begin(); }
    auto		end (void) const	{ return const_iterator (_entries.end()); }
    auto		end (void)		{ return iterator (_entries.end()); }
    auto		cend (void) const	{ return end(); }
    auto&		at (unsigned i) const	{ return *(begin()+i); }
    auto&		operator[] (unsigned i) const	{ return at(i); }
    constexpr size_type	max_size (void) const	{ return _entries.max_size()/4; }
    size_type		size (void) const;
    bool		empty (void) const	{ return !begin().remaining(); }
    void		shrink_to_fit (void)	{ _entries.shrink_to_fit(); }
    void		clear (void)		{ auto eb = begin().base(); _entries.erase (eb, _entries.end()-eb); }
    void		erase (iterator ei)	{ _entries.erase (ei->name, ei->next-ei->name); }
    void		subtract (const SettingsKey& k);
    void		merge (const SettingsKey& k);
    bool		is_valid (void) const;
    constexpr bool	is_on_path (const string_view& p) const
			    { return 0 == __builtin_strncmp (p.c_str(), path(), p.size()); }
    const_iterator	get_entry (const char* name) const;
    void		get_entry_str (const char* name, string& s) const;
    void		get_entry_str (const char* name, string& s, const string_view& dv) const;
    void		get_entry_str (const char* name, string_view& s) const;
    void		get_entry_str (const char* name, string_view& s, const string_view& dv) const;
    long		get_entry_int (const char* name, long dv = 0) const;
    double		get_entry_float (const char* name, double dv = 0) const;
    unsigned		get_entry_enum (const char* name, const char* evn, size_t evnsz, unsigned dev = 0) const;
    template <typename E, unsigned N>
    E			get_entry_enum (const char* name, const char (&evn)[N], E dev = E()) const
			    { return E(get_entry_enum (name, ARRAY_BLOCK(evn), unsigned(dev))); }
    bool		get_entry_bool (const char* name, bool dv = false) const;
    void		get_entry_array (const char* name, vector<string>& v) const;
    void		get_entry_array (const char* name, vector<string_view>& v) const;
    void		set_entry (const char* name, const char* value, const char* desc = nullptr);
    void		set_entry_int (const char* name, long value, const char* desc, long dv = 0);
    void		set_entry_int (const char* name, long value, long dv = 0)
			    { set_entry_int (name, value, nullptr, dv); }
    void		set_entry_float (const char* name, double value, const char* desc = nullptr, double dv = 0);
    void		set_entry_float (const char* name, double value, double dv = 0)
			    { set_entry_int (name, value, nullptr, dv); }
    void		set_entry_str (const char* name, const string_view& value);
    void		set_entry_str (const char* name, const string_view& value, const string_view& dv);
    void		set_entry_enum (const char* name, unsigned ev, const char* evn, size_t evnsz, unsigned evd = 0);
    template <typename E, unsigned N>
    void		set_entry_enum (const char* name, E ev, const char (&evn)[N], E evd = E())
			    { set_entry_enum (name, unsigned(ev), ARRAY_BLOCK(evn), unsigned(evd)); }
    void		set_entry_bool (const char* name, bool value, bool dv = false)
			    { set_entry_enum (name, value, c_Bool_names, dv); }
    void		set_entry_array (const char* name, const vector<string>& v);
    void		set_entry_array (const char* name, const vector<string_view>& v);
    void		delete_entry (const char* name)
			    { set_entry (name, nullptr); }
    void		read (istream& is);
    void		write (sstream& os) const;
    void		write (ostream& os) const;
    static constexpr const streamsize stream_alignment = string::stream_alignment;
private:
    memblaz		_entries;
    time_t		_modified;
};

#define SIGNATURE_SettingsKey	"ssua(sss)"

//}}}-------------------------------------------------------------------
//{{{ Settings

class Settings {
    using keyset_t	= multiset<SettingsKey>;
public:
    using value_type		= keyset_t::value_type;
    using size_type		= keyset_t::size_type;
    using difference_type	= keyset_t::difference_type;
    using pointer		= keyset_t::pointer;
    using const_pointer		= keyset_t::const_pointer;
    using reference		= keyset_t::reference;
    using const_reference	= keyset_t::const_reference;
    using iterator		= keyset_t::iterator;
    using const_iterator	= keyset_t::const_iterator;
public:
			Settings (void)		:_keys() {}
    auto		size (void) const	{ return _keys.size(); }
    auto		max_size (void) const	{ return _keys.max_size(); }
    auto		capacity (void) const	{ return _keys.capacity(); }
    auto		empty (void) const	{ return _keys.empty(); }
    auto		begin (void) const	{ return _keys.begin(); }
    auto		begin (void)		{ return _keys.begin(); }
    auto		end (void) const	{ return _keys.end(); }
    auto		end (void)		{ return _keys.end(); }
    auto		iat (size_type i) const	{ return _keys.iat(i); }
    auto		iat (size_type i)	{ return _keys.iat(i); }
    auto&		at (size_type i) const	{ return _keys.at(i); }
    auto&		at (size_type i)	{ return _keys.at(i); }
    auto&		operator[] (size_type i) const	{ return at(i); }
    auto&		operator[] (size_type i)	{ return at(i); }
    void		link (pointer p, size_type n)	{ _keys.link (p,n); }
    void		link (iterator p, iterator q)	{ _keys.link (p,q); }
    void		link (keyset_t& v)	{ _keys.link (v); }
    void		link (Settings& v)	{ _keys.link (v._keys); }
    void		clear (void)		{ _keys.clear(); }
    void		deallocate (void)	{ _keys.deallocate(); }
    auto		erase (const_iterator ep, size_type n = 1)	{ return _keys.erase (ep,n); }
    auto		erase (const_iterator ep, const_iterator epe)	{ return _keys.erase (ep,epe); }
    void		shrink_to_fit (void)	{ _keys.shrink_to_fit(); }
    void		swap (Settings& s)	{ _keys.swap (s._keys); }
    void		read (istream& is)	{ _keys.read (is); }
    void		write (sstream& os) const	{ _keys.write (os); }
    void		write (ostream& os) const	{ _keys.write (os); }
    void		merge_key (const SettingsKey& k);
    void		merge_key (SettingsKey&& k);
    void		merge (const Settings& s)	{ for (auto& k : s) merge_key (k); }
    void		merge (Settings&& s)		{ for (auto& k : s) merge_key (move(k)); }
    void		set_key (const SettingsKey& k);
    void		set_key (SettingsKey&& k);
    void		set_keys (const Settings& s)	{ for (auto& k : s) set_key (k); }
    void		set_keys (Settings&& s)		{ for (auto& k : s) set_key (move(k)); }
    bool		is_valid (void) const;
    auto		filename (void) const	{ return empty() ? "" : at(0).filename(); }
    void		set_filename (const char* fn)	{ if (!empty()) at(0).set_filename (fn); }
    auto		modified (void) const	{ return empty() ? 0 : at(0).modified(); }
    void		set_modified (time_t t)	{ if (!empty()) at(0).set_modified(t); }
    auto		get_key (const char* key) const NONNULL()
			    { return binary_search (begin(), end(), key); }
    auto		get_key (const char* key) NONNULL()
			    { return UNCONST_MEMBER_FN (get_key,key); }
    void		match (Settings& result, const string_view& path, unsigned depth = UINT_MAX) const;
    auto		match (const string_view& path, unsigned depth = UINT_MAX) const
			    { Settings r; match (r, path, depth); return r; }
    void		extract_match_from (Settings&& src, const string_view& path);
    reference		create_key (const char* key);
    size_type		delete_key (const string_view& key);
    void		create_all_paths (void);
    void		read_ini_file (const char* filename, const char* sfn = "") NONNULL();
    void		read_ini (string ini, const char* sfn = "", time_t mtime = 0) NONNULL();
    string		write_ini (void) const;
private:
    keyset_t		_keys;
};

#define SIGNATURE_Settings	"a(" SIGNATURE_SettingsKey ")"

//}}}-------------------------------------------------------------------
//{{{ PSettings

class PSettings : public Proxy {
    DECLARE_INTERFACE_E (Proxy, Settings,
	(get_keys, "qqs")
	(set_keys, SIGNATURE_Settings)
	(delete_key, "s")
	(delete_entry, "ss")
	(flush, "")
	(keys, SIGNATURE_Settings)
	(flushed,""),
	"@~cwiclui/settings.socket","p1u"
    )
public:
    enum class Scope : uint16_t { Merged, User, System };
public:
    explicit	PSettings (mrid_t caller)		: Proxy (caller) {}
    void	get_keys (const string_view& path = "", uint16_t depth = UINT16_MAX, Scope scope = Scope::Merged) const
							{ send (m_get_keys(), depth, scope, path); }
    void	get_key (const string_view& path) const	{ get_keys (path, 0); }
    void	set_keys (const Settings& keys) const	{ send (m_set_keys(), keys); }
    void	set_key (SettingsKey& key) const	{ Settings keys; keys.link (&key, 1); set_keys (keys); }
    void	delete_key (const string_view& path) const	{ send (m_delete_key(), path); }
    void	delete_entry (const string_view& path, const string_view& entry) const
							{ send (m_delete_entry(), path, entry); }
    void	flush (void) const			{ send (m_flush()); }
    template <typename O>
    inline constexpr static bool dispatch (O* o, const Msg& msg) {
	if (msg.method() == m_get_keys()) {
	    auto is = msg.read();
	    auto depth = is.read<uint16_t>();
	    auto scope = is.read<Scope>();
	    o->Settings_get_keys (is.read<string_view>(), depth, scope);
	} else if (msg.method() == m_set_keys())
	    o->Settings_set_keys (msg.read().read<Settings>());
	else if (msg.method() == m_delete_key())
	    o->Settings_delete_key (msg.read().read<string_view>());
	else if (msg.method() == m_delete_entry()) {
	    auto is = msg.read();
	    auto path = is.read<string_view>();
	    o->Settings_delete_entry (path, is.read<string_view>());
	} else if (msg.method() == m_flush())
	    o->Settings_flush();
	else
	    return false;
	return true;
    }
public:
    class Reply : public Proxy::Reply {
    public:
	constexpr explicit Reply (Msg::Link l)		: Proxy::Reply(l) {}
	void	keys (const Settings& keys) const	{ send (m_keys(), keys); }
	void	flushed (void) const			{ send (m_flushed()); }
	template <typename O>
	inline constexpr static bool dispatch (O* o, const Msg& msg) {
	    if (msg.method() == m_keys())
		o->Settings_keys (msg.read().read<Settings>());
	    else if (msg.method() == m_flushed())
		o->Settings_flushed();
	    else
		return false;
	    return true;
	}
    };
};

} // namespace cwiclui
//}}}-------------------------------------------------------------------
